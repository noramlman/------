/**
 * @file    user_proc.h
 * @brief   ★ 你的算法接口 —— 板子上唯一需要你改的函数
 *
 * 和电脑端 图片调试工具 里的 img_proc() 用法完全一致，
 * 只是这里给的是"灰度缓冲区指针"，不是 GrayImage 结构体。
 */

#ifndef USER_PROC_H
#define USER_PROC_H

#include <stdint.h>

/* ============================================================
 *  算法输出结果
 *
 *  你的 user_proc() 跑完后把结果写进这几个变量，
 *  vision_task.c 会把它们抄给控制层。
 * ============================================================ */
typedef struct {
    int valid;          /* 1 = 找到赛道，0 = 丢线 */
    int left_edge;      /* 左边界列号，-1 表示没找到 */
    int right_edge;     /* 右边界列号，-1 表示没找到 */
    int mid_offset;     /* 中线相对画面中心的偏差（像素，右为正） */
    int threshold;      /* 本次用的阈值，方便调试 */
} UserResult;

/* 全局结果变量，你在 user_proc() 里直接赋值 */
extern UserResult g_user_result;

/**
 * @brief 你的图像处理函数（板子上版）
 *
 * @param gray  灰度图缓冲区，长度 = w*h，按行优先
 *              gray[y*w + x] = 第 y 行第 x 列的灰度值（0~255）
 *              这个缓冲区你可以随便改，改完由 debug_draw 显示
 * @param w     宽度（现在固定 320）
 * @param h     高度（现在固定 240）
 *
 * 和电脑端的对应关系：
 *    电脑端  img->rows[y][x]   ←→   板子上  gray[y*w + x]
 *    电脑端  img->width        ←→   板子上  w
 *    电脑端  img->height       ←→   板子上  h
 */
void user_proc(uint8_t *gray, int w, int h);

#endif /* USER_PROC_H */
