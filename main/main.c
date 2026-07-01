/**
 * @file      main.c
 * @brief     基于ESP32S3的BMS主控测试，本版本基于第一块测试板
 * @author    Bear Zhan
 * @date      2026-06-28
 * @version   V0.5
 *
 * @description
 *   通过 I²C 通信读取 ADS1115 四通道电压值：
 *   AIN0 - 电位器分压
 *   AIN1 - 5V 电源监测
 *   AIN2 - 3.3V 电源监测
 *   AIN3 - NTC 热敏电阻
 *   通过 CAN(ESP叫TWAI) 通信向外发送各自的转换值
 *   header:0x00001234
 *   注意每组各自的增益不尽相同，具体参考组件代码
 *
 * @hardware
 *   MCU:       ESP32-S3
 *   ADC:       ADS1115 (I²C Address: 0x48)
 *   SDA:       GPIO42
 *   SCL:       GPIO41
 *   CAN XCVR:  TJA1051T/3
 *   CAN TX:    GPIO1
 *   CAN RX:    GPIO2
 */
#include <stdio.h>
#include <math.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ads1115.h"
#include "tja1051t_3.h"
#include "ltc6820.h"
#include "cmd_line.h"

#define NTC_R25 10000.0f
#define NTC_B 3950.0f
#define NTC_R_FIXED 10000.0f
#define NTC_T0_K 298.15f

static float ntc_calc_temperature(float v_ntc, float vcc)
{
    if (v_ntc <= 0.0f || v_ntc >= vcc)
        return -999.0f;

    float r_ntc = v_ntc * NTC_R_FIXED / (vcc - v_ntc);
    float t_k = 1.0f / (1.0f / NTC_T0_K + (1.0f / NTC_B) * logf(r_ntc / NTC_R25));
    return t_k - 273.15f;
}

static void adcread_task(void *arg)
{
    static const char *TAG = "main_adc";
    int16_t adc_result[4] = {};

    while (1)
    {
        ESP_LOGI(TAG, "ADC task running.\n");
        printf("-------------------------------\n");

        for (int ch = 0; ch < 4; ch++)
        {
            adc_result[ch] = ads1115_read_channel(ch);
            const ads1115_channel_t *info = ads1115_get_channel_info(ch);
            float voltage = (float)adc_result[ch] * info->fsr / 32768.0f;

            printf("%s  Raw: %6d  Voltage: %.4f V\n",
                   info->name, adc_result[ch], voltage);
        }

        float vcc_3v3 = (float)adc_result[2] * ads1115_get_channel_info(2)->fsr / 32768.0f;
        float v_ntc = (float)adc_result[3] * ads1115_get_channel_info(3)->fsr / 32768.0f;
        float temp = ntc_calc_temperature(v_ntc, vcc_3v3);

        printf("VCC: %.4f V, NTC: %.4f V, Temp: %.1f °C\n",
               vcc_3v3, v_ntc, temp);
        printf("-------------------------------\n");

        uint8_t can_data[8];
        for (int ch = 0; ch < 4; ch++)
        {
            can_data[ch * 2] = (adc_result[ch] >> 8) & 0xFF;
            can_data[ch * 2 + 1] = adc_result[ch] & 0xFF;
        }
        tja1051t_3_send(0x1234, can_data, 8, true);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void spi_test_task(void *arg)
{
    static const char *TAG = "main_spi";
    uint8_t tx_cs6 = 0xAB;
    uint8_t rx_cs6 = 0;
    uint8_t tx_cs5 = 0xCD;
    uint8_t rx_cs5 = 0;

    while (1)
    {
        /* CS6: 等1000us → 拉低 → 等1000us → 发送 → 等500us → 释放 */
        ltc6820_transfer(LTC6820_CS6_PIN, &tx_cs6, &rx_cs6, 1, 200, 200, 200);
        ESP_LOGI(TAG, "CS6 TX:0x%02X RX:0x%02X", tx_cs6, rx_cs6);

        esp_rom_delay_us(500);

        /* CS5: 等200us → 拉低 → 等200us → 发送 → 等500us → 释放 */
        ltc6820_transfer(LTC6820_CS5_PIN, &tx_cs5, &rx_cs5, 1, 200, 200, 200);
        ESP_LOGI(TAG, "CS5 TX:0x%02X RX:0x%02X", tx_cs5, rx_cs5);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void app_main(void)
{
    static const char *TAG = "main";
    ESP_LOGI(TAG, "BMS initializing...");
    /* 初始化前确保所有可控 GPIO 为低电平 */
    gpio_num_t safe_gpios[] = {GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7};
    for (int i = 0; i < sizeof(safe_gpios) / sizeof(safe_gpios[0]); i++)
    {
        gpio_reset_pin(safe_gpios[i]);
        gpio_set_direction(safe_gpios[i], GPIO_MODE_OUTPUT);
        gpio_set_level(safe_gpios[i], 0);
    }
    ads1115_init();
    tja1051t_3_init();
    ltc6820_init();

    xTaskCreate(adcread_task, "adcread", 4096, NULL, 5, NULL);
    xTaskCreate(spi_test_task, "spi_test", 4096, NULL, 5, NULL);
    cmd_line_start();
}