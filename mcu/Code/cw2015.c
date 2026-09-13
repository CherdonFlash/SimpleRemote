#include "cw2015.h"
#include "delay.h"

/* STM32F103C6 只有 I2C1；PB10/PB11 必须用普通开漏 GPIO 模拟 I2C。 */
#define CW_SCL GPIO_Pin_10
#define CW_SDA GPIO_Pin_11
#define CW_LINES (CW_SCL | CW_SDA)
#define CW_TIMEOUT_MS 2UL
#define CW_MODE 0x0A

volatile CW2015_Data g_cw2015_data = {0};
volatile CW2015_Diagnostics g_cw2015_diag = {0};
static uint8_t cw_ready = 0;

/* 使用已运行的 SysTick 计数器，避免空循环延时受编译优化影响。
 * 本驱动仅从主循环调用，SystemCoreClock 和 SysTick 必须先初始化。 */
static void CW_Delay(void)
{
    uint32_t previous = SysTick->VAL;
    uint32_t elapsed = 0;
    uint32_t target = SystemCoreClock / 200000UL; /* 至少 5 us */
    while (elapsed < target)
    {
        uint32_t current = SysTick->VAL;
        elapsed += previous >= current ? previous - current :
                   previous + (SysTick->LOAD + 1UL) - current;
        previous = current;
    }
}

static CW2015_Status CW_ClockHigh(void)
{
    uint32_t start = GetTick();
    GPIOB->BSRR = CW_SCL; /* 开漏释放，绝不推挽输出高 */
    while ((GPIOB->IDR & CW_SCL) == 0)
    {
        if (GetTick() - start >= CW_TIMEOUT_MS)
            return CW2015_ERR_TIMEOUT;
    }
    CW_Delay();
    return CW2015_OK;
}

static CW2015_Status CW_Stop(void)
{
    CW2015_Status status;
    GPIOB->BRR = CW_SCL;
    GPIOB->BRR = CW_SDA;
    CW_Delay();
    status = CW_ClockHigh();
    GPIOB->BSRR = CW_LINES; /* 即使超时也释放两线 */
    CW_Delay();
    if (status == CW2015_OK && (GPIOB->IDR & CW_LINES) != CW_LINES)
        status = CW2015_ERR_BUS;
    return status;
}

static CW2015_Status CW_Recover(void)
{
    uint8_t i;
    CW2015_Status status;
    GPIOB->BSRR = CW_LINES;
    status = CW_ClockHigh();
    if (status != CW2015_OK) return status;
    /* MCU 中途复位后，从机可能仍等待剩余时钟；最多补 9 个脉冲。 */
    for (i = 0; i < 9 && (GPIOB->IDR & CW_SDA) == 0; ++i)
    {
        GPIOB->BRR = CW_SCL;
        CW_Delay();
        status = CW_ClockHigh();
        if (status != CW2015_OK)
        {
            GPIOB->BSRR = CW_LINES;
            return status;
        }
    }
    return CW_Stop();
}

static CW2015_Status CW_Start(void)
{
    CW2015_Status status;
    GPIOB->BSRR = CW_SDA;
    CW_Delay();
    status = CW_ClockHigh();
    if (status != CW2015_OK) return status;
    if ((GPIOB->IDR & CW_SDA) == 0) return CW2015_ERR_BUS;
    GPIOB->BRR = CW_SDA;
    CW_Delay();
    GPIOB->BRR = CW_SCL;
    return CW2015_OK;
}

static CW2015_Status CW_WriteByte(uint8_t value)
{
    uint8_t i;
    CW2015_Status status;
    for (i = 0; i < 8; ++i)
    {
        if (value & 0x80) GPIOB->BSRR = CW_SDA;
        else GPIOB->BRR = CW_SDA;
        CW_Delay();
        status = CW_ClockHigh();
        if (status != CW2015_OK) return status;
        GPIOB->BRR = CW_SCL;
        value <<= 1;
    }
    GPIOB->BSRR = CW_SDA;
    CW_Delay();
    status = CW_ClockHigh();
    if (status != CW2015_OK) return status;
    status = (GPIOB->IDR & CW_SDA) ? CW2015_ERR_NACK : CW2015_OK;
    GPIOB->BRR = CW_SCL;
    return status;
}

static CW2015_Status CW_ReadByte(uint8_t *value, uint8_t ack)
{
    uint8_t i, data = 0;
    CW2015_Status status;
    GPIOB->BSRR = CW_SDA;
    for (i = 0; i < 8; ++i)
    {
        CW_Delay();
        status = CW_ClockHigh();
        if (status != CW2015_OK) return status;
        data = (uint8_t)((data << 1) | ((GPIOB->IDR & CW_SDA) ? 1U : 0U));
        GPIOB->BRR = CW_SCL;
    }
    if (ack) GPIOB->BRR = CW_SDA;
    else GPIOB->BSRR = CW_SDA;
    CW_Delay();
    status = CW_ClockHigh();
    GPIOB->BRR = CW_SCL;
    GPIOB->BSRR = CW_SDA;
    *value = data;
    return status;
}

static CW2015_Status CW_Transfer(uint8_t reg, uint8_t *data,
                                 uint8_t length, uint8_t read)
{
    uint8_t i;
    CW2015_Status status, stop_status;
    if (data == 0 || length == 0) return CW2015_ERR_DATA;
    g_cw2015_diag.last_register = reg;
    g_cw2015_diag.stage = 1;
    GPIOB->BSRR = CW_LINES;
    CW_Delay();
    status = (GPIOB->IDR & CW_LINES) == CW_LINES ? CW2015_OK : CW_Recover();
    if (status != CW2015_OK) goto finish;
    status = CW_Start();
    if (status != CW2015_OK) goto finish;
    g_cw2015_diag.stage = 2;
    status = CW_WriteByte(CW2015_I2C_ADDRESS);
    if (status != CW2015_OK) goto finish;
    g_cw2015_diag.stage = 3;
    status = CW_WriteByte(reg);
    if (status != CW2015_OK) goto finish;
    if (read)
    {
        g_cw2015_diag.stage = 4;
        status = CW_Start(); /* repeated START */
        if (status != CW2015_OK) goto finish;
        status = CW_WriteByte(CW2015_I2C_ADDRESS | 1U);
        if (status != CW2015_OK) goto finish;
    }
    g_cw2015_diag.stage = 5;
    for (i = 0; i < length; ++i)
    {
        status = read ? CW_ReadByte(&data[i], i + 1U < length) :
                        CW_WriteByte(data[i]);
        if (status != CW2015_OK) break;
    }
finish:
    stop_status = CW_Stop();
    if (status == CW2015_OK) status = stop_status;
    g_cw2015_diag.bus_levels = (GPIOB->IDR >> 10) & 3U;
    if (status == CW2015_OK) g_cw2015_diag.stage = 0;
    return status;
}

static CW2015_Status CW2015_ReadRegister(uint8_t reg, uint8_t *value)
{
    return CW_Transfer(reg, value, 1, 1);
}

CW2015_Status CW2015_Init(void)
{
    GPIO_InitTypeDef gpio;
    CW2015_Status status;
    uint8_t version = 0, mode = 0;
    cw_ready = 0;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIOB->BSRR = CW_LINES;
    gpio.GPIO_Pin = CW_LINES;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(GPIOB, &gpio);
    Delay_ms(10);
    status = CW_Recover();
    if (status == CW2015_OK) status = CW2015_ReadVersion(&version);
    if (status == CW2015_OK)
    {
        g_cw2015_data.version = version;
        status = CW2015_ReadRegister(CW_MODE, &mode);
    }
    if (status == CW2015_OK && (mode & 0xC0U) != 0)
    {
        mode = 0; /* 退出睡眠，不触发 quick start 或 POR */
        status = CW_Transfer(CW_MODE, &mode, 1, 0);
        Delay_ms(10);
    }
    if (status == CW2015_OK) cw_ready = 1;
    g_cw2015_data.status = status;
    return status;
}

CW2015_Status CW2015_ReadVersion(uint8_t *version)
{
    return CW2015_ReadRegister(CW2015_REG_VERSION, version);
}

CW2015_Status CW2015_ReadVoltage(uint16_t *voltage_mv)
{
    CW2015_Status status;
    uint8_t bytes[2];
    uint16_t raw;

    if (voltage_mv == 0)
    {
        return CW2015_ERR_DATA;
    }

    status = CW_Transfer(CW2015_REG_VCELL_H, bytes, 2, 1);
    if (status != CW2015_OK) return status;

    raw = (((uint16_t)bytes[0] << 8) | bytes[1]) & 0x3FFF;
    *voltage_mv = (uint16_t)(((uint32_t)raw * 305UL) / 1000UL);
    return CW2015_OK;
}

CW2015_Status CW2015_ReadSoc(uint8_t *percent, uint8_t *fraction)
{
    uint8_t bytes[2];
    CW2015_Status status;
    if (percent == 0 || fraction == 0) return CW2015_ERR_DATA;
    status = CW_Transfer(CW2015_REG_SOC_H, bytes, 2, 1);
    if (status != CW2015_OK) return status;
    if (bytes[0] > 100U) return CW2015_ERR_DATA;
    *percent = bytes[0];
    *fraction = bytes[1];
    return CW2015_OK;
}

CW2015_Status CW2015_ReadRemainingTime(uint16_t *minutes, uint8_t *alert)
{
    CW2015_Status status;
    uint8_t bytes[2];
    uint16_t raw;

    if ((minutes == 0) || (alert == 0))
    {
        return CW2015_ERR_DATA;
    }

    status = CW_Transfer(CW2015_REG_RRT_ALERT_H, bytes, 2, 1);
    if (status != CW2015_OK) return status;

    raw = ((uint16_t)bytes[0] << 8) | bytes[1];
    *alert = (raw & 0x8000U) ? 1U : 0U;
    *minutes = raw & 0x1FFFU;
    return CW2015_OK;
}

CW2015_Status CW2015_Update(void)
{
    CW2015_Status status;
    uint16_t voltage_mv;
    uint8_t soc_percent;
    uint8_t soc_fraction;
    uint16_t remaining_time_min;
    uint8_t alert;

    ++g_cw2015_diag.attempts;
    status = cw_ready ? CW2015_OK : CW2015_Init();
    if (status == CW2015_OK) status = CW2015_ReadVoltage(&voltage_mv);
    if (status == CW2015_OK)
    {
        status = CW2015_ReadSoc(&soc_percent, &soc_fraction);
    }
    if (status == CW2015_OK)
    {
        status = CW2015_ReadRemainingTime(&remaining_time_min, &alert);
    }

    if (status == CW2015_OK)
    {
        ++g_cw2015_diag.successes;
        g_cw2015_diag.last_success_ms = GetTick();
        g_cw2015_data.voltage_mv = voltage_mv;
        g_cw2015_data.soc_percent = soc_percent;
        g_cw2015_data.soc_fraction = soc_fraction;
        g_cw2015_data.remaining_time_min = remaining_time_min;
        g_cw2015_data.alert = alert;
    }

    if (status != CW2015_OK)
    {
        ++g_cw2015_diag.errors;
        cw_ready = 0;
    }
    g_cw2015_data.status = status;
    return status;
}
