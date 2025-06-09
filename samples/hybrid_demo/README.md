# 星闪混合模式

## 案例提供者

[kidwjb](https://gitee.com/kidwjb)(将open Harmony 上的例程移植到ws63的sdk环境下)

原为open Harmony操作系统上的例程，开源案例提供者：[**Dragon**](https://gitee.com/hbu-dragon)

开源仓库：[sle_hybrid_demo](https://gitee.com/hbu-dragon/openharmony-nearlink-ws63-cases/tree/master/applications/sample/wifi-iot/app/sle_hybrid_demo)

## 案例设计

本案例旨在建立一个星闪混合模式节点，可以实现同一个星闪设备共存client和server，从而可以拓展星闪sle的网络结构。

### 硬件参考资料

- [HiHope ws63开发板](https://gitee.com/hihopeorg_group/near-link/blob/master/NearLink_Pi_IOT/%E6%98%9F%E9%97%AA%E6%B4%BE%E7%89%A9%E8%81%94%E7%BD%91%E5%BC%80%E5%8F%91%E5%A5%97%E4%BB%B6%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E%E4%B9%A6_V1.1.pdf)

- [HiHope ws63开发板原理图](https://gitee.com/hihopeorg_group/near-link/blob/master/NearLink_DK_WS63E/NearLink_DK_WS63E%E5%BC%80%E5%8F%91%E6%9D%BF%E5%8E%9F%E7%90%86%E5%9B%BE.pdf)

### 软件参考资料

- [HiHope ws63开发板驱动开发指南](../../../docs/board/WS63V100%20%E8%AE%BE%E5%A4%87%E9%A9%B1%E5%8A%A8%20%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97_02.pdf)
- [WS63V100 软件开发指南_03.pdf](https://gitee.com/HiSpark/fbb_ws63/blob/master/docs/board/WS63V100%20%E8%BD%AF%E4%BB%B6%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97_03.pdf)

### 参考头文件

- sle_device_discovery.h
- sle_connection_manager.h
- sle_ssap_client.h
- sle_ssap_server.h

## 实验平台

`HiHope_NearLink_DK_WS63_V03` 开发板

## 实验目的

本实验旨在通过HiHope_NearLink_DK_WS63_V03开发板，帮助开发者建立一个星闪混合节点

## 实验原理

**设备同时注册客户端和服务端的ssap服务，然后分别注册服务端的设备公开和设备服务，客户端注册设备寻找和设备连接**

### API 讲解

1. **ssapc_register_callbacks**
   - **功能**：注册SSAP客户端回调函数。
   - **参数**：
     - ssapc_callbacks_t *func
   - **返回值**： ERRCODE_SUCC 成功 ，Other 失败

2. **ssaps_notify_indicate_by_uuid**
   - **功能**：通过uuid向对端发送通知或指示。
   - **参数**：
     - uint8_t server_id
     -  uint16_t conn_id
     - ssaps_ntf_ind_t *param
   - **返回值**： ERRCODE_SUCC 成功 ，Other 失败

5. **ssapc_write_req**

   - **功能**：客户端发起写请求。
   - **参数**：
     - uint8_t client_id
     - uint16_t conn_id
     - ssapc_write_param_t *param
   - **返回值**： ERRCODE_SUCC 成功 ，Other 失败

---

### 流程图说明（以客户端为例）

- **步骤 1**：注册sle客户端和服务端的ssap

- **步骤 2**：注册客户端的seek 和 connect

- **步骤 3**：注册服务端的设备公开和设备服务

- **步骤 4**：使能sle

- **实验大致流程图**

  ![](E:\open_source\fbb_ws63\vendor\others\demo\hybrid_demo\pics\sle流程图.png)

## 实验步骤

### 例程代码

sle_hybrid.c

```c
#include <stdio.h>  
#include <string.h>   
#include <unistd.h>    
#include "app_init.h" 
#include "cmsis_os2.h" 
#include "common_def.h"
#include "soc_osal.h"
#include "sle_device_discovery.h"
#include "sle_uuid_client.h"
#include "sle_uuid_server.h"

// static void TestHybridCSend(void)
// {
//     osal_printk("Hybrid-C Send\r\n");

//     sle_hybridc_wait_service_found();

//     char data[32] = {0};
//     int count = 1;
//     while (1)
//     {
//         sprintf(data, "%d", count);
//         sle_hybridc_send_data((uint8_t *)data, strlen(data));
//         count++;
//         osDelay(100);
//     }
// }

static void TestHybridSSend(void)
{
    osal_printk("Hybrid-S Send\r\n");

    sle_hybrids_wait_client_connected();

    char data[16] = {0};
    int count = 1;
    errcode_t ret = 0;
    while (1)
    {
        sprintf(data, "%d", count);
        ret = sle_hybrids_send_data((uint8_t *)data, strlen(data));
        if(ret != ERRCODE_SUCC)
        {
            osal_printk("sle_hybrids_send_data FAIL\r\n");
        }
        else
        {
            osal_printk("sle_hybrids_send_data SUCC\r\n");
        }
        count++;
        osDelay(100);
    }
}

extern errcode_t sle_register_common_cbks(void);

void sle_hybrid_task(char *arg)
{
    unused(arg);
    errcode_t ret = 0;
    osal_printk("[sle hybrid] sle hybrid-s init\r\n");
    ret = sle_hybridS_init();
    if(ret != ERRCODE_SUCC)
    {
        osal_printk("[sle hybrid] sle hybrid-s init FAIL\r\n");
        return;
    }

    sle_set_remote_server_name("sle_server");
    
    osal_printk("[sle hybrid] sle hybrid-c init\r\n");

    ret = sle_hybridC_init();
    if(ret != ERRCODE_SUCC)
    {
        osal_printk("[sle hybrid] sle hybrid-c init FAIL\r\n");
        return;
    }

    ret = sle_register_common_cbks();
    if(ret != ERRCODE_SUCC)
    {
        osal_printk("[sle hybrid] sle_register_common_cbks FAIL\r\n");
        return;
    }

    ret = enable_sle();
    if (ret != 0)
    {
        osal_printk("enable_sle fail :%x\r\n", ret);
        return;
    }
    osal_printk("enable_sle succ\r\n");
    sle_set_hybridc_addr();

    //TestHybridCSend();
    TestHybridSSend();
}

#define SLE_HYBRIDTASK_PRIO 24
#define SLE_HYBRID_STACK_SIZE 0x2000

static void sle_hybrid_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle= osal_kthread_create((osal_kthread_handler)sle_hybrid_task, 0, "sle_gatt_client",
        SLE_HYBRID_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_HYBRIDTASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

app_run(sle_hybrid_entry);
```

