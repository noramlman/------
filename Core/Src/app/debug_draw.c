/**
 * @file    debug_draw.c
 * @brief   调试画图工具实现（直接操作 rgb_frame）
 *
 * 全部函数都是"往 rgb_frame 里写颜色"，
 * 写完之后由 vision_task.c 调 Lcd_ShowCameraFrameScaled() 显示。
 *
 * 内存：不额外分配任何缓冲区，只借用驱动已有的 rgb_frame。
 */

#include "debug_draw.h"

#include "dcmi_app.h"   /* rgb_frame, RGB_W, RGB_H, RGB_FRAME_PIXELS */

/* ============================================================
 *  转换
 * ============================================================ */

uint16_t dbg_gray_to_rgb565(uint8_t g)
{
    /* 8 位灰度 → 5/6/5 位，三个通道相等 */
    uint16_t r5 = (uint16_t)((g >> 3) & 0x1Fu);
    uint16_t g6 = (uint16_t)((g >> 2) & 0x3Fu);
    return (uint16_t)((r5 << 11) | (g6 << 5) | r5);
}

uint8_t dbg_rgb565_to_gray(uint16_t px)
{
    /* 5/6/5 位 → 8 位：低位补高位，保证 0x1F → 255 */
    uint32_t r = (uint32_t)((px >> 11) & 0x1Fu);
    uint32_t g = (uint32_t)((px >> 5) & 0x3Fu);
    uint32_t b = (uint32_t)(px & 0x1Fu);

    uint32_t r8 = (r << 3) | (r >> 2);
    uint32_t g8 = (g << 2) | (g >> 4);
    uint32_t b8 = (b << 3) | (b >> 2);

    return (uint8_t)((r8 * 77u + g8 * 151u + b8 * 28u) >> 8);
}

void dbg_rgb565_to_gray_buf(const uint16_t *src, uint8_t *dst, int w, int h)
{
    int32_t i;
    int32_t n;

    if (src == NULL || dst == NULL || w <= 0 || h <= 0) {
        return;
    }
    n = (int32_t)w * (int32_t)h;
    for (i = 0; i < n; i++) {
        dst[i] = dbg_rgb565_to_gray(src[i]);
    }
}

/* ============================================================
 *  画图原语
 * ============================================================ */

void dbg_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= (int)RGB_W || y < 0 || y >= (int)RGB_H) {
        return;
    }
    rgb_frame[(uint32_t)y * RGB_W + (uint32_t)x] = color;
}

void dbg_line(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx, dy, sx, sy, err, e2;

    dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = dx - dy;

    for (;;) {
        dbg_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = err << 1;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void dbg_cross(int x, int y, int size, uint16_t color)
{
    int d;
    for (d = -size; d <= size; d++) {
        dbg_pixel(x + d, y, color);
        dbg_pixel(x, y + d, color);
    }
}

void dbg_rect(int x, int y, int w, int h, uint16_t color)
{
    dbg_line(x, y, x + w - 1, y, color);
    dbg_line(x, y + h - 1, x + w - 1, y + h - 1, color);
    dbg_line(x, y, x, y + h - 1, color);
    dbg_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void dbg_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    int i, j;
    for (j = y; j < y + h; j++) {
        for (i = x; i < x + w; i++) {
            dbg_pixel(i, j, color);
        }
    }
}

/* ============================================================
 *  整图操作
 * ============================================================ */

void dbg_clear(uint16_t color)
{
    uint32_t i;
    for (i = 0; i < RGB_FRAME_PIXELS; i++) {
        rgb_frame[i] = color;
    }
}

void dbg_show_gray(const uint8_t *gray, int w, int h)
{
    int x, y;

    if (gray == NULL) {
        return;
    }
    /* 尺寸必须和 rgb_frame 一致，否则只画重叠部分 */
    if (w > (int)RGB_W) {
        w = (int)RGB_W;
    }
    if (h > (int)RGB_H) {
        h = (int)RGB_H;
    }

    for (y = 0; y < h; y++) {
        const uint8_t *src = gray + (uint32_t)y * (uint32_t)w;
        uint16_t *dst = rgb_frame + (uint32_t)y * RGB_W;
        for (x = 0; x < w; x++) {
            dst[x] = dbg_gray_to_rgb565(src[x]);
        }
    }
}

void dbg_show_binary(const uint8_t *gray, int w, int h, uint8_t threshold)
{
    int x, y;

    if (gray == NULL) {
        return;
    }
    if (w > (int)RGB_W) {
        w = (int)RGB_W;
    }
    if (h > (int)RGB_H) {
        h = (int)RGB_H;
    }

    for (y = 0; y < h; y++) {
        const uint8_t *src = gray + (uint32_t)y * (uint32_t)w;
        uint16_t *dst = rgb_frame + (uint32_t)y * RGB_W;
        for (x = 0; x < w; x++) {
            dst[x] = (src[x] > threshold) ? DBG_WHITE : DBG_BLACK;
        }
    }
}
