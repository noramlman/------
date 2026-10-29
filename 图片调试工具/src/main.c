/**
 * @file    main.c
 * @brief   图片调试工具：读 BMP → 你的算法 → 存 BMP
 *
 * 用法（控制台）：
 *     bmp_tool images/0.bmp              → 生成 out.bmp（图在这张图旁边）
 *     bmp_tool images/0.bmp out.bmp      → 指定输出名
 *     bmp_tool                          → 用默认 images/0.bmp
 *
 * 结果用系统看图软件打开 out.bmp 即可，改一次算法重跑只要零点几秒。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bmp_io.h"
#include "user_proc.h"

#define DEFAULT_IN  "images/0.bmp"
#define DEFAULT_OUT "out.bmp"

int main(int argc, char *argv[])
{
    const char *in_path;
    const char *out_path;
    GrayImage img;

    in_path  = (argc > 1) ? argv[1] : DEFAULT_IN;
    out_path = (argc > 2) ? argv[2] : DEFAULT_OUT;

    printf("输入: %s\n", in_path);

    if (!bmp_load(in_path, &img)) {
        fprintf(stderr, "加载失败: %s\n", in_path);
        return 1;
    }
    printf("       %d x %d, %d 字节/像素\n", img.width, img.height, 1);

    /* ★ 你的算法 */
    img_proc(&img);

    if (!bmp_save(out_path, &img)) {
        fprintf(stderr, "保存失败: %s\n", out_path);
        bmp_free(&img);
        return 1;
    }
    printf("输出: %s\n", out_path);

    bmp_free(&img);
    return 0;
}
