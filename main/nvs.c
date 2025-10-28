#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

typedef struct {
    int32_t restart_count;
    char device_id[32];
    int32_t last_update_time;
    char device_name[32];
} nvs_data_t;

void initialize_nvs() {
    esp_err_t ret = nvs_flash_init();
    // 常见的初始化失败原因：首次烧录或分区表更改
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); // 擦除整个NVS分区
        ret = nvs_flash_init();             // 重新初始化
    }
    ESP_ERROR_CHECK(ret);
}

void app_main(void) {

    nvs_data_t nvs_data = {.restart_count = 0, .device_id = "123456", .device_name = "esp32"};
    printf("nvs_data: %ld, %s, %s\r\n", nvs_data.restart_count, nvs_data.device_id, nvs_data.device_name);

    printf("application start\r\n");
    // 1. 初始化NVS
    initialize_nvs();

    // 2. 打开命名空间
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        printf("Error opening NVS handle: %s", esp_err_to_name(err));
        return;
    }

    // 3. 读取重启计数器
    int32_t restart_count = 0;
    err = nvs_get_i32(handle, "restart_count", &restart_count);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        printf("Error reading from NVS: %s", esp_err_to_name(err));
    } else {
        printf("Restart count: %ld", restart_count);
    }

    // 4. 更新计数器并写入
    restart_count++;
    err = nvs_set_i32(handle, "restart_count", restart_count);
    if (err != ESP_OK) {
        printf("Error writing to NVS: %s", esp_err_to_name(err));
    } else {
        // 5. 提交更改
        err = nvs_commit(handle);
        if (err != ESP_OK) {
            printf("Commit failed: %s", esp_err_to_name(err));
        } else {
            printf("Restart count updated and saved.\r\n");
        }
    }

    // 6. 关闭句柄
    nvs_close(handle);
}