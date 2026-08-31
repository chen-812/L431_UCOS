#ifndef KEY_H
#define KEY_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    KEY_VALUE_NONE = 0,

    /* 第一行第四列：KEY_H1 + KEY_L1 */
    KEY_VALUE_ROW1_COL4,

    /* 第二行第四列：KEY_H2 + KEY_L1 */
    KEY_VALUE_ROW2_COL4

} KEY_Value_t;

/**
 * @brief 初始化按键扫描输出状态。
 *
 * GPIO本身仍由CubeMX的MX_GPIO_Init()初始化。
 */
void KEY_Init(void);

/**
 * @brief 立即扫描一次矩阵按键。
 *
 * @return 当前检测到的键值。
 *
 * @note 该函数不包含完整的软件消抖。
 */
KEY_Value_t KEY_ScanRaw(void);

/**
 * @brief 带软件消抖的阻塞式扫描。
 *
 * @return 稳定按下的键值；没有按键时返回KEY_VALUE_NONE。
 *
 * @note 函数内部使用HAL_Delay()，适合裸机测试。
 *       uC/OS任务中建议使用KEY_DebounceProcess()。
 */
KEY_Value_t KEY_Scan(void);

/**
 * @brief 周期式消抖处理。
 *
 * @param scan_period_ms 此函数实际被调用的周期，单位ms。
 *
 * @return 只在确认一次新的按键按下时返回键值；
 *         按住期间不会持续重复返回。
 *
 * @note 建议每10ms调用一次。
 */
KEY_Value_t KEY_DebounceProcess(uint32_t scan_period_ms);

#ifdef __cplusplus
}
#endif

#endif /* KEY_H */
