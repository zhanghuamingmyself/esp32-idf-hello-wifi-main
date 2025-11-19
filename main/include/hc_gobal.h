#ifndef HC_GLOBAL_H
#define HC_GLOBAL_H

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    uint8_t g; // 绿色
    uint8_t r; // 红色
    uint8_t b; // 蓝色
} ws2812b_color_t;

extern QueueHandle_t colorCmdQueue;

void init_gobal(void);

#endif