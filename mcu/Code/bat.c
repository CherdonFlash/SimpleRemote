#include "bat.h"
//电池电压检测
extern uint16_t BAT_ADC;//电源ADC原始值

float V_Bat = 0.0f;
uint8_t BAT_Percent = 0; //电量百分比变量 (0~100)

//转换电池电压为实际电压，并返回百分比
float BAT_Get(void){

    const float VREF = 3.3f;        // V 单片机参考电压 VDDA
    const float scale = 2.0f;       // 硬件分压系数
    
    // --- 一阶低通滤波参数 ---
    const float alpha = 0.01f;       // 滤波系数 (可根据实际跳动情况修改，比如0.05或0.2)
    static float filtered_adc = 0;  // 静态变量：用于保存上一次的滤波结果
    static uint8_t is_first_run = 1;// 静态变量：用于标记是否为首次运行

    // 1. 获取滤波后的 ADC 值
    if (is_first_run) {
        // 第一次运行直接赋初值，防止电压从 0 开始缓慢爬升
        filtered_adc = (float)BAT_ADC; 
        is_first_run = 0;
    } else {
        // 滤波核心公式：新滤波值 = 系数 * 当前原始值 + (1 - 系数) * 上次滤波值
        filtered_adc = alpha * (float)BAT_ADC + (1.0f - alpha) * filtered_adc;
    }

    // 2. 使用滤波后的 ADC 值计算实际电压
    float v_adc = (filtered_adc / 4095.0f) * VREF;
    V_Bat = v_adc * scale;
    
    // --- 计算电量百分比 ---
    float v_calc = V_Bat;
    
    // 限制上下限，防止计算出超过 100% 或负数的百分比
    if (v_calc > 4.2f) v_calc = 4.2f;
    if (v_calc < 3.2f) v_calc = 3.2f;

    // 线性映射: (当前电压 - 最低电压) / (最高电压 - 最低电压) * 100
    BAT_Percent = (uint8_t)(((v_calc - 3.2f) / (4.2f - 3.2f)) * 100.0f);

    return BAT_Percent; 
}











