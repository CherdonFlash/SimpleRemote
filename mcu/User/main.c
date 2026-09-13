#include "stm32f10x.h"
#include "delay.h"
#include "key.h"
#include "uart.h"
#include "turn.h"
#include "mobile.h"
#include "nrf24.h"
#include "bat.h"
#include "cw2015.h"
#include "iic.h"
#include "app_display.h"

/* 普通遥控器原开源项目：
 * https://github.com/mcforyous/MB_Control
 * http://oshwhub.com/foryous/ordinary-remote-control-mbcontro
 */

static uint32_t battery_tick;
static void App_Init(void);
static void Battery_Task(void);
static void Watchdog_Init(uint16_t timeout_ms);

int main(void)
{
    App_Init();

    while (1) {
        I2C1_Service();       /* OLED异步DMA收尾、总线错误与超时检查 */
        Battery_Task();       /* 首次立即读取，以后每秒读取一次CW2015 */
        mobile_data();        /* 每轮独立更新摇杆；屏幕故障不冻结控制量 */
        NRF_SendAll();        /* 保留原32字节协议、重发配置和串口日志 */
        App_DisplayTask();    /* 仅显示最新状态；缺屏时低频尝试恢复 */
        IWDG_ReloadCounter();
    }
}

static void App_Init(void)
{
    while (SystemClock_HSE_72MHz() == 0);
    SysTick_Init();
    Watchdog_Init(1000U);     /* 固定1秒看门狗 */

    USART1_Init(115200);
    Key_Init_All();
    turn_init();
    mobile_init();           /* V1.1只采集四路摇杆，不再采集PB1电池电压 */
    CW2015_Init();            /* F103C6无I2C2，使用PB10/PB11软件开漏I2C */
    App_DisplayInit();        /* 初始化失败允许继续运行，主循环负责重连 */
    nrf24_init();

    /* 无符号时间差支持GetTick回绕，首次进入主循环立即采集。 */
    battery_tick = GetTick() - 1000U;
    UART1_Printf("Init success!\n");
}

static void Battery_Task(void)
{
    uint32_t now = GetTick();
    if (now - battery_tick < 1000U) return; /* 固定每秒读取一次 */
    battery_tick = now;

    if (CW2015_Update() == CW2015_OK) {
        V_Bat = (float)g_cw2015_data.voltage_mv / 1000.0f;
        BAT_Percent = g_cw2015_data.soc_percent;
    }
    /* 失败保留最后有效业务值，屏幕显示--%，驱动下次自动重试。 */
}

static void Watchdog_Init(uint16_t timeout_ms)
{
    /* LSI按约40kHz估算，32分频后每计数0.8ms；避免浮点计算。 */
    uint32_t reload = (uint32_t)timeout_ms * 5U / 4U;
    if (reload > 4095U) reload = 4095U;
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_32);
    IWDG_SetReload((uint16_t)reload);
    IWDG_ReloadCounter();
    IWDG_Enable();
}
