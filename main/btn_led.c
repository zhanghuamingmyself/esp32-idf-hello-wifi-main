/*|------------------------------------------------------------------------|*/
/*|WiFi connection in STA mode for ESP32 under ESP-IDF - Wokwi simulator   |*/
/*|Edited by: martinius96                                                  |*/
/*|Updated for esp-idf 5.x by Uri Shaked                                   |*/
/*|Buy me a coffee at: paypal.me/chlebovec for more examples               |*/
/*|------------------------------------------------------------------------|*/

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

#define LED_GPIO GPIO_NUM_7 // On-board LED GPIO
#define BUTTON_GPIO GPIO_NUM_9 // On-board Button GPIO

static const char *TAG = "my_task";
static int reading = 1;

void led_task(void *param){
  ESP_LOGE(TAG, "led_task running");
  while(true){
      if (reading == 0) { // Button pressed
        gpio_set_level(LED_GPIO, 1); // Turn LED on
      } else {
        gpio_set_level(LED_GPIO, 0); // Turn LED off
      }
      vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void btn_task(void *param){
  ESP_LOGE(TAG, "btn_task running");
  while(true){
    //  reading = gpio_get_level(BUTTON_GPIO);
     vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

static void IRAM_ATTR button_isr_handler(void* arg)
{
    // 读取按键状态
    if(reading == 0){
      reading = 1;
    }else{
      reading = 0;
    }
}

void init(){
  ESP_LOGE(TAG, "init");
  gpio_reset_pin(LED_GPIO);
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

  gpio_reset_pin(BUTTON_GPIO);
  gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
  gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);
}

// Main application
void app_main() {

  init();

  // ESP_ERROR_CHECK()
  BaseType_t led_task_res = xTaskCreate(&led_task,"led_task",2048,NULL,5,NULL);
  if(led_task_res != pdPASS){
    ESP_LOGE(TAG, "led xTaskCreate failed");
  }

  BaseType_t btn_task_res = xTaskCreate(&btn_task,"btn_task",2048,NULL,5,NULL);
  if(btn_task_res != pdPASS){
    ESP_LOGE(TAG, "btn xTaskCreate failed");
  }

  gpio_install_isr_service(0);
    
    // 配置按键中断
  gpio_set_intr_type(BUTTON_GPIO, GPIO_INTR_ANYEDGE); // 双边沿触发
  gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);


}
