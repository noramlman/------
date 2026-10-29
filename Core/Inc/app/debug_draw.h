/**
 * @file    debug_draw.h
 * @brief   调试画图工具（RGB565 版本，板子专用）
 *
 * 用法：直接改 rgb_frame，改完 Lcd_ShowCameraFrameScaled() 就会显示出来。
 *
 *   dbg_show_binary(gray, IMG_W, IMG_H, thr);    // 二值图当底图
 *   for (i = 0; i < h; i++) {
 *       dbg_pixel(line_l[i], i, DBG_RED);        // 左边界红
 *       dbg_pixel(line_r[i], i, DBG_GREEN);      // 右边界绿
 *   }
 *
 *   上面这些函数内部都是写 rgb_frame[y*IMG_W + x]
 */

#ifndef DEBUG_DRAW_H
#define DEBUG_DRAW_H

#include <stdint.h>

/* ============================================================
 *  RGB565 常用颜色
 *     位布局： RRRRR GGGGGG BBBBB
 *     换算：   r5 << 11 | g6 << 5 | b5
 * ============================================================ */
#define DBG_BLACK    0x0000u
#define DBG_WHITE    0xFFFFu
#define DBG_RED      0xF800u   /* 左边界 */
#define DBG_GREEN    0x07E0u   /* 右边界 */
#define DBG_BLUE     0x001Fu   /* 中线  */
#define DBG_YELLOW   0xFFE0u
#define DBG_CYAN     0x07FFu
#define DBG_MAGENTA  0xF81Fu
#define DBG_GRAY     0x8410u   /* 中灰 */
#define DBG_DARKGRAY 0x4208u

/* ============================================================
 *  灰度 / RGB565 转换
 * ============================================================ */

/* 灰度(0~255) → RGB565 白色系数（灰度图用） */
uint16_t dbg_gray_to_rgb565(uint8_t g);

/* RGB565 → 灰度(0~255)，按 77/151/28 加权 */
uint8_t dbg_rgb565_to_gray(uint16_t px);

/**
 * @brief 把整帧 RGB565 转成灰度数组
 * @param src  RGB565 源（rgb_frame）
 * @param dst  灰度目标，长度 = w*h
 * @param w,h  图像尺寸
 *
 * 注意：dst 必须够大（320x240 = 76800 字节），要放 static/全局，不能放栈
 */
void dbg_rgb565_to_gray_buf(const uint16_t *src, uint8_t *dst, int w, int h);

/* ============================================================
 *  画图原语（直接写 rgb_frame）
 * ============================================================ */

/* 画一个点，自动裁剪越界 */
void dbg_pixel(int x, int y, uint16_t color);

/* 画一条线（Bresenham），自动裁剪 */
void dbg_line(int x0, int y0, int x1, int y1, uint16_t color);

/* 画十字（中心 x,y，臂长 size） */
void dbg_cross(int x, int y, int size, uint16_t color);

/* 画矩形框（线宽 1） */
void dbg_rect(int x, int y, int w, int h, uint16_t color);

/* 填矩形 */
void dbg_fill_rect(int x, int y, int w, int h, uint16_t color);

/* ============================================================
 *  整图操作
 * ============================================================ */

/**
 * @brief 把灰度数组按原尺寸铺到 rgb_frame 上（当成底图显示）
 * @note  尺寸必须是 320x240，直接 1:1 写入
 */
void dbg_show_gray(const uint8_t *gray, int w, int h);

/**
 * @brief 把灰度数组二值化后铺到 rgb_frame（阈值 > T 判为白）
 */
void dbg_show_binary(const uint8_t *gray, int w, int h, uint8_t threshold);

/**
 * @brief 把整个 rgb_frame 填成一种颜色
 */
void dbg_clear(uint16_t color);

#endif /* DEBUG_DRAW_H */
