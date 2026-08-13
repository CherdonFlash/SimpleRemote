#ifndef __IIC_H_
#define __IIC_H_

#include "stm32f10x.h"
#include <stdio.h>  
#include <string.h>
#include "delay.h"
#include "oled096.h"
void I2C1_Init(void);
void IIC_WriteReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
void IIC_WriteMulti(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len);
uint8_t IIC_ReadReg(uint8_t dev_addr, uint8_t reg_addr);
void IIC_ReadMulti(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len);
void I2C1_DMA_Write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
uint8_t OLED_DMA_RefreshFullScreen(void);
#endif
