#ifndef __WS2812B_H__
#define __WS2812B_H__

#include "driver/rmt_tx.h"
#include "esp_err.h"
#include "hc_gobal.h"

#ifdef __cplusplus
extern "C" {
#endif

// 定义WS2812B灯珠的数量，请根据实际修改
#define WS2812B_LED_NUMBERS 1



/**
 * @brief 初始化WS2812B RMT驱动
 * @return
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
esp_err_t ws2812b_rmt_driver_install(void);

/**
 * @brief 设置所有灯珠的颜色并刷新显示
 * @param color_array 颜色数组指针
 * @param led_num 灯珠数量
 */
void ws2812b_set_colors(ws2812b_color_t *color_array, uint16_t led_num);

/**
 * @brief 清除所有灯珠（熄灭）
 */
void ws2812b_clear(void);

#ifdef __cplusplus
}
#endif

#endif