// main/main.c
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "driver/gpio.h"
#include "usb/usb_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "../include/usb_host.h"
#include "ff.h"
#include <dirent.h> 

static const char *TAG = "USB_UART_READER";

// USB主机配置
#define USB_HOST_PRIORITY          20
#define USB_HOST_EVENT_FLAG        (1 << 0)

// 全局变量
static TaskHandle_t usb_host_task_handle = NULL;
static EventGroupHandle_t usb_event_group = NULL;
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;  // Wear Levelling句柄

/**
 * USB主机事件回调
 */
static void usb_host_lib_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg) {
    if (event_msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        ESP_LOGI(TAG, "新USB设备连接");
        if (usb_event_group) {
            xEventGroupSetBits(usb_event_group, USB_HOST_EVENT_FLAG);
        }
    } else if (event_msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        ESP_LOGI(TAG, "USB设备断开");
        if (usb_event_group) {
            xEventGroupClearBits(usb_event_group, USB_HOST_EVENT_FLAG);
        }
    }
}

/**
 * USB主机任务
 */
static void usb_host_task(void *arg) {
    ESP_LOGI(TAG, "启动USB主机任务");
    
    // USB主机配置
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    
    // 安装USB主机驱动
    esp_err_t ret = usb_host_install(&host_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB主机安装失败: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    // 客户端配置
    usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = usb_host_lib_event_callback,
            .callback_arg = NULL
        }
    };
    
    usb_host_client_handle_t client_handle;
    ret = usb_host_client_register(&client_config, &client_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB客户端注册失败: %s", esp_err_to_name(ret));
        usb_host_uninstall();
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "USB主机任务启动成功");
    
    // 事件循环
    while (1) {
        uint32_t event_flags;  // 修复：添加事件标志变量
        
        // 修复：正确处理USB事件
        ret = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "USB库事件处理失败: %s", esp_err_to_name(ret));
        }
        
        // 处理客户端事件
        ret = usb_host_client_handle_events(client_handle, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "USB客户端事件处理失败: %s", esp_err_to_name(ret));
        }
        
        // 短暂延迟，避免忙等待
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // 清理（通常不会执行到这里）
    usb_host_client_deregister(client_handle);
    usb_host_uninstall();
    vTaskDelete(NULL);
}

/**
 * 初始化USB主机
 */
esp_err_t init_usb_host(void) {
    ESP_LOGI(TAG, "初始化USB主机");
    
    // 创建事件组
    usb_event_group = xEventGroupCreate();
    if (usb_event_group == NULL) {
        ESP_LOGE(TAG, "创建事件组失败");
        return ESP_FAIL;
    }
    
    // 创建USB主机任务
    BaseType_t result = xTaskCreatePinnedToCore(
        usb_host_task,
        "usb_host",
        4096,
        NULL,
        USB_HOST_PRIORITY,
        &usb_host_task_handle,
        0
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "创建USB主机任务失败");
        vEventGroupDelete(usb_event_group);
        usb_event_group = NULL;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "USB主机初始化完成");
    return ESP_OK;
}

/**
 * 等待USB设备连接
 */
static bool wait_for_usb_device(int timeout_ms) {
    ESP_LOGI(TAG, "等待USB设备连接...");
    
    EventBits_t bits = xEventGroupWaitBits(
        usb_event_group,
        USB_HOST_EVENT_FLAG,
        pdFALSE, // 不清除位
        pdTRUE,  // 等待所有位
        pdMS_TO_TICKS(timeout_ms)
    );
    
    return (bits & USB_HOST_EVENT_FLAG) != 0;
}

/**
 * 挂载U盘
 */
static esp_err_t mount_usb_drive(const char* base_path) {
    ESP_LOGI(TAG, "尝试挂载U盘到: %s", base_path);
    
    // 挂载配置
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };
    
    // 尝试挂载
    esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &s_wl_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "U盘挂载成功: %s", base_path);
        return ESP_OK;
    }
    

    ESP_LOGE(TAG, "所有挂载方法都失败: %s", esp_err_to_name(ret));
    return ret;
}

/**
 * 卸载U盘
 */
static esp_err_t unmount_usb_drive(const char* base_path) {
    ESP_LOGI(TAG, "卸载U盘: %s", base_path);
    
    // 修复：使用正确的句柄
    esp_err_t ret = esp_vfs_fat_spiflash_unmount_rw_wl(base_path, s_wl_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "U盘卸载成功");
        s_wl_handle = WL_INVALID_HANDLE;
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "卸载失败: %s", esp_err_to_name(ret));
    return ret;
}

/**
 * 列出U盘中的文件
 */
static void list_usb_files(const char* base_path) {
    ESP_LOGI(TAG, "列出U盘文件: %s", base_path);
    
    DIR *dir = opendir(base_path);
    if (dir == NULL) {
        ESP_LOGE(TAG, "无法打开目录: %s", base_path);
        return;
    }
    
    struct dirent *entry;
    int file_count = 0;
    int dir_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // 跳过当前目录和上级目录
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        if (entry->d_type == DT_REG) { // 常规文件
            ESP_LOGI(TAG, "文件: %s", entry->d_name);
            file_count++;
        } else if (entry->d_type == DT_DIR) { // 目录
            ESP_LOGI(TAG, "目录: %s/", entry->d_name);
            dir_count++;
        }
    }
    
    closedir(dir);
    ESP_LOGI(TAG, "找到 %d 个文件, %d 个目录", file_count, dir_count);
}

/**
 * 读取U盘文件内容
 */
static esp_err_t read_usb_file(const char* file_path) {
    ESP_LOGI(TAG, "读取文件: %s", file_path);
    
    FILE* file = fopen(file_path, "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "无法打开文件: %s", file_path);
        return ESP_FAIL;
    }
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    ESP_LOGI(TAG, "文件大小: %ld 字节", file_size);
    
    if (file_size > 0) {
        // 读取文件内容（限制大小避免内存问题）
        size_t read_size = (file_size < 1024) ? file_size : 1024;
        char* buffer = malloc(read_size + 1);
        
        if (buffer) {
            size_t bytes_read = fread(buffer, 1, read_size, file);
            buffer[bytes_read] = '\0';
            
            ESP_LOGI(TAG, "文件内容(前%zu字节):", bytes_read);
            // 安全输出，避免包含控制字符
            for (size_t i = 0; i < bytes_read; i++) {
                if (buffer[i] >= 32 && buffer[i] <= 126) { // 可打印ASCII字符
                    printf("%c", buffer[i]);
                } else {
                    printf("."); // 非打印字符用点代替
                }
            }
            printf("\n");
            
            free(buffer);
        } else {
            ESP_LOGE(TAG, "内存分配失败");
        }
    } else {
        ESP_LOGW(TAG, "空文件或文件大小为0");
    }
    
    fclose(file);
    return ESP_OK;
}

/**
 * 使用FatFS API读取文件（替代方法）
 */
static esp_err_t read_usb_file_fatfs(const char* file_path) {
    ESP_LOGI(TAG, "使用FatFS读取文件: %s", file_path);
    
    FIL file;
    FRESULT res = f_open(&file, file_path, FA_READ);
    if (res != FR_OK) {
        ESP_LOGE(TAG, "无法打开文件: %s, 错误: %d", file_path, res);
        return ESP_FAIL;
    }
    
    // 获取文件大小
    FSIZE_t file_size = f_size(&file);
    ESP_LOGI(TAG, "文件大小: %lu 字节", (unsigned long)file_size);
    
    if (file_size > 0) {
        // 读取文件内容
        char* buffer = malloc(file_size + 1);
        if (buffer == NULL) {
            f_close(&file);
            return ESP_ERR_NO_MEM;
        }
        
        UINT bytes_read = 0;
        res = f_read(&file, buffer, file_size, &bytes_read);
        if (res != FR_OK) {
            ESP_LOGE(TAG, "读取文件失败: %d", res);
            free(buffer);
            f_close(&file);
            return ESP_FAIL;
        }
        
        buffer[bytes_read] = '\0';
        
        // 输出内容
        size_t output_size = (bytes_read < 512) ? bytes_read : 512;
        ESP_LOGI(TAG, "FatFS文件内容(前%zu字节):", output_size);
        ESP_LOGI(TAG, "%.*s", output_size, buffer);
        
        free(buffer);
    }
    
    f_close(&file);
    return ESP_OK;
}

/**
 * 检查文件是否存在
 */
static bool file_exists(const char* file_path) {
    FILE* file = fopen(file_path, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

/**
 * U盘监控任务
 */
static void usb_monitor_task(void *arg) {
    ESP_LOGI(TAG, "启动U盘监控任务");
    
    const char* mount_point = "/usb";
    uint32_t monitor_count = 0;
    
    while (1) {
        monitor_count++;
        ESP_LOGI(TAG, "=== 监控周期 %lu ===", monitor_count);
        
        // 等待USB设备连接
        if (wait_for_usb_device(10000)) {
            ESP_LOGI(TAG, "检测到USB设备连接");
            
            // 尝试挂载U盘
            esp_err_t ret = mount_usb_drive(mount_point);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "U盘挂载成功，开始读取文件");
                
                // 列出文件
                list_usb_files(mount_point);
                
                // 尝试读取示例文件
                const char* test_files[] = {
                    "/usb/test.txt",
                    "/usb/readme.txt", 
                    "/usb/README.md",
                    "/usb/data.txt",
                    NULL
                };
                
                for (int i = 0; test_files[i] != NULL; i++) {
                    if (file_exists(test_files[i])) {
                        ESP_LOGI(TAG, "找到文件: %s", test_files[i]);
                        read_usb_file(test_files[i]);
                    } else {
                        ESP_LOGW(TAG, "文件不存在: %s", test_files[i]);
                    }
                }
                
                // 保持挂载状态一段时间
                ESP_LOGI(TAG, "保持挂载状态5秒...");
                vTaskDelay(pdMS_TO_TICKS(5000));
                
                // 卸载U盘
                ret = unmount_usb_drive(mount_point);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "U盘卸载成功");
                } else {
                    ESP_LOGE(TAG, "U盘卸载失败: %s", esp_err_to_name(ret));
                }
            } else {
                ESP_LOGE(TAG, "U盘挂载失败: %s", esp_err_to_name(ret));
            }
            
            // 清除事件标志，等待下一次连接
            xEventGroupClearBits(usb_event_group, USB_HOST_EVENT_FLAG);
        } else {
            ESP_LOGI(TAG, "未检测到USB设备，继续监控...");
        }
        
        // 每10个周期报告一次系统状态
        if (monitor_count % 10 == 0) {
            ESP_LOGI(TAG, "系统状态 - 空闲内存: %d字节, 最小空闲: %d字节", 
                    esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * 清理资源
 */
static void cleanup_resources(void) {
    ESP_LOGI(TAG, "清理资源");
    
    // 停止USB主机任务
    if (usb_host_task_handle != NULL) {
        vTaskDelete(usb_host_task_handle);
        usb_host_task_handle = NULL;
    }
    
    // 删除事件组
    if (usb_event_group != NULL) {
        vEventGroupDelete(usb_event_group);
        usb_event_group = NULL;
    }
    
    // 确保卸载文件系统
    if (s_wl_handle != WL_INVALID_HANDLE) {
        esp_vfs_fat_spiflash_unmount_rw_wl("/usb", s_wl_handle);
        s_wl_handle = WL_INVALID_HANDLE;
    }
}

void usb_host_main(void) {
    ESP_LOGI(TAG, "=== ESP32 USB U盘读取器启动 ===");
    
    // 注册清理函数（在程序退出时调用）
    atexit(cleanup_resources);
    
    // 初始化USB主机
    esp_err_t ret = init_usb_host();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB主机初始化失败");
        return;
    }
    
    // 创建U盘监控任务
    BaseType_t task_ret = xTaskCreate(
        usb_monitor_task, 
        "usb_monitor", 
        4096, 
        NULL, 
        5, 
        NULL
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "创建U盘监控任务失败");
        cleanup_resources();
        return;
    }
    
    ESP_LOGI(TAG, "系统启动完成，等待USB设备连接...");
    
    // 主循环保持运行
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
