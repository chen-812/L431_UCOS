#include "OLED.h"
#include "Showmp4.h"
#include <stdio.h>
#include <string.h>

#define OLED_WIDTH  128U
#define OLED_PAGES  8U

static uint8_t oled_buffer[OLED_WIDTH * OLED_PAGES];

static HAL_StatusTypeDef OLED_WriteCommands(const uint8_t *commands,
                                             uint16_t size)
{
    uint8_t packet[32];

    while (size > 0U)
    {
        uint16_t count = (size > (sizeof(packet) - 1U)) ?
                         (sizeof(packet) - 1U) : size;
        packet[0] = 0x00U;
        memcpy(&packet[1], commands, count);
        if (HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, packet,
                                    count + 1U, 100U) != HAL_OK)
            return HAL_ERROR;
        commands += count;
        size -= count;
    }
    return HAL_OK;
}

void OLED_SetPos(uint8_t x, uint8_t page)
{
    uint8_t commands[3] = {
        (uint8_t)(0xB0U + page),
        (uint8_t)(0x10U | ((x >> 4U) & 0x0FU)),
        (uint8_t)(x & 0x0FU)
    };
    (void)OLED_WriteCommands(commands, sizeof(commands));
}

static HAL_StatusTypeDef OLED_Update(void)
{
    uint8_t page;
    uint8_t packet[OLED_WIDTH + 1U];
    packet[0] = 0x40U;

    for (page = 0U; page < OLED_PAGES; ++page)
    {
        OLED_SetPos(0U, page);
        memcpy(&packet[1], &oled_buffer[page * OLED_WIDTH], OLED_WIDTH);
        if (HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, packet,
                                    sizeof(packet), 100U) != HAL_OK)
            return HAL_ERROR;
    }
    return HAL_OK;
}

static void OLED_GetGlyph(char c, uint8_t glyph[5])
{
    static const uint8_t digits[10][5] = {
        {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
        {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
        {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
        {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
        {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E}
    };

    memset(glyph, 0, 5U);
    if ((c >= '0') && (c <= '9'))
    {
        memcpy(glyph, digits[c - '0'], 5U);
        return;
    }
    switch (c)
    {
    case 'A': {const uint8_t g[5]={0x7E,0x11,0x11,0x11,0x7E};memcpy(glyph,g,5);break;}
    case 'C': {const uint8_t g[5]={0x3E,0x41,0x41,0x41,0x22};memcpy(glyph,g,5);break;}
    case 'D': {const uint8_t g[5]={0x7F,0x41,0x41,0x22,0x1C};memcpy(glyph,g,5);break;}
    case 'E': {const uint8_t g[5]={0x7F,0x49,0x49,0x49,0x41};memcpy(glyph,g,5);break;}
    case 'G': {const uint8_t g[5]={0x3E,0x41,0x49,0x49,0x7A};memcpy(glyph,g,5);break;}
    case 'H': {const uint8_t g[5]={0x7F,0x08,0x08,0x08,0x7F};memcpy(glyph,g,5);break;}
    case 'I': {const uint8_t g[5]={0x00,0x41,0x7F,0x41,0x00};memcpy(glyph,g,5);break;}
    case 'K': {const uint8_t g[5]={0x7F,0x08,0x14,0x22,0x41};memcpy(glyph,g,5);break;}
    case 'L': {const uint8_t g[5]={0x7F,0x40,0x40,0x40,0x40};memcpy(glyph,g,5);break;}
    case 'P': {const uint8_t g[5]={0x7F,0x09,0x09,0x09,0x06};memcpy(glyph,g,5);break;}
    case 'R': {const uint8_t g[5]={0x7F,0x09,0x19,0x29,0x46};memcpy(glyph,g,5);break;}
    case 'S': {const uint8_t g[5]={0x46,0x49,0x49,0x49,0x31};memcpy(glyph,g,5);break;}
    case 'T': {const uint8_t g[5]={0x01,0x01,0x7F,0x01,0x01};memcpy(glyph,g,5);break;}
    case 'U': {const uint8_t g[5]={0x3F,0x40,0x40,0x40,0x3F};memcpy(glyph,g,5);break;}
    case 'X': {const uint8_t g[5]={0x63,0x14,0x08,0x14,0x63};memcpy(glyph,g,5);break;}
    case 'Y': {const uint8_t g[5]={0x03,0x04,0x78,0x04,0x03};memcpy(glyph,g,5);break;}
    case ':': glyph[1]=0x36; glyph[3]=0x36; break;
    case '.': glyph[2]=0x60; break;
    case '_': for (uint8_t i = 0U; i < 5U; ++i) glyph[i]=0x40; break;
    default: break;
    }
}

static void OLED_DrawString(uint8_t x, uint8_t page, const char *text)
{
    while ((*text != '\0') && (x <= (OLED_WIDTH - 6U)) &&
           (page < OLED_PAGES))
    {
        uint8_t glyph[5];
        OLED_GetGlyph(*text++, glyph);
        memcpy(&oled_buffer[page * OLED_WIDTH + x], glyph, 5U);
        oled_buffer[page * OLED_WIDTH + x + 5U] = 0U;
        x += 6U;
    }
}

void OLED_ShowChar(uint8_t x, uint8_t page, char chr)
{
    char text[2] = {chr, '\0'};
    OLED_DrawString(x, page, text);
    (void)OLED_Update();
}

void OLED_ShowString(uint8_t x, uint8_t page, char *str)
{
    if (str != NULL)
    {
        OLED_DrawString(x, page, str);
        (void)OLED_Update();
    }
}

HAL_StatusTypeDef OLED_Init(void)
{
    static const uint8_t commands[] = {
        0xAE,0xD5,0x80,0xA8,0x3F,0xD3,0x00,0x40,0x8D,0x14,
        0x20,0x02,0xA1,0xC8,0xDA,0x12,0x81,0xCF,0xD9,0xF1,
        0xDB,0x40,0xA4,0xA6,0xAF
    };

    if (HAL_I2C_IsDeviceReady(&hi2c1, OLED_I2C_ADDR, 2U, 100U) != HAL_OK)
        return HAL_ERROR;
    memset(oled_buffer, 0, sizeof(oled_buffer));
    if (OLED_WriteCommands(commands, sizeof(commands)) != HAL_OK)
        return HAL_ERROR;
    return OLED_Update();
}

void OLED_Clear(void)
{
    memset(oled_buffer, 0, sizeof(oled_buffer));
    (void)OLED_Update();
}

HAL_StatusTypeDef OLED_DisplayLight(float lux, uint16_t adc_raw)
{
    char line[22];
    uint32_t lux_x10;

    if (lux < 0.0f) lux = 0.0f;
    if (lux > 99999.0f) lux = 99999.0f;
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

HAL_StatusTypeDef OLED_DisplayEnvironment(float lux, float temperature_c,
                                           float pressure_pa,
                                           float threshold_lux)
{
    char line[22];
    uint32_t lux_x10;
    uint32_t threshold_x10;
    int32_t temp_x10;
    uint32_t pressure_x10;

    if (lux < 0.0f) lux = 0.0f;
    if (lux > 99999.0f) lux = 99999.0f;
    if (pressure_pa < 0.0f) pressure_pa = 0.0f;
    lux_x10 = (uint32_t)(lux * 10.0f + 0.5f);
    if (threshold_lux < 0.0f) threshold_lux = 0.0f;
    if (threshold_lux > 99999.0f) threshold_lux = 99999.0f;
    threshold_x10 = (uint32_t)(threshold_lux * 10.0f + 0.5f);
    temp_x10 = (int32_t)(temperature_c * 10.0f +
                         ((temperature_c >= 0.0f) ? 0.5f : -0.5f));
    /* Display pressure in hPa: 101325 Pa becomes 1013.2 hPa. */
    pressure_x10 = (uint32_t)(pressure_pa / 10.0f + 0.5f);

    memset(oled_buffer, 0, sizeof(oled_buffer));

    (void)snprintf(line, sizeof(line), "L:%lu.%lu LUX",
                   (unsigned long)(lux_x10 / 10U),
                   (unsigned long)(lux_x10 % 10U));
    OLED_DrawString(0U, 0U, line);

    if (temp_x10 < 0)
        (void)snprintf(line, sizeof(line), "T:-%ld.%ld C",
                       (long)((-temp_x10) / 10), (long)((-temp_x10) % 10));
    else
        (void)snprintf(line, sizeof(line), "T:%ld.%ld C",
                       (long)(temp_x10 / 10), (long)(temp_x10 % 10));
    OLED_DrawString(0U, 2U, line);

    (void)snprintf(line, sizeof(line), "P:%lu.%lu HPA",
                   (unsigned long)(pressure_x10 / 10U),
                   (unsigned long)(pressure_x10 % 10U));
    OLED_DrawString(0U, 4U, line);

    (void)snprintf(line, sizeof(line), "TH:%lu.%lu LUX",
                   (unsigned long)(threshold_x10 / 10U),
                   (unsigned long)(threshold_x10 % 10U));
    OLED_DrawString(0U, 6U, line);
    return OLED_Update();
}

HAL_StatusTypeDef OLED_DisplayAnimationFrame(uint8_t frame_index)
{
    if (frame_index >= ANIM_FRAME_COUNT)
        frame_index = 0U;

    memcpy(oled_buffer, Anim_FrameTable[frame_index], ANIM_FRAME_BYTES);
    return OLED_Update();
}

uint8_t OLED_GetAnimationFrameCount(void)
{
    return (uint8_t)ANIM_FRAME_COUNT;
}
