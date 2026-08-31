#ifndef __TF_H
#define __TF_H

#include "main.h"
#include "fatfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mount the FAT filesystem. MX_FATFS_Init() must have been called first. */
uint8_t TF_Init(void);

/* Replace a file or append text to it. Return 1 only after all bytes close OK. */
uint8_t TF_Write(const char *filename, const char *data);
uint8_t TF_Append(const char *filename, const char *data);

/* Append one CSV record: elapsed_seconds,temperature_c. */
uint8_t TF_AppendTemperature(const char *filename,
                             uint32_t elapsed_seconds,
                             float temperature_c);

/* FatFs result produced by the most recent TF operation. */
FRESULT TF_GetLastResult(void);

#ifdef __cplusplus
}
#endif

#endif
