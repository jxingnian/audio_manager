#include <string.h>
#include "esp_log.h"
#include "audio_manager.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "i2s_stream.h"
#include "opus_encoder.h"

static const char *TAG = "AUDIO_MANAGER";

struct audio_manager {
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t i2s_stream_reader;
    audio_element_handle_t opus_encoder;
    audio_event_iface_handle_t evt;
    audio_manager_config_t config;
};

audio_manager_handle_t audio_manager_init(const audio_manager_config_t* config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid config");
        return NULL;
    }

    audio_manager_handle_t manager = calloc(1, sizeof(struct audio_manager));
    if (!manager) {
        ESP_LOGE(TAG, "Failed to allocate memory for audio manager");
        return NULL;
    }

    memcpy(&manager->config, config, sizeof(audio_manager_config_t));

    // 创建I2S流配置
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_cfg.transmit_mode = I2S_COMM_MODE_STD;
    
    // 配置I2S通道
    i2s_cfg.chan_cfg.id = config->i2s_num;
    i2s_cfg.chan_cfg.role = I2S_ROLE_MASTER;
    i2s_cfg.chan_cfg.dma_desc_num = 3;
    i2s_cfg.chan_cfg.dma_frame_num = 300;
    i2s_cfg.chan_cfg.auto_clear = false;

    // 配置时钟
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = config->sample_rate;
    i2s_cfg.std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    i2s_cfg.std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    // 配置时隙
    i2s_cfg.std_cfg.slot_cfg.data_bit_width = config->bit_width;
    i2s_cfg.std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
    i2s_cfg.std_cfg.slot_cfg.slot_mode = config->channel == 1 ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO;
    i2s_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    i2s_cfg.std_cfg.slot_cfg.ws_width = 24;
    i2s_cfg.std_cfg.slot_cfg.ws_pol = false;
    i2s_cfg.std_cfg.slot_cfg.bit_shift = true;

    // 配置GPIO
    i2s_cfg.std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    i2s_cfg.std_cfg.gpio_cfg.bclk = config->bclk_pin;
    i2s_cfg.std_cfg.gpio_cfg.ws = config->ws_pin;
    i2s_cfg.std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    i2s_cfg.std_cfg.gpio_cfg.din = config->din_pin;

    // 创建音频管道
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    manager->pipeline = audio_pipeline_init(&pipeline_cfg);
    if (!manager->pipeline) {
        ESP_LOGE(TAG, "Failed to create audio pipeline");
        goto _cleanup;
    }

    // 创建OPUS编码器
    opus_encoder_cfg_t opus_cfg = DEFAULT_OPUS_ENCODER_CONFIG();
    opus_cfg.sample_rate = config->sample_rate;
    opus_cfg.channel = config->channel;
    opus_cfg.bitrate = 64000;
    manager->opus_encoder = encoder_opus_init(&opus_cfg);
    if (!manager->opus_encoder) {
        ESP_LOGE(TAG, "Failed to create OPUS encoder");
        goto _cleanup;
    }

    // 创建I2S输入流
    manager->i2s_stream_reader = i2s_stream_init(&i2s_cfg);
    if (!manager->i2s_stream_reader) {
        ESP_LOGE(TAG, "Failed to create I2S stream reader");
        goto _cleanup;
    }

    // 注册元素到管道
    audio_pipeline_register(manager->pipeline, manager->i2s_stream_reader, "i2s");
    audio_pipeline_register(manager->pipeline, manager->opus_encoder, "opus");

    // 链接管道元素
    const char *link_tag[2] = {"i2s", "opus"};
    audio_pipeline_link(manager->pipeline, &link_tag[0], 2);

    // 创建事件监听器
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    manager->evt = audio_event_iface_init(&evt_cfg);
    if (!manager->evt) {
        ESP_LOGE(TAG, "Failed to create event interface");
        goto _cleanup;
    }

    // 设置事件监听
    audio_pipeline_set_listener(manager->pipeline, manager->evt);

    return manager;

_cleanup:
    audio_manager_deinit(manager);
    return NULL;
}

esp_err_t audio_manager_start(audio_manager_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return audio_pipeline_run(handle->pipeline);
}

esp_err_t audio_manager_stop(audio_manager_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return audio_pipeline_stop(handle->pipeline);
}

esp_err_t audio_manager_deinit(audio_manager_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->pipeline) {
        audio_pipeline_stop(handle->pipeline);
        audio_pipeline_wait_for_stop(handle->pipeline);
        audio_pipeline_terminate(handle->pipeline);
        
        audio_pipeline_unregister(handle->pipeline, handle->i2s_stream_reader);
        audio_pipeline_unregister(handle->pipeline, handle->opus_encoder);
        
        audio_pipeline_remove_listener(handle->pipeline);
        audio_pipeline_deinit(handle->pipeline);
    }

    if (handle->evt) {
        audio_event_iface_destroy(handle->evt);
    }

    if (handle->i2s_stream_reader) {
        audio_element_deinit(handle->i2s_stream_reader);
    }

    if (handle->opus_encoder) {
        audio_element_deinit(handle->opus_encoder);
    }

    free(handle);
    return ESP_OK;
}

audio_pipeline_handle_t audio_manager_get_pipeline(audio_manager_handle_t handle)
{
    return handle ? handle->pipeline : NULL;
}

esp_err_t audio_manager_wait_for_event(audio_manager_handle_t handle, audio_event_iface_msg_t* msg, int timeout_ms)
{
    if (!handle || !msg) {
        return ESP_ERR_INVALID_ARG;
    }
    
    TickType_t timeout_ticks = timeout_ms == -1 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return audio_event_iface_listen(handle->evt, msg, timeout_ticks);
} 