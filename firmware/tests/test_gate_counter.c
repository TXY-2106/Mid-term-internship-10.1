/** @file test_gate_counter.c —— 进出方向判别 */
#include "test_util.h"
#include "gate_counter.h"

static qsm_edge_t edge(uint8_t beam, uint8_t level, uint32_t t)
{
    qsm_edge_t e;
    e.beam  = beam;
    e.level = level;
    e.t_ms  = t;
    return e;
}

void suite_gate_counter(void)
{
    qsm_config_t cfg;
    qsm_config_default(&cfg);

    /* 1) A 先 B 后 → 进入 */
    {
        gate_counter_t g;
        gate_init(&g, &cfg, 0);
        qsm_edge_t a = edge(QSM_BEAM_IN_A, 1, 1000);
        qsm_edge_t b = edge(QSM_BEAM_IN_B, 1, 1100);
        gate_on_edge(&g, &a);
        gate_on_edge(&g, &b);
        CHECK_EQ(g.count_in, 1);
        CHECK_EQ(g.count_out, 0);
        CHECK_EQ(gate_occupancy(&g), 1);
    }

    /* 2) B 先 A 后 → 离开 */
    {
        gate_counter_t g;
        gate_init(&g, &cfg, 0);
        qsm_edge_t b = edge(QSM_BEAM_IN_B, 1, 1000);
        qsm_edge_t a = edge(QSM_BEAM_IN_A, 1, 1100);
        gate_on_edge(&g, &b);
        gate_on_edge(&g, &a);
        CHECK_EQ(g.count_in, 0);
        CHECK_EQ(g.count_out, 1);
    }

    /* 3) 超出 300 ms 符合窗口 → 不计数 */
    {
        gate_counter_t g;
        gate_init(&g, &cfg, 0);
        qsm_edge_t a = edge(QSM_BEAM_IN_A, 1, 1000);
        qsm_edge_t b = edge(QSM_BEAM_IN_B, 1, 1500);
        gate_on_edge(&g, &a);
        gate_on_edge(&g, &b);
        CHECK_EQ(g.count_in, 0);
        CHECK_EQ(g.count_out, 0);
    }

    /* 4) 只有一支被触发 → 超时丢弃 */
    {
        gate_counter_t g;
        gate_init(&g, &cfg, 0);
        qsm_edge_t a = edge(QSM_BEAM_IN_A, 1, 1000);
        gate_on_edge(&g, &a);
        gate_tick(&g, 1500);              /* 未超时，保持等待 */
        CHECK_EQ(g.state, 1);
        gate_tick(&g, 3500);              /* 超过 2000 ms → 丢弃 */
        CHECK_EQ(g.state, 0);
        CHECK_EQ(g.count_in, 0);
        CHECK_EQ(g.count_out, 0);
    }

    /* 5) 同一束重复触发不误计 */
    {
        gate_counter_t g;
        gate_init(&g, &cfg, 0);
        qsm_edge_t a1 = edge(QSM_BEAM_IN_A, 1, 1000);
        qsm_edge_t a2 = edge(QSM_BEAM_IN_A, 1, 1100);
        gate_on_edge(&g, &a1);
        gate_on_edge(&g, &a2);
        CHECK_EQ(g.count_in, 0);
        CHECK_EQ(g.state, 1);
    }

    /* 6) 恢复通光的沿不影响方向判定 */
    {
        gate_counter_t g;
        gate_init(&g, &cfg, 0);
        qsm_edge_t a_hi = edge(QSM_BEAM_IN_A, 1, 1000);
        qsm_edge_t a_lo = edge(QSM_BEAM_IN_A, 0, 1050);
        qsm_edge_t b_hi = edge(QSM_BEAM_IN_B, 1, 1100);
        gate_on_edge(&g, &a_hi);
        gate_on_edge(&g, &a_lo);
        gate_on_edge(&g, &b_hi);
        CHECK_EQ(g.count_in, 1);
    }

    /* 7) invert 交换进/出 */
    {
        gate_counter_t g;
        gate_init(&g, &cfg, 1);
        qsm_edge_t a = edge(QSM_BEAM_IN_A, 1, 1000);
        qsm_edge_t b = edge(QSM_BEAM_IN_B, 1, 1100);
        gate_on_edge(&g, &a);
        gate_on_edge(&g, &b);
        CHECK_EQ(g.count_in, 0);
        CHECK_EQ(g.count_out, 1);
    }

    /* 8) 在场人数夹紧 >= 0（出多入少） */
    {
        gate_counter_t g;
        gate_init(&g, &cfg, 0);
        qsm_edge_t b = edge(QSM_BEAM_IN_B, 1, 1000);
        qsm_edge_t a = edge(QSM_BEAM_IN_A, 1, 1100);
        gate_on_edge(&g, &b);
        gate_on_edge(&g, &a);
        CHECK_EQ(gate_occupancy(&g), 0);
    }
}
