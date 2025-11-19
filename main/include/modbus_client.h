// main/modbus_client.h
#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H

#include "esp_modbus_master.h"
#include "mbcontroller.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <stdint.h>
#include <stdbool.h>

/* 前向声明：将库的 controller 作为不透明指针使用，避免编译时找不到类型 */
typedef void* mb_controller_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Modbus客户端状态
 */
typedef enum {
    MODBUS_DISCONNECTED = 0,
    MODBUS_CONNECTING,
    MODBUS_CONNECTED,
    MODBUS_ERROR
} modbus_client_state_t;

/**
 * @brief Modbus客户端配置
 */
typedef struct {
    char server_ip[16];        // 服务器地址
    uint16_t server_port;      // 服务器端口
    uint8_t slave_addr;        // 从站地址
    uint32_t timeout_ms;       // 超时时间
    uint32_t retry_interval;   // 重试间隔
    uint8_t max_retries;       // 最大重试次数
} modbus_client_config_t;

/**
 * @brief Modbus客户端句柄
 */
typedef struct {
    mb_controller_t controller;    // Modbus控制器
    modbus_client_config_t config; // 客户端配置
    modbus_client_state_t state;   // 当前状态
    TaskHandle_t comm_task;       // 通信任务
    EventGroupHandle_t event_group; // 事件组
    bool auto_reconnect;          // 自动重连
} modbus_client_t;

/**
 * @brief 事件定义
 */
#define MODBUS_CONNECTED_BIT    BIT0
#define MODBUS_DISCONNECTED_BIT BIT1
#define MODBUS_ERROR_BIT        BIT2

/**
 * @brief 初始化Modbus客户端
 * 
 * @param client 客户端句柄
 * @param config 客户端配置
 * @return esp_err_t 执行结果
 */
esp_err_t modbus_client_init(modbus_client_t *client, const modbus_client_config_t *config);

/**
 * @brief 启动Modbus客户端
 * 
 * @param client 客户端句柄
 * @return esp_err_t 执行结果
 */
esp_err_t modbus_client_start(modbus_client_t *client);

/**
 * @brief 停止Modbus客户端
 * 
 * @param client 客户端句柄
 */
void modbus_client_stop(modbus_client_t *client);

/**
 * @brief 销毁Modbus客户端
 * 
 * @param client 客户端句柄
 */
void modbus_client_destroy(modbus_client_t *client);

/**
 * @brief 读取保持寄存器
 * 
 * @param client 客户端句柄
 * @param start_addr 起始地址
 * @param quantity 数量
 * @param registers 寄存器值数组
 * @return esp_err_t 执行结果
 */
esp_err_t modbus_read_holding_registers(modbus_client_t *client, 
                                       uint16_t start_addr, 
                                       uint16_t quantity, 
                                       uint16_t *registers);

/**
 * @brief 写入单个寄存器
 * 
 * @param client 客户端句柄
 * @param addr 寄存器地址
 * @param value 寄存器值
 * @return esp_err_t 执行结果
 */
esp_err_t modbus_write_single_register(modbus_client_t *client, 
                                      uint16_t addr, 
                                      uint16_t value);

/**
 * @brief 写入多个寄存器
 * 
 * @param client 客户端句柄
 * @param start_addr 起始地址
 * @param quantity 数量
 * @param registers 寄存器值数组
 * @return esp_err_t 执行结果
 */
esp_err_t modbus_write_multiple_registers(modbus_client_t *client, 
                                         uint16_t start_addr, 
                                         uint16_t quantity, 
                                         const uint16_t *registers);

/**
 * @brief 读取输入寄存器
 * 
 * @param client 客户端句柄
 * @param start_addr 起始地址
 * @param quantity 数量
 * @param registers 寄存器值数组
 * @return esp_err_t 执行结果
 */
esp_err_t modbus_read_input_registers(modbus_client_t *client, 
                                     uint16_t start_addr, 
                                     uint16_t quantity, 
                                     uint16_t *registers);

/**
 * @brief 读取线圈
 * 
 * @param client 客户端句柄
 * @param start_addr 起始地址
 * @param quantity 数量
 * @param coils 线圈状态数组
 * @return esp_err_t 执行结果
 */
esp_err_t modbus_read_coils(modbus_client_t *client, 
                           uint16_t start_addr, 
                           uint16_t quantity, 
                           uint8_t *coils);

/**
 * @brief 写入单个线圈
 * 
 * @param client 客户端句柄
 * @param addr 线圈地址
 * @param value 线圈值
 * @return esp_err_t 执行结果
 */
esp_err_t modbus_write_single_coil(modbus_client_t *client, 
                                  uint16_t addr, 
                                  bool value);

/**
 * @brief 写入多个线圈
 * 
 * @param client 客户端句柄
 * @param start_addr 起始地址
 * @param quantity 数量
 * @param coils 线圈状态数组
 * @return esp_err_t 执行结果
 */
esp_err_t modbus_write_multiple_coils(modbus_client_t *client, 
                                     uint16_t start_addr, 
                                     uint16_t quantity, 
                                     const uint8_t *coils);

/**
 * @brief 获取客户端状态
 * 
 * @param client 客户端句柄
 * @return modbus_client_state_t 当前状态
 */
modbus_client_state_t modbus_client_get_state(modbus_client_t *client);

/**
 * @brief 设置自动重连
 * 
 * @param client 客户端句柄
 * @param enable 是否启用自动重连
 */
void modbus_client_set_auto_reconnect(modbus_client_t *client, bool enable);

void test_modbus_tcp_client();

#ifdef __cplusplus
}
#endif

#endif