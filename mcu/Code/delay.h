#ifndef __DELAY_H_
#define __DELAY_H_

#include "stm32f10x.h"
void Delay_ms(uint32_t ms);
uint8_t SystemClock_HSE_72MHz(void);
void SysTick_Init(void);
uint32_t GetTick(void);
#endif
