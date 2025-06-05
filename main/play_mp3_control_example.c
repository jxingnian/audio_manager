// 引入标准字符串处理库，用于如memcpy、memset等操作
#include <string.h>

// 引入FreeRTOS操作系统的头文件，提供任务管理等功能
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP-IDF日志库，用于输出调试和运行日志
#include "esp_log.h"

// 音频框架相关头文件
#include "opus_decode_play.h"   // 新增头文件引用
#include "opus_encode_recorder.h" // 新增头文件引用
// 日志TAG
static const char *TAG = "AUDIO_TASK";

/**
 * @brief 主应用入口
 *        初始化音频管道、外设、事件监听，处理按键事件，实现MP3播放控制
 */
void app_main(void)
{
    // 启动opus解码播放任务
    opus_decode_play_start();

    // 启动opus编码录制任务
    opus_encode_recorder_start();
}
