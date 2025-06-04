/*
 * @Author: xingnian j_xingnian@163.com
 * @Date: 2025-06-04 20:58:41
 * @LastEditors: 星年 && j_xingnian@163.com
 * @LastEditTime: 2025-06-04 21:03:15
 * @FilePath: \audio_manager\main\opus_decode_play.c
 * @Description:
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

static audio_pipeline_handle_t pipeline = NULL;
static audio_element_handle_t raw_reader = NULL;
static TaskHandle_t decode_task_handle = NULL;

#define RAW_STREAM_BUFFER_SIZE (8 * 1024)

// 提供给上层写入opus数据的接口函数
int opus_decode_play_write(const uint8_t *data, size_t len)
{
    if (!raw_reader) return -1;
    return raw_stream_write(raw_reader, (const char *)data, len);
}

static void opus_decode_play_task(void *arg)
{
    audio_element_handle_t opus_decoder = NULL;
    audio_element_handle_t i2s_writer = NULL;

    // 1. 创建 raw_stream 作为 Opus 数据输入
    raw_stream_cfg_t raw_cfg = {
        .type = AUDIO_STREAM_READER,
        .out_rb_size = RAW_STREAM_BUFFER_SIZE,
    };
    raw_reader = raw_stream_init(&raw_cfg);

    // 2. 创建 Opus 解码器
    opus_decoder_cfg_t opus_cfg = DEFAULT_OPUS_DECODER_CONFIG();
    opus_decoder = decoder_opus_init(&opus_cfg);

    // 3. 创建 I2S 播放器
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO; // 单声道
    i2s_cfg.std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT; // 16位
    i2s_writer = i2s_stream_init(&i2s_cfg);

    // 4. 创建音频管道
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    audio_pipeline_register(pipeline, raw_reader, "raw");
    audio_pipeline_register(pipeline, opus_decoder, "opus");
    audio_pipeline_register(pipeline, i2s_writer, "i2s");

    const char *link_tag[3] = {"raw", "opus", "i2s"};
    audio_pipeline_link(pipeline, link_tag, 3);

    // 5. 事件监听
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);
    audio_pipeline_set_listener(pipeline, evt);

    audio_pipeline_run(pipeline);

    ESP_LOGI(TAG, "Opus decode play pipeline started");

    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) continue;
        // 可根据需要处理事件
    }

    // 退出清理
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, raw_reader);
    audio_pipeline_unregister(pipeline, opus_decoder);
    audio_pipeline_unregister(pipeline, i2s_writer);

    audio_pipeline_remove_listener(pipeline);
    audio_event_iface_destroy(evt);

    audio_pipeline_deinit(pipeline);
    audio_element_deinit(raw_reader);
    audio_element_deinit(opus_decoder);
    audio_element_deinit(i2s_writer);

    raw_reader = NULL;
    pipeline = NULL;
    decode_task_handle = NULL;
    vTaskDelete(NULL);
}

void opus_decode_play_start(void)
{
    if (decode_task_handle) return;
    xTaskCreate(opus_decode_play_task, "opus_decode_play_task", 8 * 1024, NULL, 5, &decode_task_handle);
}

void opus_decode_play_stop(void)
{
    if (pipeline) {
        audio_pipeline_stop(pipeline);
    }
}