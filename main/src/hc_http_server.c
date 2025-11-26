// main/main.c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "../include/hc_http_server.h"

#include "web_server.h"

static const char *TAG = "hc_http_server";

/**
 * 启动HTTP服务器
 */
static void start_http_server(void)
{
    // 配置HTTP服务器
    web_server_config_t server_config = {
        .port = 80,
        .stack_size = 8192,
        .task_priority = 5,
        .max_uri_handlers = 20};
    server_config.port = 80;
    server_config.stack_size = 8192;

    esp_err_t ret = web_server_start(&server_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "启动HTTP服务器失败: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "HTTP服务器启动成功");
        // ESP_LOGI(TAG, "服务器地址: http://%s",
        //          esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"))->ip.addr);
    }
}

/**
 * 初始化SPIFFS文件系统
 */
static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "初始化SPIFFS文件系统");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "挂载SPIFFS失败");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "未找到SPIFFS分区");
        }
        else
        {
            ESP_LOGE(TAG, "SPIFFS初始化失败: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "获取SPIFFS信息失败: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "SPIFFS分区大小: 总大小: %d, 已用: %d", total, used);
    }

    return ESP_OK;
}

/**
 * 系统信息任务
 */
static void system_monitor_task(void *pvParameter)
{
    while (1)
    {
        ESP_LOGI("SYSTEM", "系统状态 - 空闲内存: %d字节, 最小空闲: %d字节, 任务数: %d",
                 esp_get_free_heap_size(),
                 esp_get_minimum_free_heap_size(),
                 uxTaskGetNumberOfTasks());

        vTaskDelay(pdMS_TO_TICKS(30000)); // 每30秒报告一次
    }
}

void test_http_server_task(void *param)
{
    ESP_LOGI(TAG, "=== ESP32 HTTP服务器启动 ===");

    // 初始化SPIFFS
    init_spiffs();

    start_http_server();

    // 创建系统监控任务
    xTaskCreate(system_monitor_task, "system_monitor", 2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "http应用初始化完成");

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}