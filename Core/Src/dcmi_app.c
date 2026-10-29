#include "dcmi_app.h"
#include "ov2640.h"
#include "main.h"
#include "task.h"
#include <string.h>

/*==============================================================================
 *  dcmi_app.c —— DCMI snapshot 采集实现
 *  设计要点：
 *    · 复用 CubeMX 的 hdcmi / hdma_dcmi，绝不重复 HAL_DCMI_Init；
 *    · 两种模式都是“单帧 snapshot”：一帧结束后 DMA 自己停，任务读完再 *_Next；
 *    · 中断里不做 memcpy（RGB 一帧 150KB），只停 DMA + 发信号量。
 *============================================================================*/

/* CubeMX 生成的句柄在 main.c，这里只 extern 使用。 */
extern DCMI_HandleTypeDef hdcmi;
extern DMA_HandleTypeDef hdma_dcmi;

/* 当前采集模式：0 空闲，1 JPEG，2 RGB。 */
static volatile uint8_t s_mode = 0;
#define MODE_IDLE 0
#define MODE_JPEG 1
#define MODE_RGB  2

/* 单缓冲：snapshot 停住后任务再读，不需要双缓冲乒乓。 */
__attribute__((aligned(32))) uint32_t jpeg_buf[JPEG_BUF_WORDS];
__attribute__((aligned(32))) uint16_t rgb_frame[RGB_FRAME_PIXELS];

SemaphoreHandle_t xJpegFrameSem = NULL;
SemaphoreHandle_t xRgbFrameSem = NULL;

volatile uint32_t jpeg_valid_bytes = 0;
volatile uint32_t rgb_frame_cnt = 0;
volatile uint32_t rgb_drop_cnt = 0;

/* 启动一次 snapshot 采集：只开帧中断，写 CR 进 snapshot 并置 CAPTURE。 */
static void DCMI_CaptureOnce(void)
{
    DCMI->IER = 0;
    __HAL_DCMI_ENABLE_IT(&hdcmi, DCMI_IT_FRAME);
    __HAL_DCMI_ENABLE(&hdcmi);
    hdcmi.Instance->CR &= (uint32_t)(~DCMI_CR_CM);
    hdcmi.Instance->CR |= (uint32_t)DCMI_MODE_SNAPSHOT;
    hdcmi.Instance->CR |= DCMI_CR_CAPTURE;
}

/* 一次性初始化：建信号量；关掉 HAL_DCMI_Init 默认打开的
 * LINE/VSYNC/ERR/OVR，只留 FRAME，减少中断风暴。 */
void DCMI_App_Init(void)
{
    if (xJpegFrameSem == NULL)
    {
        xJpegFrameSem = xSemaphoreCreateBinary();
    }
    if (xRgbFrameSem == NULL)
    {
        xRgbFrameSem = xSemaphoreCreateBinary();
    }

    s_mode = MODE_IDLE;
    jpeg_valid_bytes = 0;
    rgb_frame_cnt = 0;
    rgb_drop_cnt = 0;

    DCMI->IER = 0;
    __HAL_DCMI_ENABLE_IT(&hdcmi, DCMI_IT_FRAME);
    __HAL_DCMI_ENABLE(&hdcmi);
}

/* JPEG 完整启动：OV2640 切 JPEG；DMA 配成 Word/Word 循环。
 * JPEG 长度不定，没法用双缓冲对齐帧边界；帧中断里冻 DMA，
 * 再用 NDTR 反算这一帧实际写入了多少字节。 */
uint8_t DCMI_JPEG_Start(uint16_t out_w, uint16_t out_h)
{
    DCMI_App_Stop();

    OV2640_JPEG_Mode();
    (void)OV2640_ImageSize_Set(1600, 1200);
    (void)OV2640_ImageWin_Set(0, 0, 1600, 1200);
    OV2640_OutSize_Set(out_w, out_h);

    /* 若上一次是 RGB（HalfWord），必须 DeInit + Init，数据宽度才生效。 */
    (void)HAL_DMA_Abort(&hdma_dcmi);
    (void)HAL_DMA_DeInit(&hdma_dcmi);
    hdma_dcmi.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_dcmi.Init.Mode = DMA_CIRCULAR;
    if (HAL_DMA_Init(&hdma_dcmi) != HAL_OK)
    {
        return 1;
    }
    __HAL_LINKDMA(&hdcmi, DMA_Handle, hdma_dcmi);

    /* 清双缓冲位，关掉全部 DMA 中断。JPEG 分帧只靠 DCMI 帧中断。 */
    ((DMA_Stream_TypeDef *)hdma_dcmi.Instance)->CR &= (uint32_t)(~DMA_SxCR_DBM);
    __HAL_DMA_DISABLE_IT(&hdma_dcmi, DMA_IT_TC | DMA_IT_HT | DMA_IT_TE | DMA_IT_DME | DMA_IT_FE);

    jpeg_valid_bytes = 0;
    s_mode = MODE_JPEG;

    if (HAL_DMA_Start(&hdma_dcmi, (uint32_t)&DCMI->DR, (uint32_t)jpeg_buf, JPEG_BUF_WORDS) != HAL_OK)
    {
        s_mode = MODE_IDLE;
        return 2;
    }
    __HAL_DMA_ENABLE(&hdma_dcmi);

    DCMI_CaptureOnce();
    return 0;
}

/* RGB 完整启动：OV2640 输出 320x240 RGB565 全视场，给 CV / 小 CNN 用。
 * 和 JPEG 一样走 snapshot：一帧写进 rgb_frame 后 DMA 停；
 * 任务处理完整帧后再 DCMI_RGB_Next。
 * 中断里不做 150KB memcpy，任务忙时 DMA 也不会覆盖缓冲。 */
uint8_t DCMI_RGB_Start(void)
{
    DCMI_App_Stop();

    OV2640_RGB565_Mode();
    /* SVGA 全视场 800x600 → 输出 320x240（约 2.5 倍缩），
     * 场景完整，又适合找线 / 小网络输入。 */
    OV2640_SetRes_SVGA();
    (void)OV2640_ImageSize_Set(800, 600);
    (void)OV2640_ImageWin_Set(0, 0, 800, 600);
    OV2640_OutSize_Set(RGB_W, RGB_H);

    (void)HAL_DMA_Abort(&hdma_dcmi);
    (void)HAL_DMA_DeInit(&hdma_dcmi);
    /* Word/Word：一次 DR 传 2 个 RGB565，NDTR=38400，16 位装得下。 */
    hdma_dcmi.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_dcmi.Init.Mode = DMA_NORMAL;
    if (HAL_DMA_Init(&hdma_dcmi) != HAL_OK)
    {
        return 1;
    }
    __HAL_LINKDMA(&hdcmi, DMA_Handle, hdma_dcmi);

    /* 清掉 HAL 可能残留的回调，避免半传输/错误中断里跑空函数。 */
    hdma_dcmi.XferCpltCallback = NULL;
    hdma_dcmi.XferM1CpltCallback = NULL;
    hdma_dcmi.XferHalfCpltCallback = NULL;
    hdma_dcmi.XferM1HalfCpltCallback = NULL;
    hdma_dcmi.XferErrorCallback = NULL;
    ((DMA_Stream_TypeDef *)hdma_dcmi.Instance)->CR &= (uint32_t)(~DMA_SxCR_DBM);
    __HAL_DMA_DISABLE_IT(&hdma_dcmi, DMA_IT_TC | DMA_IT_HT | DMA_IT_TE | DMA_IT_DME | DMA_IT_FE);

    rgb_frame_cnt = 0;
    rgb_drop_cnt = 0;
    s_mode = MODE_RGB;

    if (HAL_DMA_Start(&hdma_dcmi, (uint32_t)&DCMI->DR,
                      (uint32_t)rgb_frame, RGB_FRAME_WORDS) != HAL_OK)
    {
        s_mode = MODE_IDLE;
        return 2;
    }
    __HAL_DMA_ENABLE(&hdma_dcmi);

    DCMI_CaptureOnce();
    return 0;
}

/* 下一帧重启（JPEG）：只 Abort + Start DMA + 再 snapshot，不写 OV2640，
 * 返回快。每帧处理完的常规路径。 */
uint8_t DCMI_JPEG_Next(void)
{
    if (s_mode != MODE_JPEG)
    {
        return 1;
    }
    (void)HAL_DMA_Abort(&hdma_dcmi);
    if (HAL_DMA_Start(&hdma_dcmi, (uint32_t)&DCMI->DR, (uint32_t)jpeg_buf, JPEG_BUF_WORDS) != HAL_OK)
    {
        return 2;
    }
    __HAL_DMA_ENABLE(&hdma_dcmi);
    DCMI_CaptureOnce();
    return 0;
}

/* 下一帧重启（RGB565 320x240）。 */
uint8_t DCMI_RGB_Next(void)
{
    if (s_mode != MODE_RGB)
    {
        return 1;
    }
    (void)HAL_DMA_Abort(&hdma_dcmi);
    if (HAL_DMA_Start(&hdma_dcmi, (uint32_t)&DCMI->DR,
                      (uint32_t)rgb_frame, RGB_FRAME_WORDS) != HAL_OK)
    {
        return 2;
    }
    __HAL_DMA_ENABLE(&hdma_dcmi);
    DCMI_CaptureOnce();
    return 0;
}

/* 停止采集：清 CAPTURE 并 Abort DMA。 */
void DCMI_App_Stop(void)
{
    s_mode = MODE_IDLE;
    (void)HAL_DCMI_Stop(&hdcmi);
    (void)HAL_DMA_Abort(&hdma_dcmi);
}

/* 帧事件回调：由 DCMI_IRQHandler → HAL_DCMI_IRQHandler 调进来。
 * 两种模式都是 snapshot：一帧后 DMA 已停，任务处理完再 *_Next，
 * 所以任务读缓冲时 DMA 不会写它。 */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi_cb)
{
    BaseType_t woken = pdFALSE;

    (void)hdcmi_cb;

    if (s_mode == MODE_JPEG)
    {
        /* 冻住 DMA，用 NDTR 算出这一帧实际送来了多少字节。 */
        __HAL_DMA_DISABLE(&hdma_dcmi);
        {
            uint32_t ndtr = __HAL_DMA_GET_COUNTER(&hdma_dcmi);
            if (ndtr <= JPEG_BUF_WORDS)
            {
                jpeg_valid_bytes = (JPEG_BUF_WORDS - ndtr) * 4u;
            }
            else
            {
                jpeg_valid_bytes = 0;
            }
        }
        if (xJpegFrameSem != NULL)
        {
            xSemaphoreGiveFromISR(xJpegFrameSem, &woken);
        }
    }
    else if (s_mode == MODE_RGB)
    {
        __HAL_DMA_DISABLE(&hdma_dcmi);
        rgb_frame_cnt++;
        if (xRgbFrameSem != NULL)
        {
            xSemaphoreGiveFromISR(xRgbFrameSem, &woken);
        }
    }

    /* HAL 进中断时会关帧中断，这里必须重新打开。 */
    __HAL_DCMI_ENABLE_IT(&hdcmi, DCMI_IT_FRAME);
    portYIELD_FROM_ISR(woken);
}
