#ifndef __OV2640_H
#define __OV2640_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

// 兼容原例程大表里的 u8/u16/u32 写法，不用改表
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

// OV2640 的 SCCB 本质就是 I2C，8 位写地址 0x60，读时末位置 1
// 原例程 sccb.h 里 SCCB_ID 0x60 就是它，HAL 库 DevAddress 传左移后的形式，正好也是 0x60
#define OV2640_I2C_ADDR     ((uint16_t)0x60)
 // #define OV2640_I2C_ADDR ((uint16_t)(0x60 >> 1)) 

// 摄像头 ID：初始化时读出来跟这几个比，对不上就是没接好或者时序不对
#define OV2640_MID          0x7FA2
#define OV2640_PID1         0x2642
#define OV2640_PID2         0x2641

// 操作 sensor 区 / DSP 区要用的关键寄存器
// 0xFF 是 bank 选择：写 0x00 选 sensor，写 0x01 选 DSP，原例程就是这么切 bank 的
#define OV2640_DSP_RA_DLMT  0xFF
#define OV2640_SENSOR_COM7  0x12
#define OV2640_SENSOR_MIDH  0x1C
#define OV2640_SENSOR_MIDL  0x1D
#define OV2640_SENSOR_PIDH  0x0A
#define OV2640_SENSOR_PIDL  0x0B

// PWDN / RST 引脚：IOC 里 PA7 / PC4 已经配成推挽输出+上拉+高速，这里只管电平
// 空闲必须是 PWDN=0(正常工作) + RST=1(不在复位)，跟原例程上电时序一致
#define OV2640_PWDN(n)  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, ((n) ? GPIO_PIN_SET : GPIO_PIN_RESET))
#define OV2640_RST(n)   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, ((n) ? GPIO_PIN_SET : GPIO_PIN_RESET))

// 单寄存器读写，底层走硬件 I2C2，不用原例程的 GPIO 模拟 SCCB
// 返回 0 成功，1 失败
uint8_t OV2640_WriteReg(uint8_t reg, uint8_t data);
uint8_t OV2640_ReadReg(uint8_t reg, uint8_t *pdata);

// 上电初始化：复位时序 + 读 ID + 灌 SXGA 初始化表
// 返回 0 成功，1 是 MID 对不上，2 是 PID 对不上
uint8_t OV2640_Init(void);

// 输出格式切换，对应原例程 OV2640_JPEG_Mode / OV2640_RGB565_Mode
void OV2640_JPEG_Mode(void);
void OV2640_RGB565_Mode(void);

// 输出分辨率，必须是 4 的倍数，跟原例程 OV2640_OutSize_Set 一样，返回 0 成功
uint8_t OV2640_OutSize_Set(uint16_t width, uint16_t height);

// 图像开窗：从 sensor 画幅里裁一块，OutSize 把这块缩放到输出
// 宽高必须是 4 的倍数，且 >= OutSize 的宽高。全幅视野用 (0,0,1600,1200)
uint8_t OV2640_ImageWin_Set(uint16_t offx, uint16_t offy, uint16_t width, uint16_t height);

// DSP 图像尺寸 HSIZE/VSIZE，必须和 sensor 模式（UXGA/SVGA）一致
uint8_t OV2640_ImageSize_Set(uint16_t width, uint16_t height);

// 切到 SVGA 800x600 画幅（sensor 窗口 + DSP size）。128 缩放别从 UXGA
// 硬缩，比例太大时 DSP 可能只吐出角上一块，看起来像放大。
void OV2640_SetRes_SVGA(void);

// 当前摄像头型号，调通后可以在任务里拿去判断
extern uint16_t OV2640_CameraID;

#endif
