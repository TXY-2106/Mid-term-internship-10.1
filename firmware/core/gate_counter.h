/**
 * @file  gate_counter.h
 * @brief 进出方向判别 + 在场人数累加（纯逻辑，无硬件依赖）
 *
 * 物理布置：通道上沿行进方向前后错开约 10 cm 装两支 E18-D80NK。
 * 判定规则：两束在 gate_coincide_ms 内先后被遮挡 → 按先后顺序定方向；
 *           只触发一支、或超出窗口 → 视为无效（误触发/路过）并丢弃。
 *
 * 该规则来自参考文献 [1]（dos Santos 等，300 ms 符合窗口）。
 */
#ifndef QSM_GATE_COUNTER_H
#define QSM_GATE_COUNTER_H

#include "qsm_types.h"

typedef struct {
    const qsm_config_t *cfg;

    uint8_t  state;       /* 0 = IDLE, 1 = WAIT_SECOND          */
    uint8_t  first_beam;  /* 先被遮挡的是 IN_A 还是 IN_B        */
    uint32_t t_first;     /* 第一次遮挡时刻                     */

    uint16_t count_in;
    uint16_t count_out;

    uint8_t  invert;      /* 1 = 交换进出方向（现场接线方向不同时用） */
} gate_counter_t;

void gate_init(gate_counter_t *g, const qsm_config_t *cfg, uint8_t invert);

/** 喂入一个稳定边沿。只有 level==1（开始遮挡）会推进状态机。 */
void gate_on_edge(gate_counter_t *g, const qsm_edge_t *e);

/** 周期性调用（建议 10 ms），用于超时丢弃。 */
void gate_tick(gate_counter_t *g, uint32_t now_ms);

/** 在场人数 = count_in - count_out，夹紧在 >= 0。 */
int16_t gate_occupancy(const gate_counter_t *g);

#endif /* QSM_GATE_COUNTER_H */
