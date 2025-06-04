/*** 
 * @Author: jixingnian@gmail.com
 * @Date: 2025-06-04 17:14:03
 * @LastEditTime: 2025-06-04 17:26:53
 * @LastEditors: 星年
 * @Description: 
 * @FilePath: \audio_manager\components\audio_manager\include\audio_manager.h
 * @遇事不决，可问春风
 */
#pragma once

#include "esp_err.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "i2s_stream.h"
#include "audio_event_iface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 音频管理器配置结构体
 */
typedef struct {
    int sample_rate;           // 采样率
    int channel;              // 通道数
    int bit_width;            // 位宽
    int i2s_num;              // I2S控制器编号
    int bclk_pin;             // BCLK引脚
    int ws_pin;               // WS引脚
    int din_pin;              // DIN引脚
} audio_manager_config_t;

/**
 * @brief 音频管理器句柄
 */
typedef struct audio_manager* audio_manager_handle_t;

/**
 * @brief 初始化音频管理器
 * 
 * @param config 配置参数
 * @return audio_manager_handle_t 成功返回句柄,失败返回NULL
 */
audio_manager_handle_t audio_manager_init(const audio_manager_config_t* config);

/**
 * @brief 启动音频管理器
 * 
 * @param handle 音频管理器句柄
 * @return esp_err_t 
 */
esp_err_t audio_manager_start(audio_manager_handle_t handle);

/**
 * @brief 停止音频管理器
 * 
 * @param handle 音频管理器句柄
 * @return esp_err_t 
 */
esp_err_t audio_manager_stop(audio_manager_handle_t handle);

/**
 * @brief 销毁音频管理器
 * 
 * @param handle 音频管理器句柄
 * @return esp_err_t 
 */
esp_err_t audio_manager_deinit(audio_manager_handle_t handle);

/**
 * @brief 获取音频管道句柄
 * 
 * @param handle 音频管理器句柄
 * @return audio_pipeline_handle_t 
 */
audio_pipeline_handle_t audio_manager_get_pipeline(audio_manager_handle_t handle);

/**
 * @brief 等待并处理音频事件
 * 
 * @param handle 音频管理器句柄
 * @param msg 事件消息结构体指针
 * @param timeout_ms 超时时间(毫秒)
 * @return esp_err_t 
 */
esp_err_t audio_manager_wait_for_event(audio_manager_handle_t handle, audio_event_iface_msg_t* msg, int timeout_ms);

#ifdef __cplusplus
}
#endif 