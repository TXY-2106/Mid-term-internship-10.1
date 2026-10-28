/**
 * @file  bsp_sdcard.c
 * @brief SD 卡挂载与 CSV 追加
 *
 * 需要在 Keil 工程中加入 FatFs（ff.c / diskio.c）并实现 diskio:
 *   disk_initialize / disk_status / disk_read / disk_write / disk_ioctl
 * SPI 部分参考 FatFs 官方示例 ff_mmcp.c 或 Chan's FatFs 的 STM32 SPI 例程。
 */
#include "bsp_sdcard.h"
#include "app_config.h"

#if APP_SDCARD_ENABLE
#include "ff.h"

static FATFS s_fs;
static FIL   s_status;
static FIL   s_events;
static uint8_t s_ready;

/* 极简 strlen，避免链接完整 libc（F103 Flash 只有 64 KB） */
static SIZE_T strlen_local(const char *s)
{
    SIZE_T n = 0u;
    while (s[n] != '\0') { n++; }
    return n;
}

static uint8_t append_line(FIL *fp, const char *line)
{
    UINT bw = 0u;
    if (f_write(fp, line, (UINT)strlen_local(line), &bw) != FR_OK) {
        return 0u;
    }
    if (f_write(fp, "\r\n", 2u, &bw) != FR_OK) {
        return 0u;
    }
    return 1u;
}

uint8_t bsp_sdcard_init(void)
{
    s_ready = 0u;

    if (f_mount(&s_fs, "", 1) != FR_OK) {
        return 0u;
    }

    /* 状态日志：不存在则创建并写表头 */
    if (f_open(&s_status, APP_STATUS_CSV, FA_OPEN_APPEND | FA_WRITE | FA_READ) != FR_OK) {
        return 0u;
    }
    if (f_size(&s_status) == 0u) {
        append_line(&s_status,
            "datetime,uptime_ms,in,out,occ,q50_ms,q100_ms,grade,flags");
    }

    /* 事件日志 */
    if (f_open(&s_events, APP_EVENTS_CSV, FA_OPEN_APPEND | FA_WRITE | FA_READ) != FR_OK) {
        f_close(&s_status);
        return 0u;
    }
    if (f_size(&s_events) == 0u) {
        append_line(&s_events, "datetime,uptime_ms,beam,level");
    }

    s_ready = 1u;
    return 1u;
}

uint8_t bsp_sdcard_append_status(const char *line)
{
    if (!s_ready) { return 0u; }
    return append_line(&s_status, line);
}

uint8_t bsp_sdcard_append_event(const char *line)
{
    if (!s_ready) { return 0u; }
    return append_line(&s_events, line);
}

void bsp_sdcard_sync(void)
{
    if (!s_ready) { return; }
    f_sync(&s_status);
    f_sync(&s_events);
}

#else  /* APP_SDCARD_ENABLE == 0：空实现，不写卡但其余功能照常 */

uint8_t bsp_sdcard_init(void) { return 0u; }

uint8_t bsp_sdcard_append_status(const char *line) { (void)line; return 0u; }

uint8_t bsp_sdcard_append_event(const char *line) { (void)line; return 0u; }

void bsp_sdcard_sync(void) { }

#endif /* APP_SDCARD_ENABLE */
