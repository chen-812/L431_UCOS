/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
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
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern ADC_HandleTypeDef hadc1;

/* USER CODE BEGIN Private defines */

/* SSD1306 OLED 7-bit address. Change to 0x3D if your module uses that address. */
#define LIGHT_OLED_I2C_ADDRESS       0x3CU

/* Photoresistor divider parameters (LDR to 3.3 V, resistor to GND). */
#define LIGHT_DIVIDER_RESISTOR_OHM   10000.0f
#define LIGHT_LDR_R10_OHM            10000.0f
#define LIGHT_LDR_GAMMA              0.70f

/* USER CODE END Private defines */

void MX_ADC1_Init(void);

/* USER CODE BEGIN Prototypes */

HAL_StatusTypeDef LightSensor_Init(void);
uint16_t          LightSensor_ReadRaw(void);
float             LightSensor_ConvertToLux(uint16_t adc_raw);
float             LightSensor_ReadLux(void);
HAL_StatusTypeDef LightSensor_Display(float lux, uint16_t adc_raw);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

