/*********************************************************************************************************************
* 文件名称          usb_vcp
* 功能说明          USB CDC 虚拟串口的读写业务层
*                   接收：CDC 接收回调 -> 接收字节流缓冲 -> UsbVcp_Read()
*                   发送：UsbVcp_Write() -> 发送队列 -> 发送任务 -> CDC 发送接口
* 开发环境          Keil MDK 5 + FreeRTOS(CMSIS-RTOS V2)
* 适用平台          STM32H743VITx
* 版本信息          first version
*
* 修改记录
* 日期              作者                备注
* 2026-9-26         -                  first version
*********************************************************************************************************************/

#include "usb_vcp.h"

#include <string.h>

#include "queue.h"
#include "stream_buffer.h"
#include "task.h"

#include "usbd_cdc_if.h"

/*
 * RX 是字节流缓冲区。
 *
 * 电脑发来的数据会先由 CDC_Receive_FS() 写入这里，
 * 业务任务再通过 UsbVcp_Read() 读取。
 */
#define USB_VCP_RX_BUFFER_SIZE          512U

/*
 * TX Queue 中每一项最多保存 128 字节。
 *
 * UsbVcp_Write() 遇到更长的数据时，会拆成多个消息入队。
 */
#define USB_VCP_TX_MESSAGE_SIZE         128U

/*
 * 最多缓存 8 条待发送消息。
 *
 * 当前 TX Queue 数据容量约为：
 * 8 × 128 = 1024 字节，不包括 FreeRTOS 管理开销。
 */
#define USB_VCP_TX_QUEUE_LENGTH         8U

/*
 * FreeRTOS 的 xTaskCreate() 栈深度单位是 StackType_t，
 * 在 Cortex-M7 工程中通常是 4 字节一个单位。
 *
 * 384 大约对应 1536 字节任务栈。
 */
#define USB_VCP_TX_TASK_STACK_WORDS     384U

/*
 * VCP 发送任务优先级。
 *
 * 当前工程中 RGB 任务优先级是 2，USB/JPEG 发送任务优先级是 3，
 * 所以暂时给 VCP 发送任务设为 3。
 */
#define USB_VCP_TX_TASK_PRIORITY        (tskIDLE_PRIORITY + 3U)

/*
 * CDC 返回 USBD_BUSY 时，每次重试前等待 1 ms。
 */
#define USB_VCP_TX_BUSY_RETRY_TICKS       pdMS_TO_TICKS(1U)

/*
 * 一条消息最多等待 CDC 空闲 100 ms。
 *
 * 超过这个时间仍然是 USBD_BUSY，就放弃当前消息。
 */
#define USB_VCP_TX_BUSY_TIMEOUT_TICKS     pdMS_TO_TICKS(100U)

/*
 * 成功提交发送后，每隔 100 ms 检查一次：
 * 是否发送完成，或者 USB 是否已经断开。
 */
#define USB_VCP_TX_COMPLETE_WAIT_TICKS    pdMS_TO_TICKS(100U)

/* 发送队列中的消息保存数据副本，避免调用者缓冲区提前失效。 */
typedef struct
{
    uint16_t length;
    uint8_t data[USB_VCP_TX_MESSAGE_SIZE];
} UsbVcpTxMessage;

/* USB VCP 使用的 FreeRTOS 对象和连接状态。 */
static StreamBufferHandle_t s_rx_stream = NULL;
static QueueHandle_t s_tx_queue = NULL;
static TaskHandle_t s_tx_task = NULL;

static volatile bool s_connected = false;
static volatile bool s_tx_waiting_completion = false;
static volatile uint32_t s_rx_dropped_bytes = 0U;
static bool s_initialized = false;

static void UsbVcp_TxTask(void *argument);

bool UsbVcp_Init(void)
{
    BaseType_t task_result;

    if (s_initialized)
    {
        return true;
    }

    s_connected = false;
    s_tx_waiting_completion = false;
    s_rx_dropped_bytes = 0U;

    s_rx_stream = xStreamBufferCreate(
        USB_VCP_RX_BUFFER_SIZE,
        1U);

    if (s_rx_stream == NULL)
    {
        return false;
    }

    s_tx_queue = xQueueCreate(
        USB_VCP_TX_QUEUE_LENGTH,
        sizeof(UsbVcpTxMessage));

    if (s_tx_queue == NULL)
    {
        vStreamBufferDelete(s_rx_stream);
        s_rx_stream = NULL;
        return false;
    }

    task_result = xTaskCreate(
        UsbVcp_TxTask,
        "usb_vcp_tx",
        USB_VCP_TX_TASK_STACK_WORDS,
        NULL,
        USB_VCP_TX_TASK_PRIORITY,
        &s_tx_task);

    if (task_result != pdPASS)
    {
        vQueueDelete(s_tx_queue);
        vStreamBufferDelete(s_rx_stream);

        s_tx_queue = NULL;
        s_rx_stream = NULL;
        s_tx_task = NULL;

        return false;
    }

    s_initialized = true;
    return true;
}

bool UsbVcp_IsConnected(void)
{
    if (!s_initialized)
    {
        return false;
    }

    return s_connected;
}

void UsbVcp_OnConnected(void)
{
    if (!s_initialized)
    {
        return;
    }

    s_connected = true;
}

void UsbVcp_OnDisconnected(void)
{
    /* 断开后禁止继续提交数据，并释放发送任务对旧传输的等待。 */
    s_connected = false;
    s_tx_waiting_completion = false;
}

void UsbVcp_OnRxFromISR(const uint8_t *data,
                        uint32_t length)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    size_t bytes_written;

    if (!s_initialized)
    {
        return;
    }

    if ((s_rx_stream == NULL) ||
        (data == NULL) ||
        (length == 0U))
    {
        return;
    }

    bytes_written = xStreamBufferSendFromISR(
        s_rx_stream,
        data,
        (size_t)length,
        &higher_priority_task_woken);

    if (bytes_written < (size_t)length)
    {
        s_rx_dropped_bytes +=
            (uint32_t)((size_t)length - bytes_written);
    }

    portYIELD_FROM_ISR(higher_priority_task_woken);
}

size_t UsbVcp_Read(uint8_t *data,
                   size_t max_length,
                   TickType_t timeout)
{
    if (!s_initialized)
    {
        return 0U;
    }

    if ((s_rx_stream == NULL) ||
        (data == NULL) ||
        (max_length == 0U))
    {
        return 0U;
    }

    return xStreamBufferReceive(
        s_rx_stream,
        data,
        max_length,
        timeout);
}

size_t UsbVcp_Write(const uint8_t *data,
                    size_t length,
                    TickType_t timeout)
{
    UsbVcpTxMessage message;
    size_t total_written = 0U;
    size_t chunk_length;
    TickType_t start_tick;
    TickType_t wait_ticks;
    TickType_t elapsed_ticks;

    if (!s_initialized)
    {
        return 0U;
    }

    if ((s_tx_queue == NULL) ||
        (data == NULL) ||
        (length == 0U))
    {
        return 0U;
    }

    if (!s_connected)
    {
        return 0U;
    }

    start_tick = xTaskGetTickCount();
    wait_ticks = timeout;

    while (total_written < length)
    {
        if (!s_connected)
        {
            break;
        }

        chunk_length = length - total_written;

        if (chunk_length > USB_VCP_TX_MESSAGE_SIZE)
        {
            chunk_length = USB_VCP_TX_MESSAGE_SIZE;
        }

        message.length = (uint16_t)chunk_length;

        memcpy(message.data,
               &data[total_written],
               chunk_length);

        if (xQueueSendToBack(s_tx_queue,
                             &message,
                             wait_ticks) != pdTRUE)
        {
            break;
        }

        total_written += chunk_length;

        if (timeout != portMAX_DELAY)
        {
            elapsed_ticks = xTaskGetTickCount() - start_tick;

            if (elapsed_ticks >= timeout)
            {
                wait_ticks = 0U;
            }
            else
            {
                wait_ticks = timeout - elapsed_ticks;
            }
        }
    }

    return total_written;
}

void UsbVcp_OnTxCompleteFromISR(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (!s_initialized)
    {
        return;
    }

    if ((s_tx_task == NULL) ||
        (!s_tx_waiting_completion))
    {
        return;
    }

    /* 只唤醒由 VCP 发送任务提交的传输，忽略旧 JPEG/日志通道的完成事件。 */
    s_tx_waiting_completion = false;

    vTaskNotifyGiveFromISR(
        s_tx_task,
        &higher_priority_task_woken);

    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void UsbVcp_TxTask(void *argument)
{
    UsbVcpTxMessage message;
    TickType_t busy_start_tick;
    TickType_t busy_elapsed_ticks;
    uint8_t transmit_status;
    BaseType_t transmit_submitted;

    (void)argument;

    for (;;)
    {
        if (xQueueReceive(s_tx_queue,
                          &message,
                          portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        if (!s_connected)
        {
            continue;
        }

        /*
         * 清除可能残留的旧发送完成通知。
         *
         * 新消息只能等待属于本次发送的完成通知。
         */
        (void)ulTaskNotifyTake(pdTRUE, 0U);

        busy_start_tick = xTaskGetTickCount();
        transmit_submitted = pdFALSE;

        for (;;)
        {
            if (!s_connected)
            {
                break;
            }

            /*
             * USB 中断优先级为 5，进入临界区可避免在“提交成功”和
             * 设置所有权标志之间插入发送完成回调。
             */
            taskENTER_CRITICAL();
            s_tx_waiting_completion = true;
            transmit_status = CDC_Transmit_FS(
                message.data,
                message.length);
            if (transmit_status != USBD_OK)
            {
                s_tx_waiting_completion = false;
            }
            taskEXIT_CRITICAL();

            if (transmit_status == USBD_OK)
            {
                transmit_submitted = pdTRUE;
                break;
            }

            if (transmit_status != USBD_BUSY)
            {
                break;
            }

            busy_elapsed_ticks =
                xTaskGetTickCount() - busy_start_tick;

            if (busy_elapsed_ticks >=
                USB_VCP_TX_BUSY_TIMEOUT_TICKS)
            {
                break;
            }

            vTaskDelay(USB_VCP_TX_BUSY_RETRY_TICKS);
        }

        if (transmit_submitted != pdTRUE)
        {
            continue;
        }

        /*
         * message.data 已经交给 USB CDC。
         *
         * 在收到发送完成通知或 USB 断开之前，
         * 不能从 Queue 读取下一条消息，否则 message.data
         * 会被覆盖。
         */
        for (;;)
        {
            if (ulTaskNotifyTake(
                    pdTRUE,
                    USB_VCP_TX_COMPLETE_WAIT_TICKS) > 0U)
            {
                break;
            }

            /* USB 断开会清除此标志；即使马上重连，也不能继续等旧传输。 */
            if ((!s_connected) ||
                (!s_tx_waiting_completion))
            {
                s_tx_waiting_completion = false;
                break;
            }
        }
    }
}

