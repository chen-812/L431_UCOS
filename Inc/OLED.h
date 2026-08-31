#ifndef __OLED_H__
#define __OLED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "i2c.h"

/* HAL expects the 7-bit SSD1306 address shifted left by one bit. */
#define OLED_I2C_ADDR  (0x3CU << 1)

HAL_StatusTypeDef OLED_Init(void);
void              OLED_Clear(void);
void              OLED_SetPos(uint8_t x, uint8_t y);
void              OLED_ShowChar(uint8_t x, uint8_t y, char chr);
void              OLED_ShowString(uint8_t x, uint8_t y, char *str);
HAL_StatusTypeDef OLED_DisplayLight(float lux, uint16_t adc_raw);
HAL_StatusTypeDef OLED_DisplayEnvironment(float lux, float temperature_c,
                                           float pressure_pa,
                                           float threshold_lux);
HAL_StatusTypeDef OLED_DisplayAnimationFrame(uint8_t frame_index);
uint8_t           OLED_GetAnimationFrameCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_H__ */
