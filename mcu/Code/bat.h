#ifndef __BAT_H_
#define __BAT_H_

#include "stm32f10x.h"
#include <stdio.h>
#include <stdarg.h>
/* V1.1 电池状态由 CW2015 更新，不再使用 ADC 分压或电压线性估算 SOC。 */
extern float V_Bat;
extern uint8_t BAT_Percent;
#endif
