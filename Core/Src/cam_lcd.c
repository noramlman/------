/*********************************************************************************************************************
* 文件名称          cam_lcd
* 功能说明          摄像头 RGB565 图像实时显示到 1.44 寸屏（实现）
* 适用平台          STM32H743VITx
*
* 修改记录
* 日期              作者                备注
* 2026-9-26         -                  first version
*********************************************************************************************************************/

#include "cam_lcd.h"

#include "main.h"
#include "ov2640.h"                     // OV2640_Init
#include "dcmi_app.h"                   // DCMI 采集接口
#include "lcd_144.h"                    // 屏幕驱动
#include "usb_vcp.h"                    // 虚拟串口发送
#include "usb_device.h"                 // MX_USB_DEVICE_Init

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// *************************** 参数 ***************************
#define WAIT_USB_MS     1500U           // 上电后等电脑枚举
#define LOG_WAIT_MS      200U           // 单次打印最多等多久
#define FRAME_WAIT_MS   1000U           // 等一帧最多等多久
#define FPS_WIN_MS      1000U           // 每 1 秒统计一次帧率

/*------------------------------------------------------
 * 函数简介：往虚拟串口发一段字符串
 * 参数说明：s：要发送的字符串，必须以 0 结尾
 * 返回参数：void
 * 备注信息：USB 还没枚举好就直接丢弃，免得白等
 *------------------------------------------------------*/
void Usb_PrintStr(const char *s)
{
    if (s == NULL)
    {
        return;
    }
    if (!UsbVcp_IsConnected())
    {
        return;
    }
    (void)UsbVcp_Write((const uint8_t *)s, strlen(s), pdMS_TO_TICKS(LOG_WAIT_MS));
}

/*------------------------------------------------------
 * 函数简介：格式化打印，用法同 printf
 * 参数说明：fmt：格式串；...：可变参数
 * 返回参数：void
 * 备注信息：内部缓冲 128 字节，超出截断
 *------------------------------------------------------*/
void Usb_Printf(const char *fmt, ...)
{
    va_list ap;
    char tmp[128];

    if (fmt == NULL)
    {
        return;
    }

    va_start(ap, fmt);
    (void)vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    tmp[sizeof(tmp) - 1U] = '\0';
    Usb_PrintStr(tmp);
}

/*------------------------------------------------------
 * 函数简介：摄像头 → 屏幕 显示任务
 * 参数说明：arg：任务参数，本任务不用
 * 返回参数：void（无限循环，不会返回）
 * 备注信息：每轮做四件事：
 *           1. 等 DCMI 帧中断给出的信号量（一帧 RGB565 收完）
 *           2. 丢掉过期的 Cache（DCMI 是 DMA 直接写内存的，CPU 不知道）
 *           3. Lcd_ShowCameraFrameScaled：320x240 最近邻缩放到 128x128 显示
 *           4. 重启下一帧，并每秒打印一次帧率
 *------------------------------------------------------*/
void CamLcdTask(void *arg)
{
    uint32_t frames = 0U;
    uint32_t fps = 0U;
    TickType_t last_tick;

    (void)arg;

    MX_USB_DEVICE_Init();                       // 启动 USB，枚举成虚拟串口
    vTaskDelay(pdMS_TO_TICKS(WAIT_USB_MS));     // 等电脑枚举完并打开串口

    Usb_PrintStr("\r\n===== H743 Camera -> LCD =====\r\n");

    // 先把屏点起来，画个横幅。这样摄像头还没初始化也能看出"屏是好的"
    Lcd_Init();
    Lcd_Clear(LCD_BLUE);
    Lcd_ShowString(4,  8, "H743 CAMERA", LCD_YELLOW, LCD_BLUE);
    Lcd_ShowString(4, 28, "RGB565",      LCD_WHITE,  LCD_BLUE);
    Lcd_ShowString(4, 48, "320x240",     LCD_WHITE,  LCD_BLUE);
    Lcd_ShowString(4, 68, "-> 128x128",  LCD_WHITE,  LCD_BLUE);
    Usb_PrintStr("LCD OK\r\n");

    // 摄像头：SCCB 初始化 + 读 ID + 写寄存器表
    DCMI_App_Init();
    if (OV2640_Init() != 0U)
    {
        Lcd_ShowString(4, 108, "OV2640 FAIL", LCD_RED, LCD_BLUE);
        Usb_PrintStr("OV2640_Init FAIL\r\n");
        vTaskDelete(NULL);
    }
    Lcd_ShowString(4, 108, "OV2640 OK", LCD_GREEN, LCD_BLUE);
    Usb_PrintStr("OV2640 OK\r\n");

    // 切到 RGB565 320x240 并启动第一帧
    if (DCMI_RGB_Start() != 0U)
    {
        Usb_PrintStr("DCMI_RGB_Start FAIL\r\n");
        vTaskDelete(NULL);
    }
    Usb_PrintStr("capture started (RGB565 320x240)\r\n");

    last_tick = xTaskGetTickCount();

    for (;;)
    {
        // ---- 1. 等一帧收完 ----
        if (xSemaphoreTake(xRgbFrameSem, pdMS_TO_TICKS(FRAME_WAIT_MS)) != pdTRUE)
        {
            Usb_PrintStr("frame timeout\r\n");
            (void)DCMI_RGB_Next();
            continue;
        }

        // ---- 2. 丢掉过期 Cache ----
        // DCMI 是通过 DMA 直接往内存写像素的，CPU 的 Cache 不知道这件事，
        // 不 invalidate 的话屏幕上可能出现上一帧的残影或者碎块
        SCB_InvalidateDCache_by_Addr((uint32_t *)rgb_frame,
                                     (int32_t)(RGB_FRAME_PIXELS * 2U));

        // ---- 3. 显示（内部最近邻缩放到 128x128） ----
        Lcd_ShowCameraFrameScaled(rgb_frame, RGB_W, RGB_H);

        // ---- 4. 帧率统计 + 重启下一帧 ----
        frames++;
        {
            TickType_t now = xTaskGetTickCount();
            uint32_t dt = (uint32_t)((now - last_tick) * portTICK_PERIOD_MS);

            if (dt >= FPS_WIN_MS)
            {
                fps = (dt > 0U) ? ((frames * 1000U) / dt) : 0U;
                Usb_Printf("[fps] %u   frames %u\r\n",
                           (unsigned)fps, (unsigned)rgb_frame_cnt);
                frames = 0U;
                last_tick = now;
            }
        }

        (void)DCMI_RGB_Next();
    }
}
