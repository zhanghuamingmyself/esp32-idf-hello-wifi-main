#include "hc_mqtt.h"
#include "cJSON.h"

static const char *TAG = "MQTT_EXAMPLE";

esp_mqtt_client_handle_t client;

// MQTT事件处理函数
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            printf("MQTT_EVENT_CONNECTED\r\n");
            // 连接成功后订阅主题（可选）
            esp_mqtt_client_subscribe(client, "/things/down", 0);
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            printf("MQTT_EVENT_DISCONNECTED\r\n");
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            printf("MQTT_EVENT_SUBSCRIBED, msg_id=%d\r\n", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            printf("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d\r\n", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            printf("MQTT_EVENT_PUBLISHED, msg_id=%d\r\n", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            printf("MQTT_EVENT_DATA\r\n");  
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);

            cJSON *root = cJSON_Parse(event->data);
            if (root == NULL) {
                printf("json parse error\r\n");
                break;
            }

            cJSON *f = cJSON_GetObjectItem(root, "f");
            cJSON *v = cJSON_GetObjectItem(root, "v");
            printf("f=%d, v=%s\r\n", f->valueint, v->valuestring);

            cJSON *d = cJSON_GetObjectItem(root, "data");
            cJSON *key = cJSON_GetObjectItem(d,"key");
            if(key != NULL)
            {
                printf("key=%s\r\n", key->valuestring);
            }

            cJSON_Delete(root);

            // char data[50];
    
            // // 模拟传感器数据（实际应用中替换为真实传感器读数）
            // float temperature = 25.6;
            // float humidity = 60.4;
            // snprintf(data, sizeof(data), "{\"temp\":%.1f, \"hum\":%.1f}", temperature, humidity);

            char* json_str = NULL;
            create_json_example(&json_str);
            printf("json_str=%s\r\n", json_str);
            // 发布数据
            send_mqtt_data("/things/up", json_str);
            // 释放JSON字符串内存，避免内存泄漏
            free(json_str);
            break;
            
        case MQTT_EVENT_ERROR:
            printf("MQTT_EVENT_ERROR\r\n");
            break;
            
        default:
            printf("Other event id:%d\r\n", event->event_id);
            break;
    }
}

    void create_json_example(char **json_str) {
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddStringToObject(root, "device", "ESP32-S3");
    cJSON_AddNumberToObject(root, "temp", 26.8);
    cJSON_AddBoolToObject(root, "fan_on", true);
    
    cJSON *sensors = cJSON_CreateArray();
    cJSON_AddItemToArray(sensors, cJSON_CreateNumber(23.5));
    cJSON_AddItemToArray(sensors, cJSON_CreateNumber(24.1));
    cJSON_AddItemToObject(root, "sensors", sensors);
    
    *json_str = cJSON_PrintUnformatted(root);
    
    cJSON_Delete(root);
}

// 初始化MQTT客户端
 void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://172.24.73.22:21883", // 替换为你的MQTT服务器地址
        // 如果需要认证，添加以下配置：
        .credentials.username = "admin",
        .credentials.authentication.password = "hc123456",
        // .credentials.authentication.client_id = "hithium-cloud-gather-server-zhm",
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

// 发送MQTT数据函数
void send_mqtt_data(const char *topic, const char *data) {
    int msg_id = esp_mqtt_client_publish(client, topic, data, 0, 1, 0);
    printf("my send [%s]: %s (msg_id=%d)\r\n", topic, data, msg_id);
}