#include "ov2640.h"
#include "app_delay.h"
#include "ov2640_regs.h"
#include "main.h"

// Cube 生成的 hi2c2 在 main.c 里，这里声明一下就能用
// 不用原例程 sccb.c 的 GPIO 模拟时序，SCCB 本质就是 I2C，直接用硬件 I2C2
extern I2C_HandleTypeDef hi2c2;

uint16_t OV2640_CameraID = 0;

// 写一个寄存器：先发寄存器地址，再发数据，HAL 的 Mem_Write 一次搞定
// 对应原例程 SCCB_WR_Reg
uint8_t OV2640_WriteReg(uint8_t reg, uint8_t data)
{
    if (HAL_I2C_Mem_Write(&hi2c2, OV2640_I2C_ADDR, reg,
                          I2C_MEMADD_SIZE_8BIT, &data, 1, 100) == HAL_OK)
    {
        return 0;
    }
    return 1;
}

// 读一个寄存器：HAL 的 Mem_Read 会自动产生 写地址+重复起始+读数据
// 对应原例程 SCCB_RD_Reg
uint8_t OV2640_ReadReg(uint8_t reg, uint8_t *pdata)
{
    if (HAL_I2C_Mem_Read(&hi2c2, OV2640_I2C_ADDR, reg,
                         I2C_MEMADD_SIZE_8BIT, pdata, 1, 100) == HAL_OK)
    {
        return 0;
    }
    return 1;
}

// 上电初始化：流程照搬原例程 OV2640_Init，但三处没移植：
// 1. GPIO 初始化没搬：PA7/PC4 Cube 已经配好推挽上拉高速
// 2. PA5 补光灯没搬：那是正点/devbox 板子的外设，你母板没有
// 3. SCCB_Init 没搬：硬件 I2C 不需要初始化模拟 IO
// HAL_Delay 能直接用：你工程 HAL 时间基准是 TIM6，不是 SysTick，所以开了 FreeRTOS 也安全
uint8_t OV2640_Init(void)
{
    uint16_t i;
    uint8_t hi, lo;

    OV2640_PWDN(0);
    App_DelayMs(10);
    OV2640_RST(0);
    App_DelayMs(200);
    OV2640_RST(1);
    App_DelayMs(200);

    // 软复位：先切到 sensor 区(0xFF=0x01 是原例程的写法，其实是选 bank)，再写 COM7=0x80
    OV2640_WriteReg(OV2640_DSP_RA_DLMT, 0x01);
    OV2640_WriteReg(OV2640_SENSOR_COM7, 0x80);
    App_DelayMs(50);

    // 读厂家 ID
    OV2640_ReadReg(OV2640_SENSOR_MIDH, &hi);
    OV2640_ReadReg(OV2640_SENSOR_MIDL, &lo);
    OV2640_CameraID = ((uint16_t)hi << 8) | lo;
    if (OV2640_CameraID != OV2640_MID)
    {
        return 1;
    }

    // 读产品 ID
    OV2640_ReadReg(OV2640_SENSOR_PIDH, &hi);
    OV2640_ReadReg(OV2640_SENSOR_PIDL, &lo);
    OV2640_CameraID = ((uint16_t)hi << 8) | lo;
    if ((OV2640_CameraID != OV2640_PID1) && (OV2640_CameraID != OV2640_PID2))
    {
        return 2;
    }

    // 灌 SXGA 初始化表：纯数值表，原样复用 ov2640_regs.h 里的表
    for (i = 0; i < sizeof(ov2640_sxga_init_reg_tbl) / 2; i++)
    {
        OV2640_WriteReg(ov2640_sxga_init_reg_tbl[i][0], ov2640_sxga_init_reg_tbl[i][1]);
        if (i < 10)
        {
            App_DelayMs(5);
        }
    }

    return 0;
}

// 切 JPEG 输出：先灌 YUV422 基础表，再灌 JPEG 使能表，跟原例程 OV2640_JPEG_Mode 一样
void OV2640_JPEG_Mode(void)
{
    uint16_t i;
    for (i = 0; i < sizeof(ov2640_yuv422_reg_tbl) / 2; i++)
    {
        OV2640_WriteReg(ov2640_yuv422_reg_tbl[i][0], ov2640_yuv422_reg_tbl[i][1]);
    }
    for (i = 0; i < sizeof(ov2640_jpeg_reg_tbl) / 2; i++)
    {
        OV2640_WriteReg(ov2640_jpeg_reg_tbl[i][0], ov2640_jpeg_reg_tbl[i][1]);
    }
}

// 切 RGB565 输出：对应原例程 OV2640_RGB565_Mode，这次只做 JPEG+USB，留着给你以后点屏用
void OV2640_RGB565_Mode(void)
{
    uint16_t i;
    for (i = 0; i < sizeof(ov2640_rgb565_reg_tbl) / 2; i++)
    {
        OV2640_WriteReg(ov2640_rgb565_reg_tbl[i][0], ov2640_rgb565_reg_tbl[i][1]);
    }
}

// 输出窗口大小：算法照搬原例程，宽高必须是 4 的倍数
uint8_t OV2640_OutSize_Set(uint16_t width, uint16_t height)
{
    uint16_t outw, outh;
    uint8_t temp;

    if (width % 4)
    {
        return 1;
    }
    if (height % 4)
    {
        return 2;
    }

    outw = width / 4;
    outh = height / 4;

    OV2640_WriteReg(0xFF, 0x00);
    OV2640_WriteReg(0xE0, 0x04);
    OV2640_WriteReg(0x5A, outw & 0xFF);
    OV2640_WriteReg(0x5B, outh & 0xFF);
    temp = (outw >> 8) & 0x03;
    temp |= (outh >> 6) & 0x04;
    OV2640_WriteReg(0x5C, temp);
    OV2640_WriteReg(0xE0, 0x00);

    return 0;
}

// 图像开窗：算法照搬原例程 OV2640_ImageWin_Set
// 只调 OutSize 不调这个时，窗口还停在 SXGA 初始化表里的默认值，
// 只会把一小块视野拉伸到屏幕上，看起来像开了放大
uint8_t OV2640_ImageWin_Set(uint16_t offx, uint16_t offy, uint16_t width, uint16_t height)
{
    uint16_t hsize, vsize;
    uint8_t temp;

    if (width % 4)
    {
        return 1;
    }
    if (height % 4)
    {
        return 2;
    }

    hsize = width / 4;
    vsize = height / 4;

    OV2640_WriteReg(0xFF, 0x00);
    OV2640_WriteReg(0xE0, 0x04);
    OV2640_WriteReg(0x51, hsize & 0xFF);
    OV2640_WriteReg(0x52, vsize & 0xFF);
    OV2640_WriteReg(0x53, offx & 0xFF);
    OV2640_WriteReg(0x54, offy & 0xFF);
    temp = (vsize >> 1) & 0x80;
    temp |= (offy >> 4) & 0x70;
    temp |= (hsize >> 5) & 0x08;
    temp |= (offx >> 8) & 0x07;
    OV2640_WriteReg(0x55, temp);
    OV2640_WriteReg(0x57, (hsize >> 2) & 0x80);
    OV2640_WriteReg(0xE0, 0x00);

    return 0;
}

// DSP 图像尺寸：算法照搬原例程 OV2640_ImageSize_Set
uint8_t OV2640_ImageSize_Set(uint16_t width, uint16_t height)
{
    uint8_t temp;

    OV2640_WriteReg(0xFF, 0x00);
    OV2640_WriteReg(0xE0, 0x04);
    OV2640_WriteReg(0xC0, (width >> 3) & 0xFF);
    OV2640_WriteReg(0xC1, (height >> 3) & 0xFF);
    temp = (width & 0x07) << 3;
    temp |= height & 0x07;
    temp |= (width >> 4) & 0x80;
    OV2640_WriteReg(0x8C, temp);
    OV2640_WriteReg(0xE0, 0x00);
    return 0;
}

// SVGA 800x600：只改分辨率相关寄存器，数值对齐 ov2640_svga_init_reg_tbl
// 里那一段。UXGA 直接缩到 128 比例太大，先降到 SVGA 再出图。
void OV2640_SetRes_SVGA(void)
{
    OV2640_WriteReg(0xFF, 0x01);
    /* 0x11=0 pushes frame rate up; SPI cannot keep up and rows shift.
       0x01 is the stable divider used by slower preview configs. */
    OV2640_WriteReg(0x11, 0x01);
    OV2640_WriteReg(0x12, 0x40); // SVGA
    OV2640_WriteReg(0x17, 0x11);
    OV2640_WriteReg(0x18, 0x43);
    OV2640_WriteReg(0x19, 0x00);
    OV2640_WriteReg(0x1A, 0x4B);
    OV2640_WriteReg(0x32, 0x09);
    OV2640_WriteReg(0x37, 0xC0);
    (void)OV2640_ImageSize_Set(800, 600);
}
