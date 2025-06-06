/*
 * @Author: xingnian j_xingnian@163.com
 * @Date: 2025-06-04 20:58:41
 * @LastEditors: 星年 && j_xingnian@163.com
 * @LastEditTime: 2025-06-06 18:46:47
 * @FilePath: \audio_manager\main\opus_decode_play.c
 * @Description: Opus解码播放模块实现文件，负责将Opus编码数据解码并通过I2S播放
 *
 * Copyright (c) 2025 by ${git_name_email}, All Rights Reserved.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "opus_decoder.h"

static const char *TAG = "OPUS_DECODE_PLAY";

// 全局静态变量，保存音频管道、raw_reader元素和解码任务句柄
static audio_pipeline_handle_t pipeline = NULL;         // 音频管道句柄
static audio_element_handle_t raw_reader = NULL;        // raw_stream元素句柄（用于接收Opus数据）
static TaskHandle_t decode_task_handle = NULL;          // 解码播放任务句柄

#define RAW_STREAM_BUFFER_SIZE (8 * 1024)               // raw_stream缓冲区大小（字节）

/**
 * @brief 向Opus解码播放模块写入Opus编码数据
 *
 * 该函数供上层调用，将Opus编码数据写入raw_stream缓冲区，供后台解码任务消费。
 * @param data Opus编码数据指针
 * @param len  数据长度（字节）
 * @return 实际写入的字节数，失败返回-1
 */
int opus_decode_play_write(const uint8_t *data, size_t len)
{
    if (!raw_reader) return -1; // 若raw_reader未初始化，返回错误
    return raw_stream_write(raw_reader, (const char *)data, len); // 写入数据到raw_stream
}

/**
 * @brief Opus解码播放任务
 *
 * 该任务负责初始化音频管道，依次包括raw_stream（输入Opus数据）、Opus解码器、I2S播放，
 * 并循环监听事件，实现Opus流的实时解码与播放。
 * @param arg 未使用
 */
static void opus_decode_play_task(void *arg)
{
    audio_element_handle_t opus_decoder = NULL; // Opus解码器元素句柄
    audio_element_handle_t i2s_writer = NULL;   // I2S播放元素句柄

    // 1. 创建 raw_stream 作为 Opus 数据输入
    raw_stream_cfg_t raw_cfg = {
        .type = AUDIO_STREAM_READER,                // 作为reader，供上层写入数据
        .out_rb_size = RAW_STREAM_BUFFER_SIZE,      // 环形缓冲区大小
    };
    raw_reader = raw_stream_init(&raw_cfg);         // 初始化raw_stream元素

    // 2. 创建 Opus 解码器
    opus_decoder_cfg_t opus_cfg = DEFAULT_OPUS_DECODER_CONFIG(); // 获取默认Opus解码器配置
    // opus_cfg.sample_rate = 44100;
    // opus_cfg.channel = 1;                                         // 单声道
    opus_decoder = decoder_opus_init(&opus_cfg);                 // 初始化Opus解码器元素

    // 3. 创建 I2S 播放器
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();         // 获取默认I2S配置
    i2s_cfg.type = AUDIO_STREAM_WRITER;                          // 作为writer，输出PCM到I2S
    i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;     // 单声道输出
    i2s_cfg.std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT; // 16位数据宽度
    i2s_cfg.std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;                    // 未用MCLK
    i2s_cfg.std_cfg.gpio_cfg.bclk = 19;                                 // BCLK引脚
    i2s_cfg.std_cfg.gpio_cfg.ws = 8;                                   // WS引脚
    i2s_cfg.std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;                   // 未用DOUT
    i2s_cfg.std_cfg.gpio_cfg.din = 20;                                  // DIN引脚
    i2s_cfg.volume = 80;                                                // 默认音量
    i2s_writer = i2s_stream_init(&i2s_cfg);                      // 初始化I2S元素

    // 4. 创建音频管道
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG(); // 获取默认管道配置
    pipeline = audio_pipeline_init(&pipeline_cfg);                       // 初始化音频管道

    // 注册各元素到管道
    audio_pipeline_register(pipeline, raw_reader, "raw");        // 注册raw_stream
    audio_pipeline_register(pipeline, opus_decoder, "opus");     // 注册Opus解码器
    audio_pipeline_register(pipeline, i2s_writer, "i2s");        // 注册I2S播放

    // 链接管道元素，数据流向: raw -> opus -> i2s
    const char *link_tag[3] = {"raw", "opus", "i2s"};
    audio_pipeline_link(pipeline, link_tag, 3);

    // 5. 创建并设置事件监听器
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG(); // 默认事件接口配置
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg); // 初始化事件接口
    audio_pipeline_set_listener(pipeline, evt);                       // 设置管道事件监听

    // 启动音频管道
    audio_pipeline_run(pipeline);

    ESP_LOGI(TAG, "Opus decode play pipeline started");

    // 主循环，监听管道事件
while (1) {
    audio_event_iface_msg_t msg;
    esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
    if (ret != ESP_OK) continue;
    // ESP_LOGI(TAG, "event: source_type=%d, cmd=%d, data=%p", msg.source_type, msg.cmd, msg.data);
}

    // 退出任务时的资源清理
    audio_pipeline_stop(pipeline);                // 停止管道
    audio_pipeline_wait_for_stop(pipeline);       // 等待管道完全停止
    audio_pipeline_terminate(pipeline);           // 终止管道

    // 注销各元素
    audio_pipeline_unregister(pipeline, raw_reader);
    audio_pipeline_unregister(pipeline, opus_decoder);
    audio_pipeline_unregister(pipeline, i2s_writer);

    // 移除并销毁事件监听器
    audio_pipeline_remove_listener(pipeline);
    audio_event_iface_destroy(evt);

    // 释放管道和元素资源
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(raw_reader);
    audio_element_deinit(opus_decoder);
    audio_element_deinit(i2s_writer);

    // 清空全局句柄
    raw_reader = NULL;
    pipeline = NULL;
    decode_task_handle = NULL;
    vTaskDelete(NULL); // 删除当前任务
}

/**
 * @brief 启动Opus解码播放任务
 *
 * 若任务未启动，则创建opus_decode_play_task后台任务，负责Opus解码与播放。
 */
void opus_decode_play_start(void)
{
    if (decode_task_handle) return; // 已启动则不重复创建
    xTaskCreate(opus_decode_play_task, "opus_decode_play_task", 8 * 1024, NULL, 5, &decode_task_handle);
}

/**
 * @brief 停止Opus解码播放任务
 *
 * 停止音频管道，任务退出后会自动清理资源。
 */
void opus_decode_play_stop(void)
{
    if (pipeline) {
        audio_pipeline_stop(pipeline); // 仅停止管道，任务会检测到并退出
    }
}