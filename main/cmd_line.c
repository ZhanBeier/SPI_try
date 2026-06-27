#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cmd_line.h"
#include "esp_linenoise.h"
#include "esp_log.h"
#include "ads1115.h"
#include "tja1051t_3.h"
#include "ltc6820.h"
#include "driver/gpio.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static const char *TAG = "cmd_line";

/* ================================================================
 *  NTC 计算
 * ================================================================ */
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

/* ================================================================
 *  命令定义
 * ================================================================ */
typedef void (*cmd_func_t)(int argc, char *argv[]);

typedef struct
{
    const char *name;
    const char *args_desc;
    const char *help;
    cmd_func_t func;
} cmd_t;

/* 前向声明 */
static void cmd_help(int argc, char *argv[]);
static void cmd_adc(int argc, char *argv[]);
static void cmd_temp(int argc, char *argv[]);
static void cmd_can_send(int argc, char *argv[]);
static void cmd_led(int argc, char *argv[]);
static void cmd_info(int argc, char *argv[]);

static const cmd_t cmd_table[] = {
    {"help", "", "Show available commands", cmd_help},
    {"adc", "[0-3]", "Read ADC channel (all if omitted)", cmd_adc},
    {"temp", "", "Read NTC temperature", cmd_temp},
    {"can", "id b1 [b2..b8]", "Send CAN frame (hex bytes)", cmd_can_send},
    {"led", "", "Toggle test LED (GPIO4)", cmd_led},
    {"info", "", "Show hardware pin info", cmd_info},
    {NULL, NULL, NULL, NULL},
};

/* ================================================================
 *  命令实现
 * ================================================================ */
static void cmd_help(int argc, char *argv[])
{
    printf("\n");
    for (const cmd_t *c = cmd_table; c->name != NULL; c++)
    {
        printf("  %-8s %-20s %s\n", c->name, c->args_desc, c->help);
    }
    printf("\n");
}

static void cmd_adc(int argc, char *argv[])
{
    int ch_start = 0, ch_end = 4;

    if (argc >= 2)
    {
        int ch = atoi(argv[1]);
        if (ch < 0 || ch > 3)
        {
            printf("Invalid channel: %s (valid: 0-3)\n", argv[1]);
            return;
        }
        ch_start = ch;
        ch_end = ch + 1;
    }

    for (int ch = ch_start; ch < ch_end; ch++)
    {
        int16_t raw = ads1115_read_channel(ch);
        const ads1115_channel_t *info = ads1115_get_channel_info(ch);
        float voltage = (float)raw * info->fsr / 32768.0f;
        printf("  %s  Raw: %6d  Voltage: %.4f V\n", info->name, raw, voltage);
    }
}

static void cmd_temp(int argc, char *argv[])
{
    int16_t raw_vcc = ads1115_read_channel(ADS1115_CH_AIN2);
    int16_t raw_ntc = ads1115_read_channel(ADS1115_CH_AIN3);

    float vcc = (float)raw_vcc * ads1115_get_channel_info(ADS1115_CH_AIN2)->fsr / 32768.0f;
    float vntc = (float)raw_ntc * ads1115_get_channel_info(ADS1115_CH_AIN3)->fsr / 32768.0f;
    float temp = ntc_calc_temperature(vntc, vcc);

    printf("  VCC_3V3: %.4f V  NTC: %.4f V  Temp: %.1f C\n", vcc, vntc, temp);
}

static void cmd_can_send(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: can <id_hex> <byte1_hex> [byte2_hex ...]\n");
        printf("Example: can 1234 AB CD EF\n");
        return;
    }

    uint32_t id = (uint32_t)strtoul(argv[1], NULL, 16);
    uint8_t data[8] = {0};
    uint8_t len = argc - 2;
    if (len > 8)
        len = 8;

    for (int i = 0; i < len; i++)
        data[i] = (uint8_t)strtoul(argv[i + 2], NULL, 16);

    esp_err_t ret = tja1051t_3_send(id, data, len, true);
    printf("CAN TX ID=0x%08lX [%d bytes]: ", (unsigned long)id, len);
    for (int i = 0; i < len; i++)
        printf("%02X ", data[i]);
    printf("  %s\n", ret == ESP_OK ? "OK" : "FAIL");
}

static void cmd_led(int argc, char *argv[])
{
    gpio_set_level(GPIO_NUM_4, !gpio_get_level(GPIO_NUM_4));
    printf("GPIO4 -> %d\n", gpio_get_level(GPIO_NUM_4));
}

static void cmd_info(int argc, char *argv[])
{
    printf("\n");
    printf("  Board     : ESP32-S3 (BMS Test V0.5)\n");
    printf("  ADS1115   : I2C  SDA=%d SCL=%d  Addr=0x48\n",
           ADS1115_I2C_SDA_PIN, ADS1115_I2C_SCL_PIN);
    printf("  CAN       : TX=%d  RX=%d  (TWAI)\n",
           TJA1051T_3_TX_PIN, TJA1051T_3_RX_PIN);
    printf("  LTC6820   : SPI CLK=%d MOSI=%d MISO=%d\n",
           LTC6820_SPI_CLK_PIN, LTC6820_SPI_MOSI_PIN, LTC6820_SPI_MISO_PIN);
    printf("  LTC6820   : CS5=%d  CS6=%d\n",
           LTC6820_CS5_PIN, LTC6820_CS6_PIN);
    printf("  Test LED  : GPIO4\n");
    printf("\n");
}

/* ================================================================
 *  命令解析
 * ================================================================ */
static void process_line(char *line)
{
    while (*line == ' ')
        line++;
    if (*line == '\0')
        return;

    char *argv[12];
    int argc = 0;

    argv[argc++] = line;
    char *p = line;
    while (*p && argc < 12)
    {
        if (*p == ' ')
        {
            *p = '\0';
            p++;
            while (*p == ' ')
                p++;
            if (*p)
                argv[argc++] = p;
        }
        else
        {
            p++;
        }
    }

    for (const cmd_t *c = cmd_table; c->name != NULL; c++)
    {
        if (strcmp(argv[0], c->name) == 0)
        {
            c->func(argc, argv);
            return;
        }
    }
    printf("Unknown command: '%s'. Type 'help' for available commands.\n", argv[0]);
}

/* ================================================================
 *  esp_linenoise 实例管理
 * ================================================================ */
static esp_err_t linenoise_init(void)
{
    esp_linenoise_config_t config;
    esp_linenoise_get_instance_config_default(&config);

    config.max_cmd_line_length = 256;
    config.history_max_length = 20;
    config.allow_empty_line = true;
    config.allow_dumb_mode = true;
    config.in_fd = STDIN_FILENO;
    config.out_fd = STDOUT_FILENO;

    esp_linenoise_handle_t handle = NULL;
    esp_err_t ret = esp_linenoise_create_instance(&config, &handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "linenoise create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_linenoise_set_dumb_mode(handle, true);

    extern esp_linenoise_handle_t s_linenoise_handle;
    s_linenoise_handle = handle;

    return ESP_OK;
}

static esp_linenoise_handle_t linenoise_get_handle(void)
{
    extern esp_linenoise_handle_t s_linenoise_handle;
    return s_linenoise_handle;
}

static void linenoise_cleanup(void)
{
    esp_linenoise_handle_t handle = linenoise_get_handle();
    if (handle)
    {
        esp_linenoise_delete_instance(handle);
    }
}

/* ================================================================
 *  任务入口
 * ================================================================ */
esp_linenoise_handle_t s_linenoise_handle = NULL;

static void cmd_line_task(void *arg)
{
    if (linenoise_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "linenoise init failed");
        vTaskDelete(NULL);
        return;
    }

    esp_linenoise_handle_t handle = linenoise_get_handle();
    if (!handle)
    {
        ESP_LOGE(TAG, "handle is NULL");
        vTaskDelete(NULL);
        return;
    }

    printf("\n"
           "========================================\n"
           "  BMS Test Console (ESP32-S3 V0.5)\n"
           "  Type 'help' for available commands.\n"
           "========================================\n\n");

    size_t max_len = 0;
    esp_err_t ret = esp_linenoise_get_max_cmd_line_length(handle, &max_len);
    ESP_LOGI(TAG, "get_max_line_len ret=%d, max_len=%d", ret, (int)max_len);

    char *line = malloc(max_len);
    ESP_LOGI(TAG, "malloc(%d) -> %p", (int)max_len, line);
    if (!line)
    {
        ESP_LOGE(TAG, "Failed to allocate line buffer");
        vTaskDelete(NULL);
        return;
    }

    while (1)
    {
        ret = esp_linenoise_get_line(handle, line, max_len);
        ESP_LOGI(TAG, "get_line ret=%d, line=[%s]", ret, line);
        if (ret == ESP_OK)
        {
            esp_linenoise_history_add(handle, line);
            process_line(line);
        }
    }

    free(line);
    esp_linenoise_delete_instance(handle);
}

void cmd_line_start(void)
{
    xTaskCreate(cmd_line_task, "cmd_line", 8192, NULL, 5, NULL);
}