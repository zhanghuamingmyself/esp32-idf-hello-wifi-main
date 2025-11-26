#ifndef __HC_HTTP_H__
#define __HC_HTTP_H__
#include "esp_http_client.h"
#include "esp_log.h"

void http_get_task(void *pvParameters);

void http_server_task(void *pvParameters);

#endif
