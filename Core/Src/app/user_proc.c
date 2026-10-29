/**
 * @file    user_proc.c
 * @brief   ★★ 你的算法写在这个文件里 ★★
 *
 * 现在是一个最小示例（二值化 + 画中线），
 * 演示怎么读写 gray[]、怎么用 debug_draw 画图。
 * 把 user_proc() 里的内容换成你自己的逻辑即可。
 *
 * 编译进固件后，屏幕（128x128）上会显示 debug_draw 画出来的调试图。
 */

#include "user_proc.h"
#include "debug_draw.h"

#include "app_config.h"     /* ROI_Y0 / ROI_H / APP_FIXED_THRESHOLD 等 */

#include <stdio.h>

/* 全局结果变量（声明在 user_proc.h） */
UserResult g_user_result = {0, -1, -1, 0, 0};

/**
 * @brief 找某一行里最长的白色区间
 * @param gray  灰度缓冲
 * @param w     宽度
 * @param y     行号
 * @param thr   阈值
 * @param out_l 输出左端点（-1 表示没找到）
 * @param out_r 输出右端点
 * @return 1 找到，0 没找到
 */
static int find_white_run(const uint8_t *gray, int w, int y, int thr,
                          int *out_l, int *out_r)
{
    const uint8_t *row = gray + (unsigned)y * (unsigned)w;
    int x;
    int run_start = 0;
    int in_run = 0;
    int best_len = 0;
    int best_l = 0;
    int best_r = 0;

    for (x = 0; x < w; x++) {
        if (row[x] > thr) {
            if (!in_run) {
                in_run = 1;
                run_start = x;
            }
        } else if (in_run) {
            int len = x - run_start;
            if (len > best_len) {
                best_len = len;
                best_l = run_start;
                best_r = x - 1;
            }
            in_run = 0;
        }
    }
    if (in_run) {                       /* 白到行末 */
        int len = w - run_start;
        if (len > best_len) {
            best_len = len;
            best_l = run_start;
            best_r = w - 1;
        }
    }

    if (best_len < 8) {                 /* 太窄当噪声 */
        *out_l = -1;
        *out_r = -1;
        return 0;
    }
    *out_l = best_l;
    *out_r = best_r;
    return 1;
}

/* =========================================================================
 *  ★★★  你的算法入口  ★★★
 *
 *  现在这个示例做的事：
 *    1. 二值化整幅图（用 APP_FIXED_THRESHOLD）
 *    2. 在 SCAN_ROW_NEAR 那一行找最长白区 → 左右边界
 *    3. 用 debug_draw 画出来：二值图当底，左右边界红/绿，中线蓝
 *
 *  你把它改成自己的算法就行。不想看二值图的话，把 dbg_show_binary 换回
 *  dbg_show_gray 就是原图底。
 * ========================================================================= */
void user_proc(uint8_t *gray, int w, int h)
{
    int thr = APP_FIXED_THRESHOLD;
    int y;
    int l = -1;
    int r = -1;
    int ok;

    if (gray == NULL || w <= 0 || h <= 0) {
        return;
    }

    /* ---- 1. 二值化（就地把 gray[] 改成 0/255）---- */
    {
        int i;
        int n = w * h;
        for (i = 0; i < n; i++) {
            gray[i] = (gray[i] > thr) ? 255u : 0u;
        }
    }

    /* ---- 2. 在近处扫描行找边界 ---- */
    {
        int scan_y = (int)ROI_Y0 + (int)SCAN_ROW_NEAR;
        if (scan_y >= h) {
            scan_y = h - 1;
        }
        ok = find_white_run(gray, w, scan_y, thr, &l, &r);
    }

    /* ---- 3. 保存结果（供 vision_task 打印）---- */
    g_user_result.threshold = thr;
    if (ok) {
        g_user_result.valid = 1;
        g_user_result.left_edge = l;
        g_user_result.right_edge = r;
        g_user_result.mid_offset = ((l + r) / 2) - (w / 2);
    } else {
        g_user_result.valid = 0;
        g_user_result.left_edge = -1;
        g_user_result.right_edge = -1;
        g_user_result.mid_offset = 0;
    }

    /* ---- 4. 画调试图：二值图当底 ---- */
    dbg_show_binary(gray, w, h, thr);

    /* ---- 5. 在 ROI 框上画一条参考线（让你知道扫描行在哪）---- */
    y = (int)ROI_Y0 + (int)SCAN_ROW_NEAR;
    if (y < h) {
        dbg_line(0, y, w - 1, y, DBG_DARKGRAY);
    }

    /* ---- 6. 标出左右边界和中线 ---- */
    if (ok) {
        int i;
        for (i = 0; i < h; i++) {
            /* 只在扫描行附近标，避免满屏都是点 */
            if (i == y) {
                dbg_cross(l, i, 6, DBG_RED);
                dbg_cross(r, i, 6, DBG_GREEN);
                dbg_cross((l + r) / 2, i, 4, DBG_BLUE);
            }
        }
    }
}
