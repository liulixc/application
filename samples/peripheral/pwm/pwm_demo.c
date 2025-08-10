/**
 * @file pwm_demo.c
 * @brief WS2812 RGB LED控制演示程序
 * 
 * 本文件实现了使用PWM信号控制WS2812类型RGB LED灯带的功能。
 * WS2812是一种集成了控制电路的智能LED，通过单线串行通信协议控制。
 * 
 * WS2812协议时序要求：
 * - 逻辑0：高电平0.32μs，低电平0.68μs（总周期1μs）
 * - 逻辑1：高电平0.68μs，低电平0.32μs（总周期1μs）
 * - 复位信号：低电平>50μs
 * - 数据格式：24位GRB（绿-红-蓝）顺序
 */


// ==================== 头文件包含 ====================
#if defined(CONFIG_PWM_SUPPORT_LPM)
#include "pm_veto.h"          // 低功耗管理相关
#endif

#include "common_def.h"        // 通用定义和宏
#include "pinctrl.h"           // 引脚控制接口
#include "pwm.h"               // PWM驱动接口
#include "soc_osal.h"          // 操作系统抽象层
#include "app_init.h"          // 应用初始化接口
#include "gpio.h"              // GPIO控制接口

// ==================== 宏定义 ====================
#define TEST_TCXO_DELAY_1000MS 1000    // 测试延时1000毫秒
#define PWM_TASK_PRIO 24               // PWM任务优先级
#define PWM_TASK_STACK_SIZE 0x1000     // PWM任务栈大小（4KB）

// WS2812 LED控制相关配置
#define CONFIG_PWM1_PIN 5              // PWM输出引脚号（GPIO5）
#define CONFIG_PWM1_CHANNEL 5          // PWM通道号
#define CONFIG_PWM1_PIN_MODE 1         // 引脚复用模式（PWM功能）
#define CONFIG_PWM1_GROUP_ID 0         // PWM组ID

// LED颜色和时序配置
#define COLOR 255                      // LED最大亮度值（8位：0-255）
#define DELAY_MS 1000                  // 颜色切换延时（毫秒）
#define RES 600                        // WS2812复位信号持续时间（微秒）

/**
 * @brief 发送WS2812逻辑0信号
 * 
 * WS2812逻辑0时序要求：
 * - 高电平：0.32μs（32% 占空比）
 * - 低电平：0.68μs（68% 占空比）
 * - 总周期：1μs
 * 
 * PWM配置参数说明：
 * - duty_cycle_high: 32 (高电平占空比32%)
 * - duty_cycle_low: 68 (低电平占空比68%)
 * - offset: 0 (无偏移)
 * - cycles: 1 (执行1个周期)
 * - repeat: false (不重复)
 */
void set_code0(void)
{
  // 配置PWM参数以生成WS2812逻辑0信号
  pwm_config_t cfg_no_repeat = {32, 68, 0, 1, false};
  
  // 打开PWM通道
  uapi_pwm_open(CONFIG_PWM1_CHANNEL, &cfg_no_repeat);
  
  // 设置PWM组
  uint8_t channel_id = CONFIG_PWM1_CHANNEL;
  uapi_pwm_set_group(CONFIG_PWM1_GROUP_ID, &channel_id, 1);
  
  // 启动PWM组输出
  uapi_pwm_start_group(CONFIG_PWM1_GROUP_ID);
  
  // 关闭PWM组
  uapi_pwm_close(CONFIG_PWM1_GROUP_ID);
}

/**
 * @brief 发送WS2812逻辑1信号
 * 
 * WS2812逻辑1时序要求：
 * - 高电平：0.68μs（68% 占空比）
 * - 低电平：0.32μs（32% 占空比）
 * - 总周期：1μs
 * 
 * PWM配置参数说明：
 * - duty_cycle_high: 68 (高电平占空比68%)
 * - duty_cycle_low: 32 (低电平占空比32%)
 * - offset: 0 (无偏移)
 * - cycles: 1 (执行1个周期)
 * - repeat: false (不重复)
 */
void set_code1(void)
{
  // 配置PWM参数以生成WS2812逻辑1信号
  pwm_config_t cfg_no_repeat = {68, 32, 0, 1, false};
  
  // 打开PWM通道
  uapi_pwm_open(CONFIG_PWM1_CHANNEL, &cfg_no_repeat);
  
  // 设置PWM组
  uint8_t channel_id = CONFIG_PWM1_CHANNEL;
  uapi_pwm_set_group(CONFIG_PWM1_GROUP_ID, &channel_id, 1);
  
  // 启动PWM组输出
  uapi_pwm_start_group(CONFIG_PWM1_GROUP_ID);
  
  // 关闭PWM组
  uapi_pwm_close(CONFIG_PWM1_GROUP_ID);
}

/**
 * @brief 设置WS2812 LED的颜色
 * 
 * WS2812数据格式为24位GRB（绿-红-蓝）顺序，每个颜色分量8位。
 * 数据传输顺序：先发送高位（MSB），后发送低位（LSB）。
 * 
 * @param red   红色分量值（0-255）
 * @param green 绿色分量值（0-255）
 * @param blue  蓝色分量值（0-255）
 * 
 * 数据传输顺序：
 * 1. 绿色8位数据（G7-G0）
 * 2. 红色8位数据（R7-R0）
 * 3. 蓝色8位数据（B7-B0）
 */
static void pwm_color(uint8_t red, uint8_t green, uint8_t blue)
{
  int code;
  
  // 发送绿色分量数据（8位，从高位到低位）
  for (int bit = 0; bit < 8; bit++) {
      // 左移bit位后与0x80（10000000）进行与运算，检查最高位
      code = (green << bit) & 0x80;
      if (code != 0) {
          set_code1();  // 发送逻辑1
      } else {
          set_code0();  // 发送逻辑0
      }
  }

  // 发送红色分量数据（8位，从高位到低位）
  for (int bit = 0; bit < 8; bit++) {
      // 左移bit位后与0x80（10000000）进行与运算，检查最高位
      code = (red << bit) & 0x80;
      if (code != 0) {
          set_code1();  // 发送逻辑1
      } else {
          set_code0();  // 发送逻辑0
      }
  }

  // 发送蓝色分量数据（8位，从高位到低位）
  for (int bit = 0; bit < 8; bit++) {
      // 左移bit位后与0x80（10000000）进行与运算，检查最高位
      code = (blue << bit) & 0x80;
      if (code != 0) {
          set_code1();  // 发送逻辑1
      } else {
          set_code0();  // 发送逻辑0
      }
  }
}

/**
 * @brief 复位WS2812 LED灯带
 * 
 * WS2812复位信号要求：
 * - 低电平持续时间 > 50μs
 * - 本实现使用600μs的低电平信号确保可靠复位
 * 
 * 复位后所有LED将关闭（黑色状态），为下一次数据传输做准备。
 * 在发送新的颜色数据前必须先发送复位信号。
 */
void reset_leds(void)
{
  // 配置PWM参数生成复位信号：600μs低电平，0μs高电平
  pwm_config_t cfg_reset = {RES, 0, 0, 1, false};

  // 打开PWM通道
  uapi_pwm_open(CONFIG_PWM1_CHANNEL, &cfg_reset);

  // 设置PWM组
  uint8_t channel_id = CONFIG_PWM1_CHANNEL;
  uapi_pwm_set_group(CONFIG_PWM1_GROUP_ID, &channel_id, 1);

  // 启动PWM组输出复位信号
  uapi_pwm_start_group(CONFIG_PWM1_GROUP_ID);

  // 关闭PWM组
  uapi_pwm_close(CONFIG_PWM1_GROUP_ID);
}
/**
 * @brief PWM任务主函数
 * 
 * 该函数实现WS2812 LED颜色循环显示功能：
 * 1. 初始化PWM和引脚配置
 * 2. 复位LED灯带
 * 3. 循环显示红、绿、蓝三种颜色
 * 
 * 颜色循环顺序：红色 -> 绿色 -> 蓝色 -> 红色...
 * 每种颜色持续显示1秒钟
 * 
 * @return NULL 任务函数返回值（未使用）
 */
static void *pwm_task(void)
{
  // 打印任务启动信息
  osal_printk("PWM start. \r\n");
  
  // 配置GPIO引脚为PWM功能模式
  uapi_pin_set_mode(CONFIG_PWM1_PIN, CONFIG_PWM1_PIN_MODE);
  
  // 反初始化PWM（清除之前的配置）
  uapi_pwm_deinit();
  
  // 初始化PWM模块
  uapi_pwm_init();
  
  // 复位LED灯带，确保初始状态为关闭
  reset_leds();

  // 等待10毫秒，确保复位信号生效
  osal_msleep(10);

  // 无限循环显示颜色
  while (1) {
      // 显示红色（R=255, G=0, B=0）
      pwm_color(COLOR, 0, 0);
      osal_msleep(DELAY_MS);  // 延时1秒
      
      // 显示绿色（R=0, G=255, B=0）
      pwm_color(0, COLOR, 0);
      osal_msleep(DELAY_MS);  // 延时1秒
      
      // 显示蓝色（R=0, G=0, B=255）
      pwm_color(0, 0, COLOR);
      osal_msleep(DELAY_MS);  // 延时1秒
  }
  return NULL;
}

/**
 * @brief PWM应用程序入口函数
 * 
 * 该函数负责创建和配置PWM任务：
 * 1. 创建PWM任务线程
 * 2. 设置任务优先级
 * 3. 启动任务执行
 * 
 * 任务配置：
 * - 任务名称："PwmTask"
 * - 栈大小：4KB (PWM_TASK_STACK_SIZE)
 * - 优先级：24 (PWM_TASK_PRIO)
 */
static void pwm_entry(void)
{
  osal_task *task_handle = NULL;
  
  // 锁定内核线程操作
  osal_kthread_lock();
  
  // 创建PWM任务线程
  task_handle = osal_kthread_create((osal_kthread_handler)pwm_task, 0, "PwmTask", PWM_TASK_STACK_SIZE);
  
  // 如果任务创建成功，设置任务优先级
  if (task_handle != NULL) {
      osal_kthread_set_priority(task_handle, PWM_TASK_PRIO);
  }
  
  // 解锁内核线程操作
  osal_kthread_unlock();
}

/**
 * @brief 程序主入口
 * 
 * 通过app_run宏启动PWM演示应用程序。
 * 该宏会在系统初始化完成后自动调用pwm_entry函数。
 */
app_run(pwm_entry);