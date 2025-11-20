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
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "./include/hc_mqtt.h"
#include "./include/hc_http.h"
#include "nvs_flash.h"
#include "./include/btn_led.h"
#include "./include/ble_server.h"
#include "./include/ws2812b.h"
#include "./include/modbus_client.h"
#include "./include/hc_ota.h"
// #include "esp_task_wdt.h"

// Status LED
#define LED_RED GPIO_NUM_2

// WiFi parameters
#define WIFI_SSID "Hithium-SZ-guest"
//CONFIG_ESP_WIFI_SSID
#define WIFI_PASS "hc123456"
// CONFIG_ESP_WIFI_PASSWORD

// Event group
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

static const char *TAG = "main";


// WiFi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
  }
}

void wifi_led_config()
{
    gpio_reset_pin(LED_RED);
    gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);
}

void wifi_led_task(void *pvParameter)
{
  while (1) {
    if (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) {
      // We are connected - LED on
      gpio_set_level(LED_RED, 1);
    } else {
      // We are connecting - blink fast
      gpio_set_level(LED_RED, 0);
    }
     vTaskDelay(2000/ portTICK_PERIOD_MS);
  }
}

// Main task
void main_task(void *pvParameter) {
  esp_netif_ip_info_t ip_info;

  // wait for connection
  ESP_LOGI(TAG, "Waiting for connection to the Wi-Fi network...\n");
  xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
  ESP_LOGI(TAG, "Connected!\n");

  // Get and print the local IP address
  esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
  ESP_LOGI(TAG, "IP Address:  " IPSTR "\n", IP2STR(&ip_info.ip));
  ESP_LOGI(TAG, "Subnet mask: " IPSTR "\n", IP2STR(&ip_info.netmask));
  ESP_LOGI(TAG, "Gateway:     " IPSTR "\n", IP2STR(&ip_info.gw)); 
  ESP_LOGI(TAG, "You can connect now to any web server online! :-)\n");

  esp_err_t ret;

  init_gobal();

  vTaskDelay(2000 / portTICK_PERIOD_MS);

  // 初始化WS2812B驱动
  ret = ws2812b_rmt_driver_install();
  if (ret != ESP_OK) {
      ESP_LOGI(TAG, "WS2812B驱动初始化失败，程序停止！\n");
      return;
  }

  ws2812b_color_t color = { 
                .g = 0, 
                .r = 255, 
                .b = 0 
            };
  xQueueSend(colorCmdQueue, &color, portMAX_DELAY);

  size_t free_heap_total = esp_get_free_heap_size();
  size_t free_heap_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  ESP_LOGI(TAG, "Heap free total: %u, internal: %u", free_heap_total, free_heap_internal);

  ble_server_init();

  btn_led_init();

   xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);

  // http_get_task(NULL);
  //  // 启动MQTT客户端
  // mqtt_app_start();

  // test_modbus_tcp_client();

  while (1) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void initNet(){
 
  
  // Initialize the TCP/IP stack
  esp_netif_init();

  // Create the default event loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Create the default Wi-Fi station
  esp_netif_create_default_wifi_sta();

  // Create the event group to handle Wi-Fi events
  wifi_event_group = xEventGroupCreate();

  // Initialize the Wi-Fi driver
  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

  // Register event handlers
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                  ESP_EVENT_ANY_ID,
                  &wifi_event_handler,
                  NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                  IP_EVENT_STA_GOT_IP,
                  &wifi_event_handler,
                  NULL, NULL));

  ESP_LOGI(TAG, "\nwifi ssid: %s\n", WIFI_SSID);
  ESP_LOGI(TAG, "\nwifi pass: %s\n", WIFI_PASS);

  // Configure Wi-Fi connection settings
  wifi_config_t wifi_config = {
    .sta = {
      .ssid = WIFI_SSID,
      .password = WIFI_PASS,
    },
  };

  // Set Wi-Fi mode to STA (Station)
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

  // Start Wi-Fi
  ESP_ERROR_CHECK(esp_wifi_start());

  wifi_led_config();

   // The LED task is used to show the connection status
  xTaskCreate(&wifi_led_task, "wifi_led_task", 1024, NULL, 5, NULL);
}

// Main application
void app_main() {
 
  ESP_LOGI(TAG, "\nESP-IDF version used: %s\n", IDF_VER);

   // Initialize NVS — required before Wi-Fi / esp_netif
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  initNet();

  // Start the main task
  xTaskCreate(&main_task, "main_task", 10240, NULL, 5, NULL);

  while (1) {
    // esp_task_wdt_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

}
