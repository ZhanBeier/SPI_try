#ifndef LTC6820_H
#define LTC6820_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ---- SPI 引脚 ---- */
#define LTC6820_SPI_HOST        SPI2_HOST
#define LTC6820_SPI_CLK_PIN     21
#define LTC6820_SPI_MOSI_PIN    48
#define LTC6820_SPI_MISO_PIN    47
#define LTC6820_SPI_FREQ_HZ     500000

/* ---- CS 引脚定义 ---- */
#define LTC6820_CS5_PIN         10
#define LTC6820_CS6_PIN         9

/* ---- API ---- */
esp_err_t ltc6820_init(void);
esp_err_t ltc6820_transfer(uint8_t cs_pin, uint8_t *tx, uint8_t *rx, uint8_t len,
                           uint16_t cs_hold_us, uint16_t pre_delay_us, uint16_t post_delay_us);

#endif /* LTC6820_H */