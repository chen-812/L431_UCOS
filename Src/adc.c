/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.c
  * @brief   This file provides code for the configuration
  *          of the ADC instances.
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
/* Includes ------------------------------------------------------------------*/
#include "adc.h"

/* USER CODE BEGIN 0 */
#include "i2c.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define ADC_FULL_SCALE          4095.0f
#define ADC_SAMPLE_COUNT        16U
#define OLED_WIDTH              128U
#define OLED_PAGES              8U
#define OLED_ADDRESS            (LIGHT_OLED_I2C_ADDRESS << 1)

static uint8_t oled_buffer[OLED_WIDTH * OLED_PAGES];

static HAL_StatusTypeDef OLED_WriteCommands(const uint8_t *commands, uint16_t size)
{
  uint8_t packet[32];

  while (size > 0U)
  {
    uint16_t count = (size > (sizeof(packet) - 1U)) ?
                     (sizeof(packet) - 1U) : size;
    packet[0] = 0x00U;
    memcpy(&packet[1], commands, count);
    if (HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS, packet,
                                count + 1U, 100U) != HAL_OK)
    {
      return HAL_ERROR;
    }
    commands += count;
    size -= count;
  }
  return HAL_OK;
}

static HAL_StatusTypeDef OLED_Update(void)
{
  uint8_t page;
  uint8_t packet[OLED_WIDTH + 1U];

  packet[0] = 0x40U;
  for (page = 0U; page < OLED_PAGES; ++page)
  {
    uint8_t commands[] = {(uint8_t)(0xB0U + page), 0x00U, 0x10U};
    if (OLED_WriteCommands(commands, sizeof(commands)) != HAL_OK)
      return HAL_ERROR;
    memcpy(&packet[1], &oled_buffer[page * OLED_WIDTH], OLED_WIDTH);
    if (HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS, packet,
                                sizeof(packet), 100U) != HAL_OK)
      return HAL_ERROR;
  }
  return HAL_OK;
}

/* Compact 5x7 glyph set needed by "LIGHT", "ADC" and "LUX". */
static void OLED_GetGlyph(char c, uint8_t glyph[5])
{
  static const uint8_t digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E}
  };
  uint8_t i;
  memset(glyph, 0, 5U);
  if ((c >= '0') && (c <= '9')) { memcpy(glyph, digits[c-'0'], 5U); return; }
  switch (c)
  {
    case 'A': {const uint8_t g[5]={0x7E,0x11,0x11,0x11,0x7E}; memcpy(glyph,g,5); break;}
    case 'C': {const uint8_t g[5]={0x3E,0x41,0x41,0x41,0x22}; memcpy(glyph,g,5); break;}
    case 'D': {const uint8_t g[5]={0x7F,0x41,0x41,0x22,0x1C}; memcpy(glyph,g,5); break;}
    case 'G': {const uint8_t g[5]={0x3E,0x41,0x49,0x49,0x7A}; memcpy(glyph,g,5); break;}
    case 'H': {const uint8_t g[5]={0x7F,0x08,0x08,0x08,0x7F}; memcpy(glyph,g,5); break;}
    case 'I': {const uint8_t g[5]={0x00,0x41,0x7F,0x41,0x00}; memcpy(glyph,g,5); break;}
    case 'L': {const uint8_t g[5]={0x7F,0x40,0x40,0x40,0x40}; memcpy(glyph,g,5); break;}
    case 'T': {const uint8_t g[5]={0x01,0x01,0x7F,0x01,0x01}; memcpy(glyph,g,5); break;}
    case 'U': {const uint8_t g[5]={0x3F,0x40,0x40,0x40,0x3F}; memcpy(glyph,g,5); break;}
    case 'X': {const uint8_t g[5]={0x63,0x14,0x08,0x14,0x63}; memcpy(glyph,g,5); break;}
    case ':': glyph[1]=0x36; glyph[3]=0x36; break;
    case '.': glyph[2]=0x60; break;
    case '-': for (i=0; i<5U; ++i) glyph[i]=0x08; break;
    default: break;
  }
}

static void OLED_DrawString(uint8_t x, uint8_t page, const char *text)
{
  while ((*text != '\0') && (x <= (OLED_WIDTH - 6U)) && (page < OLED_PAGES))
  {
    uint8_t glyph[5];
    OLED_GetGlyph(*text++, glyph);
    memcpy(&oled_buffer[(page * OLED_WIDTH) + x], glyph, 5U);
    oled_buffer[(page * OLED_WIDTH) + x + 5U] = 0U;
    x += 6U;
  }
}

static HAL_StatusTypeDef OLED_Init(void)
{
  static const uint8_t init_commands[] = {
    0xAE,0xD5,0x80,0xA8,0x3F,0xD3,0x00,0x40,0x8D,0x14,
    0x20,0x02,0xA1,0xC8,0xDA,0x12,0x81,0xCF,0xD9,0xF1,
    0xDB,0x40,0xA4,0xA6,0xAF
  };
  memset(oled_buffer, 0, sizeof(oled_buffer));
  if (HAL_I2C_IsDeviceReady(&hi2c1, OLED_ADDRESS, 2U, 100U) != HAL_OK)
    return HAL_ERROR;
  if (OLED_WriteCommands(init_commands, sizeof(init_commands)) != HAL_OK)
    return HAL_ERROR;
  return OLED_Update();
}

/* USER CODE END 0 */

ADC_HandleTypeDef hadc1;

/* ADC1 init function */
void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_9;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspInit 0 */

  /* USER CODE END ADC1_MspInit 0 */
    /* ADC1 clock enable */
    __HAL_RCC_ADC_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**ADC1 GPIO Configuration
    PA4     ------> ADC1_IN9
    */
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN ADC1_MspInit 1 */

  /* USER CODE END ADC1_MspInit 1 */
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle)
{

  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspDeInit 0 */

  /* USER CODE END ADC1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_ADC_CLK_DISABLE();

    /**ADC1 GPIO Configuration
    PA4     ------> ADC1_IN9
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_4);

  /* USER CODE BEGIN ADC1_MspDeInit 1 */

  /* USER CODE END ADC1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

HAL_StatusTypeDef LightSensor_Init(void)
{
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
    return HAL_ERROR;
  return HAL_OK;
}

uint16_t LightSensor_ReadRaw(void)
{
  uint32_t sum = 0U;
  uint32_t valid = 0U;
  uint32_t i;

  for (i = 0U; i < ADC_SAMPLE_COUNT; ++i)
  {
    if (HAL_ADC_Start(&hadc1) == HAL_OK)
    {
      if (HAL_ADC_PollForConversion(&hadc1, 10U) == HAL_OK)
      {
        sum += HAL_ADC_GetValue(&hadc1);
        ++valid;
      }
      (void)HAL_ADC_Stop(&hadc1);
    }
  }
  return (valid == 0U) ? 0U : (uint16_t)(sum / valid);
}

float LightSensor_ConvertToLux(uint16_t adc_raw)
{
  float raw = (float)adc_raw;
  float ldr_resistance;

  /* LDR--3.3 V, fixed resistor--GND: Rldr = Rfixed * (4095-ADC)/ADC. */
  if (raw < 1.0f) return 0.0f;
  if (raw >= ADC_FULL_SCALE) raw = ADC_FULL_SCALE - 1.0f;
  ldr_resistance = LIGHT_DIVIDER_RESISTOR_OHM *
                   (ADC_FULL_SCALE - raw) / raw;

  /* LDR model: R = R10 * (10 lux / lux)^gamma. */
  return 10.0f * powf(LIGHT_LDR_R10_OHM / ldr_resistance,
                      1.0f / LIGHT_LDR_GAMMA);
}

float LightSensor_ReadLux(void)
{
  return LightSensor_ConvertToLux(LightSensor_ReadRaw());
}

HAL_StatusTypeDef LightSensor_Display(float lux, uint16_t adc_raw)
{
  char line[22];
  uint32_t lux_x10;
  if (lux > 99999.0f) lux = 99999.0f;
  if (lux < 0.0f) lux = 0.0f;
  lux_x10 = (uint32_t)(lux * 10.0f + 0.5f);
  memset(oled_buffer, 0, sizeof(oled_buffer));
  OLED_DrawString(0U, 1U, "LIGHT:");
  (void)snprintf(line, sizeof(line), "%lu.%lu LUX",
                 (unsigned long)(lux_x10 / 10U),
                 (unsigned long)(lux_x10 % 10U));
  OLED_DrawString(0U, 3U, line);
  (void)snprintf(line, sizeof(line), "ADC:%4u", (unsigned int)adc_raw);
  OLED_DrawString(0U, 5U, line);
  return OLED_Update();
}

/* USER CODE END 1 */
