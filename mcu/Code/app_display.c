#include "app_display.h"
#include "oled096.h"
#include "iic.h"
#include "key.h"
#include "turn.h"
#include "mobile.h"
#include "bat.h"
#include "cw2015.h"

#define DISPLAY_RETRY_MS 500U
static uint8_t display_ready;
static uint32_t retry_tick;
static void Display_Draw(void);

void App_DisplayInit(void)
{
    I2C1_Init();
    display_ready = (OLED_Init() == IIC_OK);
    retry_tick = GetTick();
}

void App_DisplayTask(void)
{
    if (!I2C1_IsHealthy()) display_ready = 0;
    if (!display_ready) {
        /* 缺屏时每500ms尝试一次，绝不在当前调用内反复重试。 */
        if (GetTick() - retry_tick < DISPLAY_RETRY_MS) return;
        retry_tick = GetTick();
        if (I2C1_Recover() != IIC_OK) return;
        if (OLED_Init() != IIC_OK) return;
        display_ready = 1;
    }
    if (!i2c_dma_tx_done) return;
    Display_Draw();
    if (OLED_Refresh() != IIC_OK) {
        display_ready = 0;
        retry_tick = GetTick();
    }
}

/* 保留原屏幕布局和绘图算法，本次只分离采样与显示职责。 */
static void Display_DrawBattery(void)
{
    char text[4];
    if (g_cw2015_diag.successes != 0 && g_cw2015_data.status == CW2015_OK)
        sprintf(text, "%3u", (unsigned int)BAT_Percent);
    else
        sprintf(text, " --");
    OLED_ShowString(42, 5, text);
    OLED_ShowString(68, 5, "%");
}

static void Display_DrawSticks(void)
{
    float nx = (float)mobile_1 / 127.0f;
    float ny = (float)mobile_2 / 127.0f;
    float radius = sqrtf(nx * nx + ny * ny);
    int length;
    int angle;
    if (radius > 1.0f) radius = 1.0f;
    length = (int)(radius * 20.0f);
    angle = (int)(atan2f(-ny, nx) * 180.0f / 3.1415f);

    OLED_DrawCircle(21, 25, 20, 0);
    OLED_DrawRay(21, 25, angle, length);

    /* 右摇杆水平、垂直填充；坐标和映射沿用原界面。 */
    OLED_DrawRectangle(60, 20, 80, 30, 0);
    OLED_DrawRectangle(80 + (mobile_3 < 0 ? mobile_3 : 0) * 20 / 128, 20, 80, 30, 1);
    OLED_DrawRectangle(90, 20, 110, 30, 0);
    OLED_DrawRectangle(90, 20, 90 + (mobile_3 > 0 ? mobile_3 : 0) * 20 / 128, 30, 1);
    OLED_DrawRectangle(80, 5, 90, 25, 0);
    OLED_DrawRectangle(80, 25 - (mobile_4 > 0 ? mobile_4 : 0) * 20 / 128, 90, 25, 1);
    OLED_DrawRectangle(80, 25, 90, 45, 0);
    OLED_DrawRectangle(80, 25, 90, 25 - (mobile_4 < 0 ? mobile_4 : 0) * 20 / 128, 1);
}

static void Display_Draw(void)
{
    OLED_ShowString(102, 5, turn_get() == 0 ? "ON " : "OFF ");
    Display_DrawBattery();
    Display_DrawSticks();
    OLED_DrawRectangle(0, 53, 16, 63, Key_IsPressed(1));
    OLED_DrawRectangle(36, 53, 53, 63, Key_IsPressed(2));
    OLED_DrawRectangle(73, 53, 90, 63, Key_IsPressed(3));
    OLED_DrawRectangle(110, 53, 126, 63, Key_IsPressed(4));
}
