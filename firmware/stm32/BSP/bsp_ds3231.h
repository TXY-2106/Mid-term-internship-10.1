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
 * 时间是否"可用"：器件在线 + OSF 已清 + 年份在 2020–2099 + 月/日在合法范围。
 * 全新芯片或换过电池、从未写过时间时寄存器是 2000-01-01，这里返回 0，
 * 调用方应把时间戳当作不可用，等网页校时后再打开。
 * @return 1 = 可用；0 = 可用性不足（含器件不在线）
 */
uint8_t bsp_ds3231_is_valid(void);

/**
 * 读取当前时间并格式化为 "YYYY-MM-DDTHH:MM:SS"（与协议字段一致）。
 * @return 1 成功；0 失败（调用方应把该字段写成 "-"）
 */
uint8_t bsp_ds3231_get_iso8601(char *buf, uint8_t len);

/**
 * 把 "YYYY-MM-DDThh:mm:ss" 写入 DS3231 并清 OSF 停振标志。
 * 用途：DS3231 掉电/换电池后时间会回到 2000-01-01，网页点一下"校准时间"即可恢复。
 * 只接受 2020–2099 的时间；非法串或 I2C 写失败返回 0。
 * @return 1 成功；0 失败
 */
uint8_t bsp_ds3231_set_iso8601(const char *iso);

/**
 * 扫描 I2C1 总线 0x08–0x77，把有应答的地址写进 addrs。
 * 用于现场区分"总线什么都没有"（供电/接线）和"有器件但不是 0x68"（地址不符）。
 * @param addrs      输出缓冲
 * @param max_found  缓冲最多放几个
 * @return 应答器件数（可能大于 max_found，此时只写了前 max_found 个）
 */
uint8_t bsp_ds3231_scan(uint8_t *addrs, uint8_t max_found);

#endif /* BSP_DS3231_H */
