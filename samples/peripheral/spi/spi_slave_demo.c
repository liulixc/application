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
 * ��ʾ����ʾ SPI �Դӻ�ģʽ�������������̣�
 * - GPIO �������õ� SPI �ӻ�����
 * - ���� SPI ������Ϊ�ӻ�������ʱ�Ӽ���/��λ��֡��ʽ��λ�����ʡ��ȴ����ڵ�
 * - ����ѡ������ DMA ���ж�ģʽ
 * - �ڶ���������ѭ��ִ�У��Ƚ����������ݲ���ӡ���ٷ���һ�ε������ݸ�����
 *
 * ���߽��飨�Գ���������Ӧ��ϵΪ����ʵ���԰弶����Ϊ׼����
 * - ���� CLK -> �ӻ� CLK
 * - ���� CS  -> �ӻ� CS
 * - ���� MOSI(DO) -> �ӻ� MOSI(DI)
 * - ���� MISO(DI) -> �ӻ� MISO(DO)
 *
 * �ؼ������ں꣨ͨ���� Kconfig/Board �����ṩ����
 * - CONFIG_SPI_SLAVE_BUS_ID��SPI ���������ߺ�
 * - CONFIG_SPI_*_SLAVE_PIN���� SPI ���ű��
 * - CONFIG_SPI_SLAVE_PIN_MODE����Ӧ���Ÿ���ģʽ
 * - CONFIG_SPI_TRANSFER_LEN�����δ�����ֽ���
 * - CONFIG_SPI_SUPPORT_DMA���Ƿ����� DMA ����
 * - CONFIG_SPI_SUPPORT_POLL_AND_DMA_AUTO_SWITCH���Ƿ��Զ�����ѯ/DMA ���л�
 * - CONFIG_SPI_SUPPORT_INTERRUPT���Ƿ������ж��շ��ص�
 */

#define SPI_SLAVE_NUM                   1   /* Ƭѡ���豸��������������Χ�������庬���� HAL ���� */
#define SPI_FREQUENCY                   2   /* ������ SPI �������ʣ�MHz ������� bus_clk ʵ�ʷ�Ƶ�� */
#define SPI_CLK_POLARITY                0   /* ʱ�Ӽ��� CPOL��0=����Ϊ�ͣ�1=����Ϊ�� */
#define SPI_CLK_PHASE                   0   /* ʱ����λ CPHA��0/1 ��Ӧ����/��תʱ�� */
#define SPI_FRAME_FORMAT                0   /* �������ڲ�֡��ʽģʽ���� HAL �����Ӧ�� */
#define SPI_FRAME_FORMAT_STANDARD       0   /* ��׼ SPI ֡���� QSPI ����չ�� */
#define SPI_FRAME_SIZE_8                0x1f/* ֡λ�������ֶΣ���Ӧ 8bit������ȡֵ�� HAL ���壩 */
#define SPI_TMOD                        0   /* ����ģʽ��TX/RX/˫�򣩣�0 ͨ��Ϊ˫�� */
#define SPI_WAIT_CYCLES                 0x10/* �ȴ����ڣ�QSPI ����չ�ã�����ӻ��Ը���Ĭ��ֵ�� */
#if defined(CONFIG_SPI_SUPPORT_DMA) && !(defined(CONFIG_SPI_SUPPORT_POLL_AND_DMA_AUTO_SWITCH))
#define SPI_DMA_WIDTH                   2   /* DMA �����ȣ���λ�ֽڣ����� 1=8bit, 2=16bit, 4=32bit�� */
#endif

#define SPI_TASK_DURATION_MS            500 /* ����ѭ����������룩��������ʾ������� */
#define SPI_TASK_PRIO                   24  /* �������ȼ�����ֵԽС���ȼ�Խ�ߣ�ȡֵ�� OSAL ʵ�֣� */
#define SPI_TASK_STACK_SIZE             0x1000 /* ����ջ��С���ֽڣ� */

/**
 * ��ʼ�� SPI �ӻ�������ŵĸ��ù��ܡ�
 * ���뱣֤��弶�ܽŶ���һ�£����� SPI �޷�����������
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
 * SPI �ӻ�д�루������ɣ��жϵ�֪ͨ�ص���
 *
 * @param buffer  ʵ���޹أ�������ɽ����¼�֪ͨ
 * @param length  ʵ���޹أ�������ɽ����¼�֪ͨ
 */
static void app_spi_slave_write_int_handler(const void *buffer, uint32_t length)
{
    unused(buffer);
    unused(length);
    osal_printk("spi slave write interrupt start!\r\n");
}

/**
 * SPI �ӻ���������жϻص���
 *
 * @param buffer  ָ����ջ�������ָ��
 * @param length  ���յ��ֽ���
 * @param error   �Ƿ��������ɵײ�����������
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

    /* ��ʾ��;�����ֽڴ�ӡ��ʾ���յ������� */
    uint8_t *buff = (uint8_t *)buffer;
    for (uint32_t i = 0; i < length; i++) {
        osal_printk("buff[%d] = %x\r\n", i, buff[i]);
    }
    osal_printk("app_spi_slave_read_int success!\r\n");
}
#endif  /* CONFIG_SPI_SUPPORT_INTERRUPT */

/**
 * ���ò���ʼ�� SPI �ӻ���������
 * - ���ù���ģʽΪ�ӻ�
 * - ָ��֡��ʽ��λ������/��λ������
 * - ��ѡ���� DMA ģʽ���ж�ģʽ
 */
static void app_spi_slave_init_config(void)
{
    spi_attr_t config = { 0 };
    spi_extra_attr_t ext_config = { 0 };

    config.is_slave = true;                    /* �ӻ�ģʽ */
    config.slave_num = SPI_SLAVE_NUM;          /* Ƭѡ/���豸�� */
    config.bus_clk = SPI_CLK_FREQ;             /* ����������ʱ�ӣ�Hz��������ƽ̨���� */
    config.freq_mhz = SPI_FREQUENCY;           /* Ŀ��ͨ�����ʣ�MHz �����������������Ƶ */
    config.clk_polarity = SPI_CLK_POLARITY;    /* CPOL */
    config.clk_phase = SPI_CLK_PHASE;          /* CPHA */
    config.frame_format = SPI_FRAME_FORMAT;    /* �������ڲ�֡��ʽ */
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_STANDARD; /* ��׼ SPI ֡ */
    config.frame_size = SPI_FRAME_SIZE_8;      /* ֡λ��8bit */
    config.tmod = SPI_TMOD;                    /* ����ģʽ��˫��/ֻ��/ֻ�գ� */
    config.sste = 0;                           /* �ӻ�Ƭѡ��չ���أ��� HAL ������ */

    ext_config.qspi_param.wait_cycles = SPI_WAIT_CYCLES; /* Ԥ���ֶΣ�QSPI ����չ�� */

    uapi_spi_init(CONFIG_SPI_SLAVE_BUS_ID, &config, &ext_config);
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
    if (uapi_spi_set_dma_mode(CONFIG_SPI_SLAVE_BUS_ID, true, &dma_cfg) != ERRCODE_SUCC) {
        osal_printk("spi%d slave set dma mode fail!\r\n");
    }
#endif
#endif  /* CONFIG_SPI_SUPPORT_DMA */

#if defined(CONFIG_SPI_SUPPORT_INTERRUPT) && (CONFIG_SPI_SUPPORT_INTERRUPT == 1)
    /* ��ѡ��ʹ���ж�ģʽ��ע��ص����������շ���ɻ�ͨ���ص�֪ͨ�� */
    if (uapi_spi_set_irq_mode(CONFIG_SPI_SLAVE_BUS_ID, true, app_spi_slave_rx_callback,
        app_spi_slave_write_int_handler) == ERRCODE_SUCC) {
        osal_printk("spi%d slave set irq mode succ!\r\n", CONFIG_SPI_SLAVE_BUS_ID);
    }
#endif  /* CONFIG_SPI_SUPPORT_INTERRUPT */
}

/**
 * SPI �ӻ�ҵ�����������Եؽ����������ݲ��ط����ݡ�
 *
 * @param arg  ���������δʹ�ã�
 * @return     �������أ���������Ϊ NULL
 */
static void *spi_slave_task(const char *arg)
{
    unused(arg);
    /* ���� SPI ���Ÿ��� */
    app_spi_init_pin();

    /* ��ʼ�� SPI �ӻ����������� */
    app_spi_slave_init_config();

    /*
     * Ԥ�����շ���������
     * - tx_data����� 0..N-1������Զ�����У��
     * - rx_data�����ڽ����������͵�����
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
    };

    while (1) {
        osal_msleep(SPI_TASK_DURATION_MS);

        /* ����һ���Ƚ����������ݣ��������ȷ��� */
        osal_printk("spi%d slave receive start!\r\n", CONFIG_SPI_SLAVE_BUS_ID);
        if (uapi_spi_slave_read(CONFIG_SPI_SLAVE_BUS_ID, &data, 0xFFFFFFFF) == ERRCODE_SUCC) {
#ifndef CONFIG_SPI_SUPPORT_INTERRUPT
            /* ��ѯģʽ�£�ֱ���ڵ��ô���ӡ���յ������� */
            for (uint32_t i = 0; i < data.rx_bytes; i++) {
                osal_printk("spi%d slave receive data is %x\r\n", CONFIG_SPI_SLAVE_BUS_ID, data.rx_buff[i]);
            }
#endif
            osal_printk("spi%d slave receive succ!\r\n", CONFIG_SPI_SLAVE_BUS_ID);
        } else {
            /* ����ʧ���������һ�� */
            continue;
        }

        /* ���������дһ֡���ݸ����� */
        osal_printk("spi%d slave send start!\r\n", CONFIG_SPI_SLAVE_BUS_ID);
        if (uapi_spi_slave_write(CONFIG_SPI_SLAVE_BUS_ID, &data, 0xFFFFFFFF) == ERRCODE_SUCC) {
            osal_printk("spi%d slave send succ!\r\n", CONFIG_SPI_SLAVE_BUS_ID);
        }
    }

    return NULL;
}

/**
 * SPI �ӻ�ʾ����ڣ�����������ҵ������
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