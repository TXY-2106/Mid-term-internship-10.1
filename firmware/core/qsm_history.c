/**
 * @file    qsm_history.c
 * @brief   历史排队人数统计与预测的实现（纯逻辑，无硬件依赖）
 */
#include "qsm_history.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* 日历换算（Howard Hinnant 的 days_from_civil，只用整数）              */
/* ------------------------------------------------------------------ */
static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t dim[12] = { 31u, 28u, 31u, 30u, 31u, 30u,
                                     31u, 31u, 30u, 31u, 30u, 31u };
    uint8_t d = dim[month - 1u];

    if (month == 2u) {
        uint8_t leap = (uint8_t)(((year % 4u) == 0u && (year % 100u) != 0u) ||
                                 ((year % 400u) == 0u));
        if (leap) {
            d = 29u;
        }
    }
    return d;
}

int32_t qsm_hist_day_index(uint16_t year, uint8_t month, uint8_t day)
{
    if (year < 2000u || year > 2199u || month < 1u || month > 12u) {
        return -1;
    }
    if (day < 1u || day > days_in_month(year, month)) {
        return -1;
    }

    int32_t y = (int32_t)year;
    uint32_t m = (uint32_t)month;
    y -= (m <= 2u) ? 1 : 0;

    int32_t era = (y >= 0 ? y : y - 399) / 400;
    uint32_t yoe = (uint32_t)(y - era * 400);
    int32_t mp = (m > 2u) ? (int32_t)m - 3 : (int32_t)m + 9;
    uint32_t doy = (uint32_t)((153 * mp + 2) / 5) + (uint32_t)day - 1u;
    uint32_t doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    int32_t days = era * 146097 + (int32_t)doe - 719468;

    /* 1970-01-01 起算 → 改用 2000-01-01 起算，序号小、好读 */
    return days - 10957;
}

/* ------------------------------------------------------------------ */
/* 记录与预测                                                          */
/* ------------------------------------------------------------------ */
void qsm_hist_init(qsm_history_t *h)
{
    if (h == NULL) {
        return;
    }
    for (uint8_t r = 0u; r < QSM_HIST_DAYS; r++) {
        memset(h->v[r], QSM_HIST_NONE, QSM_HIST_SLOTS);
        h->day[r] = -1;
    }
}

int qsm_hist_record(qsm_history_t *h,
                    int32_t day_index,
                    uint16_t minute_of_day,
                    uint16_t occ)
{
    if (h == NULL || minute_of_day >= (uint16_t)(24u * 60u)) {
        return -1;
    }

    uint8_t  slot = (uint8_t)(minute_of_day / QSM_HIST_SLOT_MIN);
    int32_t  row  = day_index % QSM_HIST_DAYS;
    if (row < 0) {
        row += QSM_HIST_DAYS;
    }

    /* 该行还是别的日期的数据（≥7 天前）→ 整行清空，避免旧值混进平均 */
    if (h->day[row] != day_index) {
        memset(h->v[row], QSM_HIST_NONE, QSM_HIST_SLOTS);
        h->day[row] = day_index;
    }

    /* 254 是能记录的上限：255 被保留作 QSM_HIST_NONE（无数据）哨兵 */
    h->v[row][slot] = (occ > 254u) ? 254u : (uint8_t)occ;
    return 0;
}

int qsm_hist_predict(const qsm_history_t *h,
                     int32_t day_index,
                     uint16_t minute_of_day,
                     uint8_t *mean,
                     uint8_t *samples)
{
    if (h == NULL || mean == NULL || samples == NULL ||
        minute_of_day >= (uint16_t)(24u * 60u)) {
        return -1;
    }

    uint8_t  slot = (uint8_t)(minute_of_day / QSM_HIST_SLOT_MIN);
    uint32_t sum  = 0u;
    uint16_t n    = 0u;

    for (uint8_t r = 0u; r < QSM_HIST_DAYS; r++) {
        int32_t age;

        if (h->day[r] < 0) {
            continue;
        }
        age = day_index - h->day[r];
        if (age < 1 || age > QSM_HIST_DAYS) {
            continue;               /* 今天自己、或已超出 7 天窗口 */
        }
        if (h->v[r][slot] == QSM_HIST_NONE) {
            continue;               /* 那天这个时段没记录 */
        }
        sum += h->v[r][slot];
        n++;
    }

    *samples = (uint8_t)n;
    /* 四舍五入，避免长期偏低 */
    *mean = (n > 0u) ? (uint8_t)((sum + (uint32_t)n / 2u) / (uint32_t)n) : 0u;
    return 0;
}

/* ------------------------------------------------------------------ */
/* 时间字符串                                                          */
/* ------------------------------------------------------------------ */
static int parse_fixed_uint(const char *p, uint8_t n, uint16_t *out)
{
    uint16_t v = 0u;

    for (uint8_t i = 0u; i < n; i++) {
        if (p[i] < '0' || p[i] > '9') {
            return -1;
        }
        v = (uint16_t)(v * 10u + (uint16_t)(p[i] - '0'));
    }
    *out = v;
    return 0;
}

int qsm_hist_parse_iso(const char *iso,
                       int32_t *day_index,
                       uint16_t *minute_of_day)
{
    uint16_t y, mo, d, hh, mm, ss;

    if (iso == NULL || day_index == NULL || minute_of_day == NULL) {
        return -1;
    }
    /* "YYYY-MM-DDTHH:MM:SS"，长度至少 19 */
    if (iso[4] != '-' || iso[7] != '-' || iso[13] != ':' || iso[16] != ':') {
        return -1;
    }
    if (iso[10] != 'T' && iso[10] != ' ') {
        return -1;
    }
    if (parse_fixed_uint(&iso[0], 4u, &y)  != 0) { return -1; }
    if (parse_fixed_uint(&iso[5], 2u, &mo) != 0) { return -1; }
    if (parse_fixed_uint(&iso[8], 2u, &d)  != 0) { return -1; }
    if (parse_fixed_uint(&iso[11], 2u, &hh) != 0) { return -1; }
    if (parse_fixed_uint(&iso[14], 2u, &mm) != 0) { return -1; }
    if (parse_fixed_uint(&iso[17], 2u, &ss) != 0) { return -1; }

    if (hh > 23u || mm > 59u || ss > 60u) {
        return -1;
    }

    int32_t di = qsm_hist_day_index(y, (uint8_t)mo, (uint8_t)d);
    if (di < 0) {
        return -1;
    }
    *day_index     = di;
    *minute_of_day = (uint16_t)((uint16_t)hh * 60u + mm);
    return 0;
}

/* ------------------------------------------------------------------ */
/* 逆运算与落盘用的字符串                                              */
/* ------------------------------------------------------------------ */
int32_t qsm_hist_civil_from_day_index(int32_t day_index,
                                      uint16_t *year,
                                      uint8_t *month,
                                      uint8_t *day)
{
    int32_t  z, era, y;
    uint32_t doe, yoe, doy, mp, d, m;

    if (year == NULL || month == NULL || day == NULL) {
        return -1;
    }
    /* 2000-01-01 起算 → 1970 起算 → 再加 719468 换成 Hinnant 的"公元 0 年 3 月 1 日"基准 */
    z = day_index + 10957 + 719468;

    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (uint32_t)(z - era * 146097);
    yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    y   = (int32_t)yoe + era * 400;

    doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    mp  = (5u * doy + 2u) / 153u;
    d   = doy - (153u * mp + 2u) / 5u + 1u;
    m   = (mp < 10u) ? (mp + 3u) : (mp - 9u);
    if (m <= 2u) {
        y++;
    }

    if (y < 2000 || y > 2199) {
        return -1;
    }
    *year  = (uint16_t)y;
    *month = (uint8_t)m;
    *day   = (uint8_t)d;
    return 0;
}

static char *put2d(char *p, uint8_t v)
{
    *p++ = (char)('0' + (v / 10u));
    *p++ = (char)('0' + (v % 10u));
    return p;
}

static char *put3d(char *p, uint16_t v)
{
    if (v >= 100u) { *p++ = (char)('0' + (v / 100u)); }
    if (v >= 10u)  { *p++ = (char)('0' + ((v / 10u) % 10u)); }
    *p++ = (char)('0' + (v % 10u));
    return p;
}

uint8_t qsm_hist_file_name(int32_t day_index, char *buf, uint8_t len)
{
    uint16_t y;
    uint8_t  m, d;
    char    *p;

    if (buf == NULL || len < 12u) {
        return 0u;
    }
    if (qsm_hist_civil_from_day_index(day_index, &y, &m, &d) != 0) {
        return 0u;
    }

    p = buf;
    *p++ = 'H';
    p = put2d(p, (uint8_t)(y % 100u));   /* 年只保留后两位：26 = 2026 */
    p = put2d(p, m);
    p = put2d(p, d);
    *p++ = '.';
    *p++ = 'C';
    *p++ = 'S';
    *p++ = 'V';
    *p   = '\0';
    return 1u;
}

uint8_t qsm_hist_line_fmt(uint16_t minute_of_day,
                          uint16_t occ,
                          char *buf,
                          uint8_t len)
{
    char    *p;

    if (buf == NULL || len < 10u || minute_of_day >= (uint16_t)(24u * 60u) || occ > 254u) {
        return 0u;
    }

    p = buf;
    p = put2d(p, (uint8_t)(minute_of_day / 60u));
    *p++ = ':';
    p = put2d(p, (uint8_t)(minute_of_day % 60u));
    *p++ = ',';
    p = put3d(p, occ);
    *p   = '\0';
    return (uint8_t)(p - buf);
}

int qsm_hist_line_parse(const char *line,
                        uint16_t *minute_of_day,
                        uint16_t *occ)
{
    uint16_t hh, mm, v;

    if (line == NULL || minute_of_day == NULL || occ == NULL) {
        return -1;
    }
    /* 最短 "HH:MM,0" 共 7 个字符 */
    if (line[0] < '0' || line[0] > '9' ||
        line[1] < '0' || line[1] > '9' ||
        line[2] != ':' ||
        line[3] < '0' || line[3] > '9' ||
        line[4] < '0' || line[4] > '9' ||
        line[5] != ',') {
        return -1;
    }
    if (parse_fixed_uint(&line[0], 2u, &hh) != 0 ||
        parse_fixed_uint(&line[3], 2u, &mm) != 0) {
        return -1;
    }
    if (hh > 23u || mm > 59u) {
        return -1;
    }

    v = 0u;
    uint8_t n = 0u;
    const char *p = &line[6];
    while (*p >= '0' && *p <= '9') {
        if (n >= 3u) {
            return -1;                       /* 超过 3 位数：不是本格式 */
        }
        v = (uint16_t)(v * 10u + (uint16_t)(*p - '0'));
        n++;
        p++;
    }
    if (n == 0u || v > 254u) {
        return -1;
    }
    /* 行尾只允许空白（'\\r' '\\n' ' '），其他字符视为损坏 */
    while (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t') {
        p++;
    }
    if (*p != '\0') {
        return -1;
    }

    *minute_of_day = (uint16_t)((uint16_t)hh * 60u + mm);
    *occ           = v;
    return 0;
}
