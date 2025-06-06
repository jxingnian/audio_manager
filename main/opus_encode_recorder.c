/*
 * @Author: jixingnian@gmail.com
 * @Date: 2025-06-05 08:35:56
 * @LastEditTime: 2025-06-05 20:35:19
 * @LastEditors: 星年 && j_xingnian@163.com
 * @Description: Opus编码录制模块实现，负责采集I2S音频、重采样、编码并输出Opus数据
 * @FilePath: \audio_manager\main\opus_encode_recorder.c
 * 遇事不决，可问春风
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "opus_encoder.h"
#include "filter_resample.h"
#include "raw_stream.h"
#include "opus_encode_recorder.h"

#define OPUS_RECORDER_TAG "OPUS_ENCODE_RECORDER"                // 日志TAG
#define OPUS_RECORDER_RAW_BUF_SIZE (8 * 1024)                   // raw_stream缓冲区大小（字节）
#define OPUS_RECORDER_TASK_STACK (4 * 1024)                     // 录制任务堆栈大小
#define OPUS_RECORDER_TASK_PRIO 5                               // 录制任务优先级

static TaskHandle_t s_opus_encode_task_handle = NULL;           // 录制任务句柄
static audio_pipeline_handle_t s_pipeline = NULL;               // 音频管道句柄
static audio_element_handle_t s_raw_writer = NULL;              // raw_stream元素句柄（用于输出Opus编码数据）

static volatile bool s_task_running = false;                    // 任务运行标志

/**
 * @brief Opus编码录制任务
 *
 * 该任务负责初始化音频管道，采集I2S音频数据，重采样，Opus编码，并输出到raw_stream。
 * 任务启动后会持续运行，直到s_task_running被置为false。
 */
static void opus_encode_recorder_task(void *arg)
{
    audio_element_handle_t i2s_stream_reader = NULL;    // I2S输入流元素
    audio_element_handle_t opus_encoder = NULL;         // Opus编码器元素
    audio_element_handle_t filter = NULL;               // 重采样滤波器元素

    // 1. 配置I2S输入流参数
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;                        // 作为reader采集音频
    i2s_cfg.transmit_mode = I2S_COMM_MODE_STD;                 // 标准I2S模式
    i2s_cfg.chan_cfg.id = I2S_NUM_0;                           // I2S通道0
    i2s_cfg.chan_cfg.role = I2S_ROLE_MASTER;                   // 主模式
    // i2s_cfg.chan_cfg.dma_desc_num = 3;                         // DMA描述符数量
    // i2s_cfg.chan_cfg.dma_frame_num = 300;                      // DMA帧数
    // i2s_cfg.chan_cfg.auto_clear = false;                       // 不自动清除
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = 44100;            // 输入采样率44.1kHz
    // i2s_cfg.std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;     // 默认时钟源
    // i2s_cfg.std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256; // MCLK倍频
    i2s_cfg.std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_24BIT; // 24位数据宽度
    // i2s_cfg.std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;  // 自动槽宽
    i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;            // 单声道
    i2s_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;             // 左声道
    i2s_cfg.std_cfg.slot_cfg.ws_width = 24;                             // WS宽度
    // i2s_cfg.std_cfg.slot_cfg.ws_pol = false;                            // WS极性
    // i2s_cfg.std_cfg.slot_cfg.bit_shift = true;                          // 位移
    i2s_cfg.std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;                    // 未用MCLK
    i2s_cfg.std_cfg.gpio_cfg.bclk = 41;                                 // BCLK引脚
    i2s_cfg.std_cfg.gpio_cfg.ws = 42;                                   // WS引脚
    i2s_cfg.std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;                   // 未用DOUT
    i2s_cfg.std_cfg.gpio_cfg.din = 2;                                  // DIN引脚
    // i2s_cfg.std_cfg.gpio_cfg.invert_flags.mclk_inv = false;            // 不反转MCLK
    // i2s_cfg.std_cfg.gpio_cfg.invert_flags.bclk_inv = false;            // 不反转BCLK
    // i2s_cfg.use_alc = false;                                           // 不使用ALC
    i2s_cfg.volume = 80;                                                // 默认音量
    i2s_cfg.out_rb_size = I2S_STREAM_RINGBUFFER_SIZE;                  // 输出环形缓冲区大小
    i2s_cfg.task_stack = I2S_STREAM_TASK_STACK;                        // 任务堆栈
    // i2s_cfg.task_core = I2S_STREAM_TASK_CORE;                          // 任务绑定核心
    // i2s_cfg.task_prio = I2S_STREAM_TASK_PRIO;                          // 任务优先级
    // i2s_cfg.stack_in_ext = false;                                      // 堆栈不在外部RAM
    // i2s_cfg.multi_out_num = 0;                                         // 不使用多路输出
    // i2s_cfg.uninstall_drv = true;                                      // 任务结束时卸载驱动
    // i2s_cfg.need_expand = false;                                       // 不需要扩展
    // i2s_cfg.expand_src_bits = I2S_DATA_BIT_WIDTH_24BIT;                // 扩展源位宽
    // i2s_cfg.buffer_len = I2S_STREAM_BUF_SIZE;                          // 缓冲区长度

    // 2. 创建音频管道
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    s_pipeline = audio_pipeline_init(&pipeline_cfg);                   // 初始化音频管道
    mem_assert(s_pipeline);                                            // 断言管道创建成功

    // 3. 创建Opus编码器元素
    opus_encoder_cfg_t opus_cfg = DEFAULT_OPUS_ENCODER_CONFIG();
    // opus_cfg.sample_rate = 44100;                                      // 输入采样率
    // opus_cfg.channel = 1;                                              // 单声道
    // opus_cfg.bitrate = 64000;                                          // 目标比特率
    // opus_cfg.complexity = 10;                                          // 编码复杂度
    opus_encoder = encoder_opus_init(&opus_cfg);                       // 初始化Opus编码器
    if (!opus_encoder) {
        ESP_LOGE(OPUS_RECORDER_TAG, "Failed to create OPUS encoder");  // 创建失败日志
        goto _exit;                                                    // 跳转退出
    }

    // 4. 创建重采样滤波器（将44.1kHz重采样为16kHz，单声道）
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 44100;                                          // 输入采样率
    rsp_cfg.src_ch = 1;                                                // 输入通道
    rsp_cfg.dest_rate = 16000;                                         // 输出采样率
    rsp_cfg.dest_ch = 1;                                               // 输出通道
    filter = rsp_filter_init(&rsp_cfg);                                // 初始化重采样滤波器

    // 5. 创建raw_stream元素用于输出Opus编码数据
    raw_stream_cfg_t raw_cfg = {
        .type = AUDIO_STREAM_WRITER,                                   // 作为writer输出数据
        .out_rb_size = OPUS_RECORDER_RAW_BUF_SIZE,                     // 环形缓冲区大小
    };
    s_raw_writer = raw_stream_init(&raw_cfg);                          // 初始化raw_stream

    // 6. 创建I2S输入流元素
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);                     // 初始化I2S输入流

    // 7. 注册所有元素到音频管道
    audio_pipeline_register(s_pipeline, i2s_stream_reader, "i2s");     // 注册I2S输入
    audio_pipeline_register(s_pipeline, filter, "filter");             // 注册重采样滤波器
    audio_pipeline_register(s_pipeline, opus_encoder, "opus");         // 注册Opus编码器
    audio_pipeline_register(s_pipeline, s_raw_writer, "raw");          // 注册raw_stream

    // 8. 链接管道元素，形成[i2s] -> [filter] -> [opus] -> [raw]的链路
    const char *link_tag[4] = {"i2s", "filter", "opus", "raw"};
    audio_pipeline_link(s_pipeline, &link_tag[0], 4);

    // 9. 创建事件监听器并绑定到管道
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg); // 创建事件接口
    audio_pipeline_set_listener(s_pipeline, evt);                     // 设置管道事件监听

    // 10. 启动音频管道，开始采集、重采样、编码、输出
    ESP_LOGI(OPUS_RECORDER_TAG, "Start audio pipeline for opus encode recorder");
    audio_pipeline_run(s_pipeline);

    s_task_running = true;                                            // 标记任务正在运行
    while (s_task_running) {
        vTaskDelay(pdMS_TO_TICKS(100));                               // 周期性延时，可扩展事件处理
        // 可在此处添加事件处理逻辑，如监听管道状态等
    }

    // 11. 停止管道并释放所有资源
    ESP_LOGI(OPUS_RECORDER_TAG, "Stop audio pipeline for opus encode recorder");
    audio_pipeline_stop(s_pipeline);                                  // 停止管道
    audio_pipeline_wait_for_stop(s_pipeline);                         // 等待管道完全停止
    audio_pipeline_terminate(s_pipeline);                             // 终止管道

    // 注销所有元素
    audio_pipeline_unregister(s_pipeline, i2s_stream_reader);
    audio_pipeline_unregister(s_pipeline, filter);
    audio_pipeline_unregister(s_pipeline, opus_encoder);
    audio_pipeline_unregister(s_pipeline, s_raw_writer);

    // 移除事件监听器并销毁
    audio_pipeline_remove_listener(s_pipeline);
    audio_event_iface_destroy(evt);

    // 释放所有元素和管道资源
    audio_pipeline_deinit(s_pipeline);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(filter);
    audio_element_deinit(opus_encoder);
    audio_element_deinit(s_raw_writer);

    s_pipeline = NULL;
    s_raw_writer = NULL;

_exit:
    s_opus_encode_task_handle = NULL;                                 // 清空任务句柄
    vTaskDelete(NULL);                                                // 删除当前任务
}

/**
 * @brief 启动Opus编码录制任务
 *
 * 创建并启动opus_encode_recorder_task后台任务，负责音频采集与编码。
 * 若任务已在运行，则直接返回。
 */
void opus_encode_recorder_start(void)
{
    if (s_opus_encode_task_handle) {
        ESP_LOGW(OPUS_RECORDER_TAG, "Opus encode recorder already running"); // 已在运行
        return;
    }
    s_task_running = true;    // 设置任务运行标志
    // 创建并绑定任务到核心0
    xTaskCreatePinnedToCore(opus_encode_recorder_task, "opus_encode_recorder_task", OPUS_RECORDER_TASK_STACK, NULL, OPUS_RECORDER_TASK_PRIO, &s_opus_encode_task_handle, 0);
}

/**
 * @brief 停止Opus编码录制任务
 *
 * 通知后台任务退出并等待其完全销毁。
 * 若任务未运行，则直接返回。
 */
void opus_encode_recorder_stop(void)
{
    if (!s_opus_encode_task_handle) {
        ESP_LOGW(OPUS_RECORDER_TAG, "Opus encode recorder not running"); // 未在运行
        return;
    }
    s_task_running = false;   // 通知任务退出
    // 等待任务句柄被清空，确保任务已销毁
    while (s_opus_encode_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief 读取Opus编码后的数据
 *
 * 从raw_stream缓冲区读取Opus编码数据，通常用于网络发送或本地存储。
 * @param data 目标缓冲区指针
 * @param len  期望读取的字节数
 * @return 实际读取的字节数，失败返回-1
 */
int opus_encode_recorder_read(uint8_t *data, size_t len)
{
    if (!s_raw_writer) return -1; // 若raw_stream未初始化，返回错误
    return raw_stream_read(s_raw_writer, (char *)data, len); // 从raw_stream读取数据
}
