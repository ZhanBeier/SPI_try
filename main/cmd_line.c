#include "cmd_line.h"
#include "esp_log.h"
#include "ads1115.h"
#include "tja1051t_3.h"
#include "ltc6820.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uart_comm.h"
#include "ntc_calc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
 *  命令实现
 * ================================================================ */
static void cmd_help(int argc, char *argv[])
{
    uart_comm_print("\n");
    for (const cmd_t *c = cmd_table; c->name != NULL; c++)
        uart_comm_print("  %-8s %-20s %s\n", c->name, c->args_desc, c->help);
    uart_comm_print("\n");
}

static void cmd_adc(int argc, char *argv[])
{
    int ch_start = 0, ch_end = 4;
    if (argc >= 2)
    {
        int ch = atoi(argv[1]);
        if (ch < 0 || ch > 3)
        {
            uart_comm_print("Invalid channel: %s\n", argv[1]);
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
        uart_comm_print("  %s  Raw: %6d  Voltage: %.4f V\n", info->name, raw, voltage);
    }
}

static void cmd_temp(int argc, char *argv[])
{
    int16_t raw_vcc = ads1115_read_channel(ADS1115_CH_AIN2);
    int16_t raw_ntc = ads1115_read_channel(ADS1115_CH_AIN3);
    float vcc = (float)raw_vcc * ads1115_get_channel_info(ADS1115_CH_AIN2)->fsr / 32768.0f;
    float vntc = (float)raw_ntc * ads1115_get_channel_info(ADS1115_CH_AIN3)->fsr / 32768.0f;
    float temp = ntc_calc_temperature(vntc, vcc);
    uart_comm_print("  VCC_3V3: %.4f V  NTC: %.4f V  Temp: %.1f C\n", vcc, vntc, temp);
}

static void cmd_can_send(int argc, char *argv[])
{
    if (argc < 3)
    {
        uart_comm_print("Usage: can <id_hex> <byte1_hex> [byte2_hex ...]\n");
        uart_comm_print("Example: can 1234 AB CD EF\n");
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
    uart_comm_print("CAN TX ID=0x%08lX [%d bytes]: ", (unsigned long)id, len);
    for (int i = 0; i < len; i++)
        uart_comm_print("%02X ", data[i]);
    uart_comm_print("  %s\n", ret == ESP_OK ? "OK" : "FAIL");
}

static void cmd_led(int argc, char *argv[])
{
    gpio_set_level(GPIO_NUM_4, !gpio_get_level(GPIO_NUM_4));
    uart_comm_print("GPIO4 -> %d\n", gpio_get_level(GPIO_NUM_4));
}

static void cmd_info(int argc, char *argv[])
{
    uart_comm_print("\n");
    uart_comm_print("  Board     : ESP32-S3 (BMS Test V0.5)\n");
    uart_comm_print("  ADS1115   : I2C  SDA=%d SCL=%d  Addr=0x48\n",
                   ADS1115_I2C_SDA_PIN, ADS1115_I2C_SCL_PIN);
    uart_comm_print("  CAN       : TX=%d  RX=%d  (TWAI)\n",
                   TJA1051T_3_TX_PIN, TJA1051T_3_RX_PIN);
    uart_comm_print("  LTC6820   : SPI CLK=%d MOSI=%d MISO=%d\n",
                   LTC6820_SPI_CLK_PIN, LTC6820_SPI_MOSI_PIN, LTC6820_SPI_MISO_PIN);
    uart_comm_print("  LTC6820   : CS5=%d  CS6=%d\n",
                   LTC6820_CS5_PIN, LTC6820_CS6_PIN);
    uart_comm_print("  Test LED  : GPIO4\n");
    uart_comm_print("\n");
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
    uart_comm_print("Unknown command: '%s'. Type 'help' for available commands.\n", argv[0]);
}

/* ================================================================
 *  命令行任务
 * ================================================================ */
#define CMD_LINE_BUF 256

static void uart_cmd_task(void *arg)
{
    uart_comm_init();

    uint8_t line[CMD_LINE_BUF];
    int pos = 0;

    uart_comm_print("\n"
                   "========================================\n"
                   "  BMS Test Console (ESP32-S3 V0.5)\n"
                   "  Type 'help' for available commands.\n"
                   "========================================\n\n");

    while (1)
    {
        uint8_t c;
        if (uart_comm_read_byte(&c, 100) > 0)
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
                    uart_comm_print("\b \b");
                }
            }
            else
            {
                if (pos < CMD_LINE_BUF - 1)
                {
                    line[pos++] = c;
                    uart_comm_print("%c", c);
                }
            }
        }
    }
}

void cmd_line_start(void)
{
    xTaskCreate(uart_cmd_task, "cmd_line", 4096, NULL, 5, NULL);
}