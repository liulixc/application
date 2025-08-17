/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: SPI Sample Source. \n
 *
 * History: \n
 * 2023-06-25, Create file. \n
 */
#include "pinctrl.h"
#include "spi.h"
#include "soc_osal.h"
#include "app_init.h"

/*
 * 本示例演示 SPI 以从机模式工作的完整流程：
 * - GPIO 复用配置到 SPI 从机引脚
 * - 配置 SPI 控制器为从机、设置时钟极性/相位、帧格式与位宽、速率、等待周期等
 * - （可选）配置 DMA 与中断模式
 * - 在独立任务中循环执行：先接收主机数据并打印，再发送一段递增数据给主机
 *
 * 接线建议（以常见命名对应关系为例，实际以板级定义为准）：
 * - 主机 CLK -> 从机 CLK
 * - 主机 CS  -> 从机 CS
 * - 主机 MOSI(DO) -> 从机 MOSI(DI)
 * - 主机 MISO(DI) -> 从机 MISO(DO)
 *
 * 关键编译期宏（通常由 Kconfig/Board 配置提供）：
 * - CONFIG_SPI_SLAVE_BUS_ID：SPI 控制器总线号
 * - CONFIG_SPI_*_SLAVE_PIN：各 SPI 引脚编号
 * - CONFIG_SPI_SLAVE_PIN_MODE：对应引脚复用模式
 * - CONFIG_SPI_TRANSFER_LEN：单次传输的字节数
 * - CONFIG_SPI_SUPPORT_DMA：是否启用 DMA 传输
 * - CONFIG_SPI_SUPPORT_POLL_AND_DMA_AUTO_SWITCH：是否自动在轮询/DMA 间切换
 * - CONFIG_SPI_SUPPORT_INTERRUPT：是否启用中断收发回调
 */

#define SPI_SLAVE_NUM                   1   /* 片选从设备数量（或索引范围），具体含义依 HAL 而定 */
#define SPI_FREQUENCY                   2   /* 期望的 SPI 工作速率（MHz 级，配合 bus_clk 实际分频） */
#define SPI_CLK_POLARITY                0   /* 时钟极性 CPOL：0=空闲为低，1=空闲为高 */
#define SPI_CLK_PHASE                   0   /* 时钟相位 CPHA：0/1 对应采样/翻转时刻 */
#define SPI_FRAME_FORMAT                0   /* 控制器内部帧格式模式（与 HAL 定义对应） */
#define SPI_FRAME_FORMAT_STANDARD       0   /* 标准 SPI 帧（非 QSPI 等扩展） */
#define SPI_FRAME_SIZE_8                0x1f/* 帧位宽配置字段，对应 8bit（具体取值见 HAL 定义） */
#define SPI_TMOD                        0   /* 传输模式（TX/RX/双向），0 通常为双向 */
#define SPI_WAIT_CYCLES                 0x10/* 等待周期（QSPI 等扩展用，这里从机仍给出默认值） */
#if defined(CONFIG_SPI_SUPPORT_DMA) && !(defined(CONFIG_SPI_SUPPORT_POLL_AND_DMA_AUTO_SWITCH))
#define SPI_DMA_WIDTH                   2   /* DMA 传输宽度，单位字节（例如 1=8bit, 2=16bit, 4=32bit） */
#endif

#define SPI_TASK_DURATION_MS            500 /* 任务循环间隔（毫秒），用于演示节奏控制 */
#define SPI_TASK_PRIO                   24  /* 任务优先级（数值越小优先级越高，取值依 OSAL 实现） */
#define SPI_TASK_STACK_SIZE             0x1000 /* 任务栈大小（字节） */

/**
 * 初始化 SPI 从机相关引脚的复用功能。
 * 必须保证与板级管脚定义一致，否则 SPI 无法正常工作。
 */
static void app_spi_init_pin(void)
{
    uapi_pin_set_mode(CONFIG_SPI_DI_SLAVE_PIN, CONFIG_SPI_SLAVE_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_DO_SLAVE_PIN, CONFIG_SPI_SLAVE_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_CLK_SLAVE_PIN, CONFIG_SPI_SLAVE_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_CS_SLAVE_PIN, CONFIG_SPI_SLAVE_PIN_MODE);
}

#if defined(CONFIG_SPI_SUPPORT_INTERRUPT) && (CONFIG_SPI_SUPPORT_INTERRUPT == 1)
/**
 * SPI 从机写入（发送完成）中断的通知回调。
 *
 * @param buffer  实际无关，发送完成仅作事件通知
 * @param length  实际无关，发送完成仅作事件通知
 */
static void app_spi_slave_write_int_handler(const void *buffer, uint32_t length)
{
    unused(buffer);
    unused(length);
    osal_printk("spi slave write interrupt start!\r\n");
}

/**
 * SPI 从机接收完成中断回调。
 *
 * @param buffer  指向接收缓冲区的指针
 * @param length  接收的字节数
 * @param error   是否发生错误（由底层驱动给出）
 */
static void app_spi_slave_rx_callback(const void *buffer, uint32_t length, bool error)
{
    if (buffer == NULL || length == 0) {
        osal_printk("spi slave transfer illegal data!\r\n");
        return;
    }
    if (error) {
        osal_printk("app_spi_slave_read_int error!\r\n");
        return;
    }

    /* 演示用途：逐字节打印显示接收到的数据 */
    uint8_t *buff = (uint8_t *)buffer;
    for (uint32_t i = 0; i < length; i++) {
        osal_printk("buff[%d] = %x\r\n", i, buff[i]);
    }
    osal_printk("app_spi_slave_read_int success!\r\n");
}
#endif  /* CONFIG_SPI_SUPPORT_INTERRUPT */

/**
 * 配置并初始化 SPI 从机控制器。
 * - 设置工作模式为从机
 * - 指定帧格式、位宽、极性/相位、速率
 * - 可选配置 DMA 模式与中断模式
 */
static void app_spi_slave_init_config(void)
{
    spi_attr_t config = { 0 };
    spi_extra_attr_t ext_config = { 0 };

    config.is_slave = true;                    /* 从机模式 */
    config.slave_num = SPI_SLAVE_NUM;          /* 片选/从设备号 */
    config.bus_clk = SPI_CLK_FREQ;             /* 控制器输入时钟（Hz），来自平台定义 */
    config.freq_mhz = SPI_FREQUENCY;           /* 目标通信速率（MHz 级），由驱动换算分频 */
    config.clk_polarity = SPI_CLK_POLARITY;    /* CPOL */
    config.clk_phase = SPI_CLK_PHASE;          /* CPHA */
    config.frame_format = SPI_FRAME_FORMAT;    /* 控制器内部帧格式 */
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_STANDARD; /* 标准 SPI 帧 */
    config.frame_size = SPI_FRAME_SIZE_8;      /* 帧位宽：8bit */
    config.tmod = SPI_TMOD;                    /* 传输模式（双向/只发/只收） */
    config.sste = 0;                           /* 从机片选扩展开关（依 HAL 而定） */

    ext_config.qspi_param.wait_cycles = SPI_WAIT_CYCLES; /* 预留字段（QSPI 等扩展） */

    uapi_spi_init(CONFIG_SPI_SLAVE_BUS_ID, &config, &ext_config);
#if defined(CONFIG_SPI_SUPPORT_DMA) && (CONFIG_SPI_SUPPORT_DMA == 1)
    /* 可选：初始化 DMA 并开启 SPI 的 DMA 模式，提高大块数据传输效率 */
    uapi_dma_init();
    uapi_dma_open();
#ifndef CONFIG_SPI_SUPPORT_POLL_AND_DMA_AUTO_SWITCH
    spi_dma_config_t dma_cfg = {
        .src_width = SPI_DMA_WIDTH,
        .dest_width = SPI_DMA_WIDTH,
        .burst_length = 0,
        .priority = 0
    };
    if (uapi_spi_set_dma_mode(CONFIG_SPI_SLAVE_BUS_ID, true, &dma_cfg) != ERRCODE_SUCC) {
        osal_printk("spi%d slave set dma mode fail!\r\n");
    }
#endif
#endif  /* CONFIG_SPI_SUPPORT_DMA */

#if defined(CONFIG_SPI_SUPPORT_INTERRUPT) && (CONFIG_SPI_SUPPORT_INTERRUPT == 1)
    /* 可选：使能中断模式并注册回调。开启后收发完成会通过回调通知。 */
    if (uapi_spi_set_irq_mode(CONFIG_SPI_SLAVE_BUS_ID, true, app_spi_slave_rx_callback,
        app_spi_slave_write_int_handler) == ERRCODE_SUCC) {
        osal_printk("spi%d slave set irq mode succ!\r\n", CONFIG_SPI_SLAVE_BUS_ID);
    }
#endif  /* CONFIG_SPI_SUPPORT_INTERRUPT */
}

/**
 * SPI 从机业务任务：周期性地接收主机数据并回发数据。
 *
 * @param arg  任务参数（未使用）
 * @return     永不返回；若返回则为 NULL
 */
static void *spi_slave_task(const char *arg)
{
    unused(arg);
    /* 配置 SPI 引脚复用 */
    app_spi_init_pin();

    /* 初始化 SPI 从机控制器参数 */
    app_spi_slave_init_config();

    /*
     * 预分配收发缓冲区：
     * - tx_data：填充 0..N-1，方便对端做简单校验
     * - rx_data：用于接收主机发送的数据
     */
    uint8_t tx_data[CONFIG_SPI_TRANSFER_LEN] = { 0 };
    for (uint32_t loop = 0; loop < CONFIG_SPI_TRANSFER_LEN; loop++) {
        tx_data[loop] = (loop & 0xFF);
    }
    uint8_t rx_data[CONFIG_SPI_TRANSFER_LEN] = { 0 };

    /* 组织一次传输需要的元信息 */
    spi_xfer_data_t data = {
        .tx_buff = tx_data,
        .tx_bytes = CONFIG_SPI_TRANSFER_LEN,
        .rx_buff = rx_data,
        .rx_bytes = CONFIG_SPI_TRANSFER_LEN,
    };

    while (1) {
        osal_msleep(SPI_TASK_DURATION_MS);

        /* 步骤一：先接收主机数据（主机需先发） */
        osal_printk("spi%d slave receive start!\r\n", CONFIG_SPI_SLAVE_BUS_ID);
        if (uapi_spi_slave_read(CONFIG_SPI_SLAVE_BUS_ID, &data, 0xFFFFFFFF) == ERRCODE_SUCC) {
#ifndef CONFIG_SPI_SUPPORT_INTERRUPT
            /* 轮询模式下：直接在调用处打印接收到的数据 */
            for (uint32_t i = 0; i < data.rx_bytes; i++) {
                osal_printk("spi%d slave receive data is %x\r\n", CONFIG_SPI_SLAVE_BUS_ID, data.rx_buff[i]);
            }
#endif
            osal_printk("spi%d slave receive succ!\r\n", CONFIG_SPI_SLAVE_BUS_ID);
        } else {
            /* 接收失败则继续下一轮 */
            continue;
        }

        /* 步骤二：回写一帧数据给主机 */
        osal_printk("spi%d slave send start!\r\n", CONFIG_SPI_SLAVE_BUS_ID);
        if (uapi_spi_slave_write(CONFIG_SPI_SLAVE_BUS_ID, &data, 0xFFFFFFFF) == ERRCODE_SUCC) {
            osal_printk("spi%d slave send succ!\r\n", CONFIG_SPI_SLAVE_BUS_ID);
        }
    }

    return NULL;
}

/**
 * SPI 从机示例入口：创建并启动业务任务。
 */
static void spi_slave_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)spi_slave_task, 0, "SpiSlaveTask", SPI_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SPI_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* Run the spi_slave_entry. */
app_run(spi_slave_entry);