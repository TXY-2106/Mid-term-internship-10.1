/**
 * @file  hist_store.h
 * @brief 把"每天每时段的排队人数"落 SD 卡 / 上电时回读（配合 qsm_history）
 *
 * 文件组织（8.3 短名，一天一个文件）：
 *   H261109.CSV   2026-11-09 的在场人数
 *       HH:MM,occ        ← 表头
 *       09:00,3
 *       09:01,3
 *       09:02,4
 *       …
 *   H261108.CSV   前一天
 *   …
 *
 * 设计取舍：
 *   - 每分钟记一行（1440 行/天，约 15 KB/天），保留原始分辨率；
 *     需要"每小时平均"时用 Excel 透视即可，不必在单片机上算。
 *   - 上电时只回读最近 7 个文件（预测窗口），不扫全卡。
 *   - 预测用的 15 分钟时段表仍放在 RAM（672 B），SD 卡只是它的持久化，
 *     这样掉电重启后预测立即可用，不必等 7 天重新积累。
 */
#ifndef HIST_STORE_H
#define HIST_STORE_H

#include <stdint.h>
#include "qsm_history.h"

/**
 * 挂载后调用：把 today-7 … today 的历史文件读进 RAM 表。
 * @param h      历史表（通常就是 app_main 里那份）
 * @param today  今天的天数序号（qsm_hist_day_index）
 * @return 读回的文件数（0 = 没找到任何历史文件，属于首次使用）
 */
uint8_t hist_store_begin(qsm_history_t *h, int32_t today);

/**
 * 追加一行"某天某时刻的在场人数"，自动按天切换文件并 f_sync。
 * @return 1 成功，0 失败（卡未挂载 / 写失败）
 */
uint8_t hist_store_append(int32_t day_index, uint16_t minute_of_day, uint16_t occ);

/** 把当前文件刷进卡（掉电最多丢一条记录）。 */
void hist_store_sync(void);

/** SD 卡上的历史文件是否可用 */
uint8_t hist_store_ok(void);

/**
 * 删除最近 8 天（today-7 … today）的 H*.CSV。
 * 供网页"清空历史"按钮使用；只删本程序自己的文件，不动卡上其它内容。
 * @return 实际删除的文件数
 */
uint8_t hist_store_clear(int32_t today);

#endif /* HIST_STORE_H */
