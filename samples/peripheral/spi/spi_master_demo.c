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
 * 本示例演示 SPI 以主机模式（Master）工作的完整流程：
 * - GPIO 复用配置到 SPI 主机引脚
 * - 配置 SPI 控制器为主机、设置 CPOL/CPHA、帧格式与位宽、速率等
 * - （可选）配置 DMA 与中断模式
 * - 在独立任务中循环执行：先发送一段递增数据，再读取从机返回的数据并打印
 *
 * 若启用 QSPI（四线）示例，会切换到四线写配置并带指令/地址字段。
 *
 * 典型连线（对接从机示例）：
 * - 主机 CLK -> 从机 CLK
 * - 主机 CS  -> 从机 CS
 * - 主机 MOSI(DO) -> 从机 MOSI(DI)
 * - 主机 MISO(DI) -> 从机 MISO(DO)
 *
 * 关键编译期宏（通常由 Kconfig/Board 配置提供）：
 * - CONFIG_SPI_MASTER_BUS_ID：SPI 控制器总线号
 * - CONFIG_SPI_*_MASTER_PIN：各 SPI 引脚编号
 * - CONFIG_SPI_MASTER_PIN_MODE：对应引脚复用模式
 * - CONFIG_SPI_TRANSFER_LEN：单次传输的字节数
 * - CONFIG_SPI_SUPPORT_DMA：是否启用 DMA 传输
 * - CONFIG_SPI_SUPPORT_POLL_AND_DMA_AUTO_SWITCH：是否自动在轮询/DMA 间切换
 * - CONFIG_SPI_SUPPORT_INTERRUPT：是否启用中断收发回调
 * - CONFIG_SPI_MASTER_SUPPORT_QSPI：是否启用 QSPI 四线相关演示
 */

#define SPI_SLAVE_NUM                   1    /* 从设备数量/片选参数，具体含义依 HAL 而定 */
#define SPI_FREQUENCY                   2    /* 期望的 SPI 工作速率（MHz 级，配合 bus_clk 实际分频） */
#define SPI_CLK_POLARITY                0    /* CPOL：0=空闲低，1=空闲高 */
#define SPI_CLK_PHASE                   0    /* CPHA：0/1 对应采样/翻转时刻 */
#define SPI_FRAME_FORMAT                0    /* 控制器内部帧格式模式（与 HAL 定义对应） */
#define SPI_FRAME_FORMAT_STANDARD       0    /* 标准 SPI 帧（非 QSPI 等扩展） */
#define SPI_FRAME_SIZE_8                0x1f /* 帧位宽配置字段，对应 8bit（具体取值见 HAL 定义） */
#define SPI_TMOD                        0    /* 传输模式（TX/RX/双向），0 通常为双向 */
#define SPI_WAIT_CYCLES                 0x10 /* QSPI 等扩展场景下的等待周期 */
#if defined(CONFIG_SPI_SUPPORT_DMA) && !(defined(CONFIG_SPI_SUPPORT_POLL_AND_DMA_AUTO_SWITCH))
#define SPI_DMA_WIDTH                   2    /* DMA 传输宽度，单位字节 */
#endif
#if defined(CONFIG_SPI_MASTER_SUPPORT_QSPI)
#define QSPI_WRITE_CMD                  0x38 /* 示例：QSPI 写指令 */
#define QSPI_WRITE_ADDR                 0x123/* 示例：QSPI 写地址 */
#endif
#define SPI_TASK_DURATION_MS            500  /* 任务循环间隔（毫秒） */
#define SPI_TASK_PRIO                   24   /* 任务优先级 */
#define SPI_TASK_STACK_SIZE             0x1000 /* 任务栈大小（字节） */

/**
 * 初始化 SPI 主机相关引脚的复用功能。
 */
static void app_spi_init_pin(void)
{
    uapi_pin_set_mode(CONFIG_SPI_DI_MASTER_PIN, CONFIG_SPI_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_DO_MASTER_PIN, CONFIG_SPI_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_CLK_MASTER_PIN, CONFIG_SPI_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_CS_MASTER_PIN, CONFIG_SPI_MASTER_PIN_MODE);
#if defined(CONFIG_SPI_MASTER_SUPPORT_QSPI)
    /* QSPI 额外两根数据线 */
    uapi_pin_set_mode(CONFIG_SPI_MASTER_D2_PIN, CONFIG_SPI_MASTER_D2_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_MASTER_D3_PIN, CONFIG_SPI_MASTER_D3_PIN_MODE);
#endif
}

#if defined(CONFIG_SPI_SUPPORT_INTERRUPT) && (CONFIG_SPI_SUPPORT_INTERRUPT == 1)
/**
 * SPI 主机写入（发送开始/完成）中断通知回调。
 *
 * @param buffer  实际无关，演示用
 * @param length  实际无关，演示用
 */
static void app_spi_master_write_int_handler(const void *buffer, uint32_t length)
{
    unused(buffer);
    unused(length);
    osal_printk("spi master write interrupt start!\r\n");
}

/**
 * SPI 主机接收完成中断回调。
 *
 * @param buffer  指向接收缓冲区的指针
 * @param length  接收的字节数
 * @param error   是否发生错误（由底层驱动给出）
 */
static void app_spi_master_rx_callback(const void *buffer, uint32_t length, bool error)
{
    if (buffer == NULL || length == 0) {
        osal_printk("spi master transfer illegal data!\r\n");
        return;
    }
    if (error) {
        osal_printk("app_spi_master_read_int error!\r\n");
        return;
    }

    /* 演示用途：逐字节打印显示接收到的数据 */
    uint8_t *buff = (uint8_t *)buffer;
    for (uint32_t i = 0; i < length; i++) {
        osal_printk("buff[%d] = %x\r\n", i, buff[i]);
    }
    osal_printk("app_spi_master_read_int success!\r\n");
}
#endif  /* CONFIG_SPI_SUPPORT_INTERRUPT */

/**
 * 配置并初始化 SPI 主机控制器。
 * - 标准 SPI：设置双向传输、8bit 帧、CPOL/CPHA、速率等
 * - QSPI 场景：切换为四线写，配置指令长度/地址长度/等待周期等
 * - 可选开启 DMA 模式与中断模式
 */
static void app_spi_master_init_config(void)
{
    spi_attr_t config = { 0 };
    spi_extra_attr_t ext_config = { 0 };

    config.is_slave = false;                   /* 主机模式 */
    config.slave_num = SPI_SLAVE_NUM;          /* 对应从设备数量/索引 */
    config.bus_clk = SPI_CLK_FREQ;             /* 控制器输入时钟（Hz），来自平台定义 */
    config.freq_mhz = SPI_FREQUENCY;           /* 目标通信速率（MHz 级） */
    config.clk_polarity = SPI_CLK_POLARITY;    /* CPOL */
    config.clk_phase = SPI_CLK_PHASE;          /* CPHA */
    config.frame_format = SPI_FRAME_FORMAT;    /* 控制器内部帧格式 */
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_STANDARD; /* 标准 SPI 帧 */
    config.frame_size = SPI_FRAME_SIZE_8;      /* 帧位宽：8bit */
    config.tmod = SPI_TMOD;                    /* 传输模式（双向） */
    config.sste = 0;                           /* 主机侧 SSTE 相关开关 */

    ext_config.qspi_param.wait_cycles = SPI_WAIT_CYCLES; /* 默认等待周期 */
#if defined(CONFIG_SPI_MASTER_SUPPORT_QSPI)
    /* 切换为 QSPI 四线写配置（仅当使能 QSPI 示例时生效） */
    config.tmod = HAL_SPI_TRANS_MODE_TX;                       /* 仅发送 */
    config.sste = 0;
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_QUAD;       /* 四线模式 */
    ext_config.qspi_param.trans_type = HAL_SPI_TRANS_TYPE_INST_S_ADDR_Q; /* 指令单线，地址四线 */
    ext_config.qspi_param.inst_len = HAL_SPI_INST_LEN_8;       /* 8bit 指令 */
    ext_config.qspi_param.addr_len = HAL_SPI_ADDR_LEN_24;      /* 24bit 地址 */
    ext_config.qspi_param.wait_cycles = 0;                     /* QSPI 等待周期 */
#endif

    uapi_spi_init(CONFIG_SPI_MASTER_BUS_ID, &config, &ext_config);
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
    if (uapi_spi_set_dma_mode(CONFIG_SPI_MASTER_BUS_ID, true, &dma_cfg) != ERRCODE_SUCC) {
        osal_printk("spi%d master set dma mode fail!\r\n");
    }
#endif
#endif  /* CONFIG_SPI_SUPPORT_DMA */

#if defined(CONFIG_SPI_SUPPORT_INTERRUPT) && (CONFIG_SPI_SUPPORT_INTERRUPT == 1)
    /* 可选：使能中断模式并注册回调。开启后收发完成会通过回调通知。 */
    if (uapi_spi_set_irq_mode(CONFIG_SPI_MASTER_BUS_ID, true, app_spi_master_rx_callback,
        app_spi_master_write_int_handler) == ERRCODE_SUCC) {
        osal_printk("spi%d master set irq mode succ!\r\n", CONFIG_SPI_MASTER_BUS_ID);
    }
#endif  /* CONFIG_SPI_SUPPORT_INTERRUPT */
}

/**
 * SPI 主机业务任务：周期性地向从机发送数据并读取回包。
 *
 * @param arg  任务参数（未使用）
 * @return     永不返回；若返回则为 NULL
 */
static void *spi_master_task(const char *arg)
{
    unused(arg);
    /* 配置 SPI 引脚复用 */
    app_spi_init_pin();

    /* 初始化 SPI 主机控制器参数 */
    app_spi_master_init_config();

    /*
     * 预分配收发缓冲区：
     * - tx_data：填充 0..N-1，方便从机校验
     * - rx_data：用于接收从机返回的数据
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
#if defined(CONFIG_SPI_MASTER_SUPPORT_QSPI)
        .cmd = QSPI_WRITE_CMD,   /* QSPI 指令 */
        .addr = QSPI_WRITE_ADDR, /* QSPI 地址 */
#endif
    };

    while (1) {
        osal_msleep(SPI_TASK_DURATION_MS);

        /* 步骤一：主机先发送一帧数据到从机 */
        osal_printk("spi%d master send start!\r\n", CONFIG_SPI_MASTER_BUS_ID);
        if (uapi_spi_master_write(CONFIG_SPI_MASTER_BUS_ID, &data, 0xFFFFFFFF) == ERRCODE_SUCC) {
            osal_printk("spi%d master send succ!\r\n", CONFIG_SPI_MASTER_BUS_ID);
        } else {
            /* 发送失败则跳过当前轮，重试 */
            continue;
        }

        /* 步骤二：随后读取从机返回的数据 */
        osal_printk("spi%d master receive start!\r\n", CONFIG_SPI_MASTER_BUS_ID);
        if (uapi_spi_master_read(CONFIG_SPI_MASTER_BUS_ID, &data, 0xFFFFFFFF) == ERRCODE_SUCC) {
#ifndef CONFIG_SPI_SUPPORT_INTERRUPT
            /* 轮询模式下：直接在调用处打印接收到的数据 */
            for (uint32_t i = 0; i < data.rx_bytes; i++) {
                osal_printk("spi%d master receive data is %x\r\n", CONFIG_SPI_MASTER_BUS_ID, data.rx_buff[i]);
            }
#endif
            osal_printk("spi%d master receive succ!\r\n", CONFIG_SPI_MASTER_BUS_ID);
        }
    }

    return NULL;
}

/**
 * SPI 主机示例入口：创建并启动业务任务。
 */
static void spi_master_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)spi_master_task, 0, "SpiMasterTask", SPI_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SPI_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* Run the spi_master_entry. */
app_run(spi_master_entry);