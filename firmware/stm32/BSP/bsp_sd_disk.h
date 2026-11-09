/**
 * @file  bsp_sd_disk.h
 * @brief SD 卡 SPI 底层驱动（SPI1 模式，给 FatFs 的 diskio.c 用）
 *
 * 接线（与 hardware/硬件接线说明.md 一致）：
 *   PA4  SPI1_CS   → SD 卡座 CS
 *   PA5  SPI1_SCK  → SD 卡座 SCK
 *   PA6  SPI1_MISO ← SD 卡座 MISO
 *   PA7  SPI1_MOSI → SD 卡座 MOSI
 *
 * 说明：本驱动只做扇区级读写，不做文件系统；文件系统由 Middlewares/FatFs 提供。
 *       卡需在电脑上格式化为 FAT32（F103 无 f_mkfs）。
 */
#ifndef BSP_SD_DISK_H
#define BSP_SD_DISK_H

#include <stdint.h>
#include "app_config.h"

#if APP_SDCARD_ENABLE

/** 初始化 SPI1 与 SD 卡（含 CMD0/CMD8/ACMD41/CMD58 流程）。
 *  @return 1 = 成功，0 = 失败（未插卡 / 卡损坏 / 接线错误） */
uint8_t bsp_sd_disk_init(void);

/** 读一个 512 字节扇区 */
uint8_t bsp_sd_disk_read(uint8_t *buf, uint32_t sector);

/** 写一个 512 字节扇区（调用方负责先擦除对齐，本驱动按块写） */
uint8_t bsp_sd_disk_write(const uint8_t *buf, uint32_t sector);

/** 卡是否已初始化成功 */
uint8_t bsp_sd_disk_ready(void);

#else  /* APP_SDCARD_ENABLE == 0 */

uint8_t bsp_sd_disk_init(void);
uint8_t bsp_sd_disk_read(uint8_t *buf, uint32_t sector);
uint8_t bsp_sd_disk_write(const uint8_t *buf, uint32_t sector);
uint8_t bsp_sd_disk_ready(void);

#endif /* APP_SDCARD_ENABLE */

#endif /* BSP_SD_DISK_H */
