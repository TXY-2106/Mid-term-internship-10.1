/** @file test_queue_grade.c —— 三级分级 / 滞回 / 交叉校验 */
#include "test_util.h"
#include "queue_grade.h"

static qsm_edge_t edge(uint8_t beam, uint8_t level, uint32_t t)
{
    qsm_edge_t e;
    e.beam  = beam;
    e.level = level;
    e.t_ms  = t;
    return e;
}

void suite_queue_grade(void)
{
    qsm_config_t cfg;
    qsm_config_default(&cfg);   /* sustain=5000, release=15000, occ_min=3 */

    /* 1) 正常升级链：FEW → HALF → FULL */
    {
        queue_grade_t q;
        qsm_edge_t q50_on  = edge(QSM_BEAM_Q50, 1, 1000);
        qsm_edge_t q100_on = edge(QSM_BEAM_Q100, 1, 7000);
        grade_init(&q, &cfg);

        CHECK_EQ(q.grade, QSM_GRADE_FEW);

        grade_on_edge(&q, &q50_on);
        grade_tick(&q, 3000, 5);                       /* 遮挡 2000 ms，不足 5000 */
        CHECK_EQ(q.grade, QSM_GRADE_FEW);
        CHECK_EQ(grade_probe_occ_ms(&q, 0), 2000);

        grade_tick(&q, 6500, 5);                       /* 遮挡 5500 ms → HALF */
        CHECK_EQ(q.grade, QSM_GRADE_HALF);

        grade_on_edge(&q, &q100_on);
        grade_tick(&q, 12500, 5);                      /* Q100 遮挡 5500 ms → FULL */
        CHECK_EQ(q.grade, QSM_GRADE_FULL);
    }

    /* 2) 滞回：降级需持续通光 15 s */
    {
        queue_grade_t q;
        qsm_edge_t q50_on   = edge(QSM_BEAM_Q50, 1, 1000);
        qsm_edge_t q100_on  = edge(QSM_BEAM_Q100, 1, 7000);
        qsm_edge_t q100_off = edge(QSM_BEAM_Q100, 0, 13000);
        grade_init(&q, &cfg);

        grade_on_edge(&q, &q50_on);
        grade_tick(&q, 6500, 5);                       /* → HALF */
        grade_on_edge(&q, &q100_on);
        grade_tick(&q, 12500, 5);                      /* → FULL */
        CHECK_EQ(q.grade, QSM_GRADE_FULL);

        grade_on_edge(&q, &q100_off);
        grade_tick(&q, 20000, 5);                      /* 通光仅 7000 ms < 15000 → 仍 FULL */
        CHECK_EQ(q.grade, QSM_GRADE_FULL);

        grade_tick(&q, 29000, 5);                      /* 通光 16000 ms ≥ 15000 → 降为 HALF */
        CHECK_EQ(q.grade, QSM_GRADE_HALF);
    }

    /* 3) 交叉校验：在场人数不足时不判全满 */
    {
        queue_grade_t q;
        qsm_edge_t q100_on = edge(QSM_BEAM_Q100, 1, 1000);
        grade_init(&q, &cfg);
        grade_on_edge(&q, &q100_on);
        grade_tick(&q, 12000, 1);                      /* 遮挡 11 s，但在场仅 1 人 */
        CHECK_EQ(q.conflict, 1);
        CHECK_EQ(q.grade, QSM_GRADE_FEW);
    }

    /* 4) 在场人数充足时正常判全满 */
    {
        queue_grade_t q;
        qsm_edge_t q100_on = edge(QSM_BEAM_Q100, 1, 1000);
        grade_init(&q, &cfg);
        grade_on_edge(&q, &q100_on);
        grade_tick(&q, 12000, 4);
        CHECK_EQ(q.conflict, 0);
        CHECK_EQ(q.grade, QSM_GRADE_FULL);
    }

    /* 5) 短脉冲（路过）不触发分级：遮挡 < 5 s 后立即恢复 */
    {
        queue_grade_t q;
        qsm_edge_t on  = edge(QSM_BEAM_Q50, 1, 1000);
        qsm_edge_t off = edge(QSM_BEAM_Q50, 0, 2000);
        grade_init(&q, &cfg);
        grade_on_edge(&q, &on);
        grade_tick(&q, 1900, 5);                       /* 仅 900 ms */
        CHECK_EQ(q.grade, QSM_GRADE_FEW);
        grade_on_edge(&q, &off);
        grade_tick(&q, 2100, 5);
        CHECK_EQ(grade_probe_occ_ms(&q, 0), 0);
        CHECK_EQ(q.grade, QSM_GRADE_FEW);
    }
}
