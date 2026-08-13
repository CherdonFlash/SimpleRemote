#ifndef __OLED096_H_
#define __OLED096_H_

#include "stm32f10x.h"
#include "iic.h"
#include <stdio.h>  // 用于 sprintf
#include <math.h>  // 需要包含math.h用于sin、cos
extern const uint8_t ASCII816[95][16];
void OLED_Init(void);

void OLED_WriteByte(uint8_t byte, uint8_t cmd);
void OLED_SetPos(uint8_t x, uint8_t y);
void OLED_ShowChar(unsigned char x, unsigned char y, char chr);
void OLED_ShowString(unsigned char x, unsigned char y, const char *str);
void OLED_ShowChinese(uint8_t x, uint8_t y, const uint8_t hz[2][16]);
void OLED_ShowInt(uint8_t x, uint8_t y, int num);
void OLED_Clear(void);

void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
uint8_t OLED_Refresh(void);
void OLED_DrawRectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t fill);
void OLED_DrawCircle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t fill);
void OLED_DrawLine(uint8_t x_start, uint8_t x_end, uint8_t y);
void OLED_DrawRay(uint8_t x0, uint8_t y0, float angle, uint8_t length);
#endif
