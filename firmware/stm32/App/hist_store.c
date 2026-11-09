/**
 * @file  hist_store.c
 * @brief 历史人数文件的读写（FatFs）
 */
#include "hist_store.h"
#include "app_config.h"

#if APP_HIST_PERSIST

#include "ff.h"
#include "data_logger.h"   /* logger_ok()：SD 卡是否已挂载成功 */

/* 文件名固定 11 字符 + '\0' */
#define HIST_NAME_LEN 12u
#define HIST_LINE_LEN 20u

static FIL     s_fil;
static int32_t s_open_day = -1;   /* -1 = 当前没有打开的文件 */
static uint8_t s_ok;

/* 极简 strlen */
static UINT len_of(const char *s)
{
    UINT n = 0u;
    while (s[n] != '\0') { n++; }
    return n;
}

static uint8_t write_all(FIL *fp, const char *s)
{
    UINT bw = 0u;
    UINT n  = len_of(s);

    if (f_write(fp, s, n, &bw) != FR_OK || bw != n) {
        return 0u;
    }
    return 1u;
}

uint8_t hist_store_ok(void)
{
    return s_ok;
}

uint8_t hist_store_begin(qsm_history_t *h, int32_t today)
{
    char    name[HIST_NAME_LEN];
    char    line[HIST_LINE_LEN];
    uint8_t found = 0u;
    uint8_t k;

    s_ok      = 0u;
    s_open_day = -1;

    if (h == NULL || !logger_ok()) {
        return 0u;
    }

    /* 从 8 天前读到昨天；今天的文件由 append 时创建/追加 */
    for (k = 8u; k >= 1u; k--) {
        int32_t day = today - (int32_t)k;

        if (!qsm_hist_file_name(day, name, (uint8_t)sizeof(name))) {
            continue;
        }
        if (f_open(&s_fil, name, FA_READ) != FR_OK) {
            continue;                       /* 那天没有数据（首次使用/没开机） */
        }
        found++;

        while (f_gets(line, (int)sizeof(line), &s_fil) != 0) {
            uint16_t minute, occ;

            if (qsm_hist_line_parse(line, &minute, &occ) == 0) {
                (void)qsm_hist_record(h, day, minute, occ);
            }
            /* 表头 "HH:MM,occ" 或空行：parse 返回 -1，直接跳过 */
        }
        f_close(&s_fil);
    }

    s_ok = 1u;
    return found;
}

uint8_t hist_store_append(int32_t day_index, uint16_t minute_of_day, uint16_t occ)
{
    char    name[HIST_NAME_LEN];
    char    line[HIST_LINE_LEN];
    uint8_t n;

    if (!s_ok || !logger_ok()) {
        return 0u;
    }

    /* 跨天：换文件 */
    if (day_index != s_open_day) {
        if (s_open_day >= 0) {
            f_close(&s_fil);
            s_open_day = -1;
        }
        if (!qsm_hist_file_name(day_index, name, (uint8_t)sizeof(name))) {
            return 0u;
        }
        if (f_open(&s_fil, name, FA_OPEN_APPEND | FA_WRITE | FA_READ) != FR_OK) {
            s_ok = 0u;                      /* 卡被拔掉等：本周期停写，不再重试 */
            return 0u;
        }
        if (f_size(&s_fil) == 0u) {
            (void)write_all(&s_fil, "HH:MM,occ\r\n");
        }
        s_open_day = day_index;
    }

    n = qsm_hist_line_fmt(minute_of_day, occ, line, (uint8_t)sizeof(line));
    if (n == 0u) {
        return 0u;
    }
    line[n++] = '\r';
    line[n++] = '\n';
    line[n]   = '\0';

    if (!write_all(&s_fil, line)) {
        return 0u;
    }
    (void)f_sync(&s_fil);                   /* 每分钟一条，立刻落盘最稳 */
    return 1u;
}

void hist_store_sync(void)
{
    if (s_ok && s_open_day >= 0) {
        (void)f_sync(&s_fil);
    }
}

uint8_t hist_store_clear(int32_t today)
{
    char    name[HIST_NAME_LEN];
    uint8_t k;
    uint8_t deleted = 0u;

    if (!logger_ok()) {
        return 0u;
    }

    /* 正在写的文件必须先关掉，否则 f_unlink 会因文件被占用而失败 */
    if (s_open_day >= 0) {
        f_close(&s_fil);
        s_open_day = -1;
    }
    s_ok = 1u;                  /* 用户主动清空：允许下次 append 重新建文件 */

    for (k = 0u; k < 8u; k++) {
        int32_t day = today - (int32_t)k;

        if (!qsm_hist_file_name(day, name, (uint8_t)sizeof(name))) {
            continue;
        }
        if (f_unlink(name) == FR_OK) {
            deleted++;
        }
    }
    return deleted;
}

#else  /* APP_HIST_PERSIST == 0：不落盘 */

uint8_t hist_store_ok(void) { return 0u; }
uint8_t hist_store_begin(qsm_history_t *h, int32_t today) { (void)h; (void)today; return 0u; }
uint8_t hist_store_append(int32_t day_index, uint16_t minute_of_day, uint16_t occ)
{
    (void)day_index; (void)minute_of_day; (void)occ;
    return 0u;
}
void hist_store_sync(void) { }
uint8_t hist_store_clear(int32_t today) { (void)today; return 0u; }

#endif /* APP_HIST_PERSIST */
