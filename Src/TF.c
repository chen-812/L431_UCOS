#include "TF.h"

#include <stdio.h>
#include <string.h>

static FATFS tf_fs;
static FIL tf_file;
static FRESULT tf_result = FR_NOT_READY;
static uint8_t tf_mounted = 0U;

static uint8_t TF_WriteInternal(const char *filename,
                                const char *data,
                                BYTE mode)
{
    UINT bytes_to_write;
    UINT bytes_written = 0U;
    FRESULT close_result;

    if ((tf_mounted == 0U) || (filename == NULL) || (data == NULL))
    {
        tf_result = FR_INVALID_OBJECT;
        return 0U;
    }

    bytes_to_write = (UINT)strlen(data);
    tf_result = f_open(&tf_file, filename, mode | FA_WRITE);
    if (tf_result != FR_OK)
        return 0U;

    tf_result = f_write(&tf_file, data, bytes_to_write, &bytes_written);
    if ((tf_result == FR_OK) && (bytes_written != bytes_to_write))
        tf_result = FR_DISK_ERR;

    /* f_close also flushes cached directory and file data to the card. */
    close_result = f_close(&tf_file);
    if ((tf_result == FR_OK) && (close_result != FR_OK))
        tf_result = close_result;

    return (tf_result == FR_OK) ? 1U : 0U;
}

uint8_t TF_Init(void)
{
    if (SDPath[0] == '\0')
    {
        tf_result = FR_NOT_ENABLED;
        tf_mounted = 0U;
        return 0U;
    }

    tf_result = f_mount(&tf_fs, SDPath, 1U);
    tf_mounted = (tf_result == FR_OK) ? 1U : 0U;
    return tf_mounted;
}

uint8_t TF_Write(const char *filename, const char *data)
{
    return TF_WriteInternal(filename, data, FA_CREATE_ALWAYS);
}

uint8_t TF_Append(const char *filename, const char *data)
{
    return TF_WriteInternal(filename, data, FA_OPEN_APPEND);
}

uint8_t TF_AppendTemperature(const char *filename,
                             uint32_t elapsed_seconds,
                             float temperature_c)
{
    char line[48];
    int32_t temperature_x100;
    uint32_t magnitude;
    int length;

    temperature_x100 = (int32_t)(temperature_c * 100.0f +
                                 ((temperature_c >= 0.0f) ? 0.5f : -0.5f));
    magnitude = (temperature_x100 < 0) ?
                (uint32_t)(-temperature_x100) : (uint32_t)temperature_x100;

    length = snprintf(line, sizeof(line), "%lu,%s%lu.%02lu\r\n",
                      (unsigned long)elapsed_seconds,
                      (temperature_x100 < 0) ? "-" : "",
                      (unsigned long)(magnitude / 100U),
                      (unsigned long)(magnitude % 100U));
    if ((length < 0) || ((size_t)length >= sizeof(line)))
    {
        tf_result = FR_INVALID_PARAMETER;
        return 0U;
    }

    return TF_Append(filename, line);
}

FRESULT TF_GetLastResult(void)
{
    return tf_result;
}
