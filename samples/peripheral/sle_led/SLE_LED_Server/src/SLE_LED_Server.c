/**
 * @file SLE_LED_Server.c
 * @brief SLE LED服务端示例代码
 *
 * 本文件实现了基于SLE协议的LED控制服务端，支持通过SLE与客户端交互，实现LED灯的远程控制与状态同步。
 * 包含服务注册、属性添加、回调注册、LED控制任务等完整流程。
 */

// ================== 1. 头文件与宏定义区 ==================
#include "securec.h"
#include "errcode.h"
#include "osal_addr.h"
#include "sle_common.h"
#include "sle_errcode.h"
#include "sle_ssap_server.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "../inc/SLE_LED_Server_adv.h"
#include "../inc/SLE_LED_Server.h"

#include "cmsis_os2.h"
#include "debug_print.h"
#include "soc_osal.h"
#include "app_init.h"
#include "common_def.h"





// ================== 2. 函数声明区 ==================

// ---- LED控制相关 ----
/**
 * LED控制任务，周期性切换LED状态并通过notify通知客户端
 * @used in example_led_control_entry
 */
static int example_led_control_task(const char *arg);
/**
 * 创建LED控制任务
 * @used in example_sle_pair_complete_cbk
 */
static void example_led_control_entry(void);
/**
 * 打印客户端LED状态
 * @used in example_ssaps_write_request_cbk
 */
static void example_print_led_state(ssaps_req_write_cb_t *write_cb_para);





// ---- UUID相关 ----
/**
 * 设置SLE UUID基地址
 * @used in example_sle_uuid_setu2
 */
static void example_sle_uuid_set_base(sle_uuid_t *out);
/**
 * 设置SLE UUID的低2字节
 * @used in example_sle_server_service_add, example_sle_server_property_add
 */
static void example_sle_uuid_setu2(uint16_t u2, sle_uuid_t *out);




// ---- SSAP Server回调相关 ----
/**
 * 读请求回调
 * @used in example_sle_ssaps_register_cbks
 */
static void example_ssaps_read_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para, errcode_t status);
/**
 * 写请求回调
 * @used in example_sle_ssaps_register_cbks
 */
static void example_ssaps_write_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para, errcode_t status);
/**
 * MTU变化回调
 * @used in example_sle_ssaps_register_cbks
 */
static void example_ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id, ssap_exchange_info_t *mtu_size, errcode_t status);
/**
 * 服务启动回调
 * @used in example_sle_ssaps_register_cbks
 */
static void example_ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status);
/**
 * 注册SSAP Server相关回调
 * @used in example_sle_led_server_task
 */
static errcode_t example_sle_ssaps_register_cbks(void);




// ---- SLE服务与属性相关 ----
/**
 * 添加SLE服务
 * @used in example_sle_server_add
 */
static errcode_t example_sle_server_service_add(void);
/**
 * 添加SLE属性（特征值）及描述符
 * @used in example_sle_server_add
 */
static errcode_t example_sle_server_property_add(void);
/**
 * 注册Server，添加Service和Property，并启动Service
 * @used in example_sle_led_server_task
 */
static errcode_t example_sle_server_add(void);
/**
 * server通过handle向client发送数据：notify
 * @used in example_led_control_task   重点
 */
static errcode_t example_sle_server_send_notify_by_handle(const uint8_t *data, uint8_t len);





// ---- SLE连接管理相关 ----
/**
 * 连接状态变化回调
 * @used in example_sle_conn_register_cbks
 */
static void example_sle_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr, sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason);
/**
 * 配对完成回调
 * @used in example_sle_conn_register_cbks
 */
static void example_sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status);
/**
 * 注册连接管理相关回调
 * @used in example_sle_led_server_task
 */
static errcode_t example_sle_conn_register_cbks(void);





// ---- SLE主任务与入口 ----
/**
 * SLE LED服务主任务，负责SLE服务端的初始化、注册、广播等流程
 * @used in example_sle_led_server_entry
 */
static int example_sle_led_server_task(const char *arg);
/**
 * SLE LED服务入口，创建主任务
 * @used in app_run
 */
static void example_sle_led_server_entry(void);




// ================== 3. 全局变量与宏定义区 ==================
#define OCTET_BIT_LEN 8
#define UUID_LEN_2 2

/**
 * @brief 小端序写2字节宏
 * @param _ptr 目标指针
 * @param data 2字节数据
 */
#define encode2byte_little(_ptr, data)                     \
    do {                                                   \
        *(uint8_t *)((_ptr) + 1) = (uint8_t)((data) >> 8); \
        *(uint8_t *)(_ptr) = (uint8_t)(data);              \
    } while (0)

/* sle server app uuid for sample */
static char g_sle_uuid_app_uuid[UUID_LEN_2] = {0x0, 0x0};
/* server property value for sample */
static char g_sle_property_value[OCTET_BIT_LEN] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
/* sle connect id，记录当前连接ID */
static uint16_t g_conn_id = 0;
/* sle server id，注册服务端时分配 */
static uint8_t g_server_id = 0;
/* sle service handle，服务句柄 */
static uint16_t g_service_handle = 0;
/* sle ntf property handle，特征值句柄 */
static uint16_t g_property_handle = 0;

/**
 * @brief LED控制类型枚举
 * 用于描述不同的LED控制命令
 */
typedef enum {
    EXAMPLE_CONTORL_LED_EXIT = 0x00,              /*!< 退出LED控制演示 */
    EXAMPLE_CONTORL_LED_MAINBOARD_LED_ON = 0x01,  /*!< 打开主板上的LED */
    EXAMPLE_CONTORL_LED_MAINBOARD_LED_OFF = 0x02, /*!< 关闭主板上的LED */
    EXAMPLE_CONTORL_LED_LEDBOARD_RLED_ON = 0x03,  /*!< 打开灯板上的红色LED */
    EXAMPLE_CONTORL_LED_LEDBOARD_RLED_OFF = 0x04, /*!< 关闭灯板上的红色LED */
    EXAMPLE_CONTORL_LED_LEDBOARD_YLED_ON = 0x05,  /*!< 打开灯板上的黄色LED */
    EXAMPLE_CONTORL_LED_LEDBOARD_YLED_OFF = 0x06, /*!< 关闭灯板上的黄色LED */
    EXAMPLE_CONTORL_LED_LEDBOARD_GLED_ON = 0x07,  /*!< 打开灯板上的绿色LED */
    EXAMPLE_CONTORL_LED_LEDBOARD_GLED_OFF = 0x08, /*!< 关闭灯板上的绿色LED */
} example_control_led_type_t;

#define LED_CONTROL_TASK_STACK_SIZE 0x1000
#define LED_CONTROL_TASK_PRIO (osPriority_t)(17)

/**
 * @brief SLE服务UUID基地址
 */
static uint8_t sle_uuid_base[] = {0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA,
                                  0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define SLE_LED_SER_TASK_PRIO 24
#define SLE_LED_SER_STACK_SIZE 0x2000

// ================== 4. LED控制任务与相关实现 ==================
/**
 * @brief LED控制任务
 * 周期性切换LED状态，并通过notify通知客户端
 * @param arg 任务参数
 * @return 0
 */
static int example_led_control_task(const char *arg)
{
    unused(arg); // 参数未使用，防止编译器告警
    // 记录上一次LED操作类型，初始为关闭绿色LED
    example_control_led_type_t last_led_operation = EXAMPLE_CONTORL_LED_LEDBOARD_GLED_OFF;

    PRINT("[SLE Server] start led control task\r\n");

    // 无限循环，周期性切换LED状态
    while (1) {
        (void)osal_msleep(500); // 每500ms切换一次
        // 按照状态机依次切换红、黄、绿LED的开关状态
        if (last_led_operation == EXAMPLE_CONTORL_LED_LEDBOARD_GLED_OFF) {
            // 发送红色LED打开的通知
            uint8_t write_req_data[] = {'R', 'L', 'E', 'D', '_', 'O', 'N'};
            example_sle_server_send_notify_by_handle(write_req_data, sizeof(write_req_data));
            last_led_operation = EXAMPLE_CONTORL_LED_LEDBOARD_RLED_ON;
        } else if (last_led_operation == EXAMPLE_CONTORL_LED_LEDBOARD_RLED_ON) {
            // 发送红色LED关闭的通知
            uint8_t write_req_data[] = {'R', 'L', 'E', 'D', '_', 'O', 'F', 'F'};
            example_sle_server_send_notify_by_handle(write_req_data, sizeof(write_req_data));
            last_led_operation = EXAMPLE_CONTORL_LED_LEDBOARD_RLED_OFF;
        } else if (last_led_operation == EXAMPLE_CONTORL_LED_LEDBOARD_RLED_OFF) {
            // 发送黄色LED打开的通知
            uint8_t write_req_data[] = {'Y', 'L', 'E', 'D', '_', 'O', 'N'};
            example_sle_server_send_notify_by_handle(write_req_data, sizeof(write_req_data));
            last_led_operation = EXAMPLE_CONTORL_LED_LEDBOARD_YLED_ON;
        } else if (last_led_operation == EXAMPLE_CONTORL_LED_LEDBOARD_YLED_ON) {
            // 发送黄色LED关闭的通知
            uint8_t write_req_data[] = {'Y', 'L', 'E', 'D', '_', 'O', 'F', 'F'};
            example_sle_server_send_notify_by_handle(write_req_data, sizeof(write_req_data));
            last_led_operation = EXAMPLE_CONTORL_LED_LEDBOARD_YLED_OFF;
        } else if (last_led_operation == EXAMPLE_CONTORL_LED_LEDBOARD_YLED_OFF) {
            // 发送绿色LED打开的通知
            uint8_t write_req_data[] = {'G', 'L', 'E', 'D', '_', 'O', 'N'};
            example_sle_server_send_notify_by_handle(write_req_data, sizeof(write_req_data));
            last_led_operation = EXAMPLE_CONTORL_LED_LEDBOARD_GLED_ON;
        } else if (last_led_operation == EXAMPLE_CONTORL_LED_LEDBOARD_GLED_ON) {
            // 发送绿色LED关闭的通知
            uint8_t write_req_data[] = {'G', 'L', 'E', 'D', '_', 'O', 'F', 'F'};
            example_sle_server_send_notify_by_handle(write_req_data, sizeof(write_req_data));
            last_led_operation = EXAMPLE_CONTORL_LED_LEDBOARD_GLED_OFF;
        }
        // 每次循环只切换一次LED状态并通知客户端
    }

    return 0;
}

/**
 * @brief 创建LED控制任务
 */
static void example_led_control_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock(); // 进入临界区，防止多线程冲突
    // 创建LED控制任务，任务函数为example_led_control_task
    task_handle = osal_kthread_create((osal_kthread_handler)example_led_control_task, 0, "LedControlTask",
                                      LED_CONTROL_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        // 设置任务优先级
        osal_kthread_set_priority(task_handle, LED_CONTROL_TASK_PRIO);
        osal_kfree(task_handle); // 释放任务句柄内存
    }
    osal_kthread_unlock(); // 退出临界区
}

/**
 * @brief 打印客户端LED状态
 * @param write_cb_para 写请求参数
 */
static void example_print_led_state(ssaps_req_write_cb_t *write_cb_para)
{
    // 判断客户端写入的数据是否为"LED_ON"，并打印对应信息
    if (write_cb_para->length == strlen("LED_ON") && write_cb_para->value[0] == 'L' && write_cb_para->value[1] == 'E' &&
        write_cb_para->value[2] == 'D' && write_cb_para->value[3] == '_' && write_cb_para->value[4] == 'O' &&
        write_cb_para->value[5] == 'N') {
        PRINT("[SLE Server] client main board led is on.\r\n");
    }
    // 判断客户端写入的数据是否为"LED_OFF"，并打印对应信息
    if (write_cb_para->length == strlen("LED_OFF") && write_cb_para->value[0] == 'L' &&
        write_cb_para->value[1] == 'E' && write_cb_para->value[2] == 'D' && write_cb_para->value[3] == '_' &&
        write_cb_para->value[4] == 'O' && write_cb_para->value[5] == 'F' && write_cb_para->value[6] == 'F') {
        PRINT("[SLE Server] client main board led is off.\r\n");
    }
}






// ================== 5. SLE服务与属性相关实现 ==================



// ---- UUID相关 ----
/**
 * @brief 设置SLE UUID基地址
 * @param out 输出UUID结构体
 */
static void example_sle_uuid_set_base(sle_uuid_t *out)
{
    (void)memcpy_s(out->uuid, SLE_UUID_LEN, sle_uuid_base, SLE_UUID_LEN);
    out->len = UUID_LEN_2;
}

/**
 * @brief 设置SLE UUID的低2字节
 * @param u2 低2字节
 * @param out 输出UUID结构体
 */
static void example_sle_uuid_setu2(uint16_t u2, sle_uuid_t *out)
{
    example_sle_uuid_set_base(out);
    out->len = UUID_LEN_2;
    encode2byte_little(&out->uuid[14], u2);
}











/**
 * @brief 读请求回调
 * @param server_id 服务ID
 * @param conn_id 连接ID
 * @param read_cb_para 读请求参数
 * @param status 状态码
 */
static void example_ssaps_read_request_cbk(uint8_t server_id,
                                           uint16_t conn_id,
                                           ssaps_req_read_cb_t *read_cb_para,
                                           errcode_t status)
{
    PRINT("[SLE Server] ssaps read request cbk server_id:0x%x, conn_id:0x%x, handle:0x%x, type:0x%x, status:0x%x\r\n",
          server_id, conn_id, read_cb_para->handle, read_cb_para->type, status);
}

/**
 * @brief 写请求回调
 * @param server_id 服务ID
 * @param conn_id 连接ID
 * @param write_cb_para 写请求参数
 * @param status 状态码
 */
static void example_ssaps_write_request_cbk(uint8_t server_id,
                                            uint16_t conn_id,
                                            ssaps_req_write_cb_t *write_cb_para,
                                            errcode_t status)
{
    PRINT("[SLE Server] ssaps write request cbk server_id:0x%x, conn_id:0x%x, handle:0x%x, status:0x%x\r\n", server_id,
          conn_id, write_cb_para->handle, status);

    for (uint16_t idx = 0; idx < write_cb_para->length; idx++) {
        PRINT("[SLE Server] write request cbk[0x%x] 0x%02x\r\n", idx, write_cb_para->value[idx]);
    }

    if (status == ERRCODE_SUCC) {
        example_print_led_state(write_cb_para);
    }
}

/**
 * @brief MTU变化回调
 * @param server_id 服务ID
 * @param conn_id 连接ID
 * @param mtu_size MTU信息
 * @param status 状态码
 */
static void example_ssaps_mtu_changed_cbk(uint8_t server_id,
                                          uint16_t conn_id,
                                          ssap_exchange_info_t *mtu_size,
                                          errcode_t status)
{
    PRINT("[SLE Server] ssaps mtu changed cbk server_id:0x%x, conn_id:0x%x, mtu_size:0x%x, status:0x%x\r\n", server_id,
          conn_id, mtu_size->mtu_size, status);
}

/**
 * @brief 服务启动回调
 * @param server_id 服务ID
 * @param handle 服务句柄
 * @param status 状态码
 */
static void example_ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    PRINT("[SLE Server] start service cbk server_id:0x%x, handle:0x%x, status:0x%x\r\n", server_id, handle, status);
}

/**
 * @brief 注册SSAP Server相关回调
 * @return 错误码
 */
static errcode_t example_sle_ssaps_register_cbks(void)
{
    ssaps_callbacks_t ssaps_cbk = {0};
    ssaps_cbk.start_service_cb = example_ssaps_start_service_cbk;
    ssaps_cbk.mtu_changed_cb = example_ssaps_mtu_changed_cbk;
    ssaps_cbk.read_request_cb = example_ssaps_read_request_cbk;
    ssaps_cbk.write_request_cb = example_ssaps_write_request_cbk;
    return ssaps_register_callbacks(&ssaps_cbk);
}














/**
 * @brief 添加SLE服务
 * @return 错误码
 */
static errcode_t example_sle_server_service_add(void)
{
    errcode_t ret = ERRCODE_FAIL;
    sle_uuid_t service_uuid = {0};
    example_sle_uuid_setu2(SLE_UUID_SERVER_SERVICE, &service_uuid);
    ret = ssaps_add_service_sync(g_server_id, &service_uuid, true, &g_service_handle);
    if (ret != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle uuid add service fail, ret:0x%x\r\n", ret);
        return ERRCODE_FAIL;
    }

    PRINT("[SLE Server] sle uuid add service service_handle: %u\r\n", g_service_handle);

    return ERRCODE_SUCC;
}

/**
 * @brief 添加SLE属性（特征值）及描述符
 * @return 错误码
 */
static errcode_t example_sle_server_property_add(void)
{
    errcode_t ret = ERRCODE_FAIL;
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t ntf_value[] = {0x01, 0x0};

    property.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    example_sle_uuid_setu2(SLE_UUID_SERVER_PROPERTY, &property.uuid);
    property.value = osal_vmalloc(sizeof(g_sle_property_value));
    if (property.value == NULL) {
        PRINT("[SLE Server] sle property mem fail\r\n");
        return ERRCODE_MALLOC;
    }

    if (memcpy_s(property.value, sizeof(g_sle_property_value), g_sle_property_value, sizeof(g_sle_property_value)) !=
        EOK) {
        osal_vfree(property.value);
        PRINT("[SLE Server] sle property mem cpy fail\r\n");
        return ERRCODE_MEMCPY;
    }
    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property, &g_property_handle);
    if (ret != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle uuid add property fail, ret:0x%x\r\n", ret);
        osal_vfree(property.value);
        return ERRCODE_FAIL;
    }

    PRINT("[SLE Server] sle uuid add property property_handle: %u\r\n", g_property_handle);

    descriptor.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    descriptor.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE;
    descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
    descriptor.value = ntf_value;
    descriptor.value_len = sizeof(ntf_value);
    
    if (descriptor.value == NULL) {
        PRINT("[SLE Server] sle descriptor mem fail\r\n");
        osal_vfree(property.value);
        return ERRCODE_MALLOC;
    }
    if (memcpy_s(descriptor.value, sizeof(ntf_value), ntf_value, sizeof(ntf_value)) != EOK) {
        PRINT("[SLE Server] sle descriptor mem cpy fail\r\n");
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_MEMCPY;
    }
    ret = ssaps_add_descriptor_sync(g_server_id, g_service_handle, g_property_handle, &descriptor);
    if (ret != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle uuid add descriptor fail, ret:0x%x\r\n", ret);
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_FAIL;
    }
    osal_vfree(property.value);
    osal_vfree(descriptor.value);
    return ERRCODE_SUCC;
}

/**
 * @brief 注册SLE Server，添加Service和Property，并启动Service
 *
 * 该函数主要完成SLE服务端的初始化流程：
 * 1. 注册SSAP Server，分配server_id。
 * 2. 添加SLE服务（调用example_sle_server_service_add）。
 * 3. 添加SLE属性（特征值）及描述符（调用example_sle_server_property_add）。
 * 4. 启动Service（调用ssaps_start_service）。
 *
 * 如果任一步失败，会注销Server并返回失败。成功后，SLE服务端的GATT服务和特征就注册并启动完成，可以被客户端发现和交互。
 *
 * @return 错误码，成功返回ERRCODE_SUCC，失败返回对应错误码。
 */
static errcode_t example_sle_server_add(void)
{
    errcode_t ret = ERRCODE_FAIL;
    sle_uuid_t app_uuid = {0};

    PRINT("[SLE Server] sle uuid add service in\r\n");
    app_uuid.len = sizeof(g_sle_uuid_app_uuid);
    if (memcpy_s(app_uuid.uuid, app_uuid.len, g_sle_uuid_app_uuid, sizeof(g_sle_uuid_app_uuid)) != EOK) {
        return ERRCODE_MEMCPY;
    }
    ssaps_register_server(&app_uuid, &g_server_id);

    if (example_sle_server_service_add() != ERRCODE_SUCC) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_FAIL;
    }

    if (example_sle_server_property_add() != ERRCODE_SUCC) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_FAIL;
    }
    PRINT("[SLE Server] sle uuid add service, server_id:0x%x, service_handle:0x%x, property_handle:0x%x\r\n",
          g_server_id, g_service_handle, g_property_handle);
    ret = ssaps_start_service(g_server_id, g_service_handle);
    if (ret != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle uuid add service fail, ret:0x%x\r\n", ret);
        return ERRCODE_FAIL;
    }
    PRINT("[SLE Server] sle uuid add service out\r\n");
    return ERRCODE_SUCC;
}

/**
 * @brief server通过handle向client发送数据：notify
 * @param data 通知数据
 * @param len 数据长度
 * @return 错误码
 */
static errcode_t example_sle_server_send_notify_by_handle(const uint8_t *data, uint8_t len)
{
    ssaps_ntf_ind_t param = {0};

    param.handle = g_property_handle;
    param.type = 0;

    param.value = osal_vmalloc(len);
    param.value_len = len;
    if (param.value == NULL) {
        PRINT("[SLE Server] send notify mem fail\r\n");
        return ERRCODE_MALLOC;
    }

    if (memcpy_s(param.value, param.value_len, data, len) != EOK) {
        PRINT("[SLE Server] send notify memcpy fail\r\n");
        osal_vfree(param.value);
        return ERRCODE_MEMCPY;
    }

    if (ssaps_notify_indicate(g_server_id, g_conn_id, &param) != ERRCODE_SUCC) {
        PRINT("[SLE Server] ssaps notify indicate fail\r\n");
        osal_vfree(param.value);
        return ERRCODE_FAIL;
    }
    osal_vfree(param.value);
    return ERRCODE_SUCC;
}














// ================== 6. SLE连接与回调相关实现 ==================
/**
 * @brief 连接状态变化回调
 * @param conn_id 连接ID
 * @param addr 设备地址
 * @param conn_state 连接状态
 * @param pair_state 配对状态
 * @param disc_reason 断开原因
 */
static void example_sle_connect_state_changed_cbk(uint16_t conn_id,
                                                  const sle_addr_t *addr,
                                                  sle_acb_state_t conn_state,
                                                  sle_pair_state_t pair_state,
                                                  sle_disc_reason_t disc_reason)
{
    PRINT(
        "[SLE Server] connect state changed conn_id:0x%02x, conn_state:0x%x, pair_state:0x%x, \
        disc_reason:0x%x\r\n",
        conn_id, conn_state, pair_state, disc_reason);
    PRINT("[SLE Server] connect state changed addr:%02x:**:**:**:%02x:%02x\r\n", addr->addr[0], addr->addr[4],
          addr->addr[5]);
    g_conn_id = conn_id;
}

/**
 * @brief 配对完成回调
 * @param conn_id 连接ID
 * @param addr 设备地址
 * @param status 状态码
 */
static void example_sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    PRINT("[SLE Server] pair complete conn_id:0x%02x, status:0x%x\r\n", conn_id, status);
    PRINT("[SLE Server] pair complete addr:%02x:**:**:**:%02x:%02x\r\n", addr->addr[0], addr->addr[4], addr->addr[5]);

    if (status == ERRCODE_SUCC) {
        example_led_control_entry();
    }
}

/**
 * @brief 注册连接管理相关回调
 * @return 错误码
 */
static errcode_t example_sle_conn_register_cbks(void)
{
    sle_connection_callbacks_t conn_cbks = {0};
    conn_cbks.connect_state_changed_cb = example_sle_connect_state_changed_cbk;
    conn_cbks.pair_complete_cb = example_sle_pair_complete_cbk;
    return sle_connection_register_callbacks(&conn_cbks);
}











// ================== 7. SLE主任务与入口实现 ==================
/**
 * @brief SLE LED服务主任务
 * 负责SLE服务端的初始化、注册、广播等流程
 * @param arg 任务参数
 * @return 0成功，-1失败
 */
static int example_sle_led_server_task(const char *arg)
{
    unused(arg);

    (void)osal_msleep(5000); /* 延时5s，等待SLE初始化完毕 */

    PRINT("[SLE Server] try enable.\r\n");
    /* 使能SLE */
    if (enable_sle() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle enbale fail !\r\n");
        return -1;
    }

    /* 注册连接管理回调函数 */
    if (example_sle_conn_register_cbks() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle conn register cbks fail !\r\n");
        return -1;
    }

    /* 注册 SSAP server 回调函数 */
    if (example_sle_ssaps_register_cbks() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle ssaps register cbks fail !\r\n");
        return -1;
    }

    /* 注册Server, 添加Service和property, 启动Service */
    if (example_sle_server_add() != ERRCODE_SUCC) {
        PRINT("[SLE Server] sle server add fail !\r\n");
        return -1;
    }//注册Server，添加Service和Property（特征值），并启动Service

    /* 设置设备公开，并公开设备 */
    if (example_sle_server_adv_init() != ERRCODE_SUCC) {//这个地方的函数是adv里的
        PRINT("[SLE Server] sle server adv fail !\r\n");
        return -1;//初始化并启动广播（让设备可被发现和连接）
    }

    PRINT("[SLE Server] init ok\r\n");

    return 0;
}

/**
 * @brief SLE LED服务入口，创建主任务
 */
static void example_sle_led_server_entry(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)example_sle_led_server_task, 0, "SLELedServerTask",
                                      SLE_LED_SER_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_LED_SER_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* 启动SLE LED服务入口 */
app_run(example_sle_led_server_entry);