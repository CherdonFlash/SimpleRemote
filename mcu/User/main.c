#include "stm32f10x.h"                  // Device header
#include "iic.h"
#include "oled096.h"
#include "oledfont.h"
#include "delay.h"
#include "key.h"
#include "uart.h"
#include "turn.h"
#include "mobile.h"
#include "nrf24.h"
#include "bat.h"
//**********普通遥控器:作者qq:1228879785********已开源至立创和Github******************************
//***********立创开源项目地址:http://oshwhub.com/foryous/ordinary-remote-control-mbcontro*********
//***********Github开源地址:https://github.com/mcforyous/MB_Control*******************************
void oled_data(void);//屏幕刷新函数
void IWDG_Init(uint16_t ms);//看门狗初始化函数


extern volatile uint16_t adc_values[5];//ADC采样数组（原始值）
extern float V_Bat;//电源电压
extern uint8_t BAT_Percent;
extern uint16_t BAT_ADC;//电源ADC原始值
int main(void){
    while(SystemClock_HSE_72MHz()==0);    //初始化HSE外部晶振8M
	SysTick_Init();             // 初始化1ms节拍计时 
    IWDG_Init(1000);            //初始化看门狗1s超时
    USART1_Init(115200);	    //串口1初始化
	Key_Init_All();		        //按键总初始化  包括引脚初始化 按键功能 和定时器配置
    turn_init();                //单刀双掷开关
    mobile_init();              //摇杆ADC采样初始化
    I2C1_Init();			    //初始化硬件IIC1 必须在OLED前
	OLED_Init();			    //初始化OLED 使用硬件IIC1
    nrf24_init();               //初始化引脚和硬件SPI
    UART1_Printf("Init success!\n");//该函数与printf用法相同
	while(1)
	{   
        oled_data();    //屏幕刷新
        BAT_Get();      //电源电压解析
        NRF_SendAll();  //发送数据包
        
        IWDG_ReloadCounter(); // 喂狗
	}
}
//oled显示按键 摇杆 nrf24等信息的屏幕函数
extern int8_t mobile_1;
extern int8_t mobile_2;
extern int8_t mobile_3;
extern int8_t mobile_4;//油门0-255
extern uint8_t i2c_dma_tx_done;
void oled_data(){
    if(i2c_dma_tx_done!=1) return;//上次传输未完成则跳过本次
    
    //显示右上角单刀双掷  设置为往上掰为ON 往下为OFF
    if(turn_get()==0) OLED_ShowString(102,5,"ON ");
    else              OLED_ShowString(102,5,"OFF ");
    
    //显示电量
    OLED_ShowInt(50,5,(int)BAT_Percent);
    OLED_ShowString(68,5,"%");

    OLED_DrawCircle(21,25,20,0);
    mobile_data();//获取摇杆映射后的数据
    //左摇杆
    float nx = (float)mobile_1 / 127.0f;
    float ny = (float)mobile_2 / 127.0f;
    float r = sqrtf(nx*nx + ny*ny);
    if (r > 1.0f) r = 1.0f;
    int length = r * 20.0f;
    int angle  = atan2f(-ny, nx) * 180.0f / 3.1415;
    OLED_DrawRay(21,25,angle,length);//左摇杆射线
    
    //右摇杆
    OLED_DrawRectangle(60,20,80,30,0);//左半
    OLED_DrawRectangle(80+(mobile_3<0 ? mobile_3 : 0)*20/128,20,80,30,1);//左半
    
    OLED_DrawRectangle(90,20,110,30,0);//右半
    OLED_DrawRectangle(90,20,90+(mobile_3>0 ? mobile_3 : 0)*20/128,30,1);
    
// 上半部分 (对应向上推油门，假设此时值为正)
    OLED_DrawRectangle(80, 5, 90, 25, 0); //上半外框
    // 填充上半：因为向上推 Y 坐标应该减小，所以用 25 减去计算出的偏移量
    OLED_DrawRectangle(80, 25 - (mobile_4 > 0 ? mobile_4 : 0) * 20 / 128, 90, 25, 1);
    
    // 下半部分 (对应向下推油门，假设此时值为负)
    OLED_DrawRectangle(80, 25, 90, 45, 0); //下半外框
    // 填充下半：mobile_4 为负数，25 减去一个负数相当于“加上”一个正数，Y 坐标变大，往下延伸
    OLED_DrawRectangle(80, 25, 90, 25 - (mobile_4 < 0 ? mobile_4 : 0) * 20 / 128, 1);
    
    uint8_t io1=(Key_GetState(1) !=0);
    uint8_t io2=(Key_GetState(2) !=0);
    uint8_t io3=(Key_GetState(3) !=0);
    uint8_t io4=(Key_GetState(4) !=0);
    OLED_DrawRectangle(  0,53,16, 63,io1);//按键1 形参:矩形左上角的列数0-127 , 矩形左上角的行数0-63,后面两个是右下角的左边 列和行,最后一个是1填充 0 不填充
    OLED_DrawRectangle( 36,53,53, 63,io2);//按键2
    OLED_DrawRectangle( 73,53,90, 63,io3);//按键3
    OLED_DrawRectangle(110,53,126,63,io4);//按键4
 
    OLED_Refresh();//缓冲区写入到屏幕
}

//看门狗
void IWDG_Init(uint16_t ms)
{
    // LSI 约 40kHz，看门狗时钟 = LSI / 32 = 40k / 32 = 1250 Hz
    // 每个计数周期 = 1 / 1250 = 0.8ms
    // 计数最大值 4095（12bit）

    // 计算重装载值
    uint32_t reload = ms / 0.8;   // ms 转换为计数值（约等于）

    if (reload > 4095) reload = 4095;

    // 允许写操作
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

    // 设置预分频 = 32
    IWDG_SetPrescaler(IWDG_Prescaler_32);

    // 设置重载值
    IWDG_SetReload(reload);

    // Reload 一次
    IWDG_ReloadCounter();

    // 启动 IWDG
    IWDG_Enable();
}
