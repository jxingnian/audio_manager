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
// #include "mp3_decoder.h"        // MP3解码器（如需使用MP3解码功能可取消注释）

// #include "esp_peripherals.h"      // ESP外设管理相关API
// #include "periph_touch.h"       // 触摸外设（如需使用可取消注释）
// #include "periph_adc_button.h"  // ADC按键外设（如需使用可取消注释）
// #include "periph_button.h"      // 按键外设（如需使用可取消注释）
// #include "board.h"              // 板级支持包（如需使用可取消注释）

// 日志TAG
static const char *TAG = "PLAY_FLASH_MP3_CONTROL";

// // 文件标记结构体，用于记录当前播放的MP3文件的起止地址和播放进度
// static struct marker {
//     int pos;                // 当前已读取的字节数
//     const uint8_t *start;   // MP3文件起始地址
//     const uint8_t *end;     // MP3文件结束地址
// } file_marker;

// // 低码率MP3音频文件（8000Hz，双声道）
// extern const uint8_t lr_mp3_start[] asm("_binary_music_16b_2c_8000hz_mp3_start");
// extern const uint8_t lr_mp3_end[]   asm("_binary_music_16b_2c_8000hz_mp3_end");

// // 中码率MP3音频文件（22050Hz，双声道）
// extern const uint8_t mr_mp3_start[] asm("_binary_music_16b_2c_22050hz_mp3_start");
// extern const uint8_t mr_mp3_end[]   asm("_binary_music_16b_2c_22050hz_mp3_end");

// // 高码率MP3音频文件（44100Hz，双声道）
// extern const uint8_t hr_mp3_start[] asm("_binary_music_16b_2c_44100hz_mp3_start");
// extern const uint8_t hr_mp3_end[]   asm("_binary_music_16b_2c_44100hz_mp3_end");

// /**
//  * @brief 切换到下一个MP3文件
//  *        按顺序循环切换低、中、高码率的MP3文件
//  */
// static void set_next_file_marker()
// {
//     static int idx = 0; // 静态变量，记录当前文件索引

//     switch (idx) {
//         case 0:
//             file_marker.start = lr_mp3_start;
//             file_marker.end   = lr_mp3_end;
//             break;
//         case 1:
//             file_marker.start = mr_mp3_start;
//             file_marker.end   = mr_mp3_end;
//             break;
//         case 2:
//             file_marker.start = hr_mp3_start;
//             file_marker.end   = hr_mp3_end;
//             break;
//         default:
//             ESP_LOGE(TAG, "[ * ] Not supported index = %d", idx);
//     }
//     if (++idx > 2) {
//         idx = 0; // 循环回到第一个文件
//     }
//     file_marker.pos = 0; // 重置读取位置
// }

// /**
//  * @brief MP3数据读取回调函数
//  *        从file_marker指定的MP3文件中读取数据到buf
//  * 
//  * @param el        音频元素句柄
//  * @param buf       目标缓冲区
//  * @param len       期望读取的字节数
//  * @param wait_time 等待时间
//  * @param ctx       用户上下文
//  * @return int      实际读取的字节数，或AEL_IO_DONE表示读取结束
//  */
// int mp3_music_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
// {
//     // 计算剩余可读字节数
//     int read_size = file_marker.end - file_marker.start - file_marker.pos;
//     if (read_size == 0) {
//         // 已读完，返回AEL_IO_DONE通知解码器
//         return AEL_IO_DONE;
//     } else if (len < read_size) {
//         // 只读取len大小
//         read_size = len;
//     }
//     // 拷贝数据到buf
//     memcpy(buf, file_marker.start + file_marker.pos, read_size);
//     file_marker.pos += read_size; // 更新已读位置
//     return read_size;
// }

/**
 * @brief 主应用入口
 *        初始化音频管道、外设、事件监听，处理按键事件，实现MP3播放控制
 */
void app_main(void)
{
    audio_pipeline_handle_t pipeline;                // 音频管道句柄
    audio_element_handle_t i2s_stream_writer;//, mp3_decoder; // 元素句柄

    // 设置日志等级
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // ESP_LOGI(TAG, "[ 1 ] Start audio codec chip");
    // // 初始化音频板卡，启动音频编解码芯片
    // audio_board_handle_t board_handle = audio_board_init();
    // audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    // int player_volume;
    // // 获取当前音量
    // audio_hal_get_volume(board_handle->audio_hal, &player_volume);

    ESP_LOGI(TAG, "[ 2 ] Create audio pipeline, add all elements to pipeline, and subscribe pipeline event");
    // 创建音频管道
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    // ESP_LOGI(TAG, "[2.1] Create mp3 decoder to decode mp3 file and set custom read callback");
    // // 创建MP3解码器，并设置自定义读取回调
    // mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    // mp3_decoder = mp3_decoder_init(&mp3_cfg);
    // audio_element_set_read_cb(mp3_decoder, mp3_music_read_cb, NULL);

//     ESP_LOGI(TAG, "[2.2] Create i2s stream to write data to codec chip");
//     // 创建I2S流，用于向编解码芯片输出音频数据
// #if defined CONFIG_ESP32_C3_LYRA_V2_BOARD
//     i2s_stream_cfg_t i2s_cfg = I2S_STREAM_PDM_TX_CFG_DEFAULT();
// #else
//     i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
// #endif
//     i2s_cfg.type = AUDIO_STREAM_WRITER; // 设置为写模式
    // i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    // ESP_LOGI(TAG, "[2.3] Register all elements to audio pipeline");
    // // 注册MP3解码器和I2S流到管道
    // audio_pipeline_register(pipeline, mp3_decoder, "mp3");
    // audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "[2.4] Link it together [mp3_music_read_cb]-->mp3_decoder-->i2s_stream-->[codec_chip]");
    // 连接管道各元素
    const char *link_tag[2] = {"mp3", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);

    // ESP_LOGI(TAG, "[ 3 ] Initialize peripherals");
    // // 初始化外设（如按键、触摸等）
    // esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    // esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    // ESP_LOGI(TAG, "[3.1] Initialize keys on board");
    // // 初始化板载按键
    // audio_board_key_init(set);

    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    // 创建事件监听器
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    // 监听管道元素事件
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    // 监听外设事件
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGW(TAG, "[ 5 ] Tap touch buttons to control music player:");
    ESP_LOGW(TAG, "      [Play] to start, pause and resume, [Set] to stop.");
    ESP_LOGW(TAG, "      [Vol-] or [Vol+] to adjust volume.");

    ESP_LOGI(TAG, "[ 5.1 ] Start audio_pipeline");
    // 设置初始MP3文件
    // set_next_file_marker();
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

    //     // 处理MP3解码器上报的音乐信息事件
    //     if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) mp3_decoder
    //         && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
    //         audio_element_info_t music_info = {0};
    //         audio_element_getinfo(mp3_decoder, &music_info);
    //         ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
    //                  music_info.sample_rates, music_info.bits, music_info.channels);
    //         // 设置I2S时钟参数
    //         i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
    //         continue;
    //     }

    //     // 处理按键/触摸/ADC按钮事件
    //     if ((msg.source_type == PERIPH_ID_TOUCH || msg.source_type == PERIPH_ID_BUTTON || msg.source_type == PERIPH_ID_ADC_BTN)
    //         && (msg.cmd == PERIPH_TOUCH_TAP || msg.cmd == PERIPH_BUTTON_PRESSED || msg.cmd == PERIPH_ADC_BUTTON_PRESSED)) {
    //         // 播放/暂停/恢复
    //         if ((int) msg.data == get_input_play_id()) {
    //             ESP_LOGI(TAG, "[ * ] [Play] touch tap event");
    //             audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
    //             switch (el_state) {
    //                 case AEL_STATE_INIT :
    //                     ESP_LOGI(TAG, "[ * ] Starting audio pipeline");
    //                     audio_pipeline_run(pipeline);
    //                     break;
    //                 case AEL_STATE_RUNNING :
    //                     ESP_LOGI(TAG, "[ * ] Pausing audio pipeline");
    //                     audio_pipeline_pause(pipeline);
    //                     break;
    //                 case AEL_STATE_PAUSED :
    //                     ESP_LOGI(TAG, "[ * ] Resuming audio pipeline");
    //                     audio_pipeline_resume(pipeline);
    //                     break;
    //                 case AEL_STATE_FINISHED :
    //                     ESP_LOGI(TAG, "[ * ] Rewinding audio pipeline");
    //                     // 复位管道，切换下一个文件
    //                     audio_pipeline_reset_ringbuffer(pipeline);
    //                     audio_pipeline_reset_elements(pipeline);
    //                     audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
    //                     set_next_file_marker();
    //                     audio_pipeline_run(pipeline);
    //                     break;
    //                 default :
    //                     ESP_LOGI(TAG, "[ * ] Not supported state %d", el_state);
    //             }
    //         } 
    //         // 停止
    //         else if ((int) msg.data == get_input_set_id()) {
    //             ESP_LOGI(TAG, "[ * ] [Set] touch tap event");
    //             ESP_LOGI(TAG, "[ * ] Stopping audio pipeline");
    //             break; // 跳出主循环，进行资源释放
    //         } 
    //         // 切换曲目
    //         else if ((int) msg.data == get_input_mode_id()) {
    //             ESP_LOGI(TAG, "[ * ] [mode] tap event");
    //             // 停止并复位管道，切换下一个MP3文件
    //             audio_pipeline_stop(pipeline);
    //             audio_pipeline_wait_for_stop(pipeline);
    //             audio_pipeline_terminate(pipeline);
    //             audio_pipeline_reset_ringbuffer(pipeline);
    //             audio_pipeline_reset_elements(pipeline);
    //             set_next_file_marker();
    //             audio_pipeline_run(pipeline);
    //         } 
    //         // 音量加
    //         else if ((int) msg.data == get_input_volup_id()) {
    //             ESP_LOGI(TAG, "[ * ] [Vol+] touch tap event");
    //             player_volume += 10;
    //             if (player_volume > 100) {
    //                 player_volume = 100;
    //             }
    //             audio_hal_set_volume(board_handle->audio_hal, player_volume);
    //             ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
    //         } 
    //         // 音量减
    //         else if ((int) msg.data == get_input_voldown_id()) {
    //             ESP_LOGI(TAG, "[ * ] [Vol-] touch tap event");
    //             player_volume -= 10;
    //             if (player_volume < 0) {
    //                 player_volume = 0;
    //             }
    //             audio_hal_set_volume(board_handle->audio_hal, player_volume);
    //             ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
    //         }
    //     }
    // }

    // 退出主循环后，释放资源
    ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    audio_pipeline_unregister(pipeline, mp3_decoder);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);

    /* 在移除事件监听器前终止管道 */
    audio_pipeline_remove_listener(pipeline);

    /* 确保audio_pipeline_remove_listener已调用后再销毁事件接口 */
    audio_event_iface_destroy(evt);

    /* 释放所有资源 */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(mp3_decoder);
}
