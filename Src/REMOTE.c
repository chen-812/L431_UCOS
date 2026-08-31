/*
 * REMOTE.c
 *
 *  Created on: Jul 20, 2026
 *      Author: HP
 */
#include "REMOTE.h"
#include <string.h>

/*
 * NEC 时序说明（按相邻下降沿之间的时间解码）：
 *
 * 引导码：
 *   9 ms 低电平 + 4.5 ms 高电平
 *   相邻下降沿间隔约 13.5 ms
 *
 * 数据 0：
 *   560 us 低电平 + 560 us 高电平
 *   相邻下降沿间隔约 1.12 ms
 *
 * 数据 1：
 *   560 us 低电平 + 1.69 ms 高电平
 *   相邻下降沿间隔约 2.25 ms
 *
 * 重复码：
 *   9 ms 低电平 + 2.25 ms 高电平 + 560 us 低电平
 *   相邻下降沿间隔约 11.25 ms
 */

/* 时间容差，单位：us */
#define REMOTE_NEC_LEADER_MIN_US       12500UL
#define REMOTE_NEC_LEADER_MAX_US       14500UL

#define REMOTE_NEC_REPEAT_MIN_US       10000UL
#define REMOTE_NEC_REPEAT_MAX_US       12000UL

#define REMOTE_NEC_BIT0_MIN_US           800UL
#define REMOTE_NEC_BIT0_MAX_US          1500UL

#define REMOTE_NEC_BIT1_MIN_US          1700UL
#define REMOTE_NEC_BIT1_MAX_US          2800UL

#define REMOTE_FRAME_TIMEOUT_US        20000UL

typedef enum
{
    REMOTE_STATE_IDLE = 0,
    REMOTE_STATE_RECEIVING
} REMOTE_State_t;

static volatile REMOTE_State_t remote_state = REMOTE_STATE_IDLE;
static volatile uint32_t remote_last_falling_us = 0U;
static volatile uint32_t remote_raw_data = 0U;
static volatile uint8_t remote_bit_index = 0U;

static volatile REMOTE_Data_t remote_latest_data;
static volatile bool remote_data_ready = false;

static uint32_t REMOTE_GetMicros(void);
static void REMOTE_ProcessFallingEdge(uint32_t now_us);
static void REMOTE_FinishFrame(void);
static bool REMOTE_InRange(uint32_t value, uint32_t min_value, uint32_t max_value);

void REMOTE_Init(void)
{
    /*
     * 开启 Cortex-M4 DWT 周期计数器。
     * 使用 DWT 可以在不额外占用定时器的情况下获得微秒时间。
     */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    REMOTE_Reset();
}

void REMOTE_Reset(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    remote_state = REMOTE_STATE_IDLE;
    remote_last_falling_us = 0U;
    remote_raw_data = 0U;
    remote_bit_index = 0U;
    memset((void *)&remote_latest_data, 0, sizeof(remote_latest_data));
    remote_data_ready = false;

    if (primask == 0U)
    {
        __enable_irq();
    }
}

void REMOTE_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != REMOTE_Pin)
    {
        return;
    }

    /*
     * PA8 配置为双边沿 EXTI。
     * 红外接收头空闲时为高电平，因此这里只处理下降沿。
     */
    if (HAL_GPIO_ReadPin(REMOTE_GPIO_Port, REMOTE_Pin) == GPIO_PIN_RESET)
    {
        REMOTE_ProcessFallingEdge(REMOTE_GetMicros());
    }
}

bool REMOTE_GetData(REMOTE_Data_t *data)
{
    if ((data == NULL) || (remote_data_ready == false))
    {
        return false;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    data->raw_data   = remote_latest_data.raw_data;
    data->address    = remote_latest_data.address;
    data->command    = remote_latest_data.command;
    data->is_repeat  = remote_latest_data.is_repeat;
    data->is_extended = remote_latest_data.is_extended;
    data->valid      = remote_latest_data.valid;

    remote_data_ready = false;

    if (primask == 0U)
    {
        __enable_irq();
    }

    return true;
}

bool REMOTE_CommandToDigit(uint8_t command, uint8_t *digit)
{
    static const struct
    {
        uint8_t command;
        uint8_t digit;
    } digit_map[] = {
        {0x42U, 0U}, {0x16U, 1U}, {0x19U, 2U}, {0x0DU, 3U},
        {0x0CU, 4U}, {0x18U, 5U}, {0x5EU, 6U}, {0x08U, 7U},
        {0x1CU, 8U}, {0x5AU, 9U}
    };
    uint32_t i;

    if (digit == NULL)
        return false;

    for (i = 0U; i < (sizeof(digit_map) / sizeof(digit_map[0])); ++i)
    {
        if (digit_map[i].command == command)
        {
            *digit = digit_map[i].digit;
            return true;
        }
    }

    return false;
}

static uint32_t REMOTE_GetMicros(void)
{
    /*
     * 先乘后除可能溢出，因此直接用周期数除以每微秒周期数。
     * STM32L431 常用主频通常是整数 MHz。
     */
    uint32_t cycles_per_us = SystemCoreClock / 1000000UL;

    if (cycles_per_us == 0U)
    {
        cycles_per_us = 1U;
    }

    return DWT->CYCCNT / cycles_per_us;
}

static void REMOTE_ProcessFallingEdge(uint32_t now_us)
{
    uint32_t interval_us;

    if (remote_last_falling_us == 0U)
    {
        remote_last_falling_us = now_us;
        return;
    }

    interval_us = now_us - remote_last_falling_us;
    remote_last_falling_us = now_us;

    /* 长时间无信号，当前帧作废，重新等待引导码 */
    if (interval_us > REMOTE_FRAME_TIMEOUT_US)
    {
        remote_state = REMOTE_STATE_IDLE;
        remote_raw_data = 0U;
        remote_bit_index = 0U;
        return;
    }

    /* NEC 完整引导码 */
    if (REMOTE_InRange(interval_us,
                       REMOTE_NEC_LEADER_MIN_US,
                       REMOTE_NEC_LEADER_MAX_US))
    {
        remote_state = REMOTE_STATE_RECEIVING;
        remote_raw_data = 0U;
        remote_bit_index = 0U;
        return;
    }

    /* NEC 重复码 */
    if (REMOTE_InRange(interval_us,
                       REMOTE_NEC_REPEAT_MIN_US,
                       REMOTE_NEC_REPEAT_MAX_US))
    {
        if (remote_latest_data.valid)
        {
            remote_latest_data.is_repeat = true;
            remote_data_ready = true;
        }

        remote_state = REMOTE_STATE_IDLE;
        remote_raw_data = 0U;
        remote_bit_index = 0U;
        return;
    }

    if (remote_state != REMOTE_STATE_RECEIVING)
    {
        return;
    }

    if (REMOTE_InRange(interval_us,
                       REMOTE_NEC_BIT0_MIN_US,
                       REMOTE_NEC_BIT0_MAX_US))
    {
        /* 逻辑 0，无需置位 */
    }
    else if (REMOTE_InRange(interval_us,
                            REMOTE_NEC_BIT1_MIN_US,
                            REMOTE_NEC_BIT1_MAX_US))
    {
        /*
         * NEC 每个字节均为低位先发送，因此第一个收到的位放在 bit0。
         */
        remote_raw_data |= (1UL << remote_bit_index);
    }
    else
    {
        /* 时序不符合 NEC，丢弃当前帧 */
        remote_state = REMOTE_STATE_IDLE;
        remote_raw_data = 0U;
        remote_bit_index = 0U;
        return;
    }

    remote_bit_index++;

    if (remote_bit_index >= 32U)
    {
        REMOTE_FinishFrame();
        remote_state = REMOTE_STATE_IDLE;
        remote_raw_data = 0U;
        remote_bit_index = 0U;
    }
}

static void REMOTE_FinishFrame(void)
{
    uint8_t byte0 = (uint8_t)(remote_raw_data >> 0);
    uint8_t byte1 = (uint8_t)(remote_raw_data >> 8);
    uint8_t byte2 = (uint8_t)(remote_raw_data >> 16);
    uint8_t byte3 = (uint8_t)(remote_raw_data >> 24);

    REMOTE_Data_t decoded;
    memset(&decoded, 0, sizeof(decoded));

    decoded.raw_data = remote_raw_data;
    decoded.command = byte2;
    decoded.is_repeat = false;

    /*
     * 命令字节必须与反码匹配。
     */
    if ((uint8_t)(byte2 ^ byte3) != 0xFFU)
    {
        decoded.valid = false;
        remote_latest_data = decoded;
        remote_data_ready = true;
        return;
    }

    /*
     * 普通 NEC：
     *   byte0 = address
     *   byte1 = ~address
     *
     * 扩展 NEC：
     *   byte0、byte1 合成 16 位地址。
     */
    if ((uint8_t)(byte0 ^ byte1) == 0xFFU)
    {
        decoded.address = byte0;
        decoded.is_extended = false;
    }
    else
    {
        decoded.address = (uint16_t)byte0 | ((uint16_t)byte1 << 8);
        decoded.is_extended = true;
    }

    decoded.valid = true;
    remote_latest_data = decoded;
    remote_data_ready = true;
}

static bool REMOTE_InRange(uint32_t value,
                           uint32_t min_value,
                           uint32_t max_value)
{
    return (value >= min_value) && (value <= max_value);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    REMOTE_EXTI_Callback(GPIO_Pin);
}

