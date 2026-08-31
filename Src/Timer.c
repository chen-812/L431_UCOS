#include "Timer.h"
#include "rtc.h"
#include <string.h>

#define SEG_BLANK        0x00U
#define SEG_DP_MASK      0x80U
#define RTC_INIT_MARKER  0x7190U

#define HC595_DATA(x)  HAL_GPIO_WritePin(HC595_DATA_GPIO_Port, HC595_DATA_Pin, (x))
#define HC595_CLK(x)   HAL_GPIO_WritePin(HC595_HCLK_GPIO_Port, HC595_HCLK_Pin, (x))
#define HC595_LATCH(x) HAL_GPIO_WritePin(HC595_LCLK_GPIO_Port, HC595_LCLK_Pin, (x))
#define HC138_A0(x)    HAL_GPIO_WritePin(HC138_A0_GPIO_Port, HC138_A0_Pin, (x))
#define HC138_A1(x)    HAL_GPIO_WritePin(HC138_A1_GPIO_Port, HC138_A1_Pin, (x))
#define HC138_A2(x)    HAL_GPIO_WritePin(HC138_A2_GPIO_Port, HC138_A2_Pin, (x))

/* The 74HC138 pulls the selected digit common low, therefore the segment
   outputs are active high (common-cathode display).
   bit0..7 = a,b,c,d,e,f,g,dp. */
static const uint8_t seg_table[10] = {
    0x3FU, 0x06U, 0x5BU, 0x4FU, 0x66U,
    0x6DU, 0x7DU, 0x07U, 0x7FU, 0x6FU
};

static volatile uint8_t seg_buf[8];
static volatile uint8_t seg_ready;
static volatile uint32_t seg_scan_count;
static uint8_t scan_index;
static RTC_TimeTypeDef rtc_time;
static RTC_DateTypeDef rtc_date;
static volatile uint8_t remote_digit;
static volatile uint8_t remote_digit_valid;

static void HC595_Send(uint8_t data)
{
    uint8_t i;
    HC595_LATCH(GPIO_PIN_RESET);
    for (i = 0U; i < 8U; ++i)
    {
        HC595_DATA((data & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HC595_CLK(GPIO_PIN_SET);
        HC595_CLK(GPIO_PIN_RESET);
        data <<= 1U;
    }
    HC595_LATCH(GPIO_PIN_SET);
    HC595_LATCH(GPIO_PIN_RESET);
}

static void HC138_Select(uint8_t digit)
{
    HC138_A0((digit & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HC138_A1((digit & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HC138_A2((digit & 0x04U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

HAL_StatusTypeDef SEG_Init(void)
{
    RTC_TimeTypeDef initial_time = {0};
    RTC_DateTypeDef initial_date = {0};

    memset((void *)seg_buf, SEG_BLANK, sizeof(seg_buf));
    scan_index = 0U;
    HC595_Send(SEG_BLANK);

    /* Backup register survives a normal reset. Do not reset a running clock. */
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != RTC_INIT_MARKER)
    {
        initial_time.Hours = 16U;
        initial_time.Minutes = 0U;
        initial_time.Seconds = 0U;
        initial_time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
        initial_time.StoreOperation = RTC_STOREOPERATION_RESET;

        initial_date.WeekDay = RTC_WEEKDAY_SUNDAY;
        initial_date.Month = RTC_MONTH_JULY;
        initial_date.Date = 19U;
        initial_date.Year = 26U;

        if (HAL_RTC_SetTime(&hrtc, &initial_time, RTC_FORMAT_BIN) != HAL_OK)
            return HAL_ERROR;
        if (HAL_RTC_SetDate(&hrtc, &initial_date, RTC_FORMAT_BIN) != HAL_OK)
            return HAL_ERROR;
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, RTC_INIT_MARKER);
    }
    if (RTC_Read_Time() != HAL_OK)
        return HAL_ERROR;
    seg_ready = 1U;
    /* Multiplexing is driven by the already-running 1 kHz SysTick. TIM6 is
       left disabled because its IRQ is not delivered on this target setup. */
    return HAL_OK;
}

HAL_StatusTypeDef RTC_Read_Time(void)
{
    /* On STM32 the date must be read after time to unlock shadow registers. */
    if (HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN) != HAL_OK)
        return HAL_ERROR;
    if (HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN) != HAL_OK)
        return HAL_ERROR;
    SEG_Set_Value();
    return HAL_OK;
}

void SEG_Set_Value(void)
{
    uint8_t next[8];
    uint8_t hour = rtc_time.Hours;
    uint8_t minute = rtc_time.Minutes;

    /* Never index the segment table with a corrupt RTC field. Keep the
       previous valid display and expose the bad RTC values diagnostically. */
    if ((hour > 23U) || (minute > 59U))
        return;

    /* The left group is reserved for the latest infrared command. */
    next[0] = SEG_BLANK;
    next[1] = SEG_BLANK;
    next[2] = SEG_BLANK;
    if (remote_digit_valid != 0U)
    {
        next[3] = seg_table[remote_digit];
    }
    else
    {
        next[3] = SEG_BLANK;
    }

    /* Four clock digits. DP after the hour is used as the time separator;
       on modules with a dedicated colon this appears as the colon output. */
    next[4] = seg_table[hour / 10U];
    next[5] = (uint8_t)(seg_table[hour % 10U] | SEG_DP_MASK);
    /* The SR420281N schematic shows two center LEDs (L1/L2). Enabling DP
       while scanning both middle commons drives the two-dot separator. */
    next[6] = (uint8_t)(seg_table[minute / 10U] | SEG_DP_MASK);
    next[7] = seg_table[minute % 10U];

    /* Byte writes are atomic on Cortex-M4. Scan may see at most one old digit
       during this very short update and will be correct on the next 8 ms frame. */
    {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    memcpy((void *)seg_buf, next, sizeof(next));
    if (primask == 0U) __enable_irq();
    }
}

void SEG_SetRemoteDigit(uint8_t digit)
{
    if (digit > 9U)
        return;

    remote_digit = digit;
    remote_digit_valid = 1U;
    SEG_Set_Value();
}

void SEG_Scan(void)
{
    if (seg_ready == 0U)
        return;

    /* Blank before changing the active-low 74HC138 digit to prevent ghosting. */
    HC595_Send(SEG_BLANK);
    HC138_Select(scan_index);
    HC595_Send(seg_buf[scan_index]);
    scan_index = (uint8_t)((scan_index + 1U) & 0x07U);
    ++seg_scan_count;
}

void SEG_GetDiagnostics(uint32_t *count, uint8_t buffer[8],
                        uint8_t *month, uint8_t *day,
                        uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    if (count != NULL) *count = seg_scan_count;
    if (buffer != NULL) memcpy(buffer, (const void *)seg_buf, 8U);
    if (month != NULL) *month = rtc_date.Month;
    if (day != NULL) *day = rtc_date.Date;
    if (hour != NULL) *hour = rtc_time.Hours;
    if (minute != NULL) *minute = rtc_time.Minutes;
    if (second != NULL) *second = rtc_time.Seconds;
}

void SEG_TimerIRQHandler(void)
{
    if ((TIM6->SR & TIM_SR_UIF) != 0U)
    {
        TIM6->SR &= ~TIM_SR_UIF;
        SEG_Scan();
    }
}
