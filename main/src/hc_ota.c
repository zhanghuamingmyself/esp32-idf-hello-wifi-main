#define OTA_URL_SIZE 256
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"

const char *TAG = "hc_ota";


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

void ota_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA example task");
    esp_http_client_config_t config = {
        .crt_bundle_attach = esp_crt_bundle_attach,  // 必须设置服务器验证
        .url = "https://pre-cn.hero-ee.com:8001/oss/hero-iot-oss-pre/iot-virtual/plugins/esp32-idf-hello-wifi.bin",
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
        .skip_cert_common_name_check = true,
    };

// #ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL_FROM_STDIN
//     char url_buf[OTA_URL_SIZE];
//     if (strcmp(config.url, "FROM_STDIN") == 0) {
//         example_configure_stdin_stdout();
//         fgets(url_buf, OTA_URL_SIZE, stdin);
//         int len = strlen(url_buf);
//         url_buf[len - 1] = '\0';
//         config.url = url_buf;
//     } else {
//         ESP_LOGE(TAG, "Configuration mismatch: wrong firmware upgrade image url");
//         abort();
//     }
// #endif

// #ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
//     config.skip_cert_common_name_check = true;
// #endif

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    ESP_LOGI(TAG, "Attempting to download update from %s", config.url);
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Succeed, Rebooting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
