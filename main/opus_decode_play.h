/** 
 * @file opus_decode_play.h
 * @author jixingnian@gmail.com
 * @date 2025-06-05 08:24:51
 * @lastEditTime 2025-06-05 08:47:43
 * @lastEditors 星年
 * @brief Opus解码播放模块头文件
 * @details 提供Opus解码播放相关的API声明，包括启动、停止解码播放任务，以及向解码器写入Opus数据的接口。
 * @filePath \audio_manager\main\opus_decode_play.h
 * @note 遇事不决，可问春风
 */
#pragma once

#include <stdint.h>   // 用于uint8_t等标准整型定义
#include <stddef.h>   // 用于size_t类型定义

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动Opus解码播放任务
 * 
 * 该函数会创建并启动一个后台任务，负责从内部缓冲区读取Opus数据，进行解码并通过I2S等音频外设播放PCM音频。
 * 通常在系统初始化或需要开始播放Opus流时调用。
 */
void opus_decode_play_start(void);

/**
 * @brief 停止Opus解码播放任务
 * 
 * 该函数会安全地终止解码播放任务，释放相关资源。调用后将不再播放新的Opus数据。
 * 通常在需要关闭音频输出或系统退出时调用。
 */
void opus_decode_play_stop(void);

/**
 * @brief 向Opus解码播放模块写入Opus编码数据
 * 
 * @param data 指向Opus编码数据的指针
 * @param len  数据长度（字节数）
 * @return int 实际写入的字节数，或负值表示错误
 * 
 * 该函数将Opus编码数据写入内部缓冲区，供后台解码播放任务消费。
 * 可用于实时流式推送Opus数据（如网络接收、文件读取等）。
 */
int opus_decode_play_write(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif