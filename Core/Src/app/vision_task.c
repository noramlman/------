/**
 * @file    vision_task.c
 * @brief   视觉任务骨架 —— 环境已跑通，算法由你自己实现
 *
 * 这里只有"数据怎么取、帧什么时候来、怎么交回去"这套管道，
 * 中间那段图像处理是空的，写你自己的东西。
 *
 * 编译验证状态：arm-none-eabi-gcc 14.3.1 编译 + 链接通过
 *
 * 数据来源：DCMI + DMA 采集，snapshot 模式（采一帧停一帧）
 *   rgb_frame  320x240 的 RGB565 数组，150KB，行优先
 *   像素格式   RRRRR GGGGGG BBBBB
 *              R = (px >> 11) & 0x1F
 *              G = (px >>  5) & 0x3F
 *              B = (px      ) & 0x1F
 */

#include "app_config.h"
#include "vision_task.h"

#include "user_proc.h"      /* ★ 你的算法入口 user_proc() */
#include "debug_draw.h"     /* 调试画图工具 + RGB565→灰度转换 */

#include "main.h"
#include "dcmi_app.h"       /* DCMI_App_Init / DCMI_RGB_Start / rgb_frame / xRgbFrameSem */
#include "ov2640.h"         /* OV2640_Init */
#include "lcd_144.h"        /* Lcd_Init / Lcd_ShowString / Lcd_ShowCameraFrameScaled */
#include "cam_lcd.h"        /* Usb_PrintStr / Usb_Printf */
#include "usb_device.h"     /* MX_USB_DEVICE_Init */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <string.h>
#include <stdio.h>

/* ===================== 参数 ===================== */

#define WAIT_USB_MS     1500U   /* 上电后等电脑枚举虚拟串口 */
#define FRAME_WAIT_MS   1000U   /* 等一帧最多等多久 */
#define FPS_WIN_MS      1000U   /* 每 1 秒统计一次帧率 */

/* ===================== 结果变量 ===================== */

static vision_result_t s_result;

void Vision_GetResult(vision_result_t *out)
{
    if (out != NULL) {
        *out = s_result;
    }
}

/* =========================================================================
 *  ★★★  你要实现的地方只有这一个函数  ★★★
 *
 *  输入： frame    —— 320x240 RGB565，共 IMG_W*IMG_H 个 uint16_t
 *         frame_id —— 帧号，写日志时用
 *  输出： 填 s_result（结构体定义见 vision_task.h）
 *
 *  现在它是空的，只把 valid 置 0。
 *  你实现完之后，屏上和 USB 就能看到真实结果。
 * ========================================================================= */
/* 灰度缓冲区：320x240 = 76800 字节
 * 必须放 static（.bss，在 AXI SRAM），不能放栈 —— 任务栈只有 8KB */
static uint8_t s_gray[IMG_W * IMG_H];

static void Vision_ProcessFrame(const uint16_t *frame, uint32_t frame_id)
{
    s_result.frame_id = frame_id;

    /* ---- 1. RGB565 → 灰度（转换，不是算法）---- */
    dbg_rgb565_to_gray_buf(frame, s_gray, IMG_W, IMG_H);

    /* ---- 2. ★ 你的算法（在 user_proc.c 里）----
     *      内部的 debug_draw 会直接改 rgb_frame，
     *      所以算法跑完，rgb_frame 里就是调试图了 */
    user_proc(s_gray, IMG_W, IMG_H);

    /* ---- 3. 把结果抄给控制层 ---- */
    s_result.valid      = (uint8_t)g_user_result.valid;
    s_result.mid_offset = (int16_t)g_user_result.mid_offset;
    s_result.left_edge  = (int16_t)g_user_result.left_edge;
    s_result.right_edge = (int16_t)g_user_result.right_edge;
    s_result.threshold  = (uint8_t)g_user_result.threshold;
    s_result.white_cnt  = 0;
}

/* =========================================================================
 *  任务入口：初始化外设 + 主循环（这段是管道，不用改）
 * ========================================================================= */
void VisionTask(void *arg)
{
    uint32_t frames = 0u;
    uint32_t fps = 0u;
    TickType_t last_tick;

    (void)arg;

    /* --- 1. USB 虚拟串口（只用来打印，不接也能跑） --- */
    MX_USB_DEVICE_Init();
    vTaskDelay(pdMS_TO_TICKS(WAIT_USB_MS));
    Usb_PrintStr("\r\n===== 校赛整合工程 / VisionTask =====\r\n");

    /* --- 2. 屏幕先点亮，证明 SPI 和屏是好的 --- */
    Lcd_Init();
    Lcd_Clear(LCD_BLUE);
    Lcd_ShowString(4,  8, "VISION",  LCD_YELLOW, LCD_BLUE);
    Lcd_ShowString(4, 28, "INIT...", LCD_WHITE,  LCD_BLUE);
    Usb_PrintStr("LCD OK\r\n");

    /* --- 3. 摄像头初始化（SCCB 配置 + 读 ID） --- */
    DCMI_App_Init();
    if (OV2640_Init() != 0u) {
        Lcd_ShowString(4, 48, "CAM FAIL", LCD_RED, LCD_BLUE);
        Usb_PrintStr("OV2640_Init FAIL\r\n");
        vTaskDelete(NULL);
    }
    Lcd_ShowString(4, 48, "CAM OK", LCD_GREEN, LCD_BLUE);
    Usb_PrintStr("OV2640 OK\r\n");

    /* --- 4. 启动 RGB565 320x240 采集 --- */
    if (DCMI_RGB_Start() != 0u) {
        Usb_PrintStr("DCMI_RGB_Start FAIL\r\n");
        vTaskDelete(NULL);
    }
    Usb_PrintStr("capture started\r\n");

    memset(&s_result, 0, sizeof(s_result));

    last_tick = xTaskGetTickCount();

    for (;;) {
        TickType_t now;

        /* ---- 1. 等一帧收完 ---- */
        if (xSemaphoreTake(xRgbFrameSem, pdMS_TO_TICKS(FRAME_WAIT_MS)) != pdTRUE) {
            Usb_PrintStr("frame timeout\r\n");
            (void)DCMI_RGB_Next();
            continue;
        }

        /* ---- 2. 丢 Cache（DMA 直接写内存，CPU 的 Cache 不知道） ---- */
        SCB_InvalidateDCache_by_Addr((uint32_t *)rgb_frame,
                                     (int32_t)(RGB_FRAME_PIXELS * 2u));

        /* ---- 3. 你的算法 ---- */
        Vision_ProcessFrame(rgb_frame, rgb_frame_cnt);

        /* ---- 4. 显示 ---- */
#if APP_SHOW_CAMERA
        /* 直接把摄像头画面缩放到 128x128 显示：有画面 = 整条链路通 */
        Lcd_ShowCameraFrameScaled(rgb_frame, RGB_W, RGB_H);
#else
        {
            char line[24];
            (void)snprintf(line, sizeof(line), "fps %u", (unsigned)fps);
            Lcd_ShowString(4, 68, line, LCD_YELLOW, LCD_BLUE);
            (void)snprintf(line, sizeof(line), "off %d", (int)s_result.mid_offset);
            Lcd_ShowString(4, 88, line, LCD_WHITE, LCD_BLUE);
            Lcd_ShowString(4, 108, s_result.valid ? "VALID  " : "INVALID", LCD_WHITE, LCD_BLUE);
        }
#endif

        /* ---- 5. 帧率统计 ---- */
        frames++;
        now = xTaskGetTickCount();
        {
            uint32_t dt = (uint32_t)((now - last_tick) * portTICK_PERIOD_MS);
            if (dt >= FPS_WIN_MS) {
                fps = (dt > 0u) ? ((frames * 1000u) / dt) : 0u;
#if APP_USB_LOG
                Usb_Printf("[fps] %u  frame %u  valid %u  off %d\r\n",
                           (unsigned)fps,
                           (unsigned)s_result.frame_id,
                           (unsigned)s_result.valid,
                           (int)s_result.mid_offset);
#endif
                frames = 0u;
                last_tick = now;
            }
        }

        /* ---- 6. 重启下一帧（snapshot 模式：处理完才采下一帧） ---- */
        (void)DCMI_RGB_Next();
    }
}
