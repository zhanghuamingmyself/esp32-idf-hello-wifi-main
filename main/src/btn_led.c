/*|------------------------------------------------------------------------|*/
/*|WiFi connection in STA mode for ESP32 under ESP-IDF - Wokwi simulator   |*/
/*|Edited by: martinius96                                                  |*/
/*|Updated for esp-idf 5.x by Uri Shaked                                   |*/
/*|Buy me a coffee at: paypal.me/chlebovec for more examples               |*/
/*|------------------------------------------------------------------------|*/

#include "../include/btn_led.h"

#define LED_GPIO GPIO_NUM_7 // On-board LED GPIO
#define BUTTON_GPIO GPIO_NUM_0 // On-board Button GPIO

static const char *TAG = "btn_led";
static int reading = 1;


void led_task(void *param){
  ESP_LOGI(TAG, "led_task running");
  while(true){
      if (reading == 0) { // Button pressed
        gpio_set_level(LED_GPIO, 1); // Turn LED on
      } else {
        gpio_set_level(LED_GPIO, 0); // Turn LED off
      }
      vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

static void IRAM_ATTR button_isr_handler(void* arg)
{
    ESP_LOGI(TAG, "button_isr_handler");  
    // Simple debounce: wait a short time before reading button state
    ets_delay_us(5000); // 5ms debounce delay
    
    // Read actual button state (0 = pressed, 1 = released due to pull-up)
    int button_state = gpio_get_level(BUTTON_GPIO);
    
    if(button_state == 0){
      // Button pressed - turn NeoPixel green
      reading = 0;
    }else{
      // Button released - turn NeoPixel red
      reading = 1;
    }
}



void init_led(){
  gpio_reset_pin(LED_GPIO);
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
}

void init_btn(){
  gpio_reset_pin(BUTTON_GPIO);
  gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
  gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);

  gpio_install_isr_service(0);
  // 配置按键中断
  gpio_set_intr_type(BUTTON_GPIO, GPIO_INTR_ANYEDGE); // 双边沿触发
  gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
}

void init(){
  ESP_LOGE(TAG, "init");
  init_led();
  init_btn();

}

// Main application
void btn_led_main() {
  init();

  // ESP_ERROR_CHECK()
  BaseType_t led_task_res = xTaskCreate(&led_task,"led_task",2048,NULL,5,NULL);
  if(led_task_res != pdPASS){
    ESP_LOGE(TAG, "led xTaskCreate failed");
  }

}


