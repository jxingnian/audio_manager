/*** 
 * @Author: jixingnian@gmail.com
 * @Date: 2025-06-05 08:35:56
 * @LastEditTime: 2025-06-05 08:37:26
 * @LastEditors: 星年
 * @Description: 
 * @FilePath: \audio_manager\main\opus_encode_recorder.h
 * @遇事不决，可问春风
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动Opus编码录制任务
 *
 * 该函数会创建并启动一个后台任务，负责从I2S等音频输入接口采集PCM数据，
 * 并进行Opus编码后缓存在内部缓冲区。调用此函数后，可通过
 * opus_encode_recorder_read()接口持续读取Opus编码数据。
 */
void opus_encode_recorder_start(void);

/**
 * @brief 停止Opus编码录制任务
 *
 * 该函数会终止后台的Opus编码录制任务，释放相关资源。
 * 停止后如需再次录制需重新调用 opus_encode_recorder_start()。
 */
void opus_encode_recorder_stop(void);

/**
 * @brief 读取Opus编码数据
 *
 * @param data 指向用于存放Opus编码数据的缓冲区指针
 * @param len  期望读取的数据长度（字节数）
 * @return 实际读取的字节数，失败返回-1
 *
 * 该接口用于从内部缓冲区读取Opus编码后的数据，通常用于网络发送或本地存储。
 * 数据由后台任务自动采集和编码，调用本接口可获取最新的编码数据。
 */
int opus_encode_recorder_read(uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif