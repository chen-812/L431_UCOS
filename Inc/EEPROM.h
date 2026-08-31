#ifndef EEPROM_H
#define EEPROM_H

#include "main.h"
#include "i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* AT24C16: A0/A1/A2 are not address pins; the upper memory address bits are
   carried in the I2C device address. */
HAL_StatusTypeDef EEPROM_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef EEPROM_LoadTemperature(float *temperature_c);
HAL_StatusTypeDef EEPROM_SaveTemperature(float temperature_c);

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_H */
