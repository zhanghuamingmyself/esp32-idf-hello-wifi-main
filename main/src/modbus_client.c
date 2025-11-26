// main/src/modbus_client.c
#include "../include/modbus_client.h"
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbcontroller.h"

static const char *TAG = "MODBUS_MINIMAL";

// #define MB_SERVER_IP "10.101.69.45"
#define MB_SERVER_PORT 502
#define START_REG 800
#define NUM_REGS 14

const static char *ip_table[2] = {"1;10.101.69.42;502",NULL};

void read_modbus_registers(void) {
    ESP_LOGI(TAG, "启动Modbus...");
    
    // 配置TCP地址
    
    // asprintf(&ip_table[0], "%s;%d", MB_SERVER_IP, MB_SERVER_PORT);
    
    // 配置通信参数
    mb_communication_info_t comm = {
        .tcp_opts.port = MB_SERVER_PORT,
        .tcp_opts.mode = MB_TCP,
        .tcp_opts.addr_type = MB_IPV4,
        .tcp_opts.ip_addr_table = ip_table,
        // .tcp_opts.ip_netif_ptr = (void*)get_example_netif(),
        .tcp_opts.uid = 1,
    };
    
    void *master = NULL;
    esp_err_t ret = mbc_master_create_tcp(&comm, &master);
    
    if (ret == ESP_OK) {
        ret = mbc_master_start(master);
        if (ret == ESP_OK) {
            uint16_t buffer[NUM_REGS];
            mb_param_request_t req = {
                .slave_addr = 1,
                .command = 0x03, // 读取保持寄存器
                .reg_start = START_REG,
                .reg_size = NUM_REGS
            };
             ESP_LOGI(TAG, "开始读取Modbus寄存器...");
            ret = mbc_master_send_request(master, &req, buffer);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "读取成功:");
                for (int i = 0; i < NUM_REGS; i++) {
                    printf("Reg %d: %d\n", START_REG + i, buffer[i]);
                }
            }else {
                ESP_LOGE(TAG, "读取寄存器失败: %d", ret);
            }
        }else {
            ESP_LOGE(TAG, "Modbus启动失败: %d", ret);
        }
        mbc_master_stop(master);
        mbc_master_delete(master);
    }else {
        ESP_LOGE(TAG, "Modbus TCP创建失败: %d", ret);
    }
}

void modbus_tcp_client_task(void *param) {
    // 或者周期性读取
    while(1) {
        read_modbus_registers();
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10秒间隔
    }
}