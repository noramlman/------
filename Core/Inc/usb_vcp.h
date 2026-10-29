/*********************************************************************************************************************
* 文件名称          usb_vcp
* 功能说明          USB CDC 虚拟串口的读写业务层
* 开发环境          Keil MDK 5 + FreeRTOS(CMSIS-RTOS V2)
* 适用平台          STM32H743VITx
* 版本信息          first version
*
* 修改记录
* 日期              作者                备注
* 2026-9-26         -                  first version
*********************************************************************************************************************/

#ifndef USB_VCP_H
#define USB_VCP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 初始化 USB VCP 业务层。
 *
 * 创建接收 Stream Buffer、发送 Queue 和发送任务。
 * 必须在 MX_USB_DEVICE_Init() 之前调用。
 *
 * 返回：
 *   true  初始化成功
 *   false 初始化失败
 */
bool UsbVcp_Init(void);

/*
 * 查询 USB CDC 是否已经完成配置。
 *
 * 该状态由 CDC_Init_FS() 和 CDC_DeInit_FS() 更新。
 */
bool UsbVcp_IsConnected(void);

/*
 * 从 USB CDC 接收缓冲中读取数据。
 *
 * data：       接收数据的目标缓冲区
 * max_length：目标缓冲区的最大容量
 * timeout：    等待数据的 FreeRTOS tick 数
 *
 * 返回实际读取到的字节数。
 * 只能在任务上下文中调用。
 */
size_t UsbVcp_Read(uint8_t *data,
                   size_t max_length,
                   TickType_t timeout);

/*
 * 将数据加入 USB CDC 发送队列。
 *
 * data：    要发送的数据
 * length：  数据长度
 * timeout：等待发送队列空位的 FreeRTOS tick 数
 *
 * 返回实际加入发送队列的字节数。
 * 返回值小于 length，表示队列空间不足、USB 未连接，
 * 或 USB VCP 尚未初始化。
 * 只能在任务上下文中调用。
 */
size_t UsbVcp_Write(const uint8_t *data,
                    size_t length,
                    TickType_t timeout);

/*
 * CDC 接口完成配置时调用。
 *
 * 由 CDC_Init_FS() 调用，不由普通业务代码调用。
 */
void UsbVcp_OnConnected(void);

/*
 * CDC 接口被释放或 USB 断开时调用。
 *
 * 由 CDC_DeInit_FS() 调用，不由普通业务代码调用。
 */
void UsbVcp_OnDisconnected(void);

/*
 * USB CDC 收到数据时调用。
 *
 * 由 CDC_Receive_FS() 调用。
 * 该函数运行在 USB 中断调用链中，内部不能阻塞。
 */
void UsbVcp_OnRxFromISR(const uint8_t *data,
                        uint32_t length);

/*
 * 一次 USB CDC IN 传输完成时调用。
 *
 * 由 CDC_TransmitCplt_FS() 调用。
 * 该函数运行在 USB 中断调用链中，内部不能阻塞。
 */
void UsbVcp_OnTxCompleteFromISR(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_VCP_H */
