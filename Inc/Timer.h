#ifndef __TIMER_H
#define __TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Initialize the display buffer and set RTC to 07-19 16:00:00 only once. */
HAL_StatusTypeDef SEG_Init(void);
void SEG_TimerIRQHandler(void);
void SEG_GetDiagnostics(uint32_t *scan_count, uint8_t buffer[8],
                        uint8_t *month, uint8_t *day,
                        uint8_t *hour, uint8_t *minute,
                        uint8_t *second);

/* Call once per millisecond to multiplex one of the eight digits. */
void SEG_Scan(void);

/* Read RTC and update display: [blank]M.DD HH.MM. */
HAL_StatusTypeDef RTC_Read_Time(void);
void SEG_Set_Value(void);

/* Replace the left/date group with a decimal infrared keypad digit. */
void SEG_SetRemoteDigit(uint8_t digit);

#ifdef __cplusplus
}
#endif

#endif
