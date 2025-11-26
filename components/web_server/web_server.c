// main/components/web_server/web_server.c
#include "web_server.h"
#include <string.h>
#include <sys/param.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "include/web_server.h"

static const char *TAG = "WEB_SERVER";

// 服务器全局句柄
static httpd_handle_t server_handle = NULL;
static web_server_config_t server_config;

/**
 * 根路径处理器 - 提供HTML页面
 */
static esp_err_t root_get_handler(httpd_req_t *req) {
    // ESP_LOGI(TAG, "根路径请求来自: %s", req->header("User-Agent"));
    
    // 读取并返回HTML文件
    extern const char index_html_start[] asm("_binary_index_html_start");
    extern const char index_html_end[] asm("_binary_index_html_end");
    size_t index_html_size = index_html_end - index_html_start;
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html_start, index_html_size);
    
    return ESP_OK;
}

/**
 * API状态处理器
 */
static esp_err_t api_status_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "API状态请求");
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "online");
    cJSON_AddStringToObject(root, "version", "1.0.0");
    // cJSON_AddNumberToObject(root, "timestamp", esp_timer_get_time() / 1000);
    cJSON_AddStringToObject(root, "device", "ESP32");
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    cJSON_Delete(root);
    free(json_str);
    
    return ESP_OK;
}

/**
 * 系统信息API
 */
static esp_err_t api_system_info_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    
    // 系统信息
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "min_free_heap", esp_get_minimum_free_heap_size());
    cJSON_AddNumberToObject(root, "task_count", uxTaskGetNumberOfTasks());
    
    // // 芯片信息
    // esp_chip_info_t chip_info;
    // esp_chip_info(&chip_info);
    // cJSON_AddStringToObject(root, "chip_model", 
    //     (chip_info.model == CHIP_ESP32) ? "ESP32" : 
    //     (chip_info.model == CHIP_ESP32S2) ? "ESP32-S2" : 
    //     (chip_info.model == CHIP_ESP32S3) ? "ESP32-S3" : "Unknown");
    // cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    // cJSON_AddNumberToObject(root, "revision", chip_info.revision);
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    cJSON_Delete(root);
    free(json_str);
    
    return ESP_OK;
}

/**
 * LED控制API
 */
static esp_err_t api_led_control_handler(httpd_req_t *req) {
    char buf[100];
    int ret, remaining = req->content_len;
    
    // 读取POST数据
    if (remaining > sizeof(buf) - 1) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return ESP_FAIL;
    }
    
    ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf) - 1));
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
        return ESP_FAIL;
    }
    
    buf[ret] = '\0';
    
    // 解析JSON
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *led_state = cJSON_GetObjectItem(root, "led");
    if (led_state == NULL || !cJSON_IsBool(led_state)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid 'led' field");
        return ESP_FAIL;
    }
    
    // 控制LED（这里只是示例，实际需要连接硬件）
    bool led_on = cJSON_IsTrue(led_state);
    ESP_LOGI(TAG, "LED控制: %s", led_on ? "ON" : "OFF");
    
    // 响应
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", led_on ? "LED turned on" : "LED turned off");
    cJSON_AddBoolToObject(response, "led_state", led_on);
    
    char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    cJSON_Delete(root);
    cJSON_Delete(response);
    free(json_str);
    
    return ESP_OK;
}

/**
 * 文件上传处理器
 */
static esp_err_t upload_post_handler(httpd_req_t *req) {
    char file_name[64];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    
    // 生成文件名
    strftime(file_name, sizeof(file_name), "/spiffs/upload_%Y%m%d_%H%M%S.bin", timeinfo);
    
    FILE *fd = fopen(file_name, "w");
    if (fd == NULL) {
        ESP_LOGE(TAG, "无法创建文件: %s", file_name);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }
    
    // 接收文件数据
    char buf[512];
    int received;
    size_t total_received = 0;
    
    while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, received, fd);
        total_received += received;
    }
    
    fclose(fd);
    
    ESP_LOGI(TAG, "文件上传完成: %s, 大小: %d字节", file_name, total_received);
    
    // 发送响应
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "filename", file_name);
    cJSON_AddNumberToObject(response, "size", total_received);
    
    char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    cJSON_Delete(response);
    free(json_str);
    
    return ESP_OK;
}

/**
 * WebSocket处理器
 */
#ifdef CONFIG_HTTPD_WS_SUPPORT
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket握手请求");
        return ESP_OK;
    }
    
    // WebSocket帧处理
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    // 接收WebSocket数据
    if (httpd_ws_recv_frame(req, &ws_pkt, 0) != ESP_OK) {
        ESP_LOGE(TAG, "WebSocket接收失败");
        return ESP_FAIL;
    }
    
    if (ws_pkt.len) {
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        
        if (httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len) != ESP_OK) {
            free(buf);
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "WebSocket收到: %.*s", ws_pkt.len, ws_pkt.payload);
        
        // 回显消息
        esp_err_t ret = httpd_ws_send_frame(req, &ws_pkt);
        free(buf);
        return ret;
    }
    
    return ESP_OK;
}
#endif

/**
 * 404错误处理器
 */
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err) {
    if (strcmp("/api/", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "API endpoint not found");
    } else {
        // 对于其他路径，返回HTML页面（SPA支持）
        extern const char index_html_start[] asm("_binary_index_html_start");
        extern const char index_html_end[] asm("_binary_index_html_end");
        size_t index_html_size = index_html_end - index_html_start;
        
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, index_html_start, index_html_size);
    }
    return ESP_OK;
}

/**
 * URI处理器表
 */
static const httpd_uri_t uri_handlers[] = {
    // 根路径
    {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_get_handler,
        .user_ctx  = NULL
    },
    // API状态
    {
        .uri       = "/api/status",
        .method    = HTTP_GET,
        .handler   = api_status_handler,
        .user_ctx  = NULL
    },
    // 系统信息
    {
        .uri       = "/api/system",
        .method    = HTTP_GET,
        .handler   = api_system_info_handler,
        .user_ctx  = NULL
    },
    // LED控制
    {
        .uri       = "/api/led",
        .method    = HTTP_POST,
        .handler   = api_led_control_handler,
        .user_ctx  = NULL
    },
    // 文件上传
    {
        .uri       = "/api/upload",
        .method    = HTTP_POST,
        .handler   = upload_post_handler,
        .user_ctx  = NULL
    },
#ifdef CONFIG_HTTPD_WS_SUPPORT
    // WebSocket
    {
        .uri       = "/ws",
        .method    = HTTP_GET,
        .handler   = ws_handler,
        .user_ctx  = NULL,
        .is_websocket = true
    },
#endif
};

/**
 * 启动HTTP服务器
 */
esp_err_t web_server_start(web_server_config_t* config) {
    if (server_handle != NULL) {
        ESP_LOGW(TAG, "HTTP服务器已经在运行");
        return ESP_OK;
    }
    
    memcpy(&server_config, config, sizeof(web_server_config_t));

    
    ESP_LOGI(TAG, "启动HTTP服务器，端口: %d", server_config.port);
    
    // 服务器配置
    httpd_config_t server_cfg = HTTPD_DEFAULT_CONFIG();
    server_cfg.server_port = server_config.port;
    server_cfg.ctrl_port = server_config.port + 1;
    server_cfg.max_uri_handlers = server_config.max_uri_handlers;
    server_cfg.stack_size = server_config.stack_size;
    server_cfg.lru_purge_enable = true;
    server_cfg.uri_match_fn = httpd_uri_match_wildcard;
    
    // 在 web_server_start 函数中，替换以下代码
    // 旧代码
    // server_cfg.err_handler_fns[HTTPD_404_NOT_FOUND] = http_404_error_handler;
    
    // 新代码 - 使用 httpd_register_err_handler 函数注册错误处理器
    esp_err_t ret = httpd_start(&server_handle, &server_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动HTTP服务器失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 注册404错误处理器
    ret = httpd_register_err_handler(server_handle, HTTPD_404_NOT_FOUND, http_404_error_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册404错误处理器失败: %s", esp_err_to_name(ret));
        httpd_stop(server_handle);
        server_handle = NULL;
        return ret;
    }
    
    // 注册URI处理器
    for (int i = 0; i < sizeof(uri_handlers) / sizeof(uri_handlers[0]); i++) {
        ret = httpd_register_uri_handler(server_handle, &uri_handlers[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "注册URI处理器失败: %s", uri_handlers[i].uri);
            httpd_stop(server_handle);
            server_handle = NULL;
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "HTTP服务器启动成功，端口: %d", server_config.port);
    return ESP_OK;
}

/**
 * 停止HTTP服务器
 */
esp_err_t web_server_stop(void) {
    if (server_handle == NULL) {
        ESP_LOGW(TAG, "HTTP服务器未运行");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "停止HTTP服务器");
    esp_err_t ret = httpd_stop(server_handle);
    if (ret == ESP_OK) {
        server_handle = NULL;
    }
    
    return ret;
}

/**
 * 获取查询参数
 */
char* web_server_get_query_param(httpd_req_t* req, const char* param_name) {
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len <= 1) {
        return NULL;
    }
    
    char* buf = malloc(buf_len);
    if (buf == NULL) {
        return NULL;
    }
    
    if (httpd_req_get_url_query_str(req, buf, buf_len) != ESP_OK) {
        free(buf);
        return NULL;
    }
    
    char param_value[64] = {0};
    if (httpd_query_key_value(buf, param_name, param_value, sizeof(param_value)) != ESP_OK) {
        free(buf);
        return NULL;
    }
    
    free(buf);
    return strdup(param_value);
}

/**
 * 发送JSON响应
 */
esp_err_t web_server_send_json_response(httpd_req_t* req, int status_code, const char* json_data) {
    httpd_resp_set_status(req, status_code == 200 ? "200 OK" : 
                         status_code == 400 ? "400 Bad Request" :
                         status_code == 404 ? "404 Not Found" : "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    
    // 设置CORS头（如果需要）
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    
    return httpd_resp_send(req, json_data, strlen(json_data));
}