/**
 * @file  queue_grade.c
 * @brief 队列分级实现（持续遮挡 + 滞回 + 交叉校验）
 */
#include "queue_grade.h"

static uint8_t beam_to_index(uint8_t beam)
{
    if (beam == QSM_BEAM_Q50)  { return 0u; }
    if (beam == QSM_BEAM_Q100) { return 1u; }
    return 0xFFu;
}

void grade_init(queue_grade_t *q, const qsm_config_t *cfg)
{
    q->cfg      = cfg;
    q->grade    = QSM_GRADE_FEW;
    q->conflict = 0u;
    for (uint8_t i = 0u; i < 2u; i++) {
        q->occ_since[i]   = 0u;
        q->clear_since[i] = 0u;
        q->occ_ms[i]      = 0u;
    }
}

void grade_on_edge(queue_grade_t *q, const qsm_edge_t *e)
{
    uint8_t i = beam_to_index(e->beam);
    if (i == 0xFFu) {
        return;
    }

    if (e->level != 0u) {
        /* 开始遮挡：若之前是通光，记起点并清掉通光计时 */
        if (q->occ_since[i] == 0u) {
            q->occ_since[i] = e->t_ms;
        }
        q->clear_since[i] = 0u;
    } else {
        /* 恢复通光：清掉遮挡起点，记录通光起点 */
        q->occ_since[i] = 0u;
        if (q->clear_since[i] == 0u) {
            q->clear_since[i] = e->t_ms;
        }
    }
}

uint32_t grade_probe_occ_ms(const queue_grade_t *q, int idx)
{
    if (idx < 0 || idx > 1) {
        return 0u;
    }
    return q->occ_ms[idx];
}

/** 该探头是否已稳定通光（可用于降级） */
static uint8_t probe_stably_clear(const queue_grade_t *q, uint8_t i, uint32_t now_ms)
{
    if (q->occ_since[i] != 0u) {
        return 0u;   /* 当前仍遮挡 */
    }
    if (q->clear_since[i] == 0u) {
        return 1u;   /* 上电以来一直通光，视为稳定通光 */
    }
    return ((now_ms - q->clear_since[i]) >= q->cfg->grade_release_ms) ? 1u : 0u;
}

void grade_tick(queue_grade_t *q, uint32_t now_ms, int16_t occupancy)
{
    /* 1) 更新连续遮挡时长 */
    for (uint8_t i = 0u; i < 2u; i++) {
        if (q->occ_since[i] != 0u) {
            q->occ_ms[i] = now_ms - q->occ_since[i];
        } else {
            q->occ_ms[i] = 0u;
        }
    }

    /* 2) 由"持续遮挡"得出目标等级 */
    uint8_t cand = QSM_GRADE_FEW;
    if (q->occ_ms[1] >= q->cfg->grade_sustain_ms) {
        cand = QSM_GRADE_FULL;
    } else if (q->occ_ms[0] >= q->cfg->grade_sustain_ms) {
        cand = QSM_GRADE_HALF;
    }

    /* 3) 滞回：升级立即，降级需归属探头稳定通光 */
    q->conflict = 0u;
    if (cand > q->grade) {
        q->grade = cand;
    } else if (cand < q->grade) {
        uint8_t owner = (q->grade == QSM_GRADE_FULL) ? 1u : 0u;
        if (probe_stably_clear(q, owner, now_ms)) {
            q->grade = cand;
        }
    }

    /* 4) 交叉校验：没有足够的人在等，就不该判"全满" */
    if (q->grade == QSM_GRADE_FULL && occupancy < q->cfg->occ_min_for_full) {
        q->grade    = (q->occ_ms[0] >= q->cfg->grade_sustain_ms) ? QSM_GRADE_HALF
                                                                : QSM_GRADE_FEW;
        q->conflict = 1u;
    }
}
