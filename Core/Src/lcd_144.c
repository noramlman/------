#include "lcd_144.h"
#include "app_delay.h"
#include "font_asc16.h"
#include "main.h"

/*==============================================================================
 *  lcd_144.c —— 1.44 寸 SPI 屏（ST7735）驱动实现
 *  底层走硬件 SPI2；像素发送前做 RGB565 字节序交换。
 *  Init 序列与常见 TFT144 例程一致，面板偏移 +2(X)/+3(Y)。
 *============================================================================*/

/* CubeMX 生成的 SPI2 句柄（main.c）。 */
extern SPI_HandleTypeDef hspi2;

/* 发一个命令字节：DC=0，CS 拉低送完再拉高。 */
static void Lcd_WriteCmd(uint8_t cmd)
{
    LCD_DC_CMD();
    LCD_CS_LOW();
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
    LCD_CS_HIGH();
}

/* 发一个数据字节：DC=1。 */
static void Lcd_WriteByte(uint8_t dat)
{
    LCD_DC_DATA();
    LCD_CS_LOW();
    HAL_SPI_Transmit(&hspi2, &dat, 1, 100);
    LCD_CS_HIGH();
}

/* 初始化。
 * PB0/PB1/PB12/PB14 已在 Cube 的 MX_GPIO_Init 里配成推挽输出，
 * 这里只做硬复位脉冲 + 写 ST7735 初始化表 + 开背光。 */
void Lcd_Init(void)
{
    LCD_CS_HIGH();
    LCD_BLK_OFF();

    /* 硬复位：低 100ms → 高后再等 120ms 让电源稳定。 */
    LCD_RST_LOW();
    App_DelayMs(100);
    LCD_RST_HIGH();
    App_DelayMs(120);

    Lcd_WriteCmd(0x11); /* Sleep out */
    App_DelayMs(120);

    /* Frame rate：全帧 / 空闲 / 部分模式三套 B1/B2/B3 参数。 */
    Lcd_WriteCmd(0xB1);
    Lcd_WriteByte(0x01);
    Lcd_WriteByte(0x2C);
    Lcd_WriteByte(0x2D);

    Lcd_WriteCmd(0xB2);
    Lcd_WriteByte(0x01);
    Lcd_WriteByte(0x2C);
    Lcd_WriteByte(0x2D);

    Lcd_WriteCmd(0xB3);
    Lcd_WriteByte(0x01);
    Lcd_WriteByte(0x2C);
    Lcd_WriteByte(0x2D);
    Lcd_WriteByte(0x01);
    Lcd_WriteByte(0x2C);
    Lcd_WriteByte(0x2D);

    Lcd_WriteCmd(0xB4); /* Dot inversion */
    Lcd_WriteByte(0x07);

    /* 电源控制 AVDD / VRH / VCOM 等。 */
    Lcd_WriteCmd(0xC0);
    Lcd_WriteByte(0xA2);
    Lcd_WriteByte(0x02);
    Lcd_WriteByte(0x84);
    Lcd_WriteCmd(0xC1);
    Lcd_WriteByte(0xC5);

    Lcd_WriteCmd(0xC2);
    Lcd_WriteByte(0x0A);
    Lcd_WriteByte(0x00);

    Lcd_WriteCmd(0xC3);
    Lcd_WriteByte(0x8A);
    Lcd_WriteByte(0x2A);
    Lcd_WriteCmd(0xC4);
    Lcd_WriteByte(0x8A);
    Lcd_WriteByte(0xEE);

    Lcd_WriteCmd(0xC5); /* VCOM */
    Lcd_WriteByte(0x0E);

    Lcd_WriteCmd(0x36); /* MADCTL：0xC8 常见横竖屏配置 */
    Lcd_WriteByte(0xC8);

    /* 正/负极性 Gamma 校正表。 */
    Lcd_WriteCmd(0xE0);
    Lcd_WriteByte(0x0F);
    Lcd_WriteByte(0x1A);
    Lcd_WriteByte(0x0F);
    Lcd_WriteByte(0x18);
    Lcd_WriteByte(0x2F);
    Lcd_WriteByte(0x28);
    Lcd_WriteByte(0x20);
    Lcd_WriteByte(0x22);
    Lcd_WriteByte(0x1F);
    Lcd_WriteByte(0x1B);
    Lcd_WriteByte(0x23);
    Lcd_WriteByte(0x37);
    Lcd_WriteByte(0x00);
    Lcd_WriteByte(0x07);
    Lcd_WriteByte(0x02);
    Lcd_WriteByte(0x10);

    Lcd_WriteCmd(0xE1);
    Lcd_WriteByte(0x0F);
    Lcd_WriteByte(0x1B);
    Lcd_WriteByte(0x0F);
    Lcd_WriteByte(0x17);
    Lcd_WriteByte(0x33);
    Lcd_WriteByte(0x2C);
    Lcd_WriteByte(0x29);
    Lcd_WriteByte(0x2E);
    Lcd_WriteByte(0x30);
    Lcd_WriteByte(0x30);
    Lcd_WriteByte(0x39);
    Lcd_WriteByte(0x3F);
    Lcd_WriteByte(0x00);
    Lcd_WriteByte(0x07);
    Lcd_WriteByte(0x03);
    Lcd_WriteByte(0x10);

    /* 列地址 0..0x7F（128 列）。 */
    Lcd_WriteCmd(0x2A);
    Lcd_WriteByte(0x00);
    Lcd_WriteByte(0x00);
    Lcd_WriteByte(0x00);
    Lcd_WriteByte(0x7F);

    /* 行地址 0..0x9F（160 行，屏只用前 128，偏移见 SetRegion）。 */
    Lcd_WriteCmd(0x2B);
    Lcd_WriteByte(0x00);
    Lcd_WriteByte(0x00);
    Lcd_WriteByte(0x00);
    Lcd_WriteByte(0x9F);

    Lcd_WriteCmd(0xF0); /* 测试寄存器，按例程打开 */
    Lcd_WriteByte(0x01);
    Lcd_WriteCmd(0xF6);
    Lcd_WriteByte(0x00);

    Lcd_WriteCmd(0x3A); /* 接口像素格式：0x05 = 16bit/pixel RGB565 */
    Lcd_WriteByte(0x05);

    Lcd_WriteCmd(0x29); /* Display ON */

    LCD_BLK_ON();
}

/* 设置显示窗口。与 TFT144 例程相同的 +2/+3 面板偏移。
 * 坐标是逻辑 0..127，写入控制器时 X+2、Y+3。 */
void Lcd_SetRegion(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye)
{
    Lcd_WriteCmd(0x2A);
    Lcd_WriteByte(0x00);
    Lcd_WriteByte((uint8_t)(xs + 2u));
    Lcd_WriteByte(0x00);
    Lcd_WriteByte((uint8_t)(xe + 2u));

    Lcd_WriteCmd(0x2B);
    Lcd_WriteByte(0x00);
    Lcd_WriteByte((uint8_t)(ys + 3u));
    Lcd_WriteByte(0x00);
    Lcd_WriteByte((uint8_t)(ye + 3u));

    Lcd_WriteCmd(0x2C); /* Memory write，之后可连续写像素 */
}

/* 画点：设 1x1 窗口后发一个 RGB565 像素（需字节交换）。 */
void Lcd_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    uint16_t v = (uint16_t)((color << 8) | (color >> 8));
    Lcd_SetRegion(x, y, x, y);
    LCD_DC_DATA();
    LCD_CS_LOW();
    HAL_SPI_Transmit(&hspi2, (uint8_t *)&v, 2, 100);
    LCD_CS_HIGH();
}

/* 填矩形：自动裁剪到屏幕范围内，逐点发同一颜色。 */
void Lcd_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint32_t n;
    uint16_t v;

    if (x >= LCD_W || y >= LCD_H)
    {
        return;
    }
    if (x + w > LCD_W)
    {
        w = LCD_W - x;
    }
    if (y + h > LCD_H)
    {
        h = LCD_H - y;
    }
    Lcd_SetRegion(x, y, (uint16_t)(x + w - 1u), (uint16_t)(y + h - 1u));
    v = (uint16_t)((color << 8) | (color >> 8));
    n = (uint32_t)w * h;
    LCD_DC_DATA();
    LCD_CS_LOW();
    for (uint32_t i = 0; i < n; i++)
    {
        HAL_SPI_Transmit(&hspi2, (uint8_t *)&v, 2, 100);
    }
    LCD_CS_HIGH();
}

/* 全屏清成一种颜色。 */
void Lcd_Clear(uint16_t color)
{
    Lcd_FillRect(0, 0, LCD_W, LCD_H, color);
}

/* 显示 8x16 ASCII 字符。字模：font_asc16.h 的 asc16[]，
 * 每字符 16 字节，高位在前，1=前景色，0=背景色。 */
void Lcd_ShowChar(uint16_t x, uint16_t y, char c, uint16_t fc, uint16_t bc)
{
    uint8_t ch = (uint8_t)c;
    const uint8_t *p;
    uint8_t row, bit;
    uint16_t f, b;

    /* 只支持可打印 ASCII 0x20..0x7E，越界直接返回。 */
    if (ch < 0x20u || ch > 0x7Eu || x + 8u > LCD_W || y + 16u > LCD_H)
    {
        return;
    }
    p = &asc16[(uint32_t)(ch - 0x20u) * 16u];
    f = (uint16_t)((fc << 8) | (fc >> 8));
    b = (uint16_t)((bc << 8) | (bc >> 8));
    Lcd_SetRegion(x, y, (uint16_t)(x + 7u), (uint16_t)(y + 15u));
    LCD_DC_DATA();
    LCD_CS_LOW();
    for (row = 0; row < 16u; row++)
    {
        for (bit = 0; bit < 8u; bit++)
        {
            uint16_t v = (p[row] & (0x80u >> bit)) ? f : b;
            HAL_SPI_Transmit(&hspi2, (uint8_t *)&v, 2, 100);
        }
    }
    LCD_CS_HIGH();
}

/* 显示 ASCII 字符串，宽度不够就截断。 */
void Lcd_ShowString(uint16_t x, uint16_t y, const char *s, uint16_t fc, uint16_t bc)
{
    if (s == NULL)
    {
        return;
    }
    while (*s != '\0')
    {
        if (x + 8u > LCD_W)
        {
            break;
        }
        Lcd_ShowChar(x, y, *s, fc, bc);
        x += 8u;
        s++;
    }
}

/* 显示 128x128 摄像头帧，1:1 不缩放。
 * 按行打包后整行一次 HAL_SPI_Transmit，比逐像素快很多。 */
void Lcd_ShowCameraFrame(const uint16_t *frame)
{
    static uint16_t row_buf[LCD_W];
    uint16_t y, x;

    if (frame == NULL)
    {
        return;
    }
    Lcd_SetRegion(0, 0, LCD_W - 1u, LCD_H - 1u);
    LCD_DC_DATA();
    LCD_CS_LOW();
    for (y = 0; y < LCD_H; y++)
    {
        for (x = 0; x < LCD_W; x++)
        {
            uint16_t v = frame[(uint32_t)y * LCD_W + x];
            row_buf[x] = (uint16_t)((v << 8) | (v >> 8));
        }
        HAL_SPI_Transmit(&hspi2, (uint8_t *)row_buf, sizeof(row_buf), 500);
    }
    LCD_CS_HIGH();
}

/* 任意分辨率帧最近邻缩放到 128x128 全屏预览（例如 320x240）。
 * 最近邻够用来看构图；不要求最终 UI 的清晰度。 */
void Lcd_ShowCameraFrameScaled(const uint16_t *frame, uint16_t w, uint16_t h)
{
    static uint16_t row_buf[LCD_W];
    uint16_t y, x;

    if (frame == NULL || w == 0u || h == 0u)
    {
        return;
    }
    Lcd_SetRegion(0, 0, LCD_W - 1u, LCD_H - 1u);
    LCD_DC_DATA();
    LCD_CS_LOW();
    for (y = 0; y < LCD_H; y++)
    {
        /* 目标行 y 对应源行 sy = y * h / 128 */
        uint16_t sy = (uint16_t)(((uint32_t)y * h) / LCD_H);
        for (x = 0; x < LCD_W; x++)
        {
            uint16_t sx = (uint16_t)(((uint32_t)x * w) / LCD_W);
            uint16_t v = frame[(uint32_t)sy * w + sx];
            row_buf[x] = (uint16_t)((v << 8) | (v >> 8));
        }
        HAL_SPI_Transmit(&hspi2, (uint8_t *)row_buf, sizeof(row_buf), 500);
    }
    LCD_CS_HIGH();
}
