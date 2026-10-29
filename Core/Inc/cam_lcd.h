/*********************************************************************************************************************
* 文件名称          cam_lcd
* 功能说明          摄像头 RGB565 图像实时显示到 1.44 寸屏
*                   采集：DCMI + DMA（NORMAL 模式，定长 320x240x2 字节），一帧一停
*                   显示：最近邻缩放到 128x128，整屏显示
* 适用平台          STM32H743VITx
*
* 修改记录
* 日期              作者                备注
* 2026-9-26         -                  first version
*********************************************************************************************************************/

#ifndef CAM_LCD_H
#define CAM_LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// *************************** 接口用法 ***************************
// 【USB 打印】往虚拟串口发文本（主要用来看帧率）
//      Usb_PrintStr("hello\r\n");
//      Usb_Printf("[fps] %u\r\n", fps);
//
// 【主任务】一个任务全包了：初始化屏和摄像头，然后一帧一帧地收 → 缩放 → 显示
//      osThreadNew(CamLcdTask, NULL, &attr);
//
// 电脑侧怎么算帧率：任务内部自己统计，每秒通过 USB 打印一行
//      [fps] 23   frames 1152
//
// 采集层（dcmi_app.h）用到的几个：
//      DCMI_App_Init();        只留帧中断
//      DCMI_RGB_Start();       切 RGB565 模式 + 设分辨率 + 启动
//      DCMI_RGB_Next();        重启下一帧
//      xRgbFrameSem            一帧收完的信号量
//      rgb_frame               320x240 的 RGB565 缓冲（uint16_t[]）

void Usb_PrintStr(const char *s);
void Usb_Printf(const char *fmt, ...);

// 摄像头 → 屏幕 显示任务（无限循环，不返回）
void CamLcdTask(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* CAM_LCD_H */
