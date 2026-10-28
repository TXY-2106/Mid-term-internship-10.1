/**
 * @file  bsp_sdcard.h
 * @brief SD 卡挂载与 CSV 追加（SPI1 + FatFs）
 *
 * SD 卡座（大卡）接 SPI1：
 *   PA5 SCK, PA6 MISO, PA7 MOSI, PA4 CS
 *
 * FatFs 关键配置（ffconf.h）：
 *   _FS_READONLY = 0    可写
 *   _USE_MKFS    = 1    首次使用可格式化
 *   _FS_TINY     = 1    省 RAM（F103 只有 20 KB）
 *   _USE_LFN     = 0    省 RAM，文件名为 8.3
 *   _MAX_SS      = 512
 */
#ifndef BSP_SDCARD_H
#define BSP_SDCARD_H

#include <stdint.h>

/** 挂载并打开两个日志文件（不存在则创建并写表头）。返回 1 成功。 */
uint8_t bsp_sdcard_init(void);

/** 追加一行到状态日志，自动补 "\r\n"。返回 1 成功。 */
uint8_t bsp_sdcard_append_status(const char *line);

/** 追加一行到事件日志。返回 1 成功。 */
uint8_t bsp_sdcard_append_event(const char *line);

/** 把 FAT 缓冲刷入卡内。建议每 10 s 调一次，掉电最多丢 10 s。 */
void bsp_sdcard_sync(void);

#endif /* BSP_SDCARD_H */
