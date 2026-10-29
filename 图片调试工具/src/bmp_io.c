/**
 * @file    bmp_io.c
 * @brief   BMP 读写实现（纯 C99，只用标准库）
 */

#include "bmp_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* BMP 文件头（14 字节），必须按 1 字节对齐 */
#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;        /* 'BM' = 0x4D42 */
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;     /* 像素数据起始偏移 */
} BmpFileHeader;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;      /* 正数 = bottom-up，负数 = top-down */
    uint16_t biPlanes;
    uint16_t biBitCount;    /* 8 / 24 / 32 */
    uint32_t biCompression; /* 0 = BI_RGB */
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BmpInfoHeader;

typedef struct {
    uint8_t rgbBlue;
    uint8_t rgbGreen;
    uint8_t rgbRed;
    uint8_t rgbReserved;
} BmpRgbQuad;
#pragma pack(pop)

#define BMP_BI_RGB 0u

/* 每行字节数向上取整到 4 的倍数 */
static int stride_of(int width, int bytes_per_pixel)
{
    return ((width * bytes_per_pixel + 3) / 4) * 4;
}

void bmp_free(GrayImage *img)
{
    if (img == NULL) {
        return;
    }
    free(img->rows);
    free(img->data);
    memset(img, 0, sizeof(*img));
}

int bmp_load(const char *path, GrayImage *out)
{
    FILE *fp;
    BmpFileHeader fh;
    BmpInfoHeader ih;
    int src_bpp, src_stride, y, x;
    size_t img_size;
    uint8_t *raw;
    BmpRgbQuad palette[256];
    uint32_t colors = 0;

    if (path == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "[bmp] 打不开文件: %s\n", path);
        return 0;
    }

    if (fread(&fh, sizeof(fh), 1, fp) != 1u) {
        fclose(fp);
        return 0;
    }
    if (fh.bfType != 0x4D42u) {
        fprintf(stderr, "[bmp] 不是 BMP 文件\n");
        fclose(fp);
        return 0;
    }
    if (fread(&ih, sizeof(ih), 1, fp) != 1u) {
        fclose(fp);
        return 0;
    }
    if (ih.biCompression != BMP_BI_RGB) {
        fprintf(stderr, "[bmp] 只支持未压缩 BI_RGB\n");
        fclose(fp);
        return 0;
    }
    if (!(ih.biBitCount == 8u || ih.biBitCount == 24u || ih.biBitCount == 32u)) {
        fprintf(stderr, "[bmp] 不支持 %u 位\n", (unsigned)ih.biBitCount);
        fclose(fp);
        return 0;
    }
    if (ih.biWidth <= 0 || ih.biHeight == 0) {
        fprintf(stderr, "[bmp] 尺寸非法\n");
        fclose(fp);
        return 0;
    }

    out->width  = (int)ih.biWidth;
    out->height = (ih.biHeight > 0) ? (int)ih.biHeight : (int)(-ih.biHeight);
    out->stride = out->width;

    /* 8 位需要调色板 */
    if (ih.biBitCount == 8u) {
        colors = ih.biClrUsed ? ih.biClrUsed : 256u;
        if (colors > 256u) {
            colors = 256u;
        }
        memset(palette, 0, sizeof(palette));
        if (fread(palette, sizeof(BmpRgbQuad), colors, fp) != colors) {
            /* 读失败就用默认灰度调色板 */
            for (x = 0; x < 256; x++) {
                palette[x].rgbRed   = (uint8_t)x;
                palette[x].rgbGreen = (uint8_t)x;
                palette[x].rgbBlue  = (uint8_t)x;
            }
            colors = 256;
        }
    }

    /* 分配输出（先清空，避免部分读失败留下脏数据） */
    img_size = (size_t)out->width * (size_t)out->height;
    out->data = (uint8_t *)malloc(img_size);
    out->rows = (uint8_t **)malloc(sizeof(uint8_t *) * (size_t)out->height);
    if (out->data == NULL || out->rows == NULL) {
        bmp_free(out);
        fclose(fp);
        return 0;
    }
    for (y = 0; y < out->height; y++) {
        out->rows[y] = out->data + (size_t)y * (size_t)out->width;
    }

    /* 读原始像素 */
    src_bpp    = (int)(ih.biBitCount / 8u);
    src_stride = stride_of(out->width, src_bpp);
    raw = (uint8_t *)malloc((size_t)src_stride * (size_t)out->height);
    if (raw == NULL) {
        bmp_free(out);
        fclose(fp);
        return 0;
    }
    if (fseek(fp, (long)fh.bfOffBits, SEEK_SET) != 0 ||
        fread(raw, 1, (size_t)src_stride * (size_t)out->height, fp)
            != (size_t)src_stride * (size_t)out->height) {
        fprintf(stderr, "[bmp] 像素读取失败\n");
        free(raw);
        bmp_free(out);
        fclose(fp);
        return 0;
    }
    fclose(fp);

    /* 转灰度 + 处理 bottom-up / top-down */
    for (y = 0; y < out->height; y++) {
        /* biHeight > 0 表示 bottom-up：文件第一行是图像最后一行 */
        int src_y = (ih.biHeight > 0) ? (out->height - 1 - y) : y;
        const uint8_t *s = raw + (size_t)src_y * (size_t)src_stride;
        uint8_t *d = out->rows[y];

        if (ih.biBitCount == 8u) {
            for (x = 0; x < out->width; x++) {
                uint8_t idx = s[x];
                const BmpRgbQuad *q = &palette[idx < colors ? idx : 0u];
                d[x] = (uint8_t)(((uint32_t)q->rgbRed * 77u +
                                  (uint32_t)q->rgbGreen * 150u +
                                  (uint32_t)q->rgbBlue * 29u) >> 8);
            }
        } else { /* 24 / 32 位，字节序是 B G R [A] */
            for (x = 0; x < out->width; x++) {
                const uint8_t *p = s + (size_t)x * (size_t)src_bpp;
                uint32_t B = p[0];
                uint32_t G = p[1];
                uint32_t R = p[2];
                d[x] = (uint8_t)((R * 77u + G * 150u + B * 29u) >> 8);
            }
        }
    }

    free(raw);
    return 1;
}

int bmp_save(const char *path, const GrayImage *img)
{
    FILE *fp;
    BmpFileHeader fh;
    BmpInfoHeader ih;
    BmpRgbQuad palette[256];
    int dst_stride, y, x;
    size_t pixel_bytes;
    uint8_t *line;

    if (path == NULL || img == NULL || img->data == NULL) {
        return 0;
    }

    dst_stride = stride_of(img->width, 3);
    pixel_bytes = (size_t)dst_stride * (size_t)img->height;

    memset(&fh, 0, sizeof(fh));
    memset(&ih, 0, sizeof(ih));

    fh.bfType = 0x4D42u;
    fh.bfOffBits = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader) + sizeof(palette);
    fh.bfSize = fh.bfOffBits + (uint32_t)pixel_bytes;

    ih.biSize = sizeof(BmpInfoHeader);
    ih.biWidth = img->width;
    ih.biHeight = img->height;     /* 正数 = bottom-up，Windows 习惯 */
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = BMP_BI_RGB;
    ih.biSizeImage = (uint32_t)pixel_bytes;
    ih.biClrUsed = 256;            /* 文件头里写 256 级灰度调色板（看图软件不用，但便于某些工具） */

    fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "[bmp] 无法写入: %s\n", path);
        return 0;
    }

    if (fwrite(&fh, sizeof(fh), 1, fp) != 1u ||
        fwrite(&ih, sizeof(ih), 1, fp) != 1u) {
        fclose(fp);
        return 0;
    }

    /* 写一个灰度调色板（24 位图其实不用，但很多工具能识别得更友好） */
    memset(palette, 0, sizeof(palette));
    for (x = 0; x < 256; x++) {
        palette[x].rgbRed   = (uint8_t)x;
        palette[x].rgbGreen = (uint8_t)x;
        palette[x].rgbBlue  = (uint8_t)x;
    }
    if (fwrite(palette, sizeof(BmpRgbQuad), 256, fp) != 256u) {
        fclose(fp);
        return 0;
    }

    line = (uint8_t *)calloc((size_t)dst_stride, 1);
    if (line == NULL) {
        fclose(fp);
        return 0;
    }

    /* bottom-up：先写图像最后一行 */
    for (y = img->height - 1; y >= 0; y--) {
        const uint8_t *src = img->rows[y];
        for (x = 0; x < img->width; x++) {
            uint8_t g = src[x];
            line[x * 3 + 0] = g;   /* B */
            line[x * 3 + 1] = g;   /* G */
            line[x * 3 + 2] = g;   /* R */
        }
        if (fwrite(line, 1, (size_t)dst_stride, fp) != (size_t)dst_stride) {
            free(line);
            fclose(fp);
            return 0;
        }
    }

    free(line);
    fclose(fp);
    return 1;
}
