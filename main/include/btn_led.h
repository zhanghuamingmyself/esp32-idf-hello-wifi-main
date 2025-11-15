#ifndef BTN_LED_H
#define BTN_LED_H

#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "rom/ets_sys.h"

void btn_led_main();
void init_neopixel();
void set_neopixel_color(uint8_t red, uint8_t green, uint8_t blue);
void test_neopixel_colors();

#endif // BTN_LED_H