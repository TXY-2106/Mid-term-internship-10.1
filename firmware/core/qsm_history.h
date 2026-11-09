/**
 * @file    qsm_history.h
 * @brief   历史排队人数统计与预测（过去 7 天同一时段的平均在场人数）
 *
 * 做法：把一天按 15 分钟切成 96 个时段，每个时段每天只记一次"在场人数"；
 *      预测当前时段时，取过去 7 天同一时段的有效记录求平均，作为预计排队人数。
 *
 *   ┌ 00:00 ┬ 00:15 ┬ … ┬ 12:30 ┬ … ┬ 23:45 ┐   96 段
 *   │  2人  │  1人  │   │  8人  │   │  0人  │   今天
 *   │  3人  │  2人  │   │  9人  │   │  0人  │   昨天
 *   │  …    │  …    │   │  …    │   │  …    │   前 7 天
 *   └───────┴───────┴───┴───────┴───┴───────┘
 *                                ↑ 预测 = 该列历史均值
 *
 * 设计取舍：
 *   - 只存"每天一个值"而不是每次采样都存，7 天 × 96 段 = 672 字节，
 *     STM32F103C8T6 的 20 KB RAM 完全放得下，也不必开 SD 卡。
 *   - 不含"今天"自己的值：今天还没过完，用今天的数据算平均会有偏。
 *     因此设备需要连续记录至少 1 个完整自然日，才有预测值。
 *   - 纯逻辑，不依赖任何硬件与 libc 时间函数；时间由调用方（DS3231）给出，
 *     因此可以在 PC 上直接跑单元测试。
 */
#ifndef QSM_HISTORY_H
#define QSM_HISTORY_H

#include <stdint.h>

/** 参与平均的历史天数 */
#define QSM_HIST_DAYS        7
/** 一个时段多少分钟（改这里不需要动别的代码） */
#define QSM_HIST_SLOT_MIN    15
/** 一天切成的时段数 */
#define QSM_HIST_SLOTS       (24 * 60 / QSM_HIST_SLOT_MIN)
/** 单元格"无记录"标记（因此 255 人以上按 254 记录） */
#define QSM_HIST_NONE        0xFFu

typedef struct {
    uint8_t v[QSM_HIST_DAYS][QSM_HIST_SLOTS];  /* 每格 = 某天某时段的在场人数 */
    int32_t day[QSM_HIST_DAYS];                /* 该行对应第几天（-1 = 空行）  */
} qsm_history_t;

/** 清零：所有格标记为无记录。 */
void qsm_hist_init(qsm_history_t *h);

/**
 * 记录一次"某天某时段的在场人数"，同一格以最后一次为准。
 * @param day_index     天数序号（用 qsm_hist_day_index() 得到）
 * @param minute_of_day 当天第几分钟，0–1439
 * @param occ           在场人数，>254 时按 254 处理（255 留给"无记录"）
 * @return 0 成功；-1 参数非法（越界）
 */
int qsm_hist_record(qsm_history_t *h,
                    int32_t day_index,
                    uint16_t minute_of_day,
                    uint16_t occ);

/**
 * 预测某天某时段的排队人数。
 * @param mean     输出：历史平均人数；-1 参数非法
 * @param samples  输出：参与平均的天数，0 表示还没有历史（此时 mean = 0）
 * @return 0 成功；-1 参数非法
 */
int qsm_hist_predict(const qsm_history_t *h,
                     int32_t day_index,
                     uint16_t minute_of_day,
                     uint8_t *mean,
                     uint8_t *samples);

/**
 * 天数序号（以 2000-01-01 为 0）。同一字符串算出的序号之差即相隔天数。
 * @return >=0；-1 日期非法
 */
int32_t qsm_hist_day_index(uint16_t year, uint8_t month, uint8_t day);

/**
 * 解析 DS3231 给出的 "YYYY-MM-DDTHH:MM:SS"。
 * @return 0 成功；-1 格式或日期非法（例如 RTC 未就绪时的 "-"）
 */
int qsm_hist_parse_iso(const char *iso,
                       int32_t *day_index,
                       uint16_t *minute_of_day);

/**
 * 天数序号 → 年月日（qsm_hist_day_index 的逆运算）。
 * @return 0 成功；-1 参数非法（NULL 或序号超出 2000–2199 年）
 */
int32_t qsm_hist_civil_from_day_index(int32_t day_index,
                                      uint16_t *year,
                                      uint8_t *month,
                                      uint8_t *day);

/**
 * 生成当天历史文件名，8.3 短名，形如 "H261109.CSV"（H + 年月日各两位 + .CSV）。
 * @param buf  输出缓冲
 * @param len  缓冲长度，至少 12
 * @return 1 成功；0 参数非法或缓冲不够
 */
uint8_t qsm_hist_file_name(int32_t day_index, char *buf, uint8_t len);

/**
 * 生成一行历史记录，形如 "07:05,12"（HH:MM,在场人数），不带换行符。
 * @return 写入的字符数（不含结尾 '\0'）；0 表示参数非法或缓冲不够
 */
uint8_t qsm_hist_line_fmt(uint16_t minute_of_day,
                          uint16_t occ,
                          char *buf,
                          uint8_t len);

/**
 * 解析一行历史记录（容忍行尾 '\r' '\n' 与空格）。
 * @return 0 成功；-1 格式非法（如表头行 "HH:MM,occ" 会被拒绝）
 */
int qsm_hist_line_parse(const char *line,
                        uint16_t *minute_of_day,
                        uint16_t *occ);

#endif /* QSM_HISTORY_H */
