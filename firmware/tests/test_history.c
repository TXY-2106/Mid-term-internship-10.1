/** @file test_history.c —— qsm_history 的单元测试 */
#include "test_util.h"
#include "qsm_history.h"
#include <string.h>

static void test_day_index(void)
{
    /* 2000-01-01 是基准日 0 */
    CHECK_EQ(qsm_hist_day_index(2000, 1, 1), 0);
    CHECK_EQ(qsm_hist_day_index(2000, 1, 2), 1);
    CHECK_EQ(qsm_hist_day_index(2000, 3, 1), 60);       /* 31 + 29（闰年） */
    CHECK_EQ(qsm_hist_day_index(2026, 11, 9), 9809);

    /* 非法日期 */
    CHECK_EQ(qsm_hist_day_index(1999, 12, 31), -1);     /* 早于 2000 */
    CHECK_EQ(qsm_hist_day_index(2026, 13, 1), -1);
    CHECK_EQ(qsm_hist_day_index(2026, 0, 1), -1);
    CHECK_EQ(qsm_hist_day_index(2026, 2, 30), -1);      /* 平年 2 月 */
    CHECK_EQ(qsm_hist_day_index(2024, 2, 29), 8825);    /* 闰年 2 月 29 合法 */
    CHECK_EQ(qsm_hist_day_index(2026, 4, 31), -1);
    CHECK_EQ(qsm_hist_day_index(2026, 1, 0), -1);
}

static void test_parse_iso(void)
{
    int32_t  day;
    uint16_t minute;

    CHECK_EQ(qsm_hist_parse_iso("2026-11-09T14:30:00", &day, &minute), 0);
    CHECK_EQ(day, 9809);
    CHECK_EQ(minute, 14u * 60u + 30u);

    /* 允许用空格代替 T（有些 RTC 输出格式如此） */
    CHECK_EQ(qsm_hist_parse_iso("2026-11-09 00:05:59", &day, &minute), 0);
    CHECK_EQ(minute, 5);

    CHECK_EQ(qsm_hist_parse_iso("2026-11-09T23:59:59", &day, &minute), 0);
    CHECK_EQ(minute, 23u * 60u + 59u);

    /* 非法输入 */
    CHECK_EQ(qsm_hist_parse_iso("2026-11-09", &day, &minute), -1);
    CHECK_EQ(qsm_hist_parse_iso("2026-13-09T00:00:00", &day, &minute), -1);
    CHECK_EQ(qsm_hist_parse_iso("2026-11-09X00:00:00", &day, &minute), -1);
    CHECK_EQ(qsm_hist_parse_iso("2026-11-09T24:00:00", &day, &minute), -1);
    CHECK_EQ(qsm_hist_parse_iso("2026-11-09Tab:00:00", &day, &minute), -1);
    CHECK_EQ(qsm_hist_parse_iso(NULL, &day, &minute), -1);
}

static void test_record_predict(void)
{
    qsm_history_t h;
    uint8_t mean, n;

    qsm_hist_init(&h);

    /* 没有历史 → 预测 0、样本 0 */
    CHECK_EQ(qsm_hist_predict(&h, 100, 870, &mean, &n), 0);
    CHECK_EQ(n, 0);
    CHECK_EQ(mean, 0);

    /* 昨天同一时段有 10 人 → 今天预测 10 */
    CHECK_EQ(qsm_hist_record(&h, 99, 870, 10), 0);
    CHECK_EQ(qsm_hist_predict(&h, 100, 870, &mean, &n), 0);
    CHECK_EQ(n, 1);
    CHECK_EQ(mean, 10);

    /* 前天 20 人 → 平均 15，两天都参与 */
    CHECK_EQ(qsm_hist_record(&h, 98, 870, 20), 0);
    CHECK_EQ(qsm_hist_predict(&h, 100, 870, &mean, &n), 0);
    CHECK_EQ(n, 2);
    CHECK_EQ(mean, 15);

    /* 今天自己的数据不参与（否则平均会被自己拉偏） */
    CHECK_EQ(qsm_hist_record(&h, 100, 870, 99), 0);
    CHECK_EQ(qsm_hist_predict(&h, 100, 870, &mean, &n), 0);
    CHECK_EQ(n, 2);
    CHECK_EQ(mean, 15);

    /* 同一时段不同分钟会落到不同的 15 min 槽 */
    CHECK_EQ(qsm_hist_record(&h, 97, 900, 40), 0);      /* 15:00 → 槽 60 */
    CHECK_EQ(qsm_hist_predict(&h, 100, 900, &mean, &n), 0);
    CHECK_EQ(n, 1);
    CHECK_EQ(mean, 40);
}

static void test_window_and_rounding(void)
{
    qsm_history_t h;
    uint8_t mean, n;

    qsm_hist_init(&h);
    CHECK_EQ(qsm_hist_record(&h, 100, 0, 10), 0);

    /* 7 天前仍参与 */
    CHECK_EQ(qsm_hist_predict(&h, 107, 0, &mean, &n), 0);
    CHECK_EQ(n, 1);
    CHECK_EQ(mean, 10);

    /* 8 天前已超出窗口 */
    CHECK_EQ(qsm_hist_predict(&h, 108, 0, &mean, &n), 0);
    CHECK_EQ(n, 0);
    CHECK_EQ(mean, 0);

    /* 四舍五入：(10 + 11 + 1) / 2 = 11，不是 11.5 截断成 11 之前先看 (22+1)/2 */
    CHECK_EQ(qsm_hist_record(&h, 106, 0, 11), 0);
    CHECK_EQ(qsm_hist_predict(&h, 107, 0, &mean, &n), 0);
    CHECK_EQ(n, 2);
    CHECK_EQ(mean, 11);                                 /* (21 + 1) / 2 = 11 */

    /* 单一槽位被 8 天后的记录覆盖后，旧数据不再参与 */
    qsm_hist_init(&h);
    CHECK_EQ(qsm_hist_record(&h, 100, 0, 50), 0);       /* 行 100%7 = 2 */
    CHECK_EQ(qsm_hist_record(&h, 100, 15, 60), 0);      /* 另一槽 */
    CHECK_EQ(qsm_hist_record(&h, 107, 0, 7), 0);        /* 同一行，整行清空后重写 */
    CHECK_EQ(qsm_hist_predict(&h, 108, 0, &mean, &n), 0);
    CHECK_EQ(n, 1);
    CHECK_EQ(mean, 7);
    CHECK_EQ(qsm_hist_predict(&h, 108, 15, &mean, &n), 0);
    CHECK_EQ(n, 0);                                     /* 60 人的旧槽已被清掉 */
}

static void test_limits(void)
{
    qsm_history_t h;
    uint8_t mean, n;

    qsm_hist_init(&h);
    CHECK_EQ(qsm_hist_record(&h, 100, 1440, 1), -1);    /* 一天只有 1440 分 */
    CHECK_EQ(qsm_hist_record(NULL, 100, 0, 1), -1);
    CHECK_EQ(qsm_hist_predict(NULL, 100, 0, &mean, &n), -1);
    CHECK_EQ(qsm_hist_predict(&h, 100, 0, NULL, &n), -1);

    /* 人数超过 254 时按 254 记录，255 保留给"无数据" */
    CHECK_EQ(qsm_hist_record(&h, 100, 0, 300), 0);
    CHECK_EQ(qsm_hist_predict(&h, 101, 0, &mean, &n), 0);
    CHECK_EQ(n, 1);
    CHECK_EQ(mean, 254);
}

static void test_civil_roundtrip(void)
{
    uint16_t y;
    uint8_t  m, d;
    int32_t  i;

    /* 逆运算：基准日与已知日期 */
    CHECK_EQ(qsm_hist_civil_from_day_index(0, &y, &m, &d), 0);
    CHECK_EQ(y, 2000); CHECK_EQ(m, 1); CHECK_EQ(d, 1);

    CHECK_EQ(qsm_hist_civil_from_day_index(9809, &y, &m, &d), 0);
    CHECK_EQ(y, 2026); CHECK_EQ(m, 11); CHECK_EQ(d, 9);

    CHECK_EQ(qsm_hist_civil_from_day_index(60, &y, &m, &d), 0);
    CHECK_EQ(y, 2000); CHECK_EQ(m, 3); CHECK_EQ(d, 1);

    /* 往返一致：2000-01-01 起 60 年，每 11 天抽一个 */
    for (i = 0; i < 22000; i += 97) {
        CHECK_EQ(qsm_hist_civil_from_day_index(i, &y, &m, &d), 0);
        CHECK_EQ(qsm_hist_day_index(y, m, d), i);
    }

    /* 非法参数 */
    CHECK_EQ(qsm_hist_civil_from_day_index(0, NULL, &m, &d), -1);
    CHECK_EQ(qsm_hist_civil_from_day_index(200000, &y, &m, &d), -1);  /* 超过 2199 年 */
}

static void test_file_name(void)
{
    char buf[16];

    CHECK_EQ(qsm_hist_file_name(9809, buf, (uint8_t)sizeof(buf)), 1);
    CHECK(strcmp(buf, "H261109.CSV") == 0);

    CHECK_EQ(qsm_hist_file_name(0, buf, (uint8_t)sizeof(buf)), 1);
    CHECK(strcmp(buf, "H000101.CSV") == 0);

    /* 闰日 */
    CHECK_EQ(qsm_hist_file_name(8825, buf, (uint8_t)sizeof(buf)), 1);   /* 2024-02-29 */
    CHECK(strcmp(buf, "H240229.CSV") == 0);

    /* 缓冲不够 / 非法序号 */
    CHECK_EQ(qsm_hist_file_name(9809, buf, 11u), 0);
    CHECK_EQ(qsm_hist_file_name(9809, NULL, (uint8_t)sizeof(buf)), 0);
}

static void test_line_format_parse(void)
{
    char     buf[16];
    uint16_t minute, occ;

    CHECK_EQ(qsm_hist_line_fmt(750, 12, buf, (uint8_t)sizeof(buf)), 8);
    CHECK(strcmp(buf, "12:30,12") == 0);

    CHECK_EQ(qsm_hist_line_fmt(0, 0, buf, (uint8_t)sizeof(buf)), 7);
    CHECK(strcmp(buf, "00:00,0") == 0);

    CHECK_EQ(qsm_hist_line_fmt(1439, 254, buf, (uint8_t)sizeof(buf)), 9);
    CHECK(strcmp(buf, "23:59,254") == 0);

    /* 越界参数 */
    CHECK_EQ(qsm_hist_line_fmt(1440, 0, buf, (uint8_t)sizeof(buf)), 0);
    CHECK_EQ(qsm_hist_line_fmt(0, 255, buf, (uint8_t)sizeof(buf)), 0);
    CHECK_EQ(qsm_hist_line_fmt(0, 0, buf, 9u), 0);

    /* 格式化和解析互为逆运算 */
    CHECK_EQ(qsm_hist_line_parse("12:30,12", &minute, &occ), 0);
    CHECK_EQ(minute, 750); CHECK_EQ(occ, 12);

    CHECK_EQ(qsm_hist_line_parse("09:00,3\r\n", &minute, &occ), 0);
    CHECK_EQ(minute, 540); CHECK_EQ(occ, 3);

    CHECK_EQ(qsm_hist_line_parse("23:59,254\n", &minute, &occ), 0);
    CHECK_EQ(minute, 1439); CHECK_EQ(occ, 254);

    /* 表头行必须被拒绝，免得被当成数据 */
    CHECK_EQ(qsm_hist_line_parse("HH:MM,occ\r\n", &minute, &occ), -1);
    CHECK_EQ(qsm_hist_line_parse("", &minute, &occ), -1);
    CHECK_EQ(qsm_hist_line_parse("24:00,1", &minute, &occ), -1);
    CHECK_EQ(qsm_hist_line_parse("09:60,1", &minute, &occ), -1);
    CHECK_EQ(qsm_hist_line_parse("09:00,", &minute, &occ), -1);
    CHECK_EQ(qsm_hist_line_parse("09:00,1x", &minute, &occ), -1);
    CHECK_EQ(qsm_hist_line_parse("09:00,9999", &minute, &occ), -1);
    CHECK_EQ(qsm_hist_line_parse(NULL, &minute, &occ), -1);
}

void suite_history(void)
{
    test_day_index();
    test_parse_iso();
    test_record_predict();
    test_window_and_rounding();
    test_limits();
    test_civil_roundtrip();
    test_file_name();
    test_line_format_parse();
}
