/*** 
 * @Author: xingnian j_xingnian@163.com
 * @Date: 2025-06-06 17:00:00
 * @LastEditTime: 2025-06-06 17:00:00
 * @LastEditors: 星年
 * @Description: 音频重采样模块头文件，提供从INMP441到MAX98357A的音频重采样功能
 * @FilePath: \audio_manager\main\audio_resample_adf.h
 * @遇事不决，可问春风
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动音频重采样任务
 *
 * 该函数会创建并启动一个后台任务，负责从INMP441采集音频数据，
 * 重采样后输出到MAX98357A。调用此函数后，音频重采样会自动进行。
 */
void audio_resample_start(void);

/**
 * @brief 停止音频重采样任务
 *
 * 该函数会终止后台的音频重采样任务，释放相关资源。
 * 停止后如需再次重采样需重新调用 audio_resample_start()。
 */
void audio_resample_stop(void);

#ifdef __cplusplus
}
#endif 