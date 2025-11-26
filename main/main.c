#include "./include/main_app.h"
#include "./include/usb_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "esp_log.h"

static const char *TAG = "main";

void app_main()
{
    ESP_LOGI(TAG, "Hello World!");
    app_main_run();
    // usb_host_main();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}