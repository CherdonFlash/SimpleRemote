#ifndef __KEY_H_
#define __KEY_H_

#include "stm32f10x.h"
#include <stdio.h>
// === 按键结构体 ===
typedef struct {
    // === 配置参数 ===
    uint8_t debounce_ticks;      // 消抖时间
    uint8_t long_press_ticks;    // 长按判定时间
    uint8_t double_wait_ticks;   // 双击最大间隔
    uint8_t long_tick_interval;  // 长按回调调用间隔

    // === 状态变量 ===
    uint8_t flag;       // 状态机标志
    uint8_t state;      // 逻辑状态: 0空闲 1单击 2双击 3长按
    uint8_t count;      // 通用计数器
    uint8_t press;      // 是否按下(0松开,1按下)
    uint8_t long_tick;  // 长按节流计数

    // === 按键编号 ===
    uint8_t id;         // 1,2,3... 代表不同的按键

    // === 回调函数 ===
    void (*single_callback)(void);
    void (*double_callback)(void);
    void (*long_callback)(void);

} Key_t;
void Key_Init_All(void);
void Key_Mange(void);
void Key_Status(Key_t *key);
void Key_GPIO_Init(void);
uint8_t Key_GetState(uint8_t id);
void Key_Init(Key_t *key, uint8_t id,
              uint8_t debounce, uint8_t long_press, uint8_t double_wait, uint8_t long_interval,
              void (*single_cb)(void), void (*double_cb)(void), void (*long_cb)(void));
uint8_t Key_Read(uint8_t key_num);
void TIM2_Init_10ms(void);
#endif
