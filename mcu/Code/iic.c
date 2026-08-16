#include "iic.h"

// I2C1初始化 (硬件)
void I2C1_Init(void)
{
    // 打开时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);   // GPIOB 时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);    // I2C1 时钟
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);      // DMA1 时钟

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);    // AFIO 时钟

    // ****** 关闭 I2C1 重映射，使用默认 PB6/PB7 ******
    GPIO_PinRemapConfig(GPIO_Remap_I2C1, DISABLE);

    // 初始化 PB6=SCL, PB7=SDA
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_OD;   // 复用开漏
    GPIO_Init(GPIOB, &gpio);

    // 初始化 I2C1
    I2C_InitTypeDef i2c;
    i2c.I2C_ClockSpeed = 300000;        // 300k  使用400k有概率会死机
    i2c.I2C_Mode = I2C_Mode_I2C;
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1 = 0x00;
    i2c.I2C_Ack = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C1, &i2c);
    I2C_Cmd(I2C1, ENABLE);

    // NVIC 配置（DMA TX: Channel 6）
    NVIC_InitTypeDef nvic;
    nvic.NVIC_IRQChannel = DMA1_Channel6_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 1;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    __enable_irq();  
}


void I2C1_DMA_Write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    // 1. 配置 DMA
    DMA_InitTypeDef dma;
    DMA_DeInit(DMA1_Channel6);  // I2C1_TX
    dma.DMA_PeripheralBaseAddr = (uint32_t)&I2C1->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)data;
    dma.DMA_DIR = DMA_DIR_PeripheralDST;
    dma.DMA_BufferSize = len;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_High;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel6, &dma);

    // 2. 启用 I2C DMA
    I2C_DMACmd(I2C1, ENABLE);

    // 3. I2C 发送 START、设备地址、寄存器地址（用软件）
    I2C_GenerateSTART(I2C1, ENABLE);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C1, dev_addr, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2C1, reg_addr);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    // 4. 启动 DMA
    DMA_Cmd(DMA1_Channel6, ENABLE);

    // 5. 等待 DMA 传输完成（可选用中断）
    while (!DMA_GetFlagStatus(DMA1_FLAG_TC6));
    DMA_ClearFlag(DMA1_FLAG_TC6);

    // 6. 发送 STOP
    I2C_GenerateSTOP(I2C1, ENABLE);

    // 7. 关闭 DMA（防止下次影响）
    DMA_Cmd(DMA1_Channel6, DISABLE);
    I2C_DMACmd(I2C1, DISABLE);
}
#define I2C_DMA_TIMEOUT_MS 100
#define OLED_ADDR 0x78  // OLED 7位地址=0x3C 左移1位变为0x78（常见0.96寸SSD1306）  
uint8_t dma_buffer[1025];  // 函数内静态缓冲，避免栈溢出
// 传输完成标志（非零表示完成）
volatile uint8_t i2c_dma_tx_done = 1;
extern uint8_t OLED_GRAM[8][128];
uint8_t OLED_DMA_RefreshFullScreen(void)
{

    if (!i2c_dma_tx_done) return 1;  // 上一次还没完成
	OLED_WriteByte(0x21, 1); // 命令
    OLED_WriteByte(0x00, 1); // 起始列
    OLED_WriteByte(0x7F, 1); // 结束列
	
	OLED_WriteByte(0x22, 1); // 命令
    OLED_WriteByte(0x00, 1); // 起始页
    OLED_WriteByte(0x07, 1); // 结束页
    // 1. 拼接发送缓冲区
    dma_buffer[0] = 0x40;  // 控制字节
    memcpy(&dma_buffer[1], (uint8_t *)OLED_GRAM, 1024); 
    // 2. 复位 DMA 标志和状态
    i2c_dma_tx_done = 0;
	
	
	DMA_DeInit(DMA1_Channel6);
    DMA_ClearFlag(DMA1_FLAG_TC6);
	
	DMA_InitTypeDef dma;
    dma.DMA_PeripheralBaseAddr = (uint32_t)&I2C1->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)dma_buffer;
    dma.DMA_DIR = DMA_DIR_PeripheralDST;
    dma.DMA_BufferSize = 1025;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_High;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel6, &dma);
	
    DMA_ITConfig(DMA1_Channel6, DMA_IT_TC, ENABLE);  // 使能传输完成中断

    // 3. 配置 DMA



    I2C_DMACmd(I2C1, ENABLE);

    // 4. I2C 启动通信（START + 地址发送）
    I2C_GenerateSTART(I2C1, ENABLE);
    uint32_t timeout = GetTick();
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT)) {
        if (GetTick() - timeout > I2C_DMA_TIMEOUT_MS) return 2;  // 超时错误
    }

    I2C_Send7bitAddress(I2C1, OLED_ADDR, I2C_Direction_Transmitter);
    timeout = GetTick();
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
        if (GetTick() - timeout > I2C_DMA_TIMEOUT_MS) return 3;  // 超时错误
    }

    // 5. 启动 DMA 传输（此时函数立即返回）
    DMA_Cmd(DMA1_Channel6, ENABLE);

    return 0;
}
//往一个从机的一个寄存器写一个字节数据
void IIC_WriteReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    I2C_GenerateSTART(I2C1, ENABLE);  // 发送起始信号
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C1, dev_addr, I2C_Direction_Transmitter);  // 发设备地址（写）
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2C1, reg_addr);  // 发寄存器地址
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2C1, data);  // 发数据
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(I2C1, ENABLE);  // 发送停止信号
}



//往一个从机的一个寄存器写多个字节数据
//形参:设备地址   寄存器地址   数据数组   数据长度
void IIC_WriteMulti(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    I2C_GenerateSTART(I2C1, ENABLE);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C1, dev_addr, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2C1, reg_addr);  // 起始地址
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    for (uint8_t i = 0; i < len; i++) {
        I2C_SendData(I2C1, data[i]);
        while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    }

    I2C_GenerateSTOP(I2C1, ENABLE);
}

//读一个从机的某个寄存器地址的一个字节数据
//形参: 设备地址  寄存器地址
uint8_t IIC_ReadReg(uint8_t dev_addr, uint8_t reg_addr)
{
    uint8_t data;

    // 第一次传输：写入寄存器地址
    I2C_GenerateSTART(I2C1, ENABLE);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C1, dev_addr, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2C1, reg_addr);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    // 第二次传输：重新启动并读取数据
    I2C_GenerateSTART(I2C1, ENABLE);  // 重启
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C1, dev_addr, I2C_Direction_Receiver);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    I2C_AcknowledgeConfig(I2C1, DISABLE);  // 不应答（只读一个字节）
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED));
    data = I2C_ReceiveData(I2C1);

    I2C_GenerateSTOP(I2C1, ENABLE);
    I2C_AcknowledgeConfig(I2C1, ENABLE);  // 恢复 ACK

    return data;
}

//读一个从机的某个寄存器地址的多个字节数据
//形参: 设备地址  寄存器地址 接受的数组地址 数据长度
void IIC_ReadMulti(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len)
{
    // 第一次传输：写入寄存器地址
    I2C_GenerateSTART(I2C1, ENABLE);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C1, dev_addr, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2C1, reg_addr);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    // 第二次传输：重新开始读数据
    I2C_GenerateSTART(I2C1, ENABLE);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C1, dev_addr, I2C_Direction_Receiver);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    // 接收 len 字节
    for (uint8_t i = 0; i < len; i++) {
        if (i == len - 1) {
            I2C_AcknowledgeConfig(I2C1, DISABLE);  // 最后一个字节：不应答
        } else {
            I2C_AcknowledgeConfig(I2C1, ENABLE);
        }

        while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED));
        buf[i] = I2C_ReceiveData(I2C1);
    }

    I2C_GenerateSTOP(I2C1, ENABLE);
    I2C_AcknowledgeConfig(I2C1, ENABLE);  // 恢复 ACK
}

