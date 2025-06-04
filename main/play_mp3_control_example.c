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

// 日志TAG
static const char *TAG = "PLAY_FLASH_MP3_CONTROL";

/**
 * @brief 主应用入口
 *        初始化音频管道、外设、事件监听，处理按键事件，实现MP3播放控制
 */
void app_main(void)
{
    audio_pipeline_handle_t pipeline;                // 音频管道句柄
    audio_element_handle_t i2s_stream_reader;       // I2S输入流句柄
    audio_element_handle_t opus_encoder;            // OPUS编码器句柄

    // 设置日志等级
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // ===================== I2S流配置（详细注释版） =====================

    // 1. 创建I2S流配置结构体，并初始化为默认值（I2S_NUM_0, 44100Hz, 16bit, 写模式）
    //    这里我们会覆盖部分默认参数以适配实际需求
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();

    // 2. 设置I2S流为"读取"模式（即从I2S外设采集音频数据）
    i2s_cfg.type = AUDIO_STREAM_READER;

    // 3. 设置I2S传输模式为标准I2S（STD），即I2S标准通信协议
    i2s_cfg.transmit_mode = I2S_COMM_MODE_STD;

    // 4. 通道（Channel）相关配置
    i2s_cfg.chan_cfg.id = I2S_NUM_0;                // 选择I2S控制器编号（I2S0）
    i2s_cfg.chan_cfg.role = I2S_ROLE_MASTER;        // 设置为主机模式（Master）
    i2s_cfg.chan_cfg.dma_desc_num = 3;              // DMA描述符数量（决定DMA缓冲区数量）
    i2s_cfg.chan_cfg.dma_frame_num = 300;           // 每个DMA缓冲区的帧数（影响延迟和吞吐）
    i2s_cfg.chan_cfg.auto_clear = false;            // 关闭自动清除（接收模式下通常不需要自动清除发送描述符）

    // 5. 标准I2S时钟相关配置
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = 44100;         // 采样率设置为44.1kHz
    i2s_cfg.std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;  // 使用默认时钟源
    i2s_cfg.std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256; // MCLK频率为采样率的256倍

    // 6. 时隙（Slot）相关配置
    i2s_cfg.std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_24BIT; // 每个采样点24位数据
    i2s_cfg.std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;  // 槽位宽度自动适配
    i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;            // 单声道模式
    i2s_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;             // 仅使用左声道（右声道数据忽略）
    i2s_cfg.std_cfg.slot_cfg.ws_width = 24;                             // WS（Word Select）宽度为24位
    i2s_cfg.std_cfg.slot_cfg.ws_pol = false;                            // WS极性：false=标准极性
    i2s_cfg.std_cfg.slot_cfg.bit_shift = true;                          // 启用bit shift（数据左对齐）

    // 7. GPIO引脚配置
    i2s_cfg.std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;    // 不使用MCLK输出
    i2s_cfg.std_cfg.gpio_cfg.bclk = 2;                  // BCLK（位时钟）连接到GPIO2
    i2s_cfg.std_cfg.gpio_cfg.ws = 3;                    // WS（字选择/帧同步）连接到GPIO3
    i2s_cfg.std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;    // 不使用DOUT（本例为接收模式）
    i2s_cfg.std_cfg.gpio_cfg.din = 4;                   // DIN（数据输入）连接到GPIO4
    i2s_cfg.std_cfg.gpio_cfg.invert_flags.mclk_inv = false; // MCLK不反相
    i2s_cfg.std_cfg.gpio_cfg.invert_flags.bclk_inv = false; // BCLK不反相

    // 8. 其他高级参数配置
    i2s_cfg.use_alc = false;                            // 不启用自动电平控制（ALC）
    i2s_cfg.volume = 0;                                 // 默认音量（0dB，未做增益/衰减）
    i2s_cfg.out_rb_size = I2S_STREAM_RINGBUFFER_SIZE;   // 输出环形缓冲区大小（默认宏定义）
    i2s_cfg.task_stack = I2S_STREAM_TASK_STACK;         // 任务堆栈大小（默认宏定义）
    i2s_cfg.task_core = I2S_STREAM_TASK_CORE;           // 任务运行核心（默认宏定义）
    i2s_cfg.task_prio = I2S_STREAM_TASK_PRIO;           // 任务优先级（默认宏定义）
    i2s_cfg.stack_in_ext = false;                       // 堆栈不放在外部RAM
    i2s_cfg.multi_out_num = 0;                          // 不启用多路输出
    i2s_cfg.uninstall_drv = true;                       // 流销毁时自动卸载I2S驱动
    i2s_cfg.need_expand = false;                        // 不扩展数据位宽
    i2s_cfg.expand_src_bits = I2S_DATA_BIT_WIDTH_24BIT; // 源数据位宽为24位
    i2s_cfg.buffer_len = I2S_STREAM_BUF_SIZE;           // 元素缓冲区长度（默认宏定义）
    // ===================== I2S流配置结束 =====================

    ESP_LOGI(TAG, "[ 2 ] Create audio pipeline, add all elements to pipeline, and subscribe pipeline event");
    // 创建音频管道
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    // 创建OPUS编码器
    ESP_LOGI(TAG, "[2.1] Create OPUS encoder");
    opus_encoder_cfg_t opus_cfg = DEFAULT_OPUS_ENCODER_CONFIG();
    opus_cfg.sample_rate = 44100;  // 设置采样率与I2S输入一致
    opus_cfg.channel = 1;          // 单声道
    opus_cfg.bitrate = 64000;      // 设置比特率
    opus_encoder = encoder_opus_init(&opus_cfg);
    if (!opus_encoder) {
        ESP_LOGE(TAG, "Failed to create OPUS encoder");
        return;
    }

    // 创建I2S输入流
    ESP_LOGI(TAG, "[2.2] Create I2S stream reader");
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

    // 注册元素到管道
    ESP_LOGI(TAG, "[2.3] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, opus_encoder, "opus");

    // 链接管道元素
    ESP_LOGI(TAG, "[2.4] Link it together [i2s]-->[opus]");
    const char *link_tag[2] = {"i2s", "opus"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);

    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    // 创建事件监听器
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    // 监听管道元素事件
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGW(TAG, "[ 5 ] Tap touch buttons to control music player:");
    ESP_LOGW(TAG, "      [Play] to start, pause and resume, [Set] to stop.");
    ESP_LOGW(TAG, "      [Vol-] or [Vol+] to adjust volume.");

    ESP_LOGI(TAG, "[ 5.1 ] Start audio_pipeline");

    // 启动音频管道
    audio_pipeline_run(pipeline);

    // 主循环，处理事件
    while (1) {
        audio_event_iface_msg_t msg;
        // 等待事件（阻塞）
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            continue;
        }
    }

    // 退出主循环后，释放资源
    ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    
    // 注销管道元素
    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, opus_encoder);

    /* 在移除事件监听器前终止管道 */
    audio_pipeline_remove_listener(pipeline);

    /* 确保audio_pipeline_remove_listener已调用后再销毁事件接口 */
    audio_event_iface_destroy(evt);

    /* 释放所有资源 */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(opus_encoder);
}
