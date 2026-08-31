#include "bmp280.h"

BMP280_Calib bmp280_calib;
volatile uint8_t bmp280_last_error = BMP280_ERROR_NONE;

static HAL_StatusTypeDef BMP280_ReadReg(I2C_HandleTypeDef *hi2c,
                                        uint8_t reg, uint8_t *data,
                                        uint16_t len)
{
    return HAL_I2C_Mem_Read(hi2c, BMP280_I2C_ADDR, reg,
                            I2C_MEMADD_SIZE_8BIT, data, len, 100U);
}

static HAL_StatusTypeDef BMP280_WriteReg(I2C_HandleTypeDef *hi2c,
                                         uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(hi2c, BMP280_I2C_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, &data, 1U, 100U);
}

static uint16_t BMP280_U16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[1] << 8) | data[0]);
}

static int16_t BMP280_S16(const uint8_t *data)
{
    return (int16_t)BMP280_U16(data);
}

static HAL_StatusTypeDef BMP280_ReadCalibration(I2C_HandleTypeDef *hi2c)
{
    uint8_t b[24];
    if (BMP280_ReadReg(hi2c, 0x88U, b, sizeof(b)) != HAL_OK)
        return HAL_ERROR;

    bmp280_calib.dig_T1 = BMP280_U16(&b[0]);
    bmp280_calib.dig_T2 = BMP280_S16(&b[2]);
    bmp280_calib.dig_T3 = BMP280_S16(&b[4]);
    bmp280_calib.dig_P1 = BMP280_U16(&b[6]);
    bmp280_calib.dig_P2 = BMP280_S16(&b[8]);
    bmp280_calib.dig_P3 = BMP280_S16(&b[10]);
    bmp280_calib.dig_P4 = BMP280_S16(&b[12]);
    bmp280_calib.dig_P5 = BMP280_S16(&b[14]);
    bmp280_calib.dig_P6 = BMP280_S16(&b[16]);
    bmp280_calib.dig_P7 = BMP280_S16(&b[18]);
    bmp280_calib.dig_P8 = BMP280_S16(&b[20]);
    bmp280_calib.dig_P9 = BMP280_S16(&b[22]);
    return (bmp280_calib.dig_P1 == 0U) ? HAL_ERROR : HAL_OK;
}

HAL_StatusTypeDef BMP280_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t id = 0U;

    if ((hi2c == NULL) ||
        (HAL_I2C_IsDeviceReady(hi2c, BMP280_I2C_ADDR, 3U, 100U) != HAL_OK) ||
        (BMP280_ReadReg(hi2c, BMP280_CHIP_ID_REG, &id, 1U) != HAL_OK) ||
        (id != 0x58U) ||
        (BMP280_ReadCalibration(hi2c) != HAL_OK))
        return HAL_ERROR;

    /* Configure in sleep mode. Each read explicitly starts a forced
       measurement, which avoids normal-mode standby timing ambiguity. */
    if (BMP280_WriteReg(hi2c, BMP280_CTRL_MEAS_REG, 0x24U) != HAL_OK)
        return HAL_ERROR;
    if (BMP280_WriteReg(hi2c, BMP280_CONFIG_REG, 0x00U) != HAL_OK)
        return HAL_ERROR;
    bmp280_last_error = BMP280_ERROR_NONE;
    return HAL_OK;
}

HAL_StatusTypeDef BMP280_ReadMeasurements(I2C_HandleTypeDef *hi2c,
                                           float *temperature_c,
                                           float *pressure_pa)
{
    uint8_t data[6];
    uint8_t status = 0U;
    uint32_t wait_count;
    int32_t adc_t, adc_p, var1_t, var2_t;
    int64_t var1_p, var2_p, p;

    if ((hi2c == NULL) || (temperature_c == NULL) || (pressure_pa == NULL))
        return HAL_ERROR;

    /* osrs_t x1, osrs_p x1, forced mode. */
    if (BMP280_WriteReg(hi2c, BMP280_CTRL_MEAS_REG, 0x25U) != HAL_OK)
    {
        bmp280_last_error = BMP280_ERROR_TRIGGER;
        return HAL_ERROR;
    }

    /* Do not poll immediately: the measuring bit may not be set yet while
       the forced conversion is starting. x1/x1 completes in about 7 ms. */
    HAL_Delay(10U);

    /* If conversion is still active, allow up to another 50 ms. */
    for (wait_count = 0U; wait_count < 50U; ++wait_count)
    {
        if (BMP280_ReadReg(hi2c, BMP280_STATUS_REG, &status, 1U) != HAL_OK)
        {
            bmp280_last_error = BMP280_ERROR_STATUS;
            return HAL_ERROR;
        }
        if ((status & 0x08U) == 0U)
            break;
        HAL_Delay(1U);
    }
    if ((status & 0x08U) != 0U)
    {
        bmp280_last_error = BMP280_ERROR_TIMEOUT;
        return HAL_ERROR;
    }

    if (BMP280_ReadReg(hi2c, BMP280_DATA_REG, data, sizeof(data)) != HAL_OK)
    {
        bmp280_last_error = BMP280_ERROR_DATA_READ;
        return HAL_ERROR;
    }

    adc_p = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) |
            ((int32_t)data[2] >> 4);
    adc_t = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) |
            ((int32_t)data[5] >> 4);
    if ((adc_t == 0x80000L) || (adc_p == 0x80000L))
    {
        bmp280_last_error = BMP280_ERROR_RAW_DATA;
        return HAL_ERROR;
    }

    var1_t = ((((adc_t >> 3) - ((int32_t)bmp280_calib.dig_T1 << 1))) *
              (int32_t)bmp280_calib.dig_T2) >> 11;
    var2_t = (((((adc_t >> 4) - (int32_t)bmp280_calib.dig_T1) *
                ((adc_t >> 4) - (int32_t)bmp280_calib.dig_T1)) >> 12) *
              (int32_t)bmp280_calib.dig_T3) >> 14;
    bmp280_calib.t_fine = var1_t + var2_t;
    *temperature_c = (float)((bmp280_calib.t_fine * 5 + 128) >> 8) / 100.0f;

    var1_p = (int64_t)bmp280_calib.t_fine - 128000;
    var2_p = var1_p * var1_p * (int64_t)bmp280_calib.dig_P6;
    var2_p += (var1_p * (int64_t)bmp280_calib.dig_P5) << 17;
    var2_p += ((int64_t)bmp280_calib.dig_P4) << 35;
    var1_p = ((var1_p * var1_p * (int64_t)bmp280_calib.dig_P3) >> 8) +
             ((var1_p * (int64_t)bmp280_calib.dig_P2) << 12);
    var1_p = (((((int64_t)1) << 47) + var1_p) *
              (int64_t)bmp280_calib.dig_P1) >> 33;
    if (var1_p == 0)
        return HAL_ERROR;

    p = 1048576 - adc_p;
    p = (((p << 31) - var2_p) * 3125) / var1_p;
    var1_p = ((int64_t)bmp280_calib.dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2_p = ((int64_t)bmp280_calib.dig_P8 * p) >> 19;
    p = ((p + var1_p + var2_p) >> 8) +
        ((int64_t)bmp280_calib.dig_P7 << 4);
    *pressure_pa = (float)p / 256.0f;
    bmp280_last_error = BMP280_ERROR_NONE;
    return HAL_OK;
}

float BMP280_ReadTemperature(I2C_HandleTypeDef *hi2c)
{
    float temperature = 0.0f, pressure = 0.0f;
    (void)BMP280_ReadMeasurements(hi2c, &temperature, &pressure);
    return temperature;
}

float BMP280_ReadPressure(I2C_HandleTypeDef *hi2c)
{
    float temperature = 0.0f, pressure = 0.0f;
    (void)BMP280_ReadMeasurements(hi2c, &temperature, &pressure);
    return pressure;
}
