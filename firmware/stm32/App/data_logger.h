/**
 * @file  data_logger.h
 * @brief 组装 CSV 行并写入 SD 卡（两个文件：状态 1 Hz，事件边沿）
 *
 * 与 README「本地保存原始计数数据，实验结束后导出」对应；
 * 事件日志是事后精算与 A7 证据的主要来源。
 */
#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include "qsm_types.h"

/** 挂载 SD 卡。返回 1 表示写日志可用。 */
uint8_t logger_init(void);

/** 写一行状态记录（1 Hz）。 */
void logger_status(const qsm_state_t *s, const char *dt);

/** 写一行边沿事件记录。 */
void logger_event(const qsm_edge_t *e, const char *dt);

/** 落盘。 */
void logger_sync(void);

/** SD 是否可用（用于 flags 位）。 */
uint8_t logger_ok(void);

#endif /* DATA_LOGGER_H */
