/*
 * key.c
 *
 *  Created on: Jul 20, 2026
 *      Author: HP
 */
#include "key.h"

/*
 * CubeMX中请使用以下User Label：
 *
 * PB0  -> KEY_H1
 * PB1  -> KEY_H2
 * PB14 -> KEY_L1
 *
 * CubeMX会在main.h中生成：
 *
 * #define KEY_H1_Pin GPIO_PIN_0
 * #define KEY_H1_GPIO_Port GPIOB
 *
 * #define KEY_H2_Pin GPIO_PIN_1
 * #define KEY_H2_GPIO_Port GPIOB
 *
 * #define KEY_L1_Pin GPIO_PIN_14
 * #define KEY_L1_GPIO_Port GPIOB
 */

/* 软件消抖确认时间 */
#define KEY_DEBOUNCE_TIME_MS    30U

/*
 * 行线全部置高。
 *
 * KEY_L1采用内部上拉：
 * 选中某一行时将该行拉低，若按键闭合，KEY_L1会读到低电平。
 */
static void KEY_SetAllRowsHigh(void)
{
    HAL_GPIO_WritePin(KEY_H1_GPIO_Port, KEY_H1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(KEY_H2_GPIO_Port, KEY_H2_Pin, GPIO_PIN_SET);
}

/*
 * 给矩阵信号留出很短的稳定时间。
 *
 * 这里只需要几个CPU周期，不应使用毫秒级延时，
 * 否则扫描速度会过慢。
 */
static void KEY_SettleDelay(void)
{
    volatile uint32_t i;

    for (i = 0U; i < 30U; i++)
    {
        __NOP();
    }
}

void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Keep both rows inactive before changing them to outputs. */
    HAL_GPIO_WritePin(GPIOB, KEY_H1_Pin | KEY_H2_Pin, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = KEY_H1_Pin | KEY_H2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = KEY_L1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(KEY_L1_GPIO_Port, &GPIO_InitStruct);

    KEY_SetAllRowsHigh();
}

KEY_Value_t KEY_ScanRaw(void)
{
    KEY_Value_t key = KEY_VALUE_NONE;

    /*
     * 扫描第一行：
     * KEY_H1拉低，KEY_H2保持高电平。
     * 如果KEY_L1为低，则第一行第四列按键按下。
     */
    KEY_SetAllRowsHigh();

    HAL_GPIO_WritePin(KEY_H1_GPIO_Port,
                      KEY_H1_Pin,
                      GPIO_PIN_RESET);

    KEY_SettleDelay();

    if (HAL_GPIO_ReadPin(KEY_L1_GPIO_Port,
                         KEY_L1_Pin) == GPIO_PIN_RESET)
    {
        key = KEY_VALUE_ROW1_COL4;
    }

    /*
     * 只有第一行未检测到按键时，才扫描第二行。
     */
    if (key == KEY_VALUE_NONE)
    {
        KEY_SetAllRowsHigh();

        HAL_GPIO_WritePin(KEY_H2_GPIO_Port,
                          KEY_H2_Pin,
                          GPIO_PIN_RESET);

        KEY_SettleDelay();

        if (HAL_GPIO_ReadPin(KEY_L1_GPIO_Port,
                             KEY_L1_Pin) == GPIO_PIN_RESET)
        {
            key = KEY_VALUE_ROW2_COL4;
        }
    }

    /*
     * 扫描结束后恢复两条行线为高电平。
     */
    KEY_SetAllRowsHigh();

    return key;
}

KEY_Value_t KEY_Scan(void)
{
    KEY_Value_t first_key;
    KEY_Value_t second_key;

    first_key = KEY_ScanRaw();

    if (first_key == KEY_VALUE_NONE)
    {
        return KEY_VALUE_NONE;
    }

    /*
     * 简单阻塞式消抖。
     */
    HAL_Delay(KEY_DEBOUNCE_TIME_MS);

    second_key = KEY_ScanRaw();

    if (second_key == first_key)
    {
        return first_key;
    }

    return KEY_VALUE_NONE;
}

KEY_Value_t KEY_DebounceProcess(uint32_t scan_period_ms)
{
    static KEY_Value_t last_raw_key = KEY_VALUE_NONE;
    static KEY_Value_t stable_key   = KEY_VALUE_NONE;
    static uint32_t stable_time_ms  = 0U;
    static uint8_t press_reported   = 0U;

    KEY_Value_t raw_key;

    if (scan_period_ms == 0U)
    {
        scan_period_ms = 1U;
    }

    raw_key = KEY_ScanRaw();

    /*
     * 当前采样值发生变化，重新开始计时。
     */
    if (raw_key != last_raw_key)
    {
        last_raw_key   = raw_key;
        stable_time_ms = 0U;
    }
    else
    {
        if (stable_time_ms < KEY_DEBOUNCE_TIME_MS)
        {
            stable_time_ms += scan_period_ms;
        }
    }

    /*
     * 同一个状态稳定达到消抖时间后，确认状态变化。
     */
    if (stable_time_ms >= KEY_DEBOUNCE_TIME_MS)
    {
        if (stable_key != raw_key)
        {
            stable_key = raw_key;

            if (stable_key == KEY_VALUE_NONE)
            {
                /*
                 * 按键释放，允许下一次按下再次上报。
                 */
                press_reported = 0U;
            }
        }
    }

    /*
     * 每次按下只上报一次。
     */
    if ((stable_key != KEY_VALUE_NONE) &&
        (press_reported == 0U))
    {
        press_reported = 1U;
        return stable_key;
    }

    return KEY_VALUE_NONE;
}

