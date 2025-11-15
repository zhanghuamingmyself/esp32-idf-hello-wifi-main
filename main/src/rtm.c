#include "../include/rtm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_PIN         48

void neopixel_init(void){}

void set_led_color(uint8_t red, uint8_t green, uint8_t blue){}
