#ifndef ADS1115_H
#define ADS1115_H

#include <stdint.h>
#include "esp_err.h"

/* ---- 硬件参数 ---- */
#define ADS1115_I2C_ADDR 0x48
#define ADS1115_I2C_SDA_PIN 42
#define ADS1115_I2C_SCL_PIN 41
#define ADS1115_I2C_FREQ_HZ 100000

/* ---- 通道索引 ---- */
#define ADS1115_CH_AIN0 0 /* 电位器   */
#define ADS1115_CH_AIN1 1 /* 5V 电源  */
#define ADS1115_CH_AIN2 2 /* 3V3 电源 */
#define ADS1115_CH_AIN3 3 /* NTC 热敏 */

/* ---- 通道信息结构 ---- */
typedef struct
{
    uint16_t config;
    const char *name;
    float fsr;
} ads1115_channel_t;

/* ---- API ---- */
esp_err_t ads1115_init(void);
int16_t ads1115_read_channel(int ch);
float ads1115_get_voltage(int ch);
const ads1115_channel_t *ads1115_get_channel_info(int ch);

#endif /* ADS1115_H */