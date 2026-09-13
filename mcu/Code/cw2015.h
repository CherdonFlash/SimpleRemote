#ifndef __CW2015_H_
#define __CW2015_H_

#include "stm32f10x.h"

/* PB10/SCL、PB11/SDA 软件 I2C；线上写地址 0xC4，读地址 0xC5。 */
#define CW2015_I2C_ADDRESS             ((uint8_t)0xC4)

#define CW2015_REG_VERSION             ((uint8_t)0x00)
#define CW2015_REG_VCELL_H             ((uint8_t)0x02)
#define CW2015_REG_VCELL_L             ((uint8_t)0x03)
#define CW2015_REG_SOC_H               ((uint8_t)0x04)
#define CW2015_REG_SOC_L               ((uint8_t)0x05)
#define CW2015_REG_RRT_ALERT_H         ((uint8_t)0x06)
#define CW2015_REG_RRT_ALERT_L         ((uint8_t)0x07)

typedef enum
{
    CW2015_OK = 0,
    CW2015_ERR_TIMEOUT,
    CW2015_ERR_BUS,
    CW2015_ERR_DATA,
    CW2015_ERR_NACK
} CW2015_Status;

typedef struct
{
    uint8_t version;
    uint16_t voltage_mv;
    uint8_t soc_percent;
    uint8_t soc_fraction;
    uint16_t remaining_time_min;
    uint8_t alert;
    CW2015_Status status;
} CW2015_Data;

/* 全 32 位字段方便 ST-Link 读取；stage 1总线/2写地址/3寄存器/4读地址/5数据。 */
typedef struct
{
    uint32_t attempts;
    uint32_t successes;
    uint32_t errors;
    uint32_t last_success_ms;
    uint32_t stage;
    uint32_t last_register;
    uint32_t bus_levels; /* bit0=SCL，bit1=SDA，空闲应为3 */
} CW2015_Diagnostics;
extern volatile CW2015_Diagnostics g_cw2015_diag;

/*
 * 诊断与业务共用的最新读数。
 * 使用 ST-Link 时可从链接映像中查到该变量地址后直接读取。
 */
extern volatile CW2015_Data g_cw2015_data;

CW2015_Status CW2015_Init(void);
CW2015_Status CW2015_ReadVersion(uint8_t *version);
CW2015_Status CW2015_ReadVoltage(uint16_t *voltage_mv);
CW2015_Status CW2015_ReadSoc(uint8_t *percent, uint8_t *fraction);
CW2015_Status CW2015_ReadRemainingTime(uint16_t *minutes, uint8_t *alert);
CW2015_Status CW2015_Update(void);

#endif
