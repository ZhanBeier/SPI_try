#include "ltc6820.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

static const char *TAG = "ltc6820";
static spi_device_handle_t ltc6820_dev = NULL;

static const int cs_pins[] = {
    LTC6820_CS5_PIN,
    LTC6820_CS6_PIN,
};

esp_err_t ltc6820_init(void)
{
    /* 初始化所有 CS 引脚为输出高电平 */
    for (int i = 0; i < sizeof(cs_pins) / sizeof(cs_pins[0]); i++)
    {
        gpio_reset_pin(cs_pins[i]);
        gpio_set_direction(cs_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(cs_pins[i], 1);
    }

    spi_bus_config_t bus_cfg = {
        .miso_io_num = LTC6820_SPI_MISO_PIN,
        .mosi_io_num = LTC6820_SPI_MOSI_PIN,
        .sclk_io_num = LTC6820_SPI_CLK_PIN,
        .max_transfer_sz = 64,
    };

    esp_err_t ret = spi_bus_initialize(LTC6820_SPI_HOST, &bus_cfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = LTC6820_SPI_FREQ_HZ,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
    };

    ret = spi_bus_add_device(LTC6820_SPI_HOST, &dev_cfg, &ltc6820_dev);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI add device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPI initialized (CLK=%d MOSI=%d MISO=%d @ %dkHz)",
             LTC6820_SPI_CLK_PIN, LTC6820_SPI_MOSI_PIN,
             LTC6820_SPI_MISO_PIN, LTC6820_SPI_FREQ_HZ / 1000);
    return ESP_OK;
}

esp_err_t ltc6820_transfer(uint8_t cs_pin, uint8_t *tx, uint8_t *rx, uint8_t len,
                           uint16_t cs_hold_us, uint16_t pre_delay_us, uint16_t post_delay_us)
{
    spi_transaction_t txn = {
        .length = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    if (pre_delay_us > 0)
        esp_rom_delay_us(pre_delay_us);

    gpio_set_level(cs_pin, 0);
    esp_rom_delay_us(cs_hold_us);

    esp_err_t ret = spi_device_transmit(ltc6820_dev, &txn);

    esp_rom_delay_us(post_delay_us);
    gpio_set_level(cs_pin, 1);

    return ret;
}