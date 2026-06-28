#include "cmd_line.h"
#include "esp_log.h"
#include "ads1115.h"
#include "tja1051t_3.h"
#include "ltc6820.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>

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

#define CMD_UART_PORT UART_NUM_0
#define CMD_UART_TX 43
#define CMD_UART_RX 44
#define CMD_UART_BUF 256

static void cmd_print(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0)
        uart_write_bytes(CMD_UART_PORT, buf, len);
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
 *  命令实现（全部保留，不改）
 * ================================================================ */
static void cmd_help(int argc, char *argv[])
{
    cmd_print("\n");
    for (const cmd_t *c = cmd_table; c->name != NULL; c++)
    {
        cmd_print("  %-8s %-20s %s\n", c->name, c->args_desc, c->help);
    }
    cmd_print("\n");
}

static void cmd_adc(int argc, char *argv[])
{
    int ch_start = 0, ch_end = 4;
    if (argc >= 2)
    {
        int ch = atoi(argv[1]);
        if (ch < 0 || ch > 3)
        {
            cmd_print("Invalid channel: %s\n", argv[1]);
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
        cmd_print("  %s  Raw: %6d  Voltage: %.4f V\n", info->name, raw, voltage);
    }
}

static void cmd_temp(int argc, char *argv[])
{
    int16_t raw_vcc = ads1115_read_channel(ADS1115_CH_AIN2);
    int16_t raw_ntc = ads1115_read_channel(ADS1115_CH_AIN3);
    float vcc = (float)raw_vcc * ads1115_get_channel_info(ADS1115_CH_AIN2)->fsr / 32768.0f;
    float vntc = (float)raw_ntc * ads1115_get_channel_info(ADS1115_CH_AIN3)->fsr / 32768.0f;
    float temp = ntc_calc_temperature(vntc, vcc);
    cmd_print("  VCC_3V3: %.4f V  NTC: %.4f V  Temp: %.1f C\n", vcc, vntc, temp);
}

static void cmd_can_send(int argc, char *argv[])
{
    if (argc < 3)
    {
        cmd_print("Usage: can <id_hex> <byte1_hex> [byte2_hex ...]\n");
        cmd_print("Example: can 1234 AB CD EF\n");
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
    cmd_print("CAN TX ID=0x%08lX [%d bytes]: ", (unsigned long)id, len);
    for (int i = 0; i < len; i++)
        cmd_print("%02X ", data[i]);
    cmd_print("  %s\n", ret == ESP_OK ? "OK" : "FAIL");
}

static void cmd_led(int argc, char *argv[])
{
    gpio_set_level(GPIO_NUM_4, !gpio_get_level(GPIO_NUM_4));
    cmd_print("GPIO4 -> %d\n", gpio_get_level(GPIO_NUM_4));
}

static void cmd_info(int argc, char *argv[])
{
    cmd_print("\n");
    cmd_print("  Board     : ESP32-S3 (BMS Test V0.5)\n");
    cmd_print("  ADS1115   : I2C  SDA=%d SCL=%d  Addr=0x48\n",
              ADS1115_I2C_SDA_PIN, ADS1115_I2C_SCL_PIN);
    cmd_print("  CAN       : TX=%d  RX=%d  (TWAI)\n",
              TJA1051T_3_TX_PIN, TJA1051T_3_RX_PIN);
    cmd_print("  LTC6820   : SPI CLK=%d MOSI=%d MISO=%d\n",
              LTC6820_SPI_CLK_PIN, LTC6820_SPI_MOSI_PIN, LTC6820_SPI_MISO_PIN);
    cmd_print("  LTC6820   : CS5=%d  CS6=%d\n",
              LTC6820_CS5_PIN, LTC6820_CS6_PIN);
    cmd_print("  Test LED  : GPIO4\n");
    cmd_print("\n");
}

/* ================================================================
 *  命令解析（保留，不改）
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
    cmd_print("Unknown command: '%s'. Type 'help' for available commands.\n", argv[0]);
}

/* ================================================================
 *  UART 命令行（替代 linenoise）
 * ================================================================ */

static void uart_cmd_init(void)
{
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(CMD_UART_PORT, 1024, 256, 0, NULL, 0);
    uart_param_config(CMD_UART_PORT, &cfg);
    uart_set_pin(CMD_UART_PORT, CMD_UART_TX, CMD_UART_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

static void uart_cmd_task(void *arg)
{
    uart_cmd_init();

    uint8_t line[CMD_UART_BUF];
    int pos = 0;

    cmd_print("\n"
              "========================================\n"
              "  BMS Test Console (ESP32-S3 V0.5)\n"
              "  Type 'help' for available commands.\n"
              "========================================\n\n");

    while (1)
    {
        uint8_t c;
        if (uart_read_bytes(CMD_UART_PORT, &c, 1, pdMS_TO_TICKS(100)) > 0)
        {
            if (c == '\r' || c == '\n')
            {
                if (pos > 0)
                {
                    line[pos] = '\0';
                    process_line((char *)line);
                    pos = 0;
                }
            }
            else if (c == '\b' || c == 0x7F)
            {
                if (pos > 0)
                {
                    pos--;
                    const char *del = "\b \b";
                    uart_write_bytes(CMD_UART_PORT, del, strlen(del));
                }
            }
            else
            {
                if (pos < CMD_UART_BUF - 1)
                {
                    line[pos++] = c;
                    uart_write_bytes(CMD_UART_PORT, &c, 1);
                }
            }
        }
    }
}

void cmd_line_start(void)
{
    xTaskCreate(uart_cmd_task, "cmd_line", 4096, NULL, 5, NULL);
}