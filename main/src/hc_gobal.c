#include "hc_gobal.h"
#include "esp_log.h"
#include "freertos/task.h"

QueueHandle_t colorCmdQueue = NULL;

static const char *TAG = "hc_gobal";


void init_gobal(void){
    // 创建全局队列（如果尚未创建）
    if (colorCmdQueue == NULL) {
        colorCmdQueue = xQueueCreate(5, sizeof(ws2812b_color_t));
        if (colorCmdQueue == NULL) {
            ESP_LOGE(TAG, "Failed to create command queue");
            return;
        }
        ESP_LOGI(TAG, "Command queue created successfully");
    }
}