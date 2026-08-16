#include "key.h"
//------开源作者EzWalk  qq:1228879785-----------
//------本文件日期2025/9/29 版本1.0-------------
//你有几个按键就全局几个结构体对象 并且修改函数Key_Init_All ,Key_GPIO_Init ,Key_Mange,Key_Read , Key_GetState里面的内容调整到自己的情况
//最后把Key_Mange函数 放入中断函数中  外部只需要调用一次Key_Init_All 进行初始化 然后就可以通过Key_GetState(x)进行获得按键的逻辑状态 
//根据需求可以选择在回调函数里面放入你自己要执行的回调函数,单击和双击只会执行一次,长按触发后会一直执行 执行间隔和次数根据长按回调调用间隔决定
//注意双击的时候 前一个逻辑状态一定是单击的逻辑状态  因此如果你想做一些比较精细的逻辑 请使用回调函数
Key_t IO1;
Key_t IO2;
Key_t IO3;
Key_t IO4;
//按键功能总初始化函数
void Key_Init_All(){
    //初始化按键引脚
    Key_GPIO_Init();
    //每个按键结构体对应的ID, 消抖时间:1x10ms(定时器周期),长按判定时间50x10ms,双击最大间隔40x10ms,长按回调调用间隔4x10ms,单击,双击,长按的回调函数.
    Key_Init(&IO1, 1, 1, 50, 40, 4,NULL, NULL, NULL);
    Key_Init(&IO2, 2, 1, 50, 40, 4,NULL, NULL, NULL);
    Key_Init(&IO3, 3, 1, 50, 40, 4,NULL, NULL, NULL);
    Key_Init(&IO4, 4, 1, 50, 40, 4,NULL, NULL, NULL);
    
    TIM2_Init_10ms();//初始化按键对应的定时器中断并开始

}
//总按键处理  请把这个函数放入中断函数
void Key_Mange(){
	Key_Status(&IO1);
	Key_Status(&IO2);
	Key_Status(&IO3);
	Key_Status(&IO4);

}
//按键初始化  已外部上拉
void Key_GPIO_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  // 改为浮空输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;


    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 禁用调试口，释放 PA15 PB3 PB4 做普通IO（如有必要）
    //GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
}
//定时器中断 10ms一次
void TIM2_Init_10ms(void)
{
    // 1. 开启定时器时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // 2. 定时器时基结构体配置
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    /*
     * 假设 APB1 时钟 = 36MHz
     * 为产生 10ms 中断，需要定时器溢出周期为 10ms = 10000us
     * 可设置：
     *    预分频器 Prescaler = 7199  （即 36MHz / (7199 + 1) = 5kHz）
     *    自动重装载 ARR = 49         （即 50个计数 = 10ms）
     *    10ms = (ARR+1) * (PSC+1) / 36MHz
     */

    TIM_TimeBaseStructure.TIM_Prescaler = 7200-1;             // 分频系数
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // 向上计数
    TIM_TimeBaseStructure.TIM_Period = 50-1;                  // 自动重装载值
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; // 时钟分频
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;        // 重复计数器（仅高级定时器）
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    // 3. 清除中断标志位
    TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE); // 开启更新中断

    // 4. 配置 NVIC
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1; // 抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;        // 响应优先级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 5. 启动定时器
    TIM_Cmd(TIM2, ENABLE);
}
// 按键物理状态：0 表示按下，1 表示松开
uint8_t Key_Read(uint8_t key_id)
{
	if(key_id ==1 )     return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12);
	else if(key_id ==2 )return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13);
	else if(key_id ==3 )return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14);
	else if(key_id ==4 )return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15);


	//不会执行到这  消除编译警告
	return 1;
}

// 获取按键逻辑状态(单击 双击 长按)
uint8_t Key_GetState(uint8_t id)
{
    switch (id) {
        case 1: return IO1.state;
        case 2: return IO2.state;
        case 3: return IO3.state;
        case 4: return IO4.state;
        default: return 0;  // 不存在的按键
    }
}
//按键结构体对象初始化函数
void Key_Init(Key_t *key, uint8_t id,
              uint8_t debounce, uint8_t long_press, uint8_t double_wait, uint8_t long_interval,
              void (*single_cb)(void), void (*double_cb)(void), void (*long_cb)(void))
{
    // 配置参数
    key->debounce_ticks = debounce;
    key->long_press_ticks = long_press;
    key->double_wait_ticks = double_wait;
    key->long_tick_interval = long_interval;

    // 状态变量初始化
    key->flag = 0;
    key->state = 0;
    key->count = 0;
    key->press = 0;
    key->long_tick = 0;

    // 编号
    key->id = id;

    // 回调函数
    key->single_callback = single_cb;
    key->double_callback = double_cb;
    key->long_callback = long_cb;
}

//按键处理函数（每10ms调用一次）
void Key_Status(Key_t *key)
{
    uint8_t key_val = Key_Read(key->id);  // 0=按下, 1=松开

    switch (key->flag)
    {
    case 0: // 空闲
        if (key_val == 0) {
            key->flag = 1;
            key->count = 0;
        }
        key->state = 0;// 保持空闲
        break;

    case 1: // 第一次按下消抖
        if (key_val == 0) {
        	key->count++;
            if (key->count >= key->debounce_ticks) {
                key->flag = 2;
                key->count = 0;
            }
        } else {
            key->flag = 0;
        }
        key->state = 0;
        break;

    case 2: // 第一次完全按下
        key->count++;
        if (key_val == 1) {
            if (key->count < key->long_press_ticks) {
                key->flag = 3;
                key->count = 0;
                key->state = 1;
                if (key->single_callback) key->single_callback();
            } else {
                key->flag = 8;
                key->state = 0;
            }
        } else if (key->count >= key->long_press_ticks) {
            key->flag = 9;
            key->state = 3;
        } else {
        	key->state = 0;
        }
        break;

    case 3: // 第一次松开消抖
        if (key_val == 1) {
        	key->count++;
            if (key->count >= key->debounce_ticks) {
                key->flag = 4;
                key->count = 0;
                key->state = 1;
            }
        } else {
            key->flag = 2;
            key->count = 0;
        }
        break;

    case 4: // 等待第二次按下
    	key->count++;
        if (key_val == 0) {
            if (key->count < key->double_wait_ticks) {
            	key->flag = 5; // 第二次按下消抖
                key->count = 0;
            } else {
            	key->flag = 0;
            	key->state = 0; // 单击已确认，超时
            }
        } else if (key->count >= key->double_wait_ticks) {
        	key->flag = 0;

        	key->state = 0; // 单击成立后超时
        } else {
        	key->state = 0;
        }
        break;

    case 5: // 第二次按下消抖
        if (key_val == 0) {
        	key->count++;
            if (key->count >= key->debounce_ticks) {
                key->flag = 6;
                key->count = 0;
            }
        } else {
            key->flag = 4;
            key->count = 0;
        }
        key->state = 0;
        break;

    case 6: // 第二次完全按下
        key->count++;
        if (key_val == 1) {
            key->flag = 7;
            key->count = 0;
            key->state = 0;
        } else if (key->count >= key->long_press_ticks) {
            key->flag = 9;
            key->state = 3;
            key->count = 0;
        }else {
        	key->state = 0;
		}
        break;

    case 7: // 第二次松开消抖
        if (key_val == 1) {
        	key->count++;
            if (key->count >= key->debounce_ticks) {
                key->flag = 8;
                key->state = 2;
                if (key->double_callback) key->double_callback();
            }
        } else {
            key->flag = 6;
            key->count = 0;
        }
        break;

    case 8: // 最终空闲
        key->flag = 0;
        key->count = 0;
        key->state = 0;
        break;

    case 9: // 长按持续中
        if (key_val == 1) {
            key->flag = 0;
            key->count = 0;
            key->state = 0;
            key->long_tick = 0;
        } else {
            key->state = 3;
            key->long_tick++;
            if (key->long_tick >= key->long_tick_interval) {
                key->long_tick = 0;
                if (key->long_callback) key->long_callback();
            }
        }
        break;

    default:
    	key->flag = 0;
    	key->count = 0;
    	key->state = 0;
        break;
    }



    // 更新 press 状态
    if (key->flag==1 || key->flag==2 || key->flag==5 || key->flag==6 || key->state==3 || key->state==2){
        key->press = 1;
    } else {
        key->press = 0;
    }
}

