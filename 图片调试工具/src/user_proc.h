/**
 * @file    user_proc.h
 * @brief   ★ 你的算法入口 —— 只改这个文件
 */

#ifndef USER_PROC_H
#define USER_PROC_H

#include "bmp_io.h"

/* ============================================================
 *  img_proc()：你的算法写在这里
 *
 *  输入：img 已经加载好的灰度图，可以直接读写
 *          img->width / img->height   图像尺寸
 *          img->rows[y][x]            第 y 行第 x 列的灰度值（0~255）
 *
 *  输出：直接改 img->rows[y][x] 就行，main.c 会自动存成 out.bmp
 *
 *  灰度值建议：
 *      255  纯白
 *      0    纯黑
 *      100  中灰（适合当"标记色"）
 *      150  浅灰
 * ============================================================ */
void img_proc(GrayImage *img);

#endif /* USER_PROC_H */
