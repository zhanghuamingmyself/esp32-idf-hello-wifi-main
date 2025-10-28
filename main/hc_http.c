#include "hc_http.h"

// HTTP 事件处理函数
esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            printf("HTTP_EVENT_ERROR\r\n");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            printf("HTTP_EVENT_ON_CONNECTED\r\n");
            break;
        case HTTP_EVENT_HEADER_SENT:
            printf("HTTP_EVENT_HEADER_SENT\r\n");
            break;
        case HTTP_EVENT_ON_HEADER:
            printf("HTTP_EVENT_ON_HEADER, key=%s, value=%s\r\n", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            printf("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // 打印接收到的数据
                printf("%.*s\r\n", evt->data_len, (char*)evt->data);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            printf("HTTP_EVENT_ON_FINISH\r\n");
            break;
        case HTTP_EVENT_DISCONNECTED:
            printf("HTTP_EVENT_DISCONNECTED\r\n");
            break;
        default:
            printf("HTTP_EVENT_OTHER %d\r\n", evt->event_id);
            break;
    }
    return ESP_OK;
}

void http_get_task(void *pvParameters) {
    esp_http_client_config_t config = {
        .url = "http://httpbin.org/get",
        .event_handler = http_event_handler,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    // 执行 GET 请求
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        printf("HTTP GET Status = %d, content_length = %lld\r\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        printf("HTTP GET request failed: %s\r\n", esp_err_to_name(err));
    }
    
    // 清理资源
    esp_http_client_cleanup(client);
}
