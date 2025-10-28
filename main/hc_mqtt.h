#ifndef __HC_MQTT_H__
#define __HC_MQTT_H__

#include "esp_log.h"
#include "mqtt_client.h"



void mqtt_app_start(void);
void send_mqtt_data(const char *topic, const char *data);
void create_json_example(char **json_str);


#endif
