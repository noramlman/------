/**
 * @file    vision_task.h
 * @brief   视觉应用层对外接口
 *
 * 分层说明（这是本工程的核心约定）：
 *   驱动层  dcmi_app.c / ov2640.c / lcd_144.c  —— 只调用，绝不修改
 *   应用层  本文件 + vision_task.c              —— 你自己的代码写在这里
 *   系统层  main.c / stm32h7xx_it.c / hal_msp.c —— 整合外设时才动
 */

#ifndef VISION_TASK_H
#define VISION_TASK_H

#include <stdint.h>

/* 一帧的视觉处理结果 */
typedef struct {
    uint32_t frame_id;      /* 帧号 */
    uint8_t  valid;         /* 1 = 找到赛道，0 = 丢线 */
    int16_t  mid_offset;    /* 近处中线相对画面中心的偏差（像素，右为正） */
    int16_t  left_edge;     /* 近处左边界列号，-1 表示没找到 */
    int16_t  right_edge;    /* 近处右边界列号，-1 表示没找到 */
    uint8_t  threshold;     /* 本次用的二值化阈值，方便调试 */
    uint16_t white_cnt;     /* ROI 里判为白的像素数，判断阈值是否合适 */
} vision_result_t;

/* 视觉任务入口：给 osThreadNew 用。无限循环，不返回。 */
void VisionTask(void *arg);

/* 取最近一次处理结果（给控制层用，现在是占位） */
void Vision_GetResult(vision_result_t *out);

#endif /* VISION_TASK_H */
