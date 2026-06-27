
# 6804BMS

基于 ESP32-S3 的 BMS 主控测试项目，第一块测试板。

## 硬件

| 模块 | 型号 | 接口 | 引脚 |
|------|------|------|------|
| MCU | ESP32-S3 | - | - |
| ADC | ADS1115 | I2C | SDA=42 SCL=41 |
| CAN XCVR | TJA1051T/3 | TWAI | TX=1 RX=2 |
| 电池监测 | LTC6811-2 | isoSPI(LTC6820) | CLK=21 MOSI=48 MISO=47 |

## 项目结构
```
├── main/              主程序
├── components/BSP/
│   ├── ADS1115/       ADS1115驱动
│   ├── TJA1051T_3/    CAN收发驱动
│   └── LTC6820/       SPI通信驱动
```

## 构建

idf.py build
idf.py -p COM11 flash monitor


## 功能

- ADS1115 四通道电压采集（电位器/5V/3.3V/NTC温度）
- CAN 扩展帧发送采集数据（ID: 0x1234）
- LTC6820 SPI 通信测试

## 依赖

- ESP-IDF v5.4.1
