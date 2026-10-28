/**
 * @file  bsp_ds3231.h
 * @brief DS3231 离线时间戳（I2C1: PB6/PB7）
 */
#ifndef BSP_DS3231_H
#define BSP_DS3231_H

#include <stdint.h>

#define DS3231_I2C_ADDR 0x68u

/** 初始化 I2C1 并探测器件。返回 1 表示找到 DS3231。 */
uint8_t bsp_ds3231_init(void);

/**
 * 读取当前时间并格式化为 "YYYY-MM-DDTHH:MM:SS"（与协议字段一致）。
 * @return 1 成功；0 失败（调用方应把该字段写成 "-"）
 */
uint8_t bsp_ds3231_get_iso8601(char *buf, uint8_t len);

#endif /* BSP_DS3231_H */
