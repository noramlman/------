#ifndef __LCD_144_H
#define __LCD_144_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

/*==============================================================================
 *  lcd_144 —— 1.44 寸 128x128 SPI 彩屏（ST7735 类）驱动
 *  硬件：SPI2 + 若干 GPIO（CS / DC / RST / BLK）
 *  颜色格式：RGB565，发送时需高低字节交换（见 .c 里 (c<<8)|(c>>8)）。
 *============================================================================*/

/* 逻辑分辨率 128x128；控制器显存窗口会再偏移 +2/+3，见 Lcd_SetRegion。 */
#define LCD_W 128u
#define LCD_H 128u

/* 常用 RGB565 颜色。 */
#define LCD_BLACK  0x0000u
#define LCD_WHITE  0xFFFFu
#define LCD_RED    0xF800u
#define LCD_GREEN  0x07E0u
#define LCD_BLUE   0x001Fu
#define LCD_YELLOW 0xFFE0u
#define LCD_GRAY0  0xEF7Du

/* 控制脚：
 *   CS   PB12  片选，低有效
 *   DC   PB1   0=命令，1=数据
 *   RST  PB14  硬复位，低有效
 *   BLK  PB0   背光，高亮 */
#define LCD_CS_LOW()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET)
#define LCD_CS_HIGH() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET)
#define LCD_DC_CMD()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET)
#define LCD_DC_DATA() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET)
#define LCD_RST_LOW() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET)
#define LCD_RST_HIGH() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET)
#define LCD_BLK_ON()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET)
#define LCD_BLK_OFF() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET)

/* 初始化：复位脉冲 + ST7735 寄存器序列 + 开背光。GPIO 已由 Cube 配好。 */
void Lcd_Init(void);

/* 全屏填一种颜色。 */
void Lcd_Clear(uint16_t color);

/* 设置显示窗口 [xs..xe] x [ys..ye]，并自动发 0x2C 写显存命令。
 * 注意内部会做 +2/+3 面板偏移。 */
void Lcd_SetRegion(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye);

/* 画一个像素。 */
void Lcd_DrawPoint(uint16_t x, uint16_t y, uint16_t color);

/* 填矩形，越界会自动裁到屏内。 */
void Lcd_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/* 显示一个 8x16 ASCII 字符（字模在 font_asc16.h）。 */
void Lcd_ShowChar(uint16_t x, uint16_t y, char c, uint16_t fc, uint16_t bc);

/* 显示 ASCII 字符串，超出右边截断。 */
void Lcd_ShowString(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc);

/* 显示 128x128 摄像头帧，1:1 像素，不缩放。按行发 SPI。 */
void Lcd_ShowCameraFrame(const uint16_t *frame);

/* 任意分辨率帧最近邻缩放到全屏预览（如 320x240 → 128x128）。
 * 只适合看构图/调参，不要当最终 UI 清晰度标准。 */
void Lcd_ShowCameraFrameScaled(const uint16_t *frame, uint16_t w, uint16_t h);

#endif
