#ifndef APP_DELAY_H
#define APP_DELAY_H

#include <stdint.h>

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/* Preserve the hardware reset intervals while allowing other tasks to run. */
static inline void App_DelayMs(uint32_t ms)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        TickType_t ticks = pdMS_TO_TICKS(ms);
        vTaskDelay((ticks == 0U) ? 1U : ticks);
    }
    else
    {
        HAL_Delay(ms);
    }
}

#endif /* APP_DELAY_H */
