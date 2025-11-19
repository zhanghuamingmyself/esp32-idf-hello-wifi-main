// main/modbus_client.c
#include "../include/modbus_client.h"
#include "string.h"

static const char *TAG = "MODBUS_CLIENT";

// Modbus通信任务
static void modbus_communication_task(void *pvParameter) {
    modbus_client_t *client = (modbus_client_t *)pvParameter;
    ESP_LOGI(TAG, "Modbus通信任务启动");
    
    while (1) {
        switch (client->state) {
            case MODBUS_DISCONNECTED:
                ESP_LOGI(TAG, "尝试连接Modbus服务器...");
                if (modbus_client_start(client) == ESP_OK) {
                    client->state = MODBUS_CONNECTED;
                    xEventGroupSetBits(client->event_group, MODBUS_CONNECTED_BIT);
                    ESP_LOGI(TAG, "Modbus连接成功");
                } else {
                    client->state = MODBUS_ERROR;
                    xEventGroupSetBits(client->event_group, MODBUS_ERROR_BIT);
                    ESP_LOGE(TAG, "Modbus连接失败");
                }
                break;
                
            case MODBUS_CONNECTED:
                // 保持连接状态，等待操作
                ESP_LOGI(TAG, "Modbus已连接，等待操作...");
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
                
            case MODBUS_ERROR:
                if (client->auto_reconnect) {
                    ESP_LOGI(TAG, "%d秒后尝试重新连接...", client->config.retry_interval / 1000);
                    vTaskDelay(pdMS_TO_TICKS(client->config.retry_interval));
                    client->state = MODBUS_DISCONNECTED;
                } else {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                break;
                
            default:
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
        }
        uint16_t read_buffer[8];
        modbus_read_input_registers(client, 199, 2, read_buffer);
        ESP_LOGI(TAG, "读取输入寄存器:");
        for (int i = 0; i < 8; i++) {
            ESP_LOGI(TAG, "寄存器 %d: %d", 199 + i, read_buffer[i]);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    
}

esp_err_t modbus_client_init(modbus_client_t *client, const modbus_client_config_t *config) {
    if (client == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "初始化Modbus客户端");
    
    // 初始化客户端结构
    memset(client, 0, sizeof(modbus_client_t));
    memcpy(&client->config, config, sizeof(modbus_client_config_t));
    client->state = MODBUS_DISCONNECTED;
    client->auto_reconnect = true;
    
    // 创建事件组
    client->event_group = xEventGroupCreate();
    if (client->event_group == NULL) {
        ESP_LOGE(TAG, "创建事件组失败");
        return ESP_FAIL;
    }
    
    // 设置默认值
    if (client->config.server_port == 0) {
        client->config.server_port = 502;  // 默认Modbus端口
    }
    if (client->config.timeout_ms == 0) {
        client->config.timeout_ms = 5000;  // 默认5秒超时
    }
    if (client->config.retry_interval == 0) {
        client->config.retry_interval = 10000;  // 默认10秒重试
    }
    if (client->config.max_retries == 0) {
        client->config.max_retries = 3;  // 默认重试3次
    }
    
    ESP_LOGI(TAG, "Modbus客户端初始化完成");
    ESP_LOGI(TAG, "服务器: %s:%d, 从站地址: %d", 
             client->config.server_ip, client->config.server_port, client->config.slave_addr);
    
    return ESP_OK;
}

const char *slave_tcp_addr_table[] = {
    "01;10.101.69.56;502",     // Corresponds to characteristic MB_DEVICE_ADDR2
    NULL                            // End of table condition (must be included)
};

// 用新的 esp-modbus API 创建 TCP 主站并保存 ctx（master_handler）
esp_err_t modbus_client_start(modbus_client_t *client) {
    if (!client) return ESP_ERR_INVALID_ARG;

   // 配置通信参数
    mb_tcp_opts_t tcp_opts = {
            .mode = MB_TCP,
            .addr_type = MB_IPV4,
            .ip_addr_table = (void *)slave_tcp_addr_table,
            .port = client->config.server_port,
            .uid = client->config.slave_addr       // 从站地址（Modbus TCP中通常不使用，但协议中保留，可设为0）
    };

    mb_communication_info_t comm_info = (mb_communication_info_t)tcp_opts;
    

    ESP_LOGI(TAG, "测试1 %s:%d", 
             client->config.server_ip, client->config.server_port);

    void *master_ctx = NULL;
    esp_err_t ret = mbc_master_create_tcp(&comm_info, &master_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mbc_master_create_tcp failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* mb_controller_t is an opaque controller handle in this SDK — store ctx directly */
    client->controller = (mb_controller_t)master_ctx;

    ret = mbc_master_start(master_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mbc_master_start failed: %s", esp_err_to_name(ret));
        mbc_master_delete(master_ctx);
        client->controller = (mb_controller_t)NULL;
        return ret;
    }

    // 若需要设置 descriptor:
    // ret = mbc_master_set_descriptor(client->controller.master, descriptor, num);
    // if (ret != ESP_OK) { ... }

    // 创建通信任务等...
    return ESP_OK;
}

// 停止/销毁
void modbus_client_stop(modbus_client_t *client) {
    if (client == NULL) return;

    // 删除任务等...
    if (client->controller) {
        mbc_master_stop((void*)client->controller);
        mbc_master_delete((void*)client->controller);
        client->controller = (mb_controller_t)NULL;
    }
    // 其它清理...
}

// 通用操作：构造 mb_param_request_t 并调用 mbc_master_send_request
static esp_err_t modbus_operation(modbus_client_t *client, uint8_t mb_command,
                                  uint16_t start_addr, uint16_t quantity,
                                  void *data, uint8_t *error_code) {
    if (!client || client->controller == (mb_controller_t)NULL || !data) return ESP_ERR_INVALID_ARG;

    mb_param_request_t req = {
        .slave_addr = client->config.slave_addr,
        .command = mb_command,
        .reg_start = start_addr,
        .reg_size = quantity,
    };

    esp_err_t ret = mbc_master_send_request((void*)client->controller, &req, data);

    if (error_code) *error_code = 0; // 如需异常码，依据库的返回机制获取

    if (ret != ESP_OK) {
        client->state = MODBUS_ERROR;
        // 设置事件位等...
    }
    return ret;
}

esp_err_t modbus_read_holding_registers(modbus_client_t *client, 
                                       uint16_t start_addr, 
                                       uint16_t quantity, 
                                       uint16_t *registers) {
    ESP_LOGD(TAG, "读取保持寄存器: 地址=%d, 数量=%d", start_addr, quantity);
    return modbus_operation(client, MB_PARAM_HOLDING, start_addr, quantity, registers, NULL);
}

esp_err_t modbus_write_single_register(modbus_client_t *client, 
                                      uint16_t addr, 
                                      uint16_t value) {
    ESP_LOGD(TAG, "写入单个寄存器: 地址=%d, 值=0x%04X", addr, value);
    return modbus_operation(client, MB_PARAM_HOLDING, addr, 1, &value, NULL);
}

esp_err_t modbus_write_multiple_registers(modbus_client_t *client, 
                                         uint16_t start_addr, 
                                         uint16_t quantity, 
                                         const uint16_t *registers) {
    ESP_LOGD(TAG, "写入多个寄存器: 地址=%d, 数量=%d", start_addr, quantity);
    
    // 注意：需要复制数据，因为modbus_operation会修改数据
    uint16_t *temp_registers = malloc(quantity * sizeof(uint16_t));
    if (temp_registers == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    memcpy(temp_registers, registers, quantity * sizeof(uint16_t));
    esp_err_t ret = modbus_operation(client, MB_PARAM_HOLDING, start_addr, quantity, temp_registers, NULL);
    free(temp_registers);
    
    return ret;
}

esp_err_t modbus_read_input_registers(modbus_client_t *client, 
                                     uint16_t start_addr, 
                                     uint16_t quantity, 
                                     uint16_t *registers) {
    ESP_LOGD(TAG, "读取输入寄存器: 地址=%d, 数量=%d", start_addr, quantity);
    return modbus_operation(client, MB_PARAM_INPUT, start_addr, quantity, registers, NULL);
}

esp_err_t modbus_read_coils(modbus_client_t *client, 
                           uint16_t start_addr, 
                           uint16_t quantity, 
                           uint8_t *coils) {
    ESP_LOGD(TAG, "读取线圈: 地址=%d, 数量=%d", start_addr, quantity);
    return modbus_operation(client, MB_PARAM_COIL, start_addr, quantity, coils, NULL);
}

esp_err_t modbus_write_single_coil(modbus_client_t *client, 
                                  uint16_t addr, 
                                  bool value) {
    ESP_LOGD(TAG, "写入单个线圈: 地址=%d, 值=%s", addr, value ? "ON" : "OFF");
    
    uint16_t coil_value = value ? 0xFF00 : 0x0000;
    return modbus_operation(client, MB_PARAM_COIL, addr, 1, &coil_value, NULL);
}

esp_err_t modbus_write_multiple_coils(modbus_client_t *client, 
                                     uint16_t start_addr, 
                                     uint16_t quantity, 
                                     const uint8_t *coils) {
    ESP_LOGD(TAG, "写入多个线圈: 地址=%d, 数量=%d", start_addr, quantity);
    
    // 复制线圈数据
    uint8_t *temp_coils = malloc(quantity);
    if (temp_coils == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    memcpy(temp_coils, coils, quantity);
    esp_err_t ret = modbus_operation(client, MB_PARAM_COIL, start_addr, quantity, temp_coils, NULL);
    free(temp_coils);
    
    return ret;
}

modbus_client_state_t modbus_client_get_state(modbus_client_t *client) {
    if (client == NULL) {
        return MODBUS_ERROR;
    }
    return client->state;
}

void modbus_client_set_auto_reconnect(modbus_client_t *client, bool enable) {
    if (client != NULL) {
        client->auto_reconnect = enable;
    }
}


void test_modbus_tcp_client(){
    static modbus_client_t modbus_client;
    modbus_client_config_t config = {
        .server_ip = "10.101.69.56",
        .server_port = 502,
        .slave_addr = 1,

        .timeout_ms = 5000,
        .retry_interval = 10000,
        .max_retries = 3
    };
    esp_err_t ret = modbus_client_init(&modbus_client, &config);
    if (ret) {
        ESP_LOGE(TAG, "modbus_client_initr error, error code = %x", ret);
        return;
    }
    ret = modbus_client_start(&modbus_client);
    if (ret) {
        ESP_LOGE(TAG, "modbus_client_start error, error code = %x", ret);
        return;
    }
    // The LED task is used to show the connection status
    xTaskCreate(&modbus_communication_task, "modbus_communication_task", 4096, &modbus_client, 5, NULL);
}