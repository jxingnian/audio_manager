// 引入标准字符串处理库，用于如memcpy、memset等操作
#include <string.h>

// 引入FreeRTOS操作系统的头文件，提供任务管理等功能
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP-IDF日志库，用于输出调试和运行日志
#include "esp_log.h"

// ESP-ADF音频框架相关头文件
#include "audio_element.h"        // 音频元素相关API
#include "audio_pipeline.h"       // 音频流水线相关API
#include "audio_event_iface.h"    // 音频事件接口相关API
#include "audio_mem.h"            // 音频内存管理相关API
#include "audio_common.h"         // 音频通用定义和工具
#include "i2s_stream.h"           // I2S音频流相关API
#include "opus_encoder.h"         // Opus编码器相关API
#include "audio_manager.h"        // 音频管理器相关API

// 日志TAG
static const char *TAG = "PLAY_FLASH_MP3_CONTROL";

/**
 * @brief 主应用入口
 *        初始化音频管道、外设、事件监听，处理按键事件，实现MP3播放控制
 */
void app_main(void)
{
    // 设置日志等级
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // 配置音频管理器
    audio_manager_config_t config = {
        .sample_rate = 44100,
        .channel = 1,
        .bit_width = I2S_DATA_BIT_WIDTH_24BIT,
        .i2s_num = I2S_NUM_0,
        .bclk_pin = 2,
        .ws_pin = 3,
        .din_pin = 4
    };

    // 初始化音频管理器
    audio_manager_handle_t audio_mgr = audio_manager_init(&config);
    if (!audio_mgr) {
        ESP_LOGE(TAG, "Failed to initialize audio manager");
        return;
    }

    ESP_LOGW(TAG, "[ 5 ] Tap touch buttons to control music player:");
    ESP_LOGW(TAG, "      [Play] to start, pause and resume, [Set] to stop.");
    ESP_LOGW(TAG, "      [Vol-] or [Vol+] to adjust volume.");

    // 启动音频管理器
    ESP_LOGI(TAG, "[ 5.1 ] Start audio_pipeline");
    audio_manager_start(audio_mgr);

    // 主循环，处理事件
    while (1) {
        audio_event_iface_msg_t msg;
        // 等待事件（阻塞）
        esp_err_t ret = audio_manager_wait_for_event(audio_mgr, &msg, -1);
        if (ret != ESP_OK) {
            continue;
        }
    }

    // 退出主循环后，释放资源
    ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
    audio_manager_stop(audio_mgr);
    audio_manager_deinit(audio_mgr);
}
