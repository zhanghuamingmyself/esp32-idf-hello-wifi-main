#include "../include/hc_http.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"

static const char *TAG = "hc_http";

typedef struct {
    char *buf;
    int len;
    int cap;
} http_resp_ctx_t;

// HTTP 事件处理函数
esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    http_resp_ctx_t *ctx = (http_resp_ctx_t *) evt->user_data;
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (ctx) {
                if (ctx->buf == NULL) {
                    ctx->cap = evt->data_len + 1;
                    ctx->buf = malloc(ctx->cap);
                    ctx->len = 0;
                } else if (ctx->len + evt->data_len + 1 > ctx->cap) {
                    ctx->cap = ctx->len + evt->data_len + 1;
                    ctx->buf = realloc(ctx->buf, ctx->cap);
                }
                if (ctx->buf) {
                    memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
                    ctx->len += evt->data_len;
                    ctx->buf[ctx->len] = '\0';
                }
            }
            // 可选：打印每块数据片段
            ESP_LOGI(TAG, "%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            if (ctx && ctx->buf) {
                ESP_LOGI(TAG, "Full response: %s", ctx->buf);
                free(ctx->buf);
                ctx->buf = NULL;
                ctx->len = ctx->cap = 0;
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            ESP_LOGI(TAG, "HTTP_EVENT_OTHER %d", evt->event_id);
            break;
    }
    return ESP_OK;
}

void http_get_task(void *pvParameters) {
    http_resp_ctx_t resp_ctx = {0};

    esp_http_client_config_t config = {
        .crt_bundle_attach = esp_crt_bundle_attach,  // 必须设置服务器验证
        .skip_cert_common_name_check = false,        // 启用CN检查
        .url = "https://www.hero-ee.com/api/device/api/geturl?v=1.0",
        .event_handler = http_event_handler,
        .timeout_ms = 5000,
        .user_data = &resp_ctx, // 传递上下文用于累积响应
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);



    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "env", "pre");
    cJSON_AddStringToObject(root, "region", "CN");
    char* post_data = cJSON_PrintUnformatted(root);
    
    cJSON_Delete(root);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    // 执行 GET 请求
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGI(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    
    // 清理资源
    esp_http_client_cleanup(client);
}
