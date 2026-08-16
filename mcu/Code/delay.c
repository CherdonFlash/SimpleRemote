#include "delay.h"

// 用户定义主频（单位 MHz）
#define F_CPU 72  // 72 MHz

// 计算主频（单位 Hz）
#define CPU_CLOCK_HZ  (F_CPU * 1000000UL)

// 时钟初始化函数  外部晶振8M
uint8_t SystemClock_HSE_72MHz(void)
{
    ErrorStatus HSEStartUpStatus;

    RCC_DeInit(); // 复位 RCC 所有寄存器
    RCC_HSEConfig(RCC_HSE_ON); // 开启 HSE
    HSEStartUpStatus = RCC_WaitForHSEStartUp(); // 等待 HSE 稳定

    if (HSEStartUpStatus == SUCCESS)
    {
        // 设置时钟分频
        RCC_HCLKConfig(RCC_SYSCLK_Div1);   // AHB = SYSCLK
        RCC_PCLK2Config(RCC_HCLK_Div1);    // APB2 = HCLK
        RCC_PCLK1Config(RCC_HCLK_Div2);    // APB1 = HCLK/2（不能超36MHz）

        // 设置PLL：PLLCLK = HSE × 9 = 72MHz
        RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
        RCC_PLLCmd(ENABLE);

        // 等待 PLL 就绪
        while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

        // 选择 PLL 作为系统时钟
        RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

        // 等待切换完成
        while (RCC_GetSYSCLKSource() != 0x08);
    }
    else
    {
        // 启动失败，可进行错误处理
        return 0;
    }

    // 更新 SystemCoreClock 全局变量
    SystemCoreClockUpdate();
    return 1;
}

volatile uint32_t systick_ms = 0;
void SysTick_Init(void)
{
    // SystemCoreClock = 72MHz（由 SystemClock_HSE_72MHz 决定）
    // 每毫秒进入一次中断 = 72MHz / 1000 = 72000
    if(SysTick_Config(SystemCoreClock / 1000) != 0){  // 每1ms中断一次
			while(1);
	}
	NVIC_SetPriority(SysTick_IRQn, 0);  // 设置最高优先级
	__enable_irq(); // 确保中断使能
}


uint32_t GetTick(void)
{
    return systick_ms;
}
void Delay_ms(uint32_t ms)
{
    uint32_t start = systick_ms;
    while ((systick_ms - start) < ms);
}
