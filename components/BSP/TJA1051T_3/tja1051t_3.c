#include "tja1051t_3.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/twai.h"
#include <string.h>

static const char *TAG = "tja1051t_3";

esp_err_t tja1051t_3_init(void)
{
    /* ---- CAN 速率配置 ---- */
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();

    /* ---- 默认滤波器：接收所有帧 ---- */
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    /* ---- 通用配置 ---- */
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        TJA1051T_3_TX_PIN, TJA1051T_3_RX_PIN, TWAI_MODE_NORMAL);

    /* ---- 安装驱动 ---- */
    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ---- 启动 ---- */
    ret = twai_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "TWAI initialized (TX=%d RX=%d 500kbps)",
             TJA1051T_3_TX_PIN, TJA1051T_3_RX_PIN);
    return ESP_OK;
}

esp_err_t tja1051t_3_send(uint32_t id, const uint8_t *data, uint8_t len, bool extd)
{
    if (len > 8)
        len = 8;

    twai_message_t msg = {
        .identifier = id,
        .data_length_code = len,
        .self = 0,
        .rtr = 0,
        .flags = extd ? TWAI_MSG_FLAG_EXTD : 0,
    };

    if (data && len > 0)
        memcpy(msg.data, data, len);

    esp_err_t ret = twai_transmit(&msg, pdMS_TO_TICKS(20));
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "TX failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}