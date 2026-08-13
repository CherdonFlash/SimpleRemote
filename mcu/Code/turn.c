#include "turn.h"

//单刀双掷开关 对应PA8 输入模式
void turn_init(){
    GPIO_InitTypeDef GPIO_InitStructure;

    // 1. 使能 GPIOA 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // 2. 配置 PA8 为输入浮空
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;   
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; 
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

int turn_get(void){
    return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8);
}



