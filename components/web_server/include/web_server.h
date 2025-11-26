// main/components/web_server/include/web_server.h
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 服务器配置
typedef struct {
    uint16_t port;
    size_t stack_size;
    UBaseType_t task_priority;
    size_t max_uri_handlers;
} web_server_config_t;


// 服务器句柄
typedef struct web_server_s web_server_t;

// API响应结构
typedef struct {
    int code;
    char* message;
    char* data;
} api_response_t;

// 函数声明
esp_err_t web_server_start(web_server_config_t* config);
esp_err_t web_server_stop(void);
web_server_t* web_server_get_handle(void);

// REST API函数
esp_err_t web_server_register_api_handler(const char* uri, httpd_method_t method, 
                                          esp_err_t (*handler)(httpd_req_t* req));
// 静态文件服务
esp_err_t web_server_register_static_handler(const char* base_path, const char* root_path);

// 工具函数
char* web_server_get_query_param(httpd_req_t* req, const char* param_name);
esp_err_t web_server_send_json_response(httpd_req_t* req, int status_code, const char* json_data);
esp_err_t web_server_send_file_response(httpd_req_t* req, const char* file_path, const char* content_type);

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H