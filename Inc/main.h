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
#include "stm32l4xx_hal.h"

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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define KEY_H1_Pin GPIO_PIN_0
#define KEY_H1_GPIO_Port GPIOB
#define KEY_L1_Pin GPIO_PIN_14
#define KEY_L1_GPIO_Port GPIOB
#define REMOTE_Pin GPIO_PIN_8
#define REMOTE_GPIO_Port GPIOA
#define REMOTE_EXTI_IRQn EXTI9_5_IRQn
#define BEEP_Pin GPIO_PIN_11
#define BEEP_GPIO_Port GPIOA
#define HC595_DATA_Pin GPIO_PIN_15
#define HC595_DATA_GPIO_Port GPIOA
#define HC595_LCLK_Pin GPIO_PIN_3
#define HC595_LCLK_GPIO_Port GPIOB
#define HC595_HCLK_Pin GPIO_PIN_4
#define HC595_HCLK_GPIO_Port GPIOB
#define HC138_A2_Pin GPIO_PIN_5
#define HC138_A2_GPIO_Port GPIOB
#define HC138_A1_Pin GPIO_PIN_6
#define HC138_A1_GPIO_Port GPIOB
#define HC138_A0_Pin GPIO_PIN_7
#define HC138_A0_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define BUZZER_LIGHT_THRESHOLD_DEFAULT_LUX  100.0f

#define KEY_H1_Pin GPIO_PIN_0
#define KEY_H1_GPIO_Port GPIOB
#define KEY_H2_Pin GPIO_PIN_1
#define KEY_H2_GPIO_Port GPIOB
#define KEY_L1_Pin GPIO_PIN_14
#define KEY_L1_GPIO_Port GPIOB

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
