#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_system.h"

// 输入采样率（麦克风采集的原始采样率）
#define SAMPLE_RATE_IN     (44100)
// 输出采样率（扬声器播放的目标采样率）
#define SAMPLE_RATE_OUT    (16000)
// 采样位宽（每个采样点的位数，16位）
#define SAMPLE_BITS        (16)
// WAV文件头大小（未用到，仅保留）
#define WAV_HEADER_SIZE    (44)
// I2S DMA缓冲区数量
#define DMA_BUF_COUNT      (3)
// I2S DMA每个缓冲区长度（采样点数）
#define DMA_BUF_LEN        (1024)

// I2S0（录音）引脚定义
#define I2S0_BCK_IO       (GPIO_NUM_26)  // I2S0 串行时钟线
#define I2S0_WS_IO        (GPIO_NUM_18)  // I2S0 字选择线
#define I2S0_DI_IO        (GPIO_NUM_27)  // I2S0 数据输入（麦克风）

// I2S1（播放）引脚定义
#define I2S1_BCK_IO       (GPIO_NUM_19)  // I2S1 串行时钟线（举例）
#define I2S1_WS_IO        (GPIO_NUM_18)  // I2S1 字选择线（举例）
#define I2S1_DO_IO        (GPIO_NUM_5)   // I2S1 数据输出（扬声器）（举例）

// 日志TAG
static const char *TAG = "AUDIO_RESAMPLE";

// 采样缓冲区（用于I2S读取原始音频数据）
static int16_t resample_buffer[DMA_BUF_LEN * 2];
// 重采样输出缓冲区（用于存放重采样后的音频数据）
static int16_t output_buffer[DMA_BUF_LEN];

/**
 * @brief 简单线性插值重采样函数
 *        将input中的input_len个采样点重采样为output_len个采样点，结果存入output
 * @param input      输入音频数据指针
 * @param output     输出音频数据指针
 * @param input_len  输入采样点数
 * @param output_len 输出采样点数
 */
static void resample_audio(int16_t *input, int16_t *output, int input_len, int output_len) {
    float ratio = (float)input_len / output_len; // 输入/输出采样点比率
    for (int i = 0; i < output_len; i++) {
        float pos = i * ratio;                   // 对应输入的浮点位置
        int pos_int = (int)pos;                  // 取整得到输入下标
        float frac = pos - pos_int;              // 小数部分用于插值

        // 边界处理：如果超出输入范围，直接取最后一个采样点
        if (pos_int + 1 >= input_len) {
            output[i] = input[input_len - 1];
        } else {
            // 线性插值：output = (1-frac)*input[pos_int] + frac*input[pos_int+1]
            output[i] = (int16_t)(input[pos_int] * (1 - frac) + input[pos_int + 1] * frac);
        }
    }
}

/**
 * @brief I2S外设初始化
 *        配置I2S0用于麦克风输入，I2S1用于扬声器输出，设置采样率、位宽、通道格式等
 */
static void i2s_init(void) {
    // 配置I2S0用于INMP441数字麦克风（只接收）
    i2s_config_t i2s0_mic_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,                  // 主机模式+接收
        .sample_rate = SAMPLE_RATE_IN,                          // 采样率
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,           // 16位采样
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,            // 只用左声道
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,      // 标准I2S格式
        .dma_buf_count = DMA_BUF_COUNT,                         // DMA缓冲区数量
        .dma_buf_len = DMA_BUF_LEN,                             // 每个缓冲区长度
        .use_apll = false,                                      // 不用APLL
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1                // 中断优先级
    };

    // 配置I2S1用于MAX98357A数字功放（只发送）
    i2s_config_t i2s1_spk_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,                  // 主机模式+发送
        .sample_rate = SAMPLE_RATE_OUT,                         // 采样率
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,           // 16位采样
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,            // 只用左声道
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,      // 标准I2S格式
        .dma_buf_count = DMA_BUF_COUNT,                         // DMA缓冲区数量
        .dma_buf_len = DMA_BUF_LEN,                             // 每个缓冲区长度
        .use_apll = false,                                      // 不用APLL
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1                // 中断优先级
    };

    // I2S0引脚配置（录音）
    i2s_pin_config_t i2s0_pin_config = {
        .bck_io_num = I2S0_BCK_IO,      // 串行时钟
        .ws_io_num = I2S0_WS_IO,        // 字选择
        .data_out_num = -1,             // 不输出
        .data_in_num = I2S0_DI_IO       // 数据输入
    };

    // I2S1引脚配置（播放）
    i2s_pin_config_t i2s1_pin_config = {
        .bck_io_num = I2S1_BCK_IO,      // 串行时钟
        .ws_io_num = I2S1_WS_IO,        // 字选择
        .data_out_num = I2S1_DO_IO,     // 数据输出
        .data_in_num = -1               // 不输入
    };

    // 安装I2S0驱动（麦克风输入）
    i2s_driver_install(I2S_NUM_0, &i2s0_mic_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &i2s0_pin_config);
    i2s_set_clk(I2S_NUM_0, SAMPLE_RATE_IN, SAMPLE_BITS, I2S_CHANNEL_MONO);

    // 安装I2S1驱动（扬声器输出）
    i2s_driver_install(I2S_NUM_1, &i2s1_spk_config, 0, NULL);
    i2s_set_pin(I2S_NUM_1, &i2s1_pin_config);
    i2s_set_clk(I2S_NUM_1, SAMPLE_RATE_OUT, SAMPLE_BITS, I2S_CHANNEL_MONO);
}

/**
 * @brief 主应用入口
 *        初始化I2S，循环采集音频数据，重采样后输出到扬声器
 */
void app_main(void) {
    ESP_LOGI(TAG, "Initializing I2S...");
    i2s_init(); // 初始化I2S0和I2S1外设

    size_t bytes_read = 0;     // 实际读取的字节数
    size_t bytes_written = 0;  // 实际写入的字节数

    ESP_LOGI(TAG, "Starting audio resampling loop...");

    while (1) {
        // 1. 从麦克风I2S0读取原始音频数据到resample_buffer
        i2s_read(I2S_NUM_0, resample_buffer, DMA_BUF_LEN * sizeof(int16_t), &bytes_read, portMAX_DELAY);

        // 2. 计算本次读取的采样点数
        int samples_read = bytes_read / sizeof(int16_t);
        // 3. 计算重采样后应输出的采样点数
        int samples_to_write = (samples_read * SAMPLE_RATE_OUT) / SAMPLE_RATE_IN;

        // 4. 对音频数据进行重采样（44100Hz -> 16kHz）
        resample_audio(resample_buffer, output_buffer, samples_read, samples_to_write);

        // 5. 将重采样后的音频数据写入I2S1（扬声器播放）
        i2s_write(I2S_NUM_1, output_buffer, samples_to_write * sizeof(int16_t), &bytes_written, portMAX_DELAY);

        // 6. 短暂延时，防止CPU占用过高
        vTaskDelay(pdMS_TO_TICKS(10));
    }
} 