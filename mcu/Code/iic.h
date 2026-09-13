#ifndef __IIC_H_
#define __IIC_H_

#include "stm32f10x.h"
#include "delay.h"
#include <string.h>

typedef enum {
    IIC_OK = 0, IIC_BUSY, IIC_TIMEOUT, IIC_BUS_ERROR, IIC_DMA_ERROR, IIC_ARGUMENT
} IIC_Status;

/* DMA完成不等于最后一字节已经出线；由主循环 Service 完成 BTF/STOP 收尾。 */
extern volatile uint8_t i2c_dma_tx_done;
typedef struct {
    uint32_t started;
    uint32_t completed;
    uint32_t errors;
    uint32_t recoveries;
    uint32_t last_error;
} IIC_Diagnostics;
extern volatile IIC_Diagnostics g_iic_diag;

void I2C1_Init(void);
void I2C1_Service(void);
void I2C1_DMA_IRQHandler(void);
uint8_t I2C1_IsHealthy(void);
IIC_Status I2C1_Recover(void);
IIC_Status IIC_WriteReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
IIC_Status IIC_WriteMulti(uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, uint16_t len);
/* 兼容原单字节调用；同步有界传输，允许传入局部变量地址。 */
IIC_Status I2C1_DMA_Write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
uint8_t IIC_ReadReg(uint8_t dev_addr, uint8_t reg_addr);
IIC_Status IIC_ReadMulti(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len);
uint8_t OLED_DMA_RefreshFullScreen(void);
#endif
