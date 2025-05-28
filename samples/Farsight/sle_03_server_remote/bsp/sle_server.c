/*
 * Copyright (c) 2024 HiSilicon Technologies CO., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "sle_server.h"
#include "bsp_init.h"
#include "pid.h"
#define OCTET_BIT_LEN 8
#define UUID_LEN_2 2

#define SLE_MTU_SIZE_DEFAULT 512
#define DEFAULT_PAYLOAD_SIZE (SLE_MTU_SIZE_DEFAULT - 12) // 设置有效载荷
/* Service UUID */
#define SLE_UUID_SERVER_SERVICE 0xABCD
/* Property UUID */
#define SLE_UUID_SERVER_NTF_REPORT 0xBCDE
/* 广播ID */
#define SLE_ADV_HANDLE_DEFAULT 1
/* sle server app uuid for test */
char g_sle_uuid_app_uuid[UUID_LEN_2] = {0x0, 0x0};
/* server notify property uuid for test */
char g_sle_property_value[OCTET_BIT_LEN] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
/* sle connect acb handle */
uint16_t g_sle_conn_hdl = 0;
/* sle server handle */
uint8_t g_server_id = 0;
/* sle service handle */
uint16_t g_service_handle = 0;
/* sle ntf property handle */
uint16_t g_property_handle = 0;

uint16_t g_sle_conn_flag = 0;

#define UUID_16BIT_LEN 2
#define UUID_128BIT_LEN 16

uint8_t g_sle_base[] = {0x73, 0x6C, 0x65, 0x5F, 0x74, 0x65, 0x73, 0x74};

void encode2byte_little(uint8_t *ptr, uint16_t data)
{
    *(uint8_t *)((ptr) + 1) = (uint8_t)((data) >> 0x8);
    *(uint8_t *)(ptr) = (uint8_t)(data);
}

void sle_uuid_set_base(sle_uuid_t *out)
{
    errcode_t ret;
    ret = memcpy_s(out->uuid, SLE_UUID_LEN, g_sle_base, SLE_UUID_LEN);
    if (ret != EOK) {
        printf("[%s] memcpy fail\n", __FUNCTION__);
        out->len = 0;
        return;
    }
    out->len = UUID_LEN_2;
}

void sle_uuid_setu2(uint16_t u2, sle_uuid_t *out)
{
    sle_uuid_set_base(out);
    out->len = UUID_LEN_2;
    encode2byte_little(&out->uuid[14], u2); /* 14: index */
}
void sle_uuid_print(sle_uuid_t *uuid)
{
    printf("[%s] ", __FUNCTION__);
    if (uuid == NULL) {
        printf("uuid is null\r\n");
        return;
    }
    if (uuid->len == UUID_16BIT_LEN) {
        printf("uuid: %02x %02x.\n", uuid->uuid[14], uuid->uuid[15]); /* 14 15: 下标 */
    } else if (uuid->len == UUID_128BIT_LEN) {
        for (size_t i = 0; i < UUID_128BIT_LEN; i++) {
            /* code */
            printf("0x%02x ", uuid->uuid[i]);
        }
    }
}

void ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id, ssap_exchange_info_t *mtu_size, errcode_t status)
{
    printf("[%s] server_id:%d, conn_id:%d, mtu_size:%d, status:%d\r\n", __FUNCTION__, server_id, conn_id,
           mtu_size->mtu_size, status);
}

void ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    printf("[%s] server_id:%d, handle:%x, status:%x\r\n", __FUNCTION__, server_id, handle, status);
}
void ssaps_add_service_cbk(uint8_t server_id, sle_uuid_t *uuid, uint16_t handle, errcode_t status)
{
    printf("[%s] server_id:%x, handle:%x, status:%x\r\n", __FUNCTION__, server_id, handle, status);
    sle_uuid_print(uuid);
}
void ssaps_add_property_cbk(uint8_t server_id,
                            sle_uuid_t *uuid,
                            uint16_t service_handle,
                            uint16_t handle,
                            errcode_t status)
{
    printf("[%s] server_id:%x, service_handle:%x,handle:%x, status:%x\r\n", __FUNCTION__, server_id, service_handle,
           handle, status);
    sle_uuid_print(uuid);
}
void ssaps_add_descriptor_cbk(uint8_t server_id,
                              sle_uuid_t *uuid,
                              uint16_t service_handle,
                              uint16_t property_handle,
                              errcode_t status)
{
    printf("[%s] server_id:%x, service_handle:%x, property_handle:%x,status:%x\r\n", __FUNCTION__, server_id,
           service_handle, property_handle, status);
    sle_uuid_print(uuid);
}
void ssaps_delete_all_service_cbk(uint8_t server_id, errcode_t status)
{
    printf("[%s] server_id:%x, status:%x\r\n", __FUNCTION__, server_id, status);
}
void ssaps_read_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para, errcode_t status)
{
    printf("[%s] server_id:%x, conn_id:%x, handle:%x, status:%x\r\n", __FUNCTION__, server_id, conn_id,
           read_cb_para->handle, status);
}
void ssaps_write_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para, errcode_t status)
{
    unused(status);
    unused(server_id);
    unused(conn_id);
    // 检查 write_cb_para->value 中是否包含 "CAR_ADD" 字符串
    if (strstr((char *)write_cb_para->value, "CAR_ADD") != NULL) {
        // 如果系统中汽车速度小于 2
        if (systemValue.car_speed < 2) {
            // 汽车速度加 1
            systemValue.car_speed += 1;
        }

        // 检查 write_cb_para->value 中是否包含 "CAR_SUBTRACT" 字符串
    } else if (strstr((char *)write_cb_para->value, "CAR_SUBTRACT") != NULL) {
        // 如果系统中汽车速度大于等于 1
        if (systemValue.car_speed >= 1) {
            // 汽车速度减 1
            systemValue.car_speed -= 1;
        }
    }
    // 检查 write_cb_para->value 中是否包含 "CAR_OFF" 字符串
    if (strstr((char *)write_cb_para->value, "CAR_OFF") != NULL) {
        systemValue.car_status = CAR_STATUS_STOP; // 将汽车状态设置为停止状态
    } else if (strstr((char *)write_cb_para->value, "CAR_GO") != NULL) {
        systemValue.car_status = CAR_STATUS_RUN;
    } else if (strstr((char *)write_cb_para->value, "CAR_BACK") != NULL) {
        systemValue.car_status = CAR_STATUS_BACK;
    } else if (strstr((char *)write_cb_para->value, "CAR_LEFT") != NULL) {
        systemValue.car_status = CAR_STATUS_LEFT;
    } else if (strstr((char *)write_cb_para->value, "CAR_RIGHT") != NULL) {
        systemValue.car_status = CAR_STATUS_RIGHT;
    } else if (strstr((char *)write_cb_para->value, "CAR_CONFIG") != NULL) {
        printf("IR控制模式\n");
    } else if (strstr((char *)write_cb_para->value, "CAR_OK") != NULL) {
        systemValue.car_status = CAR_STATUS_STOP;
    } else if (strstr((char *)write_cb_para->value, "CAR_DEFAULT") != NULL) {
        systemValue.car_status = CAR_STATUS_STOP;
    } else if (strstr((char *)write_cb_para->value, "CAR_RETURN") != NULL) { // 关闭寻迹模式
        systemValue.auto_track_flag = 0;
        tmp_io.bit.p3 = 1;
        PCF8574_Write(tmp_io.all);
    } else if (strstr((char *)write_cb_para->value, "CAR_MENU") != NULL) { // 开启寻迹模式
        systemValue.car_speed = 0;
        systemValue.auto_track_flag = 1;
        tmp_io.bit.p3 = 0;
        PCF8574_Write(tmp_io.all);
    }
}

errcode_t sle_ssaps_register_cbks(void)
{
    errcode_t ret;
    ssaps_callbacks_t ssaps_cbk = {0};
    ssaps_cbk.add_service_cb = ssaps_add_service_cbk;
    ssaps_cbk.add_property_cb = ssaps_add_property_cbk;
    ssaps_cbk.add_descriptor_cb = ssaps_add_descriptor_cbk;
    ssaps_cbk.start_service_cb = ssaps_start_service_cbk;
    ssaps_cbk.delete_all_service_cb = ssaps_delete_all_service_cbk;
    ssaps_cbk.mtu_changed_cb = ssaps_mtu_changed_cbk;
    ssaps_cbk.read_request_cb = ssaps_read_request_cbk;
    ssaps_cbk.write_request_cb = ssaps_write_request_cbk;
    ret = ssaps_register_callbacks(&ssaps_cbk);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail :%x\r\n", __FUNCTION__, ret);
        return ret;
    }
    return ERRCODE_SLE_SUCCESS;
}

errcode_t sle_uuid_server_service_add(void)
{
    errcode_t ret;
    sle_uuid_t service_uuid = {0};
    sle_uuid_setu2(SLE_UUID_SERVER_SERVICE, &service_uuid);
    ret = ssaps_add_service_sync(g_server_id, &service_uuid, 1, &g_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail, ret:%x\r\n", __FUNCTION__, ret);
        return ERRCODE_SLE_FAIL;
    }
    return ERRCODE_SLE_SUCCESS;
}

errcode_t sle_uuid_server_property_add(void)
{
    errcode_t ret;
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t ntf_value[] = {0x01, 0x0};

    property.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    property.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_NOTIFY;
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &property.uuid);
    property.value = (uint8_t *)osal_vmalloc(sizeof(g_sle_property_value));
    if (property.value == NULL) {
        printf("[%s] property mem fail.\r\n", __FUNCTION__);
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(property.value, sizeof(g_sle_property_value), g_sle_property_value, sizeof(g_sle_property_value)) !=
        EOK) {
        printf("[%s] property mem cpy fail.\r\n", __FUNCTION__);
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property, &g_property_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail, ret:%x\r\n", __FUNCTION__, ret);
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    descriptor.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
    descriptor.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE;
    descriptor.value = ntf_value;
    descriptor.value_len = sizeof(ntf_value);

    ret = ssaps_add_descriptor_sync(g_server_id, g_service_handle, g_property_handle, &descriptor);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail, ret:%x\r\n", __FUNCTION__, ret);
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_SLE_FAIL;
    }
    osal_vfree(property.value);
    return ERRCODE_SLE_SUCCESS;
}

errcode_t sle_server_add(void)
{
    errcode_t ret;
    sle_uuid_t app_uuid = {0};

    printf("[%s] in\r\n", __FUNCTION__);
    app_uuid.len = sizeof(g_sle_uuid_app_uuid);
    if (memcpy_s(app_uuid.uuid, app_uuid.len, g_sle_uuid_app_uuid, sizeof(g_sle_uuid_app_uuid)) != EOK) {
        return ERRCODE_SLE_FAIL;
    }
    ssaps_register_server(&app_uuid, &g_server_id);

    if (sle_uuid_server_service_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }
    if (sle_uuid_server_property_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }
    printf("[%s] server_id:%x, service_handle:%x, property_handle:%x\r\n", __FUNCTION__, g_server_id, g_service_handle,
           g_property_handle);
    ret = ssaps_start_service(g_server_id, g_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail, ret:%x\r\n", __FUNCTION__, ret);
        return ERRCODE_SLE_FAIL;
    }
    printf("[%s] out\r\n", __FUNCTION__);
    return ERRCODE_SLE_SUCCESS;
}

/* device通过uuid向host发送数据：report */
errcode_t sle_server_send_report_by_uuid(msg_data_t msg_data)
{
    ssaps_ntf_ind_by_uuid_t param = {0};
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.start_handle = g_service_handle;
    param.end_handle = g_property_handle;
    param.value_len = msg_data.value_len;
    param.value = msg_data.value;
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &param.uuid);
    ssaps_notify_indicate_by_uuid(g_server_id, g_sle_conn_hdl, &param);
    return ERRCODE_SLE_SUCCESS;
}

/* device通过handle向host发送数据：report */
errcode_t sle_server_send_report_by_handle(msg_data_t msg_data)
{
    ssaps_ntf_ind_t param = {0};
    param.handle = g_property_handle;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.value = msg_data.value;
    param.value_len = msg_data.value_len;
    return ssaps_notify_indicate(g_server_id, g_sle_conn_hdl, &param);
}

void sle_connect_state_changed_cbk(uint16_t conn_id,
                                   const sle_addr_t *addr,
                                   sle_acb_state_t conn_state,
                                   sle_pair_state_t pair_state,
                                   sle_disc_reason_t disc_reason)
{
    printf("[%s] conn_id:0x%02x, conn_state:0x%x, pair_state:0x%x, disc_reason:0x%x\r\n", __FUNCTION__, conn_id,
           conn_state, pair_state, disc_reason);
    printf("[%s] addr:%02x:**:**:**:%02x:%02x\r\n", __FUNCTION__, addr->addr[0], addr->addr[4]); /* 0 4: index */
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        g_sle_conn_hdl = conn_id;
        sle_set_data_len(conn_id, DEFAULT_PAYLOAD_SIZE); // 设置有效载荷
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_sle_conn_hdl = 0;
        sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    }
    g_sle_conn_flag = conn_state;
}

void sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    printf("[%s] conn_id:%02x, status:%x\r\n", __FUNCTION__, conn_id, status);
    printf("[%s] addr:%02x:**:**:**:%02x:%02x\r\n", __FUNCTION__, addr->addr[0], addr->addr[4]); /* 0 4: index */
}

errcode_t sle_conn_register_cbks(void)
{
    errcode_t ret;
    sle_connection_callbacks_t conn_cbks = {0};
    conn_cbks.connect_state_changed_cb = sle_connect_state_changed_cbk;
    conn_cbks.pair_complete_cb = sle_pair_complete_cbk;
    ret = sle_connection_register_callbacks(&conn_cbks);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail :%x\r\n", __FUNCTION__, ret);
        return ret;
    }
    return ERRCODE_SLE_SUCCESS;
}

/* 初始化uuid server */
errcode_t sle_server_init(void)
{
    errcode_t ret;

    /* 使能SLE */
    if (enable_sle() != ERRCODE_SUCC) {
        printf("sle enbale fail !\r\n");
        return ERRCODE_FAIL;
    }

    ret = sle_announce_register_cbks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] sle_announce_register_cbks fail :%x\r\n", __FUNCTION__, ret);
        return ret;
    }
    ret = sle_conn_register_cbks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] sle_conn_register_cbks fail :%x\r\n", __FUNCTION__, ret);
        return ret;
    }
    ret = sle_ssaps_register_cbks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] sle_ssaps_register_cbks fail :%x\r\n", __FUNCTION__, ret);
        return ret;
    }
    ret = sle_server_add();
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] sle_server_add fail :%x\r\n", __FUNCTION__, ret);
        return ret;
    }
    ssap_exchange_info_t parameter = {0};
    parameter.mtu_size = SLE_MTU_SIZE_DEFAULT;
    parameter.version = 1;
    ssaps_set_info(g_server_id, &parameter);
    ret = sle_server_adv_init();
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] sle_server_adv_init fail :%x\r\n", __FUNCTION__, ret);
        return ret;
    }
    printf("[%s] init ok\r\n", __FUNCTION__);
    return ERRCODE_SLE_SUCCESS;
}
