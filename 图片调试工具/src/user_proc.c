/**
 * @file    user_proc.c
 * @brief   ★★ 你的算法写在这个文件里 ★★
 *
 * 目前是空实现（原样输出），你把自己的逻辑填进 img_proc()。
 * 这里只放几个"画图工具函数"，方便你在图上标记结果。
 */

#include "user_proc.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 *  画图工具函数（可以直接用，也可以不要）
 * ============================================================ */

/* 画一个像素（自动裁剪越界） */
void draw_pixel(GrayImage *img, int x, int y, uint8_t color)
{
    if (img == NULL || img->data == NULL) {
        return;
    }
    if (x < 0 || x >= img->width || y < 0 || y >= img->height) {
        return;
    }
    img->rows[y][x] = color;
}

/* 画十字（十字中心在 (x,y)，臂长为 size） */
void draw_cross(GrayImage *img, int x, int y, int size, uint8_t color)
{
    int d;
    if (img == NULL || img->data == NULL) {
        return;
    }
    for (d = -size; d <= size; d++) {
        draw_pixel(img, x + d, y, color);
        draw_pixel(img, x, y + d, color);
    }
}

/* Bresenham 画线 */
void draw_line(GrayImage *img, int x0, int y0, int x1, int y1, uint8_t color)
{
    int dx, dy, sx, sy, err, e2;

    if (img == NULL || img->data == NULL) {
        return;
    }
    dx = abs(x1 - x0);
    dy = abs(y1 - y0);
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = dx - dy;

    for (;;) {
        draw_pixel(img, x0, y0, color);
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

/* 画一个矩形框（线宽 1） */
void draw_rect(GrayImage *img, int x, int y, int w, int h, uint8_t color)
{
    draw_line(img, x, y, x + w - 1, y, color);
    draw_line(img, x, y + h - 1, x + w - 1, y + h - 1, color);
    draw_line(img, x, y, x, y + h - 1, color);
    draw_line(img, x + w - 1, y, x + w - 1, y + h - 1, color);
}

/* ============================================================
 *  ★★★  你的算法入口  ★★★
 *
 *  现在是空的：什么都不做，原图直接输出。
 *  填上你的逻辑后，程序会把改过的图存成 out.bmp。
 * ============================================================ */
void img_proc(GrayImage *img)
{
    int w, h, x, y;

    if (img == NULL || img->data == NULL) {
        return;
    }
    w = img->width;
    h = img->height;

    /* ------------------------------------------------------------------
     * TODO: 在这里写你的算法
     *
     * 下面是一个「最小示例」，演示怎么读写像素，跑通后删掉换成你自己的：
     *   把画面上半部分涂黑，下半部分涂白，中间画一条横线
     * ------------------------------------------------------------------ */

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            if (y < h / 2) {
                img->rows[y][x] = 0;      /* 上半：黑 */
            } else {
                img->rows[y][x] = 255;    /* 下半：白 */
            }
        }
    }
    draw_line(img, 0, h / 2, w - 1, h / 2, 100);   /* 中间灰横线，证明工具可用 */
    draw_cross(img, w / 2, h / 2, 6, 0);           /* 中心点十字    */
    draw_rect(img, 10, 10, 40, 30, 0);             /* 左上角框，试试 draw_rect */

    printf("[img_proc] 图像 %dx%d 处理完毕\n", w, h);
}
