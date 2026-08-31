/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "adc.h"
#include "fatfs.h"
#include "i2c.h"
#include "rtc.h"
#include "sdmmc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "os.h"
#include "cpu.h"
#include "OLED.h"
#include "bmp280.h"
#include "Timer.h"
#include "TF.h"
#include "key.h"
#include "EEPROM.h"
#include "REMOTE.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BUZZER_FORCE_TEST            0U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
OS_TCB ADCTaskTCB;

CPU_STK ADCTaskStk[256];

OS_TCB OLEDTaskTCB;
CPU_STK OLEDTaskStk[256];

OS_TCB SegmentTaskTCB;
CPU_STK SegmentTaskStk[256];

OS_TCB TFTaskTCB;
CPU_STK TFTaskStk[256];

OS_TCB KeyTaskTCB;
CPU_STK KeyTaskStk[256];


static volatile uint16_t LatestAdcRaw = 0U;
static volatile float LatestLightLux = 0.0f;
static volatile float LatestTemperatureC = 0.0f;
static volatile CPU_BOOLEAN TemperatureValid = DEF_FALSE;
static volatile CPU_BOOLEAN DisplayKey1Page = DEF_FALSE;
static volatile CPU_BOOLEAN EnvironmentDisplayDirty = DEF_FALSE;
static volatile float BuzzerLightThresholdLux =
    BUZZER_LIGHT_THRESHOLD_DEFAULT_LUX;

void ADCTask(void *p_arg);
void OLEDTask(void *p_arg);
void SegmentTask(void *p_arg);
void TFTask(void *p_arg);
void KeyTask(void *p_arg);
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */
void ADCTask(void *p_arg)
{
    OS_ERR err;
    CPU_BOOLEAN buzzer_on = DEF_FALSE;

    (void)p_arg;

    /* Initialize the uC/OS tick in the first/highest-priority app task. */
    OS_CPU_SysTickInitFreq(SystemCoreClock);

    if (LightSensor_Init() != HAL_OK)
    {
        printf("ADC init failed\r\n");
    }

#if BUZZER_FORCE_TEST
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) == HAL_OK)
    {
        buzzer_on = DEF_TRUE;
        printf("Buzzer force test: PWM started, BEEP=ON\r\n");
    }
    else
    {
        printf("Buzzer force test: PWM start failed, BEEP=OFF\r\n");
    }
#endif

    while(1)
    {
        uint16_t adc_raw = LightSensor_ReadRaw();
        float lux = LightSensor_ConvertToLux(adc_raw);
        LatestAdcRaw = adc_raw;
        LatestLightLux = lux;

#if !BUZZER_FORCE_TEST
        if ((lux > BuzzerLightThresholdLux) &&
            (buzzer_on == DEF_FALSE))
        {
            if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) == HAL_OK)
                buzzer_on = DEF_TRUE;
            else
                printf("Buzzer PWM start failed\r\n");
        }
        else if ((lux <= BuzzerLightThresholdLux) &&
                 (buzzer_on == DEF_TRUE))
        {
            if (HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_4) == HAL_OK)
                buzzer_on = DEF_FALSE;
            else
                printf("Buzzer PWM stop failed\r\n");
        }
#endif

        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_1);
        printf("ADC=%u, Light=%lu.%lu lux, BEEP=%s\r\n", adc_raw,
               (unsigned long)((uint32_t)(lux * 10.0f) / 10U),
               (unsigned long)((uint32_t)(lux * 10.0f) % 10U),
               (buzzer_on == DEF_TRUE) ? "ON" : "OFF");

        OSTimeDly(OSCfg_TickRate_Hz, OS_OPT_TIME_DLY, &err);
        if (err != OS_ERR_NONE)
        {
            printf("ADCTask delay error=%u\r\n", (unsigned int)err);
        }
    }
}

void OLEDTask(void *p_arg)
{
    OS_ERR err;
    float temperature_c = 0.0f;
    float pressure_pa = 0.0f;
    CPU_BOOLEAN bmp280_ready = DEF_FALSE;
    CPU_BOOLEAN eeprom_ready = DEF_FALSE;
    CPU_BOOLEAN previous_key_page = DEF_FALSE;
    uint8_t animation_frame = 0U;
    uint8_t sensor_ticks = 10U;
    uint8_t eeprom_sample_count = 10U;
    (void)p_arg;

    if (OLED_Init() != HAL_OK)
        printf("OLED init failed\r\n");

    if (EEPROM_Init(&hi2c1) == HAL_OK)
    {
        eeprom_ready = DEF_TRUE;
        if (EEPROM_LoadTemperature(&temperature_c) == HAL_OK)
        {
            LatestTemperatureC = temperature_c;
            TemperatureValid = DEF_TRUE;
            printf("EEPROM restored temperature=%ld.%02ld C\r\n",
                   (long)((int32_t)(temperature_c * 100.0f) / 100),
                   (long)((int32_t)(temperature_c * 100.0f) >= 0 ?
                          ((int32_t)(temperature_c * 100.0f) % 100) :
                          -((int32_t)(temperature_c * 100.0f) % 100)));
        }
        else
        {
            printf("EEPROM ready, no saved temperature\r\n");
        }
    }
    else
    {
        uint8_t address;
        printf("EEPROM not detected on I2C1 address 0x50\r\n");
        printf("I2C1 scan:");
        for (address = 0x03U; address <= 0x77U; ++address)
        {
            if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)address << 1U,
                                      1U, 10U) == HAL_OK)
            {
                printf(" 0x%02X", (unsigned int)address);
            }
        }
        printf("\r\n");
    }

    if (BMP280_Init(&hi2c2) == HAL_OK)
        bmp280_ready = DEF_TRUE;
    else
        printf("BMP280 init failed on I2C2 (PB10/PB11)\r\n");

    while (1)
    {
        CPU_BOOLEAN key_page = DisplayKey1Page;
        CPU_BOOLEAN environment_update = DEF_FALSE;
        float threshold_lux = BuzzerLightThresholdLux;

        /* Take a local snapshot so ADC task may continue sampling. */
        float lux = LatestLightLux;

        /* The task runs every 100 ms; update the BMP280 about once a second. */
        if (sensor_ticks >= 10U)
        {
            sensor_ticks = 0U;
            environment_update = DEF_TRUE;

            if (bmp280_ready == DEF_TRUE)
            {
                if (BMP280_ReadMeasurements(&hi2c2, &temperature_c,
                                            &pressure_pa) != HAL_OK)
                {
                    printf("BMP280 read failed, error=%u, i2c=%u\r\n",
                           (unsigned int)bmp280_last_error,
                           (unsigned int)HAL_I2C_GetError(&hi2c2));
                }
                else
                {
                    LatestTemperatureC = temperature_c;
                    TemperatureValid = DEF_TRUE;
                    printf("Temperature=%ld.%ld C, Pressure=%lu.%lu hPa\r\n",
                           (long)((int32_t)(temperature_c * 10.0f) / 10),
                           (long)((int32_t)(temperature_c * 10.0f) % 10),
                           (unsigned long)((uint32_t)(pressure_pa / 10.0f) / 10U),
                           (unsigned long)((uint32_t)(pressure_pa / 10.0f) % 10U));

                    if (eeprom_ready == DEF_TRUE)
                    {
                        if (eeprom_sample_count >= 10U)
                        {
                            eeprom_sample_count = 0U;
                            if (EEPROM_SaveTemperature(temperature_c) == HAL_OK)
                                printf("EEPROM temperature saved\r\n");
                            else
                                printf("EEPROM temperature save failed, i2c=%u\r\n",
                                       (unsigned int)HAL_I2C_GetError(&hi2c1));
                        }
                        else
                        {
                            eeprom_sample_count++;
                        }
                    }
                }
            }
        }
        else
        {
            sensor_ticks++;
        }

        if (key_page == DEF_TRUE)
        {
            if (previous_key_page == DEF_FALSE)
                animation_frame = 0U;

            (void)OLED_DisplayAnimationFrame(animation_frame);
            animation_frame++;
            if (animation_frame >= OLED_GetAnimationFrameCount())
                animation_frame = 0U;
        }
        else if ((previous_key_page == DEF_TRUE) ||
                 (environment_update == DEF_TRUE) ||
                 (EnvironmentDisplayDirty == DEF_TRUE))
        {
            (void)OLED_DisplayEnvironment(lux, temperature_c, pressure_pa,
                                          threshold_lux);
            EnvironmentDisplayDirty = DEF_FALSE;
        }

        previous_key_page = key_page;
        OSTimeDly((OSCfg_TickRate_Hz + 9U) / 10U,
                  OS_OPT_TIME_DLY, &err);
    }
}

void KeyTask(void *p_arg)
{
    OS_ERR err;
    (void)p_arg;

    KEY_Init();

    while (1)
    {
        KEY_Value_t key = KEY_DebounceProcess(10U);

        if (key == KEY_VALUE_ROW1_COL4)
        {
            DisplayKey1Page = (DisplayKey1Page == DEF_FALSE) ?
                              DEF_TRUE : DEF_FALSE;
            printf("KEY1_1 pressed, OLED=%s\r\n",
                   (DisplayKey1Page == DEF_TRUE) ? "KEY PAGE" : "ENV PAGE");
        }
        else if (key == KEY_VALUE_ROW2_COL4)
        {
            BuzzerLightThresholdLux += 10.0f;
            DisplayKey1Page = DEF_FALSE;
            EnvironmentDisplayDirty = DEF_TRUE;
            printf("KEY2_1 pressed, threshold=%lu lux\r\n",
                   (unsigned long)BuzzerLightThresholdLux);
        }

        OSTimeDly((OSCfg_TickRate_Hz + 99U) / 100U,
                  OS_OPT_TIME_DLY, &err);
    }
}

void TFTask(void *p_arg)
{
    OS_ERR err;
    const char *filename = "0:/temperature.csv";
    uint32_t elapsed_seconds = 0U;
    uint32_t records_written = 0U;
    uint32_t mount_attempts = 1U;
    CPU_BOOLEAN tf_ready = DEF_FALSE;
    (void)p_arg;

    printf("TF task started, drive=%s\r\n", SDPath);

    if (TF_Init() != 0U)
    {
        tf_ready = DEF_TRUE;
        printf("TF mounted successfully\r\n");
        /* A repeated header is harmless and clearly marks each power-up. */
        if (TF_Append(filename, "elapsed_s,temperature_c\r\n") == 0U)
            printf("TF header write failed, FatFs error=%u\r\n",
                   (unsigned int)TF_GetLastResult());
    }
    else
        printf("TF mount failed, FatFs=%u, HAL_SD state=%u, error=0x%08lX\r\n",
               (unsigned int)TF_GetLastResult(),
               (unsigned int)HAL_SD_GetState(&hsd1),
               (unsigned long)HAL_SD_GetError(&hsd1));
        printf("SD diag: clk=%luHz type=%lu ver=%lu rca=0x%04lX "
               "STA=0x%08lX RESP1=0x%08lX CLKCR=0x%08lX\r\n",
               (unsigned long)HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC1),
               (unsigned long)hsd1.SdCard.CardType,
               (unsigned long)hsd1.SdCard.CardVersion,
               (unsigned long)hsd1.SdCard.RelCardAdd,
               (unsigned long)hsd1.Instance->STA,
               (unsigned long)hsd1.Instance->RESP1,
               (unsigned long)hsd1.Instance->CLKCR);
        printf("SD pins: PC_IDR=0x%04lX PD_IDR=0x%04lX "
               "PC_AFRH=0x%08lX PD_AFRL=0x%08lX\r\n",
               (unsigned long)(GPIOC->IDR & 0xFFFFU),
               (unsigned long)(GPIOD->IDR & 0xFFFFU),
               (unsigned long)GPIOC->AFR[1],
               (unsigned long)GPIOD->AFR[0]);

    while (1)
    {
        /* Allow the system to recover if the card was inserted after boot. */
        if (tf_ready == DEF_FALSE)
        {
            mount_attempts++;
            if (TF_Init() != 0U)
            {
                tf_ready = DEF_TRUE;
                printf("TF recovered and mounted on attempt %lu\r\n",
                       (unsigned long)mount_attempts);
                (void)TF_Append(filename, "elapsed_s,temperature_c\r\n");
            }
            else
            {
                printf("TF still not ready, attempt=%lu, FatFs=%u, "
                       "state=%u, HAL error=0x%08lX\r\n",
                       (unsigned long)mount_attempts,
                       (unsigned int)TF_GetLastResult(),
                       (unsigned int)HAL_SD_GetState(&hsd1),
                       (unsigned long)HAL_SD_GetError(&hsd1));
                printf("SD retry diag: type=%lu ver=%lu rca=0x%04lX "
                       "blocks=%lu block_size=%lu RESP1=0x%08lX "
                       "STA=0x%08lX CLKCR=0x%08lX\r\n",
                       (unsigned long)hsd1.SdCard.CardType,
                       (unsigned long)hsd1.SdCard.CardVersion,
                       (unsigned long)hsd1.SdCard.RelCardAdd,
                       (unsigned long)hsd1.SdCard.LogBlockNbr,
                       (unsigned long)hsd1.SdCard.LogBlockSize,
                       (unsigned long)hsd1.Instance->RESP1,
                       (unsigned long)hsd1.Instance->STA,
                       (unsigned long)hsd1.Instance->CLKCR);
            }
        }

        if ((tf_ready == DEF_TRUE) && (TemperatureValid == DEF_TRUE))
        {
            float temperature_c = LatestTemperatureC;
            if (TF_AppendTemperature(filename, elapsed_seconds,
                                     temperature_c) == 0U)
            {
                tf_ready = DEF_FALSE;
                printf("TF temperature write failed, FatFs error=%u\r\n",
                       (unsigned int)TF_GetLastResult());
            }
            else
            {
                records_written++;
                if ((records_written == 1U) ||
                    ((records_written % 10U) == 0U))
                {
                    printf("TF wrote %lu record(s) to %s\r\n",
                           (unsigned long)records_written, filename);
                }
            }
        }

        elapsed_seconds++;
        OSTimeDly(OSCfg_TickRate_Hz, OS_OPT_TIME_DLY, &err);
    }
}

void SegmentTask(void *p_arg)
{
    OS_ERR err;
    uint32_t scan_count;
    uint32_t task_ticks = 0U;
    uint8_t b[8], month, day, hour, minute, second;
    (void)p_arg;

    if (SEG_Init() != HAL_OK)
        printf("7-segment/RTC init failed\r\n");

    while (1)
    {
        REMOTE_Data_t remote_data;

        if (REMOTE_GetData(&remote_data) && remote_data.valid)
        {
            uint8_t digit;
            if (REMOTE_CommandToDigit(remote_data.command, &digit))
                SEG_SetRemoteDigit(digit);
            if (!remote_data.is_repeat)
            {
                printf("REMOTE raw=0x%08lX address=0x%04X command=0x%02X\r\n",
                       (unsigned long)remote_data.raw_data,
                       (unsigned int)remote_data.address,
                       (unsigned int)remote_data.command);
            }
        }

        if (task_ticks >= 50U)
        {
            task_ticks = 0U;
            if (RTC_Read_Time() != HAL_OK)
                printf("RTC read failed\r\n");
            SEG_GetDiagnostics(&scan_count, b, &month, &day, &hour, &minute,
                               &second);
            printf("SEG scan=%lu RTC=%02u-%02u %02u:%02u:%02u "
                   "buf=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                   (unsigned long)scan_count, month, day, hour, minute, second,
                   b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
        }
        else
        {
            task_ticks++;
        }

        OSTimeDly((OSCfg_TickRate_Hz + 49U) / 50U,
                  OS_OPT_TIME_DLY, &err);
    }
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  MX_I2C2_Init();
  MX_SDMMC1_SD_Init();
  MX_FATFS_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  OS_ERR err;

  REMOTE_Init();

  printf("FatFs driver link: ret=%u, path=%s\r\n",
         (unsigned int)retSD, SDPath);


  /* 初始化uCOS */
  OSInit(&err);


  /* 创建第一个任务 */
  OSTaskCreate(
      &ADCTaskTCB,
      "ADC Task",
      ADCTask,
      0,
      2,
      &ADCTaskStk[0],
      256/10,
      256,
      0,
      0,
      0,
      OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
      &err
  );

  OSTaskCreate(
      &OLEDTaskTCB,
      "OLED Task",
      OLEDTask,
      0,
      3,
      &OLEDTaskStk[0],
      256/10,
      256,
      0,
      0,
      0,
      OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
      &err
  );

  OSTaskCreate(
      &SegmentTaskTCB,
      "Segment Task",
      SegmentTask,
      0,
      4,
      &SegmentTaskStk[0],
      256/10,
      256,
      0,
      0,
      0,
      OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
      &err
  );

  OSTaskCreate(
      &TFTaskTCB,
      "TF Task",
      TFTask,
      0,
      5,
      &TFTaskStk[0],
      256/10,
      256,
      0,
      0,
      0,
      OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
      &err
  );
  if (err != OS_ERR_NONE)
      printf("TF task create failed, OS error=%u\r\n", (unsigned int)err);

  OSTaskCreate(
      &KeyTaskTCB,
      "Key Task",
      KeyTask,
      0,
      6,
      &KeyTaskStk[0],
      256/10,
      256,
      0,
      0,
      0,
      OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
      &err
  );
  if (err != OS_ERR_NONE)
      printf("Key task create failed, OS error=%u\r\n", (unsigned int)err);



  /* 启动调度器 */
  OSStart(&err);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 9;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_SDMMC1|RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_PLLSAI1;
  PeriphClkInit.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_PLLSAI1;
  PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_HSI;
  PeriphClkInit.PLLSAI1.PLLSAI1M = 1;
  PeriphClkInit.PLLSAI1.PLLSAI1N = 8;
  PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV7;
  PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV4;
  PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_48M2CLK|RCC_PLLSAI1_ADC1CLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
