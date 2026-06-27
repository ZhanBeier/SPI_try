#ifndef TJA1051T_3_H
#define TJA1051T_3_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ---- 硬件参数 ---- */
#define TJA1051T_3_TX_PIN 1
#define TJA1051T_3_RX_PIN 2
#define TJA1051T_3_BITRATE 250000

/* ---- API ---- */
esp_err_t tja1051t_3_init(void);
esp_err_t tja1051t_3_send(uint32_t id, const uint8_t *data, uint8_t len, bool extd);

#endif