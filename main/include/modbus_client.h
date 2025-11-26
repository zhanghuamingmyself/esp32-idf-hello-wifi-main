// include/modbus_client.h
#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mbcontroller.h"


// 测试函数
void modbus_tcp_client_task(void *param);

#endif // MODBUS_CLIENT_H