#include "../include/hc_mqtt.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "../include/hc_gobal.h"


static const char *TAG = "hc_mqtt";

esp_mqtt_client_handle_t client;

static char sn[] = "24367029320240308";
static char pwd[] = "1e58d9979963446749f42fbe89be7c23";
static char client_id[] = "sn-24367029320240308";
static char uri[] = "mqtts://pre-cn-gather.hero-ee.com:10086";

static void create_json_example(char **json_str) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "f", 1);
    cJSON_AddStringToObject(root, "sn", sn);


    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "44", "2");
    cJSON_AddNumberToObject(data, "45", 26.8);
    cJSON_AddBoolToObject(data, "fan_on", true);

    cJSON *sensors = cJSON_CreateArray();
    cJSON_AddItemToArray(sensors, cJSON_CreateNumber(23.5));
    cJSON_AddItemToArray(sensors, cJSON_CreateNumber(24.1));
    cJSON_AddItemToObject(data, "sensors", sensors);
    
    cJSON_AddItemToObject(root, "data", data);
    
    *json_str = cJSON_PrintUnformatted(root);
    
    cJSON_Delete(root);
}

static void create_45res(double color,char **json_str) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "f", 1);
    cJSON_AddStringToObject(root, "sn", sn);


    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "45", color);

    cJSON_AddItemToObject(root, "data", data);
    *json_str = cJSON_PrintUnformatted(root);
    
    cJSON_Delete(root);
}

// MQTT事件处理函数
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED\r\n");
            // 连接成功后订阅主题（可选）
            char topic_subscribe[64];
            snprintf(topic_subscribe, sizeof(topic_subscribe), "/things/down/%s", sn);
            ESP_LOGI(TAG, "Subscribing to topic: %s\r\n", topic_subscribe);
            esp_mqtt_client_subscribe(client, topic_subscribe, 0);

        
            // char data[50];
    
            // // 模拟传感器数据（实际应用中替换为真实传感器读数）
            // float temperature = 25.6;
            // float humidity = 60.4;
            // snprintf(data, sizeof(data), "{\"temp\":%.1f, \"hum\":%.1f}", temperature, humidity);

            char* json_str = NULL;
            create_json_example(&json_str);
            ESP_LOGI(TAG, "json_str=%s\r\n", json_str);
            // 发布数据
            {
                char topic_publish[64];
                snprintf(topic_publish, sizeof(topic_publish), "/things/up/%s", sn);
                send_mqtt_data(topic_publish, json_str);
            }
            // 释放JSON字符串内存，避免内存泄漏
            free(json_str);

            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED\r\n");
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d\r\n", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d\r\n", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d\r\n", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA\r\n");  
            ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s\r\n", event->data_len, event->data);

            /* event->data is not guaranteed to be null-terminated — make a copy and terminate it */
            char *data_copy = malloc(event->data_len + 1);
            if (!data_copy) {
                ESP_LOGE(TAG, "malloc failed for data_copy");
                break;
            }
            memcpy(data_copy, event->data, event->data_len);
            data_copy[event->data_len] = '\0';

            cJSON *root = cJSON_Parse(data_copy);
            if (root == NULL) {
                ESP_LOGI(TAG, "json parse error\r\n");
                free(data_copy);
                break;
            }

            cJSON *f = cJSON_GetObjectItem(root, "f");
            cJSON *v = cJSON_GetObjectItem(root, "v");
            if (f) ESP_LOGI(TAG, "f=%d", f->valueint);
            if (v && v->valuestring) ESP_LOGI(TAG, "v=%s", v->valuestring);

            cJSON *d = cJSON_GetObjectItem(root, "data");
            cJSON *key = cJSON_GetObjectItem(d,"45");
            if(key != NULL)
            {
                ESP_LOGI(TAG, "key=%.3f\r\n", key->valuedouble);

                char* json_str = NULL;
                create_45res(key->valuedouble, &json_str);
                // 发布数据
                {
                    char topic_publish[64];
                    snprintf(topic_publish, sizeof(topic_publish), "/things/up/%s", sn);
                    send_mqtt_data(topic_publish, json_str);
                }
                free(json_str);

                ws2812b_color_t color = { 
                .g = key->valuedouble, 
                .r = 0, 
                .b = 0 
                };
                xQueueSend(colorCmdQueue, &color, portMAX_DELAY);
            }

            cJSON_Delete(root);
            free(data_copy);

            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR\r\n");
            break;
            
        default:
            ESP_LOGI(TAG, "Other event id:%d\r\n", event->event_id);
            break;
    }
}

static void mqtt_task(void *arg) {
     // 初始化全局CA存储（如果尚未初始化）
    esp_err_t ret = esp_tls_init_global_ca_store();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize global CA store: %s", esp_err_to_name(ret));
    }
    
    esp_mqtt_client_config_t mqtt_cfg = {
         .broker = {
            .address.uri = uri,
            .verification = {
                .skip_cert_common_name_check = true, // 跳过CN检查
            }
        },
        .credentials = {
            .client_id = client_id,
            .username = sn,
            .authentication = {
                .password = pwd,
            }
        },
        .session = {
            .last_will = {
                .topic = "esp32/status",
                .msg = "offline",
                .msg_len = 7,
                .qos = 1,
                .retain = 1
            }
        },
        .network = {
            .reconnect_timeout_ms = 60 * 1000,
            .timeout_ms = 10000
        }
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// 初始化MQTT客户端
void mqtt_app_start(void) {
    xTaskCreate(&mqtt_task, "mqtt_task", 2048, NULL, 5, NULL);
}

// 发送MQTT数据函数
void send_mqtt_data(const char *topic, const char *data) {
    int msg_id = esp_mqtt_client_publish(client, topic, data, 0, 1, 0);
    ESP_LOGI(TAG, "my send [%s]: %s (msg_id=%d)\r\n", topic, data, msg_id);
}