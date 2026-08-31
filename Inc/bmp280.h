#ifndef __BMP280_H
#define __BMP280_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"

#define BMP280_I2C_ADDR          (0x76U << 1)
#define BMP280_CHIP_ID_REG       0xD0U
#define BMP280_CTRL_MEAS_REG     0xF4U
#define BMP280_CONFIG_REG        0xF5U
#define BMP280_STATUS_REG        0xF3U
#define BMP280_DATA_REG          0xF7U

typedef struct
{
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    int32_t  t_fine;
} BMP280_Calib;

extern BMP280_Calib bmp280_calib;
extern volatile uint8_t bmp280_last_error;

#define BMP280_ERROR_NONE        0U
#define BMP280_ERROR_TRIGGER     1U
#define BMP280_ERROR_STATUS      2U
#define BMP280_ERROR_TIMEOUT     3U
#define BMP280_ERROR_DATA_READ   4U
#define BMP280_ERROR_RAW_DATA    5U

HAL_StatusTypeDef BMP280_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef BMP280_ReadMeasurements(I2C_HandleTypeDef *hi2c,
                                           float *temperature_c,
                                           float *pressure_pa);
float BMP280_ReadTemperature(I2C_HandleTypeDef *hi2c);
float BMP280_ReadPressure(I2C_HandleTypeDef *hi2c);

#ifdef __cplusplus
}
#endif

#endif
