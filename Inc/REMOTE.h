#ifndef __REMOTE_H
#define __REMOTE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * REMOTE 红外遥控接收驱动
 * 适用协议：NEC / 扩展 NEC
 * 硬件：IRM-3638T 等 38 kHz 解调型红外接收头
 * 输入引脚：CubeMX 中配置为双边沿 EXTI，用户标签为 REMOTE
 */

/* 解码结果 */
typedef struct
{
    uint32_t raw_data;      /* 32 位原始数据，按接收顺序保存 */
    uint16_t address;       /* 普通 NEC 为 8 位地址；扩展 NEC 为 16 位地址 */
    uint8_t  command;       /* 命令值 */
    bool     is_repeat;     /* NEC 重复码 */
    bool     is_extended;   /* 扩展 NEC 地址格式 */
    bool     valid;         /* 数据校验是否通过 */
} REMOTE_Data_t;

/**
 * @brief 初始化红外遥控驱动。
 * @note  请在 MX_GPIO_Init() 和 SystemClock_Config() 之后调用。
 */
void REMOTE_Init(void);

/**
 * @brief 在 HAL_GPIO_EXTI_Callback() 中调用。
 * @param GPIO_Pin HAL 传入的中断引脚编号。
 */
void REMOTE_EXTI_Callback(uint16_t GPIO_Pin);

/**
 * @brief 获取一帧新的遥控数据。
 * @param data 用于保存解码结果。
 * @return true：获得一帧新数据；false：当前没有新数据。
 */
bool REMOTE_GetData(REMOTE_Data_t *data);

/* Translate common 21-key NEC remote commands to decimal digits. */
bool REMOTE_CommandToDigit(uint8_t command, uint8_t *digit);

/**
 * @brief 清除当前解码状态和待取数据。
 */
void REMOTE_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __REMOTE_H */
