
#include "Track.h"
#include "pid.h"
// 巡线模式，黑色非反光胶带，宽度20mm，白色背景，巡线指示灯为绿色，非白色背景，巡线指示灯黄色、红色或者白色
void Car_search_black_Line(void)
{
    if (systemValue.track_data != 0xE7) { // 如果不是走直线，不使用pid，调速值清零
        R_Motor_PWM = 0;
        L_Motor_PWM = 0;
    }
    // 根据gLineOut信号，调节左右电机转速
    switch (systemValue.track_data) {
        case 0xFE: // 黑线偏左，右电机提速
            car_advance(0, 17000);
            break;
        case 0xFD: // 黑线偏左，右电机提速
            car_advance(0, 17000);
            break;
        case 0xFB: // 黑线偏左，右电机提速
            car_advance(14000, 18000);
            break;
        case 0xE7: // 中间，速度值相等
            car_advance(L_Motor_PWM, R_Motor_PWM);
            break;
        case 0xDF: // 黑线偏右，左电机提速
            car_advance(18000, 14000);
            break;
        case 0xBF: // 黑线偏右，左电机提速
            car_advance(17000, 0);
            break;
        case 0x7F: // 黑线偏右，左电机提速
            car_advance(17000, 0);
            break;
        default:

            break;
    }
    // 电机输出
    if (systemValue.distance <= 150) // 距离小于150mm
    {
        car_stop();
    }
}
