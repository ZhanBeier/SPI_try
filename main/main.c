/**
 * @file      main.c
 * @brief     ADS1115 四通道 ADC 采集程序
 * @author    Bear Zhan
 * @date      2026-06-27
 * @version   V0.3
 *
 * @description
 *   通过 I²C 通信读取 ADS1115 四通道电压值：
 *   AIN0 - 电位器分压
 *   AIN1 - 5V 电源监测
 *   AIN2 - 3.3V 电源监测
 *   AIN3 - NTC 热敏电阻
 *
 * @hardware
 *   MCU:   ESP32-S3
 *   ADC:   ADS1115 (I²C Address: 0x48)
 *   SDA:   GPIO42
 *   SCL:   GPIO41
 *
 * @changelog
 *   V0.1  2026-06-26  初始版本，验证ADS1115功能正常，验证Toggle Pin配USB-OTG调试正常
 *   V0.2  2026-06-26  在原来基础上修改了各通道的PGA值以更精确地测量；增加温度换算公式
 */
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ads1115.h"
#include "tja1051t_3.h"

static const char *TAG = "main";

#define NTC_R25       10000.0f
#define NTC_B         3950.0f
#define NTC_R_FIXED   10000.0f
#define NTC_T0_K      298.15f

static float ntc_calc_temperature(float v_ntc, float vcc)
{
    if (v_ntc <= 0.0f || v_ntc >= vcc)
        return -999.0f;

    float r_ntc = v_ntc * NTC_R_FIXED / (vcc - v_ntc);
    float t_k   = 1.0f / (1.0f / NTC_T0_K + (1.0f / NTC_B) * logf(r_ntc / NTC_R25));
    return t_k - 273.15f;
}

void app_main(void)
{
    ESP_LOGI(TAG, "ADS1115 4-Channel Reader");
    ads1115_init();
    tja1051t_3_init();

    int16_t adc_result[4] = {};

    while (1)
    {
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
        float v_ntc   = (float)adc_result[3] * ads1115_get_channel_info(3)->fsr / 32768.0f;
        float temp    = ntc_calc_temperature(v_ntc, vcc_3v3);

        printf("VCC: %.4f V, NTC: %.4f V, Temp: %.1f °C\n",
               vcc_3v3, v_ntc, temp);
        printf("-------------------------------\n");

        /* 打包四个通道原始值为 CAN 扩展帧发送 */
        uint8_t can_data[8];
        for (int ch = 0; ch < 4; ch++)
        {
            can_data[ch * 2]     = (adc_result[ch] >> 8) & 0xFF;  // 高字节
            can_data[ch * 2 + 1] = adc_result[ch] & 0xFF;         // 低字节
        }

        tja1051t_3_send(0x1234, can_data, 8, true);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}