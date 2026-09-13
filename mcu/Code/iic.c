#include "iic.h"
#include "oled096.h"

#define OLED_ADDR          0x78U
#define OLED_I2C_CLOCK_HZ  400000U /* 先测试400kHz；波形不稳时回退300kHz */
#define IIC_WAIT_MS        5U
#define IIC_FRAME_MS       100U
#define IIC_SCL            GPIO_Pin_6
#define IIC_SDA            GPIO_Pin_7
#define IIC_LINES          (IIC_SCL | IIC_SDA)
#define IIC_ERROR_BITS     (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR)

extern uint8_t OLED_GRAM[8][128];
static uint8_t dma_buffer[1025];
static volatile uint8_t dma_phase; /* 0空闲、1搬运、2等待BTF、3等待STOP */
static volatile uint8_t dma_failed;
static uint8_t bus_fault;
static uint32_t dma_started_ms;
volatile uint8_t i2c_dma_tx_done = 1;
volatile IIC_Diagnostics g_iic_diag = {0};

static void IIC_ConfigPins(GPIOMode_TypeDef mode)
{
    GPIO_InitTypeDef gpio;
    GPIOB->BSRR = IIC_LINES;
    gpio.GPIO_Pin = IIC_LINES;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = mode;
    GPIO_Init(GPIOB, &gpio);
}

void I2C1_Init(void)
{
    I2C_InitTypeDef i2c;
    NVIC_InitTypeDef nvic;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_I2C1, DISABLE);
    DMA_Cmd(DMA1_Channel6, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL6);
    I2C_DeInit(I2C1);
    IIC_ConfigPins(GPIO_Mode_AF_OD);
    I2C_StructInit(&i2c);
    i2c.I2C_ClockSpeed = OLED_I2C_CLOCK_HZ;
    I2C_Init(I2C1, &i2c);
    I2C_Cmd(I2C1, ENABLE);

    nvic.NVIC_IRQChannel = DMA1_Channel6_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 1;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
    dma_phase = 0;
    dma_failed = 0;
    bus_fault = 0;
    i2c_dma_tx_done = 1;
}

/* 所有失败统一收尾；这里不循环重试，避免缺屏拖住遥控主循环。 */
static IIC_Status IIC_Abort(IIC_Status reason)
{
    DMA_ITConfig(DMA1_Channel6, DMA_IT_TC | DMA_IT_TE, DISABLE);
    DMA_Cmd(DMA1_Channel6, DISABLE);
    I2C_DMACmd(I2C1, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL6);
    if (I2C1->SR2 & I2C_SR2_MSL) I2C_GenerateSTOP(I2C1, ENABLE);
    I2C_Cmd(I2C1, DISABLE);
    IIC_ConfigPins(GPIO_Mode_Out_OD); /* 释放双线，恢复留给低频重连任务 */
    dma_phase = 0;
    dma_failed = 0;
    bus_fault = 1;
    i2c_dma_tx_done = 1;
    ++g_iic_diag.errors;
    g_iic_diag.last_error = reason;
    return reason;
}

static IIC_Status IIC_WaitFlag(uint32_t flag, FlagStatus wanted)
{
    uint32_t start = GetTick();
    while (I2C_GetFlagStatus(I2C1, flag) != wanted) {
        if (I2C1->SR1 & IIC_ERROR_BITS) return IIC_BUS_ERROR;
        if (GetTick() - start >= IIC_WAIT_MS) return IIC_TIMEOUT;
    }
    return (I2C1->SR1 & IIC_ERROR_BITS) ? IIC_BUS_ERROR : IIC_OK;
}

static IIC_Status IIC_WaitStop(void)
{
    uint32_t start = GetTick();
    while ((I2C1->CR1 & I2C_CR1_STOP) || (I2C1->SR2 & I2C_SR2_BUSY)) {
        if (I2C1->SR1 & IIC_ERROR_BITS) return IIC_BUS_ERROR;
        if (GetTick() - start >= IIC_WAIT_MS) return IIC_TIMEOUT;
    }
    return IIC_OK;
}

static void IIC_ClearAddr(void)
{
    volatile uint32_t unused = I2C1->SR1;
    unused = I2C1->SR2;
    (void)unused;
}

/* 地址阶段保留 ADDR 标志，接收单字节时需要先关ACK再清ADDR。 */
static IIC_Status IIC_Address(uint8_t address, uint8_t direction)
{
    IIC_Status status;
    I2C_GenerateSTART(I2C1, ENABLE);
    status = IIC_WaitFlag(I2C_FLAG_SB, SET);
    if (status != IIC_OK) return status;
    I2C_Send7bitAddress(I2C1, address, direction);
    return IIC_WaitFlag(I2C_FLAG_ADDR, SET);
}

static IIC_Status IIC_BeginWrite(uint8_t address)
{
    IIC_Status status;
    if (bus_fault) return IIC_BUS_ERROR;
    if (!i2c_dma_tx_done) return IIC_BUSY;
    status = IIC_WaitStop();
    if (status == IIC_OK) status = IIC_Address(address, I2C_Direction_Transmitter);
    if (status == IIC_OK) IIC_ClearAddr();
    else IIC_Abort(status);
    return status;
}

IIC_Status IIC_WriteMulti(uint8_t address, uint8_t reg, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    IIC_Status status;
    if (data == NULL || len == 0) return IIC_ARGUMENT;
    status = IIC_BeginWrite(address);
    if (status != IIC_OK) return status;
    I2C_SendData(I2C1, reg);
    status = IIC_WaitFlag(I2C_FLAG_BTF, SET);
    for (i = 0; i < len && status == IIC_OK; ++i) {
        I2C_SendData(I2C1, data[i]);
        status = IIC_WaitFlag(I2C_FLAG_BTF, SET);
    }
    if (status != IIC_OK) return IIC_Abort(status);
    I2C_GenerateSTOP(I2C1, ENABLE);
    status = IIC_WaitStop();
    return status == IIC_OK ? status : IIC_Abort(status);
}

IIC_Status IIC_WriteReg(uint8_t address, uint8_t reg, uint8_t value)
{
    return IIC_WriteMulti(address, reg, &value, 1);
}

IIC_Status I2C1_DMA_Write(uint8_t address, uint8_t reg, uint8_t *data, uint16_t len)
{
    return IIC_WriteMulti(address, reg, data, len);
}

/* 旧通用读取接口保留：逐寄存器单字节事务，遵守F1单字节接收ACK/ADDR顺序。
 * 当前OLED不用这些接口，CW2015使用独立的软件I2C驱动。 */
IIC_Status IIC_ReadMulti(uint8_t address, uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    uint32_t primask;
    IIC_Status status;
    if (buf == NULL || len == 0) return IIC_ARGUMENT;
    for (i = 0; i < len; ++i) {
        status = IIC_BeginWrite(address);
        if (status != IIC_OK) return status;
        I2C_SendData(I2C1, (uint8_t)(reg + i));
        status = IIC_WaitFlag(I2C_FLAG_BTF, SET);
        if (status == IIC_OK) status = IIC_Address(address, I2C_Direction_Receiver);
        if (status != IIC_OK) return IIC_Abort(status);
        primask = __get_PRIMASK();
        __disable_irq();
        I2C_AcknowledgeConfig(I2C1, DISABLE);
        IIC_ClearAddr();
        I2C_GenerateSTOP(I2C1, ENABLE);
        __set_PRIMASK(primask);
        status = IIC_WaitFlag(I2C_FLAG_RXNE, SET);
        if (status != IIC_OK) return IIC_Abort(status);
        buf[i] = I2C_ReceiveData(I2C1);
        status = IIC_WaitStop();
        I2C_AcknowledgeConfig(I2C1, ENABLE);
        if (status != IIC_OK) return IIC_Abort(status);
    }
    return IIC_OK;
}

uint8_t IIC_ReadReg(uint8_t address, uint8_t reg)
{
    uint8_t value = 0;
    (void)IIC_ReadMulti(address, reg, &value, 1);
    return value; /* 新调用应使用带状态返回的 ReadMulti，避免把失败当作零值。 */
}

uint8_t OLED_DMA_RefreshFullScreen(void)
{
    static const uint8_t window[] = {0x21, 0x00, 0x7F, 0x22, 0x00, 0x07};
    DMA_InitTypeDef dma;
    IIC_Status status;
    if (!i2c_dma_tx_done) return IIC_BUSY;
    status = IIC_WriteMulti(OLED_ADDR, 0x00, window, sizeof(window));
    if (status != IIC_OK) return status;
    status = IIC_BeginWrite(OLED_ADDR);
    if (status != IIC_OK) return status;

    dma_buffer[0] = 0x40;
    memcpy(&dma_buffer[1], OLED_GRAM, sizeof(OLED_GRAM));
    DMA_DeInit(DMA1_Channel6);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&I2C1->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)dma_buffer;
    dma.DMA_DIR = DMA_DIR_PeripheralDST;
    dma.DMA_BufferSize = sizeof(dma_buffer);
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_Priority = DMA_Priority_High;
    DMA_Init(DMA1_Channel6, &dma);
    DMA_ClearFlag(DMA1_FLAG_GL6);
    DMA_ITConfig(DMA1_Channel6, DMA_IT_TC | DMA_IT_TE, ENABLE);
    dma_started_ms = GetTick();
    dma_failed = 0;
    dma_phase = 1;
    i2c_dma_tx_done = 0;
    ++g_iic_diag.started;
    I2C_DMACmd(I2C1, ENABLE);
    DMA_Cmd(DMA1_Channel6, ENABLE);
    return IIC_OK;
}

/* 中断只记录完成/错误，禁止在中断中长时间等待BTF或总线释放。 */
void I2C1_DMA_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TE6)) {
        DMA_Cmd(DMA1_Channel6, DISABLE);
        I2C_DMACmd(I2C1, DISABLE);
        DMA_ClearFlag(DMA1_FLAG_GL6);
        dma_failed = 1;
    } else if (DMA_GetITStatus(DMA1_IT_TC6)) {
        DMA_Cmd(DMA1_Channel6, DISABLE);
        I2C_DMACmd(I2C1, DISABLE);
        DMA_ClearFlag(DMA1_FLAG_GL6);
        if (dma_phase == 1) dma_phase = 2;
    }
}

void I2C1_Service(void)
{
    if (i2c_dma_tx_done) return;
    if (dma_failed) {
        IIC_Abort(IIC_DMA_ERROR);
    } else if (I2C1->SR1 & IIC_ERROR_BITS) {
        IIC_Abort(IIC_BUS_ERROR);
    } else if (GetTick() - dma_started_ms >= IIC_FRAME_MS) {
        IIC_Abort(IIC_TIMEOUT);
    } else if (dma_phase == 2 && (I2C1->SR1 & I2C_SR1_BTF)) {
        I2C_GenerateSTOP(I2C1, ENABLE);
        dma_phase = 3;
    } else if (dma_phase == 3 && !(I2C1->CR1 & I2C_CR1_STOP) &&
               !(I2C1->SR2 & I2C_SR2_BUSY)) {
        dma_phase = 0;
        i2c_dma_tx_done = 1;
        ++g_iic_diag.completed;
        g_iic_diag.last_error = IIC_OK;
    }
}

uint8_t I2C1_IsHealthy(void)
{
    return !bus_fault;
}

static void IIC_RecoveryDelay(void)
{
    uint32_t previous = SysTick->VAL, elapsed = 0;
    while (elapsed < SystemCoreClock / 200000U) {
        uint32_t current = SysTick->VAL;
        elapsed += previous >= current ? previous - current :
                   previous + SysTick->LOAD + 1U - current;
        previous = current;
    }
}

static uint8_t IIC_ReleaseClock(void)
{
    uint32_t start = GetTick();
    GPIOB->BSRR = IIC_SCL;
    while (!(GPIOB->IDR & IIC_SCL)) {
        if (GetTick() - start >= IIC_WAIT_MS) return 0;
    }
    IIC_RecoveryDelay();
    return 1;
}

IIC_Status I2C1_Recover(void)
{
    uint8_t i;
    if (!i2c_dma_tx_done) return IIC_BUSY;
    ++g_iic_diag.recoveries;
    I2C_Cmd(I2C1, DISABLE);
    IIC_ConfigPins(GPIO_Mode_Out_OD);
    /* 单主机总线：最多9个补时钟，再生成STOP；始终开漏，不能强拉高。 */
    if (!IIC_ReleaseClock()) return IIC_Abort(IIC_TIMEOUT);
    for (i = 0; i < 9 && !(GPIOB->IDR & IIC_SDA); ++i) {
        GPIOB->BRR = IIC_SCL;
        IIC_RecoveryDelay();
        if (!IIC_ReleaseClock()) return IIC_Abort(IIC_TIMEOUT);
    }
    GPIOB->BRR = IIC_SCL;
    GPIOB->BRR = IIC_SDA;
    IIC_RecoveryDelay();
    if (!IIC_ReleaseClock()) return IIC_Abort(IIC_TIMEOUT);
    GPIOB->BSRR = IIC_SDA;
    IIC_RecoveryDelay();
    if ((GPIOB->IDR & IIC_LINES) != IIC_LINES) return IIC_Abort(IIC_BUS_ERROR);
    I2C1_Init();
    return IIC_OK;
}

