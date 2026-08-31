/*
 * EEPROM.c
 *
 *  Created on: Jul 20, 2026
 *      Author: HP
 */

#include "EEPROM.h"

#include <string.h>

#define EEPROM_BASE_ADDR_7BIT  0x50U
#define EEPROM_TOTAL_BYTES     2048U
#define EEPROM_PAGE_BYTES      16U
#define EEPROM_RECORD_COUNT    (EEPROM_TOTAL_BYTES / EEPROM_PAGE_BYTES)
#define EEPROM_RECORD_MAGIC    0x544DU
#define EEPROM_WRITE_TIMEOUT   100U

static I2C_HandleTypeDef *eeprom_i2c;
static uint32_t latest_sequence;
static uint16_t latest_slot;
static int32_t latest_temperature_x100;
static uint8_t valid_record;

static uint16_t EEPROM_Crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;

    while (length-- > 0U)
    {
        crc ^= (uint16_t)(*data++) << 8U;
        for (i = 0U; i < 8U; ++i)
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1U) ^ 0x1021U) :
                                    (uint16_t)(crc << 1U);
    }
    return crc;
}

static uint16_t EEPROM_DeviceAddress(uint16_t memory_address)
{
    return (uint16_t)((EEPROM_BASE_ADDR_7BIT |
                      ((memory_address >> 8U) & 0x07U)) << 1U);
}

static HAL_StatusTypeDef EEPROM_ReadPage(uint16_t slot, uint8_t data[16])
{
    uint16_t address = (uint16_t)(slot * EEPROM_PAGE_BYTES);
    return HAL_I2C_Mem_Read(eeprom_i2c, EEPROM_DeviceAddress(address),
                            (uint8_t)address, I2C_MEMADD_SIZE_8BIT,
                            data, EEPROM_PAGE_BYTES, EEPROM_WRITE_TIMEOUT);
}

static HAL_StatusTypeDef EEPROM_WritePage(uint16_t slot,
                                          const uint8_t data[16])
{
    uint16_t address = (uint16_t)(slot * EEPROM_PAGE_BYTES);
    uint16_t device = EEPROM_DeviceAddress(address);
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Write(eeprom_i2c, device, (uint8_t)address,
                               I2C_MEMADD_SIZE_8BIT, (uint8_t *)data,
                               EEPROM_PAGE_BYTES, EEPROM_WRITE_TIMEOUT);
    if (status != HAL_OK)
        return status;

    /* ACK polling waits for the internal EEPROM write cycle to finish. */
    return HAL_I2C_IsDeviceReady(eeprom_i2c, device, 20U,
                                 EEPROM_WRITE_TIMEOUT);
}

static uint16_t EEPROM_GetU16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8U);
}

static uint32_t EEPROM_GetU32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static void EEPROM_PutU16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8U);
}

static void EEPROM_PutU32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8U);
    p[2] = (uint8_t)(value >> 16U);
    p[3] = (uint8_t)(value >> 24U);
}

HAL_StatusTypeDef EEPROM_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t data[EEPROM_PAGE_BYTES];
    uint16_t slot;

    if (hi2c == NULL)
        return HAL_ERROR;

    eeprom_i2c = hi2c;
    valid_record = 0U;

    if (HAL_I2C_IsDeviceReady(eeprom_i2c,
                              EEPROM_BASE_ADDR_7BIT << 1U,
                              3U, EEPROM_WRITE_TIMEOUT) != HAL_OK)
        return HAL_ERROR;

    for (slot = 0U; slot < EEPROM_RECORD_COUNT; ++slot)
    {
        uint32_t sequence;
        uint16_t stored_crc;

        if (EEPROM_ReadPage(slot, data) != HAL_OK)
            return HAL_ERROR;
        if (EEPROM_GetU16(&data[0]) != EEPROM_RECORD_MAGIC)
            continue;

        stored_crc = EEPROM_GetU16(&data[14]);
        if (EEPROM_Crc16(data, 14U) != stored_crc)
            continue;

        sequence = EEPROM_GetU32(&data[2]);
        if ((valid_record == 0U) ||
            ((int32_t)(sequence - latest_sequence) > 0))
        {
            latest_sequence = sequence;
            latest_slot = slot;
            latest_temperature_x100 = (int32_t)EEPROM_GetU32(&data[6]);
            valid_record = 1U;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef EEPROM_LoadTemperature(float *temperature_c)
{
    if ((temperature_c == NULL) || (valid_record == 0U))
        return HAL_ERROR;

    *temperature_c = (float)latest_temperature_x100 / 100.0f;
    return HAL_OK;
}

HAL_StatusTypeDef EEPROM_SaveTemperature(float temperature_c)
{
    uint8_t data[EEPROM_PAGE_BYTES];
    int32_t temperature_x100;
    uint16_t slot;
    uint32_t sequence;

    if (eeprom_i2c == NULL)
        return HAL_ERROR;

    temperature_x100 = (int32_t)(temperature_c * 100.0f +
                         ((temperature_c >= 0.0f) ? 0.5f : -0.5f));
    if ((valid_record != 0U) &&
        (temperature_x100 == latest_temperature_x100))
        return HAL_OK;

    slot = (valid_record != 0U) ?
           (uint16_t)((latest_slot + 1U) % EEPROM_RECORD_COUNT) : 0U;
    sequence = (valid_record != 0U) ? latest_sequence + 1U : 1U;

    memset(data, 0, sizeof(data));
    EEPROM_PutU16(&data[0], EEPROM_RECORD_MAGIC);
    EEPROM_PutU32(&data[2], sequence);
    EEPROM_PutU32(&data[6], (uint32_t)temperature_x100);
    EEPROM_PutU16(&data[14], EEPROM_Crc16(data, 14U));

    if (EEPROM_WritePage(slot, data) != HAL_OK)
        return HAL_ERROR;

    latest_slot = slot;
    latest_sequence = sequence;
    latest_temperature_x100 = temperature_x100;
    valid_record = 1U;
    return HAL_OK;
}


