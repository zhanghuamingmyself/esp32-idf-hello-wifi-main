#ifndef BTN_LED_H
#define BTN_LED_H

#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"

#include "rom/ets_sys.h"

void btn_led_init();

#endif // BTN_LED_H