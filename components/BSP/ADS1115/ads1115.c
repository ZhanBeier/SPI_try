#include "ads1115.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "ads1115";

#define ADS1115_REG_CONFIG 0x01
#define ADS1115_REG_CONVERSION 0x00

/* ---- 通道配置表 ---- */
static const ads1115_channel_t channels[4] = {
    {0xC383, "AIN0 (电位器) ", 4.096f},
    {0xD183, "AIN1 (5V电源) ", 6.144f},
    {0xE383, "AIN2 (3V3电源)", 4.096f},
    {0xF383, "AIN3 (NTC热敏)", 4.096f},
};

static i2c_master_dev_handle_t ads1115_handle = NULL;

/* ---- 内部函数 ---- */

static esp_err_t ads1115_write_config(uint16_t config)
{
    uint8_t buf[3] = {
        ADS1115_REG_CONFIG,
        (config >> 8) & 0xFF,
        config & 0xFF,
    };
    return i2c_master_transmit(ads1115_handle, buf, 3, pdMS_TO_TICKS(100));
}

static uint16_t ads1115_read_reg(uint8_t reg)
{
    uint8_t data[2] = {0};
    esp_err_t ret = i2c_master_transmit_receive(
        ads1115_handle, &reg, 1, data, 2, pdMS_TO_TICKS(100));
    if (ret != ESP_OK)
        return 0xFFFF;
    return (data[0] << 8) | data[1];
}

/* ---- 公开 API ---- */

esp_err_t ads1115_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = ADS1115_I2C_SDA_PIN,
        .scl_io_num = ADS1115_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
    };

    i2c_master_bus_handle_t bus_handle;
    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus_handle);
    if (ret != ESP_OK)
        return ret;

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ADS1115_I2C_ADDR,
        .scl_speed_hz = ADS1115_I2C_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(bus_handle, &dev_config, &ads1115_handle);
    if (ret != ESP_OK)
        return ret;

    ESP_LOGI(TAG, "ADS1115 initialized (SDA=%d SCL=%d)",
             ADS1115_I2C_SDA_PIN, ADS1115_I2C_SCL_PIN);
    return ESP_OK;
}

int16_t ads1115_read_channel(int ch)
{
    ads1115_write_config(channels[ch].config & 0x7FFF);
    vTaskDelay(pdMS_TO_TICKS(10));

    ads1115_write_config(channels[ch].config);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint16_t raw = ads1115_read_reg(ADS1115_REG_CONVERSION);
    return (int16_t)raw;
}

float ads1115_get_voltage(int ch)
{
    int16_t raw = ads1115_read_channel(ch);
    return (float)raw * channels[ch].fsr / 32768.0f;
}

const ads1115_channel_t *ads1115_get_channel_info(int ch)
{
    if (ch < 0 || ch >= 4)
        return NULL;
    return &channels[ch];
}