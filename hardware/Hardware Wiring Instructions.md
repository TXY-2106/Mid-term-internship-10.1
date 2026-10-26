# Hardware Wiring Guide

> STM32F103C8T6 / ESP32-WROOM-32 / E18-D80NK ×4 / DS3231 / full-size SD card slot

## 1. Power

```
Power bank, 5V/2A or higher
   │ USB-A → Type-C
   ▼
ESP32 Type-C
   │
   ├── VIN ──► STM32 5V
   ├── VIN ──► E18 ×4 (brown wire)
   └── VIN ──► SD module +5
GND: all common ground

STM32 3V3 ──► DS3231 VCC, four 10k pull-ups
```

- Do not plug in the STM32 Type-C port; do not connect the ESP32 3V3 and STM32 3V3 in parallel.

## 2. STM32 Pin Assignment

| Function | Pin | Notes |
| --- | --- | --- |
| Entry sensor A | PB12 | Input, falling edge |
| Entry sensor B | PB13 | Input, falling edge |
| Queue 50% | PB14 | Input, level + timing |
| Queue 100% | PB15 | Input, level + timing |
| SD CS | PA4 | Output, software chip select |
| SD SCK | PA5 | SPI1 |
| SD MISO | PA6 | SPI1 |
| SD MOSI | PA7 | SPI1 |
| DS3231 SCL | PB6 | I2C1 |
| DS3231 SDA | PB7 | I2C1 |
| ESP32 RX | PA2 | USART2_TX |
| ESP32 TX | PA3 | USART2_RX |
| ST-Link | PA13 / PA14 | SWDIO / SWCLK |
| Power | 5V / GND | From ESP32 VIN |
| Peripheral 3.3V | 3V3 | For DS3231 and pull-ups |

PB12–PB15, PA4–PA7, PB6–PB7 and PA2–PA3 map to GPIO, SPI1, I2C1 and USART2 respectively; there are no conflicts.

## 3. Module Wiring

### E18-D80NK ×4

| Sensor | Brown | Blue | Black |
| --- | --- | --- | --- |
| Entry A | 5V | GND | PB12 |
| Entry B | 5V | GND | PB13 |
| Queue 50% | 5V | GND | PB14 |
| Queue 100% | 5V | GND | PB15 |

Add a **10k pull-up to 3.3V** on each black wire; NPN normally open, active low.

### DS3231

| Module | Connect to |
| --- | --- |
| VCC | 3.3V (STM32 3V3) |
| GND | GND |
| SCL | PB6 |
| SDA | PB7 |
| SQW / 32K | Leave unconnected |

I²C address 0x68. If the battery is a CR2032, remove R5 (charging resistor) or replace it with an LIR2032.

### SD Card Slot

| Module | Connect to |
| --- | --- |
| GND | GND |
| +3.3 | Leave unconnected |
| +5 | 5V (ESP32 VIN) |
| CS | PA4 |
| MOSI | PA7 |
| SCK | PA5 |
| MISO | PA6 |
| GND | GND |

### ESP32

| Module | Connect to |
| --- | --- |
| Type-C | Power bank (USB-A to Type-C) |
| VIN | 5V output to STM32 / E18 / SD |
| GND | GND |
| GPIO16 (U2RXD) | PA2 |
| GPIO17 (U2TXD) | PA3 |

Use VIN for the 5V output; do not use the 3.3 pin.

### ST-Link

| ST-Link | STM32 |
| --- | --- |
| SWDIO | PA13 |
| SWCLK | PA14 |
| GND | GND |
| 3.3V | Not connected |

## 4. Power-on Checks

1. With the power bank unloaded, measure the USB output: 5V.
2. Connect only the ESP32; measure VIN: about 4.7V.
3. Connect STM32 5V/GND and flash the blink program.
4. Connect the E18 sensors one by one: black wire 3.3V when unobstructed, 0V when obstructed.
5. Connect the DS3231; scan the I²C bus and find 0x68.
6. Connect the SD card (FAT32) and confirm it mounts.
7. Connect the ESP32 serial port or Wi-Fi page and confirm the data.
8. Run continuously for 30 minutes and check whether it resets during Wi-Fi transmission.

## 5. Notes

- The E18 requires a 5V supply; do not use the 24V version.
- If the ESP32 repeatedly reboots at Wi-Fi peaks, use a shorter USB cable or a laptop USB 3.0 port.
- The STM32 Type-C is for power only, not serial; flash through ST-Link.
