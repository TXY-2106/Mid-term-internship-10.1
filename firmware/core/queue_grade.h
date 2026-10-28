/**
 * @file  queue_grade.h
 * @brief 队列三级分级：持续遮挡判定 + 滞回 + 交叉校验（纯逻辑）
 *
 * 判定规则（README「队列分级规则」）：
 *   较少 : 50% 探头未被持续遮挡
 *   约半 : 50% 探头持续遮挡 >= grade_sustain_ms，且 100% 探头未遮挡
 *   全满 : 100% 探头持续遮挡 >= grade_sustain_ms
 *
 * 滞回  ：升级立即生效；降级需"当前等级的归属探头"持续通光 >= grade_release_ms。
 * 交叉校验：判为"全满"但在场人数 < occ_min_for_full 时降一档，并置 CROSS_CONFLICT。
 *
 * 注意：降级必须逐级判断（FULL → HALF → FEW），不能用一个全局计时器，
 *       否则会出现跳过中间档的抖动。
 */
#ifndef QSM_QUEUE_GRADE_H
#define QSM_QUEUE_GRADE_H

#include "qsm_types.h"

typedef struct {
    const qsm_config_t *cfg;

    /* index 0 = 50% 探头, index 1 = 100% 探头 */
    uint32_t occ_since[2];    /* 本次连续遮挡起点，0 表示当前通光   */
    uint32_t clear_since[2];  /* 本次连续通光起点，0 表示当前遮挡   */
    uint32_t occ_ms[2];       /* 由 tick 计算出的连续遮挡时长       */

    uint8_t  grade;           /* qsm_grade_t                        */
    uint8_t  conflict;        /* 1 = 本次交叉校验触发降档           */
} queue_grade_t;

void grade_init(queue_grade_t *q, const qsm_config_t *cfg);

/** 喂入 Q50 / Q100 的稳定边沿。 */
void grade_on_edge(queue_grade_t *q, const qsm_edge_t *e);

/** 周期性调用（建议 10 ms）：更新遮挡时长、判级、滞回、交叉校验。 */
void grade_tick(queue_grade_t *q, uint32_t now_ms, int16_t occupancy);

uint32_t grade_probe_occ_ms(const queue_grade_t *q, int idx);

#endif /* QSM_QUEUE_GRADE_H */
