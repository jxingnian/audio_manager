/*** 
 * @Author: xingnian j_xingnian@163.com
 * @Date: 2025-06-06 17:00:00
 * @LastEditTime: 2025-06-06 17:00:00
 * @LastEditors: 星年
 * @Description: 音频重采样模块实现，使用ESP-ADF框架实现从INMP441到MAX98357A的音频重采样
 * @FilePath: \audio_manager\main\audio_resample_adf.c
 * @遇事不决，可问春风
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "filter_resample.h"
#include "raw_stream.h"
#include "audio_resample_adf.h"

static const char *TAG = "AUDIO_RESAMPLE";

// 音频参数配置
#define SAMPLE_RATE_IN    44100
#define SAMPLE_RATE_OUT   16000
#define SAMPLE_BITS       16
#define CHANNELS          1

// GPIO配置
#define I2S_BCK_IO       2
#define I2S_WS_IO        3
#define I2S_DO_IO        4
#define I2S_DI_IO        5

// 任务句柄
static TaskHandle_t resample_task_handle = NULL;
static audio_pipeline_handle_t pipeline = NULL;
static bool is_running = false;

// 重采样任务
static void resample_task(void *arg)
{
    ESP_LOGI(TAG, "Starting audio resampling task");

    // 创建音频管道
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    if (!pipeline) {
        ESP_LOGE(TAG, "Failed to create pipeline");
        is_running = false;
        vTaskDelete(NULL);
        return;
    }

    // 配置I2S输入流
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_cfg.transmit_mode = I2S_COMM_MODE_STD;
    i2s_cfg.chan_cfg.id = I2S_NUM_0;
    i2s_cfg.chan_cfg.role = I2S_ROLE_MASTER;
    i2s_cfg.chan_cfg.dma_desc_num = 3;
    i2s_cfg.chan_cfg.dma_frame_num = 300;
    i2s_cfg.chan_cfg.auto_clear = false;
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = SAMPLE_RATE_IN;
    i2s_cfg.std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    i2s_cfg.std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    i2s_cfg.std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_24BIT;
    i2s_cfg.std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
    i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    i2s_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    i2s_cfg.std_cfg.slot_cfg.ws_width = 24;
    i2s_cfg.std_cfg.slot_cfg.ws_pol = false;
    i2s_cfg.std_cfg.slot_cfg.bit_shift = true;
    i2s_cfg.std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    i2s_cfg.std_cfg.gpio_cfg.bclk = I2S_BCK_IO;
    i2s_cfg.std_cfg.gpio_cfg.ws = I2S_WS_IO;
    i2s_cfg.std_cfg.gpio_cfg.dout = I2S_DO_IO;
    i2s_cfg.std_cfg.gpio_cfg.din = I2S_DI_IO;
    i2s_cfg.std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    i2s_cfg.std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    i2s_cfg.use_alc = false;
    i2s_cfg.volume = 80;
    i2s_cfg.out_rb_size = I2S_STREAM_RINGBUFFER_SIZE;
    i2s_cfg.task_stack = I2S_STREAM_TASK_STACK;
    i2s_cfg.task_core = I2S_STREAM_TASK_CORE;
    i2s_cfg.task_prio = I2S_STREAM_TASK_PRIO;
    i2s_cfg.stack_in_ext = false;
    i2s_cfg.multi_out_num = 0;
    i2s_cfg.uninstall_drv = true;
    i2s_cfg.need_expand = false;
    i2s_cfg.expand_src_bits = I2S_DATA_BIT_WIDTH_24BIT;
    i2s_cfg.buffer_len = I2S_STREAM_BUF_SIZE;

    audio_element_handle_t i2s_stream_reader = i2s_stream_init(&i2s_cfg);
    if (!i2s_stream_reader) {
        ESP_LOGE(TAG, "Failed to create I2S stream reader");
        audio_pipeline_deinit(pipeline);
        is_running = false;
        vTaskDelete(NULL);
        return;
    }

    // 配置重采样滤波器
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = SAMPLE_RATE_IN;
    rsp_cfg.src_ch = CHANNELS;
    rsp_cfg.dest_rate = SAMPLE_RATE_OUT;
    rsp_cfg.dest_ch = CHANNELS;
    audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);
    if (!filter) {
        ESP_LOGE(TAG, "Failed to create resample filter");
        audio_element_deinit(i2s_stream_reader);
        audio_pipeline_deinit(pipeline);
        is_running = false;
        vTaskDelete(NULL);
        return;
    }

    // 配置I2S输出流
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = SAMPLE_RATE_OUT;
    audio_element_handle_t i2s_stream_writer = i2s_stream_init(&i2s_cfg);
    if (!i2s_stream_writer) {
        ESP_LOGE(TAG, "Failed to create I2S stream writer");
        audio_element_deinit(filter);
        audio_element_deinit(i2s_stream_reader);
        audio_pipeline_deinit(pipeline);
        is_running = false;
        vTaskDelete(NULL);
        return;
    }

    // 注册管道元素
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s_read");
    audio_pipeline_register(pipeline, filter, "filter");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s_write");

    // 连接管道元素
    const char *link_tag[3] = {"i2s_read", "filter", "i2s_write"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);

    // 设置事件监听
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);
    audio_pipeline_set_listener(pipeline, evt);

    // 启动管道
    audio_pipeline_run(pipeline);
    ESP_LOGI(TAG, "Audio pipeline started");

    // 事件循环
    while (is_running) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.cmd == AEL_MSG_CMD_DESTROY) {
            ESP_LOGE(TAG, "[ * ] Pipeline destroyed");
            break;
        }
    }

    // 清理资源
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, filter);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_remove_listener(pipeline);
    audio_event_iface_destroy(evt);
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(filter);
    audio_element_deinit(i2s_stream_writer);

    ESP_LOGI(TAG, "Audio resampling task ended");
    is_running = false;
    resample_task_handle = NULL;
    vTaskDelete(NULL);
}

void audio_resample_start(void)
{
    if (is_running) {
        ESP_LOGW(TAG, "Audio resampling is already running");
        return;
    }

    is_running = true;
    xTaskCreatePinnedToCore(resample_task, "resample_task", 8 * 1024, NULL, 5, &resample_task_handle, 0);
}

void audio_resample_stop(void)
{
    if (!is_running) {
        ESP_LOGW(TAG, "Audio resampling is not running");
        return;
    }

    is_running = false;
    if (resample_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100)); // 等待任务结束
        resample_task_handle = NULL;
    }
} 