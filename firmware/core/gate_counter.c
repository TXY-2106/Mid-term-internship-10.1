/**
 * @file  gate_counter.c
 * @brief 进出方向判别状态机实现
 */
#include "gate_counter.h"

#define GATE_IDLE        0u
#define GATE_WAIT_SECOND 1u

void gate_init(gate_counter_t *g, const qsm_config_t *cfg, uint8_t invert)
{
    g->cfg        = cfg;
    g->state      = GATE_IDLE;
    g->first_beam = 0xFFu;
    g->t_first    = 0u;
    g->count_in   = 0u;
    g->count_out  = 0u;
    g->invert     = invert ? 1u : 0u;
}

static void emit_direction(gate_counter_t *g, uint8_t first_beam)
{
    /* 约定：IN_A 先遮挡 → 进入；IN_B 先遮挡 → 离开。
       现场若装反，把 invert 置 1 即可，无需改代码。 */
    uint8_t went_in = (first_beam == QSM_BEAM_IN_A) ? 1u : 0u;
    if (g->invert) {
        went_in = (uint8_t)!went_in;
    }

    if (went_in) {
        if (g->count_in < UINT16_MAX) { g->count_in++; }
    } else {
        if (g->count_out < UINT16_MAX) { g->count_out++; }
    }
}

void gate_on_edge(gate_counter_t *g, const qsm_edge_t *e)
{
    /* 只关心"开始遮挡"沿；恢复通光的沿不推进方向判定。 */
    if (e->level == 0u) {
        return;
    }
    if (e->beam != QSM_BEAM_IN_A && e->beam != QSM_BEAM_IN_B) {
        return;
    }

    if (g->state == GATE_IDLE) {
        g->state      = GATE_WAIT_SECOND;
        g->first_beam = e->beam;
        g->t_first    = e->t_ms;
        return;
    }

    /* WAIT_SECOND */
    if (e->beam == g->first_beam) {
        /* 同一束再次触发：是抖动或有人停在光束里，刷新起点即可 */
        g->t_first = e->t_ms;
        return;
    }

    /* 另一束在窗口内到达 → 判定方向 */
    if ((e->t_ms - g->t_first) <= g->cfg->gate_coincide_ms) {
        emit_direction(g, g->first_beam);
    }
    /* 超窗则整次作废，不计数 */
    g->state      = GATE_IDLE;
    g->first_beam = 0xFFu;
}

void gate_tick(gate_counter_t *g, uint32_t now_ms)
{
    if (g->state != GATE_WAIT_SECOND) {
        return;
    }
    /* 超时仍未见第二束 → 丢弃，避免一支探头长期遮挡锁死状态机 */
    if ((now_ms - g->t_first) > g->cfg->gate_timeout_ms) {
        g->state      = GATE_IDLE;
        g->first_beam = 0xFFu;
    }
}

int16_t gate_occupancy(const gate_counter_t *g)
{
    int32_t d = (int32_t)g->count_in - (int32_t)g->count_out;
    if (d < 0) {
        d = 0;   /* 有人未经过出口闸即离开（如从侧面包围栏）——夹紧并交由上层记录 */
    }
    if (d > INT16_MAX) {
        d = INT16_MAX;
    }
    return (int16_t)d;
}
