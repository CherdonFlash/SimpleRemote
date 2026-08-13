#ifndef __UART_H_
#define __UART_H_

#include "stm32f10x.h"
#include <stdio.h>
#include <stdarg.h>
void USART1_Init(uint32_t baudrate);
void UART1_SendChar(uint8_t ch);
void UART1_SendString(const char* str);
void UART1_Printf(const char *fmt, ...);
#endif
