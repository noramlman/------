/**
 * @file    bmp_io.h
 * @brief   BMP 读写 + 灰度图容器（纯 C，无第三方库，无 Windows API）
 *
 * 设计要点：
 *   1. 只依赖标准 C 库（stdio / stdlib / string），可以原封不动搬到单片机
 *   2. 读：支持 8 / 24 / 32 位未压缩 BMP，自动转成 8 位灰度、top-down
 *   3. 写：输出 24 位灰度 BMP，用系统看图软件打开
 *
 * 典型用法（你的 main.c 里）：
 *     GrayImage img;
 *     if (!bmp_load("images/0.bmp", &img)) { ... }
 *     img_proc(&img);                  // ← 这里改 img 的像素
 *     bmp_save("out.bmp", &img);
 *     bmp_free(&img);
 */

#ifndef BMP_IO_H
#define BMP_IO_H

#include <stdint.h>

/* 灰度图容器
 *   data[]  连续内存，长度 = height * width，按行优先
 *   rows[]  行指针数组，rows[0] 是第 0 行（图像顶行）
 *           用法： img.rows[y][x] 就是第 y 行第 x 列的灰度值
 *
 *   两种访问方式等价：
 *       img.rows[y][x]
 *       img.data[y * img.width + x]
 */
typedef struct {
    int       width;
    int       height;
    int       stride;       /* 每行有效字节数（这里恒等于 width） */
    uint8_t  *data;         /* 像素内存，由 bmp_load 分配 */
    uint8_t **rows;         /* 行指针，由 bmp_load 分配 */
} GrayImage;

/**
 * @brief 读 BMP 文件并转成灰度
 * @param path BMP 路径
 * @param out  输出（内部会 malloc，用完必须 bmp_free）
 * @return 1 成功，0 失败
 *
 * 支持：8 位（调色板）、24 位、32 位，未压缩（BI_RGB）
 * 已处理：bottom-up 翻转、行字节 4 字节对齐、24/32 位转灰度
 */
int bmp_load(const char *path, GrayImage *out);

/**
 * @brief 把灰度图保存成 24 位 BMP
 * @return 1 成功，0 失败
 */
int bmp_save(const char *path, const GrayImage *img);

/**
 * @brief 释放 bmp_load 分配的内存，并把结构体清零
 */
void bmp_free(GrayImage *img);

#endif /* BMP_IO_H */
