#ifndef __DCMI_APP_H
#define __DCMI_APP_H

#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdint.h>

/*==============================================================================
 *  dcmi_app —— DCMI 摄像头采集应用层
 *  职责：
 *    1. 复用 CubeMX 生成的 hdcmi / hdma_dcmi，不在本文件里再 Init 外设；
 *    2. 管理 JPEG / RGB565 两种采集模式（snapshot 单帧，DMA 停死后任务再读）；
 *    3. 帧中断里只做“停 DMA + 给信号量”，图像处理全部放到任务里。
 *============================================================================*/

/* JPEG 采集缓冲：128KB（按 32 位字计 32768）。
 * 320x240 / 640x480 的 JPEG 一般只有几十 KB，128KB 留有余量。 */
#define JPEG_BUF_WORDS      32768u

/* RGB 采集（给视觉 / AI 用）：320x240 全视场 RGB565。
 * LCD 预览时再缩放到 128x128，不在采集路径里缩。 */
#define RGB_W               320u
#define RGB_H               240u
#define RGB_FRAME_PIXELS    ((RGB_W) * (RGB_H))
/* NDTR 是 16 位：Word 对齐一次传 2 个 RGB565 像素，38400 < 65535，装得下。 */
#define RGB_FRAME_WORDS     ((RGB_FRAME_PIXELS) / 2u)

/* DMA / CPU 共用缓冲，32 字节对齐，便于按 Cache 行做 Clean/Invalidate。 */
extern uint32_t jpeg_buf[JPEG_BUF_WORDS];
extern uint16_t rgb_frame[RGB_FRAME_PIXELS];

/* 帧完成信号量：DCMI 帧中断里 Give，工作任务 Take。
 * 二值信号量，防止事件丢失导致任务永久阻塞。 */
extern SemaphoreHandle_t xJpegFrameSem;
extern SemaphoreHandle_t xRgbFrameSem;

/* 最近一帧 JPEG 的有效字节数，在帧中断里由 NDTR 反算并冻结。 */
extern volatile uint32_t jpeg_valid_bytes;

/* RGB 帧计数。snapshot 模式不会覆盖正在处理的缓冲，丢帧计数基本用不到。 */
extern volatile uint32_t rgb_frame_cnt;
extern volatile uint32_t rgb_drop_cnt;

/* 一次性初始化：建信号量，关掉 HAL_DCMI_Init 默认打开的多余中断
 * （LINE / VSYNC / ERR / OVR），只保留 FRAME。 */
void DCMI_App_Init(void);

/* 完整启动某模式：停当前模式 → 配 OV2640 输出格式 → 重配 DMA → 开一帧 snapshot。
 * 参数 out_w/out_h 为 JPEG 模式下的输出分辨率；RGB 模式固定 320x240。
 * 返回：0 成功；1 DMA 初始化失败；2 DMA 启动失败。 */
uint8_t DCMI_JPEG_Start(uint16_t out_w, uint16_t out_h);
uint8_t DCMI_RGB_Start(void);

/* 下一帧轻量重启：只重挂 DMA + 再开一次 snapshot，不写 OV2640 寄存器，
 * 返回快。每处理完一帧后调用，保证 DMA 不会覆盖任务还在读的缓冲。
 * 返回：0 成功；1 当前模式不匹配；2 DMA 启动失败。 */
uint8_t DCMI_JPEG_Next(void);
uint8_t DCMI_RGB_Next(void);

/* 停止采集：清 CAPTURE 位并 Abort DMA。 */
void DCMI_App_Stop(void);

#endif
