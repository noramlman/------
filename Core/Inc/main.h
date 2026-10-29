/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define M2_IN2_Pin GPIO_PIN_0
#define M2_IN2_GPIO_Port GPIOC
#define M3_IN1_Pin GPIO_PIN_1
#define M3_IN1_GPIO_Port GPIOC
#define M3_IN2_Pin GPIO_PIN_2
#define M3_IN2_GPIO_Port GPIOC
#define M4_IN1_Pin GPIO_PIN_3
#define M4_IN1_GPIO_Port GPIOC
#define M1_IN1_Pin GPIO_PIN_7
#define M1_IN1_GPIO_Port GPIOE
#define M1_IN2_Pin GPIO_PIN_8
#define M1_IN2_GPIO_Port GPIOE
#define M2_IN1_Pin GPIO_PIN_10
#define M2_IN1_GPIO_Port GPIOE
#define GYRO2_CS_Pin GPIO_PIN_15
#define GYRO2_CS_GPIO_Port GPIOE
#define Screen_CLK_Pin GPIO_PIN_13
#define Screen_CLK_GPIO_Port GPIOB
#define Screen_MOSI_Pin GPIO_PIN_15
#define Screen_MOSI_GPIO_Port GPIOB
#define GYRO1_CS_Pin GPIO_PIN_15
#define GYRO1_CS_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
