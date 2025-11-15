#include "../include/ws2812b.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WS2812B";

static rmt_channel_handle_t tx_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;

// WS2812B的时序定义（单位：RMT时钟周期，当前分辨率10MHz，1周期=0.1微秒）
#define WS2812B_T0H 4  // 0码高电平时间，0.4us (典型值0.35us)
#define WS2812B_T0L 8  // 0码低电平时间，0.8us (典型值0.8us)
#define WS2812B_T1H 8  // 1码高电平时间，0.8us (典型值0.7us)
#define WS2812B_T1L 6  // 1码低电平时间，0.6us (典型值0.6us)
#define WS2812B_RESET_DELAY 50 // 复位码延迟，5us (要求>50us)

// RMT字节编码器配置
static rmt_bytes_encoder_config_t bytes_encoder_config = {
    .bit0 = {
        .level0 = 1,
        .duration0 = WS2812B_T0H,
        .level1 = 0,
        .duration1 = WS2812B_T0L,
    },
    .bit1 = {
        .level0 = 1,
        .duration0 = WS2812B_T1H,
        .level1 = 0,
        .duration1 = WS2812B_T1L,
    },
    .flags.msb_first = 1 // WS2812B协议要求高位先发
};

// RMT发送配置
static rmt_transmit_config_t tx_config = {
    .loop_count = 0, // 不循环发送
};

// 全局颜色数组
static ws2812b_color_t led_colors[WS2812B_LED_NUMBERS];

esp_err_t ws2812b_rmt_driver_install(void)
{
    esp_err_t ret = ESP_OK;

    // 1. 配置RMT发送通道
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // 选择默认时钟源
        .gpio_num = GPIO_NUM_48,        // 您的GPIO48
        .mem_block_symbols = 64,       // RMT内存块大小
        .resolution_hz = 10 * 1000 * 1000, // 10MHz分辨率，0.1us/计数
        .trans_queue_depth = 4,         // 传输队列深度
        .flags.invert_out = false,      // 输出信号不反转
        .flags.with_dma = false,        // 不使用DMA（可用于大量灯珠）
    };
    ret = rmt_new_tx_channel(&tx_chan_config, &tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建RMT发送通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 2. 创建字节编码器
    ret = rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建RMT字节编码器失败: %s", esp_err_to_name(ret));
        rmt_del_channel(tx_chan);
        return ret;
    }

    // 3. 启用RMT通道
    ret = rmt_enable(tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用RMT通道失败: %s", esp_err_to_name(ret));
        rmt_del_encoder(led_encoder);
        rmt_del_channel(tx_chan);
        return ret;
    }

    ESP_LOGI(TAG, "WS2812B RMT驱动初始化成功，GPIO: %d", GPIO_NUM_48);
    return ESP_OK;
}

void ws2812b_set_colors(ws2812b_color_t *color_array, uint16_t led_num)
{
    if (color_array == NULL || led_num == 0 || tx_chan == NULL || led_encoder == NULL) {
        ESP_LOGE(TAG, "参数错误或驱动未初始化");
        return;
    }

    // 传输数据（GRB顺序）
    esp_err_t ret = rmt_transmit(tx_chan, led_encoder, (void*)color_array, led_num * 3, &tx_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RMT传输失败: %s", esp_err_to_name(ret));
        return;
    }

    // 等待传输完成
    ret = rmt_tx_wait_all_done(tx_chan, pdMS_TO_TICKS(100)); // 100ms超时
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "等待RMT传输完成时超时或错误: %s", esp_err_to_name(ret));
    }
}

void ws2812b_set_pixel_color(uint16_t index, ws2812b_color_t color)
{
    if (index >= WS2812B_LED_NUMBERS) {
        return;
    }
    led_colors[index] = color;
}

void ws2812b_clear(void)
{
    for (int i = 0; i < WS2812B_LED_NUMBERS; i++) {
        led_colors[i] = (ws2812b_color_t){ .g = 0, .r = 0, .b = 0 };
    }
    ws2812b_set_colors(led_colors, WS2812B_LED_NUMBERS);
}