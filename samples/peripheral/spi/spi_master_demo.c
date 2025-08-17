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
 * ��ʾ����ʾ SPI ������ģʽ��Master���������������̣�
 * - GPIO �������õ� SPI ��������
 * - ���� SPI ������Ϊ���������� CPOL/CPHA��֡��ʽ��λ�����ʵ�
 * - ����ѡ������ DMA ���ж�ģʽ
 * - �ڶ���������ѭ��ִ�У��ȷ���һ�ε������ݣ��ٶ�ȡ�ӻ����ص����ݲ���ӡ
 *
 * ������ QSPI�����ߣ�ʾ�������л�������д���ò���ָ��/��ַ�ֶΡ�
 *
 * �������ߣ��ԽӴӻ�ʾ������
 * - ���� CLK -> �ӻ� CLK
 * - ���� CS  -> �ӻ� CS
 * - ���� MOSI(DO) -> �ӻ� MOSI(DI)
 * - ���� MISO(DI) -> �ӻ� MISO(DO)
 *
 * �ؼ������ں꣨ͨ���� Kconfig/Board �����ṩ����
 * - CONFIG_SPI_MASTER_BUS_ID��SPI ���������ߺ�
 * - CONFIG_SPI_*_MASTER_PIN���� SPI ���ű��
 * - CONFIG_SPI_MASTER_PIN_MODE����Ӧ���Ÿ���ģʽ
 * - CONFIG_SPI_TRANSFER_LEN�����δ�����ֽ���
 * - CONFIG_SPI_SUPPORT_DMA���Ƿ����� DMA ����
 * - CONFIG_SPI_SUPPORT_POLL_AND_DMA_AUTO_SWITCH���Ƿ��Զ�����ѯ/DMA ���л�
 * - CONFIG_SPI_SUPPORT_INTERRUPT���Ƿ������ж��շ��ص�
 * - CONFIG_SPI_MASTER_SUPPORT_QSPI���Ƿ����� QSPI ���������ʾ
 */

#define SPI_SLAVE_NUM                   1    /* ���豸����/Ƭѡ���������庬���� HAL ���� */
#define SPI_FREQUENCY                   2    /* ������ SPI �������ʣ�MHz ������� bus_clk ʵ�ʷ�Ƶ�� */
#define SPI_CLK_POLARITY                0    /* CPOL��0=���еͣ�1=���и� */
#define SPI_CLK_PHASE                   0    /* CPHA��0/1 ��Ӧ����/��תʱ�� */
#define SPI_FRAME_FORMAT                0    /* �������ڲ�֡��ʽģʽ���� HAL �����Ӧ�� */
#define SPI_FRAME_FORMAT_STANDARD       0    /* ��׼ SPI ֡���� QSPI ����չ�� */
#define SPI_FRAME_SIZE_8                0x1f /* ֡λ�������ֶΣ���Ӧ 8bit������ȡֵ�� HAL ���壩 */
#define SPI_TMOD                        0    /* ����ģʽ��TX/RX/˫�򣩣�0 ͨ��Ϊ˫�� */
#define SPI_WAIT_CYCLES                 0x10 /* QSPI ����չ�����µĵȴ����� */
#if defined(CONFIG_SPI_SUPPORT_DMA) && !(defined(CONFIG_SPI_SUPPORT_POLL_AND_DMA_AUTO_SWITCH))
#define SPI_DMA_WIDTH                   2    /* DMA �����ȣ���λ�ֽ� */
#endif
#if defined(CONFIG_SPI_MASTER_SUPPORT_QSPI)
#define QSPI_WRITE_CMD                  0x38 /* ʾ����QSPI дָ�� */
#define QSPI_WRITE_ADDR                 0x123/* ʾ����QSPI д��ַ */
#endif
#define SPI_TASK_DURATION_MS            500  /* ����ѭ����������룩 */
#define SPI_TASK_PRIO                   24   /* �������ȼ� */
#define SPI_TASK_STACK_SIZE             0x1000 /* ����ջ��С���ֽڣ� */

/**
 * ��ʼ�� SPI ����������ŵĸ��ù��ܡ�
 */
static void app_spi_init_pin(void)
{
    uapi_pin_set_mode(CONFIG_SPI_DI_MASTER_PIN, CONFIG_SPI_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_DO_MASTER_PIN, CONFIG_SPI_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_CLK_MASTER_PIN, CONFIG_SPI_MASTER_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_CS_MASTER_PIN, CONFIG_SPI_MASTER_PIN_MODE);
#if defined(CONFIG_SPI_MASTER_SUPPORT_QSPI)
    /* QSPI �������������� */
    uapi_pin_set_mode(CONFIG_SPI_MASTER_D2_PIN, CONFIG_SPI_MASTER_D2_PIN_MODE);
    uapi_pin_set_mode(CONFIG_SPI_MASTER_D3_PIN, CONFIG_SPI_MASTER_D3_PIN_MODE);
#endif
}

#if defined(CONFIG_SPI_SUPPORT_INTERRUPT) && (CONFIG_SPI_SUPPORT_INTERRUPT == 1)
/**
 * SPI ����д�루���Ϳ�ʼ/��ɣ��ж�֪ͨ�ص���
 *
 * @param buffer  ʵ���޹أ���ʾ��
 * @param length  ʵ���޹أ���ʾ��
 */
static void app_spi_master_write_int_handler(const void *buffer, uint32_t length)
{
    unused(buffer);
    unused(length);
    osal_printk("spi master write interrupt start!\r\n");
}

/**
 * SPI ������������жϻص���
 *
 * @param buffer  ָ����ջ�������ָ��
 * @param length  ���յ��ֽ���
 * @param error   �Ƿ��������ɵײ�����������
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

    /* ��ʾ��;�����ֽڴ�ӡ��ʾ���յ������� */
    uint8_t *buff = (uint8_t *)buffer;
    for (uint32_t i = 0; i < length; i++) {
        osal_printk("buff[%d] = %x\r\n", i, buff[i]);
    }
    osal_printk("app_spi_master_read_int success!\r\n");
}
#endif  /* CONFIG_SPI_SUPPORT_INTERRUPT */

/**
 * ���ò���ʼ�� SPI ������������
 * - ��׼ SPI������˫���䡢8bit ֡��CPOL/CPHA�����ʵ�
 * - QSPI �������л�Ϊ����д������ָ���/��ַ����/�ȴ����ڵ�
 * - ��ѡ���� DMA ģʽ���ж�ģʽ
 */
static void app_spi_master_init_config(void)
{
    spi_attr_t config = { 0 };
    spi_extra_attr_t ext_config = { 0 };

    config.is_slave = false;                   /* ����ģʽ */
    config.slave_num = SPI_SLAVE_NUM;          /* ��Ӧ���豸����/���� */
    config.bus_clk = SPI_CLK_FREQ;             /* ����������ʱ�ӣ�Hz��������ƽ̨���� */
    config.freq_mhz = SPI_FREQUENCY;           /* Ŀ��ͨ�����ʣ�MHz ���� */
    config.clk_polarity = SPI_CLK_POLARITY;    /* CPOL */
    config.clk_phase = SPI_CLK_PHASE;          /* CPHA */
    config.frame_format = SPI_FRAME_FORMAT;    /* �������ڲ�֡��ʽ */
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_STANDARD; /* ��׼ SPI ֡ */
    config.frame_size = SPI_FRAME_SIZE_8;      /* ֡λ��8bit */
    config.tmod = SPI_TMOD;                    /* ����ģʽ��˫�� */
    config.sste = 0;                           /* ������ SSTE ��ؿ��� */

    ext_config.qspi_param.wait_cycles = SPI_WAIT_CYCLES; /* Ĭ�ϵȴ����� */
#if defined(CONFIG_SPI_MASTER_SUPPORT_QSPI)
    /* �л�Ϊ QSPI ����д���ã�����ʹ�� QSPI ʾ��ʱ��Ч�� */
    config.tmod = HAL_SPI_TRANS_MODE_TX;                       /* ������ */
    config.sste = 0;
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_QUAD;       /* ����ģʽ */
    ext_config.qspi_param.trans_type = HAL_SPI_TRANS_TYPE_INST_S_ADDR_Q; /* ָ��ߣ���ַ���� */
    ext_config.qspi_param.inst_len = HAL_SPI_INST_LEN_8;       /* 8bit ָ�� */
    ext_config.qspi_param.addr_len = HAL_SPI_ADDR_LEN_24;      /* 24bit ��ַ */
    ext_config.qspi_param.wait_cycles = 0;                     /* QSPI �ȴ����� */
#endif

    uapi_spi_init(CONFIG_SPI_MASTER_BUS_ID, &config, &ext_config);
#if defined(CONFIG_SPI_SUPPORT_DMA) && (CONFIG_SPI_SUPPORT_DMA == 1)
    /* ��ѡ����ʼ�� DMA ������ SPI �� DMA ģʽ����ߴ�����ݴ���Ч�� */
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
    /* ��ѡ��ʹ���ж�ģʽ��ע��ص����������շ���ɻ�ͨ���ص�֪ͨ�� */
    if (uapi_spi_set_irq_mode(CONFIG_SPI_MASTER_BUS_ID, true, app_spi_master_rx_callback,
        app_spi_master_write_int_handler) == ERRCODE_SUCC) {
        osal_printk("spi%d master set irq mode succ!\r\n", CONFIG_SPI_MASTER_BUS_ID);
    }
#endif  /* CONFIG_SPI_SUPPORT_INTERRUPT */
}

/**
 * SPI ����ҵ�����������Ե���ӻ��������ݲ���ȡ�ذ���
 *
 * @param arg  ���������δʹ�ã�
 * @return     �������أ���������Ϊ NULL
 */
static void *spi_master_task(const char *arg)
{
    unused(arg);
    /* ���� SPI ���Ÿ��� */
    app_spi_init_pin();

    /* ��ʼ�� SPI �������������� */
    app_spi_master_init_config();

    /*
     * Ԥ�����շ���������
     * - tx_data����� 0..N-1������ӻ�У��
     * - rx_data�����ڽ��մӻ����ص�����
     */
    uint8_t tx_data[CONFIG_SPI_TRANSFER_LEN] = { 0 };
    for (uint32_t loop = 0; loop < CONFIG_SPI_TRANSFER_LEN; loop++) {
        tx_data[loop] = (loop & 0xFF);
    }
    uint8_t rx_data[CONFIG_SPI_TRANSFER_LEN] = { 0 };

    /* ��֯һ�δ�����Ҫ��Ԫ��Ϣ */
    spi_xfer_data_t data = {
        .tx_buff = tx_data,
        .tx_bytes = CONFIG_SPI_TRANSFER_LEN,
        .rx_buff = rx_data,
        .rx_bytes = CONFIG_SPI_TRANSFER_LEN,
#if defined(CONFIG_SPI_MASTER_SUPPORT_QSPI)
        .cmd = QSPI_WRITE_CMD,   /* QSPI ָ�� */
        .addr = QSPI_WRITE_ADDR, /* QSPI ��ַ */
#endif
    };

    while (1) {
        osal_msleep(SPI_TASK_DURATION_MS);

        /* ����һ�������ȷ���һ֡���ݵ��ӻ� */
        osal_printk("spi%d master send start!\r\n", CONFIG_SPI_MASTER_BUS_ID);
        if (uapi_spi_master_write(CONFIG_SPI_MASTER_BUS_ID, &data, 0xFFFFFFFF) == ERRCODE_SUCC) {
            osal_printk("spi%d master send succ!\r\n", CONFIG_SPI_MASTER_BUS_ID);
        } else {
            /* ����ʧ����������ǰ�֣����� */
            continue;
        }

        /* �����������ȡ�ӻ����ص����� */
        osal_printk("spi%d master receive start!\r\n", CONFIG_SPI_MASTER_BUS_ID);
        if (uapi_spi_master_read(CONFIG_SPI_MASTER_BUS_ID, &data, 0xFFFFFFFF) == ERRCODE_SUCC) {
#ifndef CONFIG_SPI_SUPPORT_INTERRUPT
            /* ��ѯģʽ�£�ֱ���ڵ��ô���ӡ���յ������� */
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
 * SPI ����ʾ����ڣ�����������ҵ������
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