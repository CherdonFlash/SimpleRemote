#include "mobile.h"

//使用中断+DMA的方式
#define ADC_CHANNELS 5
volatile uint16_t adc_values[ADC_CHANNELS];//原始值
int8_t mobile_1;
int8_t mobile_2;
int8_t mobile_3;
int8_t mobile_4;//油门0-255

uint16_t BAT_ADC;//电源ADC原始值
//将摇杆的原始数据映射到0-255 也就是1字节 方便传输
void mobile_data(){
    mobile_1 = (int8_t)(((4095 -adc_values[0]) * 255) / 4095 - 128);
    mobile_2 = (int8_t)(((4095 -adc_values[1]) * 255) / 4095 - 128);
    mobile_3 = (int8_t)((adc_values[2] * 255) / 4095 - 128);
    mobile_4 = (int8_t)((adc_values[3] * 255) / 4095 - 128);//油门是0-255
    
    //电源ADC原始值
    BAT_ADC = (uint16_t)adc_values[4];
}

// 初始化 PA2、PA3、PA4、PB0、 作为 摇杆ADC 通道 注意PB1 作为电源ADC
void mobile_init(){
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;
    DMA_InitTypeDef DMA_InitStructure;

    // 1. 使能时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | 
                           RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_ADC1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // 2. 配置 GPIO (PA2, PA3, PA4 为模拟输入)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PB0、PB1 为模拟输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 3. 配置 DMA1_Channel1 (ADC1 -> 内存)
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)adc_values;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = ADC_CHANNELS; // 5通道
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    DMA_Cmd(DMA1_Channel1, ENABLE);

    // 4. 配置 ADC
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;         // 多通道
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = ADC_CHANNELS;   // 5通道
    ADC_Init(ADC1, &ADC_InitStructure);

    // 设置转换顺序
    // PA2 = ADC2
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 1, ADC_SampleTime_239Cycles5);
    // PA3 = ADC3
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 2, ADC_SampleTime_239Cycles5);
    // PA4 = ADC4
    ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 3, ADC_SampleTime_239Cycles5);
    // PB0 = ADC8
    ADC_RegularChannelConfig(ADC1, ADC_Channel_8, 4, ADC_SampleTime_239Cycles5);
    // PB1 = ADC9
    ADC_RegularChannelConfig(ADC1, ADC_Channel_9, 5, ADC_SampleTime_239Cycles5);

    // 使能 ADC DMA
    ADC_DMACmd(ADC1, ENABLE);

    // 5. 启动 ADC
    ADC_Cmd(ADC1, ENABLE);

    // 校准
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));

    // 开始转换
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}






