#ifndef _NRF24_H_
#define _NRF24_H_

#include "stm32f10x.h"
#include "uart.h"
#include "delay.h"
/**********  NRF24L01寄存器操作命令  ***********/
#define nRF_READ_REG        0x00 
#define nRF_WRITE_REG       0x20 
#define RD_RX_PLOAD     0x61  
#define WR_TX_PLOAD     0xA0  
#define FLUSH_TX        0xE1  
#define FLUSH_RX        0xE2  
#define REUSE_TX_PL     0xE3  
#define NOP             0xFF        //空地址
/**********  NRF24L01寄存器地址   *************/
//以下寄存器在数据手册57页开始有说明
#define CONFIG          0x00        //配置寄存器                   
#define EN_AA           0x01        //自动确认功能
#define EN_RXADDR       0x02        //启用RX地址
#define SETUP_AW        0x03        //地址宽度的设置
#define SETUP_RETR      0x04        //自动重传设置
#define RF_CH           0x05        //射频通道
#define RF_SETUP        0x06        //射频设置寄存器
#define STATUS          0x07        //状态寄存器
#define OBSERVE_TX      0x08        //发送观测寄存器
#define CD              0x09        //接受功率检测器
#define RX_ADDR_P0      0x0A        //接受地址数据管道0 最大5字节
#define RX_ADDR_P1      0x0B        //接受地址数据管道1 最大5字节
#define RX_ADDR_P2      0x0C        //是管道1的低8位
#define RX_ADDR_P3      0x0D        //是管道1的低8位
#define RX_ADDR_P4      0x0E        //是管道1的低8位
#define RX_ADDR_P5      0x0F        //是管道1的低8位
#define TX_ADDR         0x10        //传输地址
#define RX_PW_P0        0x11        //管道0中有效负载的字节数(1-32)
#define RX_PW_P1        0x12        //管道1中有效负载的字节数(1-32)
#define RX_PW_P2        0x13        //管道2中有效负载的字节数(1-32)
#define RX_PW_P3        0x14        //管道3中有效负载的字节数(1-32)
#define RX_PW_P4        0x15        //管道4中有效负载的字节数(1-32)
#define RX_PW_P5        0x16        //管道5中有效负载的字节数(1-32)
#define FIFO_STATUS     0x17        //先进先出状态寄存器

/******   STATUS寄存器bit位定义      *******/
#define MAX_TX  	0x10  	  //达到最大发送次数中断
#define TX_OK   	0x20  	  //TX发送完成中断
#define RX_OK   	0x40  	  //接收到数据中断
void nrf24_gpio(void);

uint8_t SPI1_ReadWriteByte(uint8_t TxData);
void NRF_Write_Buf(uint8_t reg, uint8_t *pBuf, uint8_t len);
void NRF_Read_Buf(uint8_t reg, uint8_t *pBuf, uint8_t len);
void nrf24_init(void);
uint8_t nrf24_check(void);
void NRF_Read_Buf(uint8_t reg, uint8_t *pBuf, uint8_t len);
void NRF_Write_Reg(uint8_t reg, uint8_t data);
uint8_t NRF_Read_Reg(uint8_t reg);
uint8_t SPI1_ReadWriteByte(uint8_t TxData);
void nrf24_TX_init(void);
uint8_t nrf24_send(uint8_t *Buf);
void NRF_SendAll(void);
uint8_t nrf24_get(uint8_t *Buf);
uint8_t NRF_Write_Cmd(uint8_t cmd);
#endif







