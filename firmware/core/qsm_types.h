/**
 * @file    qsm_types.h
 * @brief   Queue Status Monitor —— 全工程共用类型与默认参数
 *
 * 本文件被三处使用：
 *   1) firmware/core/         纯逻辑（可在 PC 上编译测试）
 *   2) firmware/stm32/        目标固件（Keil MDK）
 *   3) firmware/esp32/        上位网关（Arduino IDE，协议解析端自行镜像）
 *
 * 设计原则：core/ 不 include 任何 HAL / StdPeriph 头文件，
 *           因此可以脱离硬件用 gcc 编译并跑单元测试。
 */
#ifndef QSM_TYPES_H
#define QSM_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* 队列分级（对应 README「队列分级规则」）                              */
/* ------------------------------------------------------------------ */
typedef enum {
    QSM_GRADE_FEW  = 0,   /* 较少：50% 探头未被持续遮挡 */
    QSM_GRADE_HALF = 1,   /* 约半：50% 持续遮挡，100% 未遮挡 */
    QSM_GRADE_FULL = 2    /* 全满：100% 持续遮挡 */
} qsm_grade_t;

/* ------------------------------------------------------------------ */
/* 四路探头编号                                                        */
/* ------------------------------------------------------------------ */
typedef enum {
    QSM_BEAM_IN_A  = 0,   /* 通道探头 A —— 进出方向判别前束 */
    QSM_BEAM_IN_B  = 1,   /* 通道探头 B —— 进出方向判别后束 */
    QSM_BEAM_Q50   = 2,   /* 队列 50% 位置阈值探头 */
    QSM_BEAM_Q100  = 3,   /* 队列 100% 位置阈值探头 */
    QSM_BEAM_COUNT = 4
} qsm_beam_t;

/* ------------------------------------------------------------------ */
/* 状态标志位（上报帧的 flags 字段）                                    */
/* ------------------------------------------------------------------ */
#define QSM_FLAG_RTC_OK         (1u << 0)  /* DS3231 正常，时间戳可信      */
#define QSM_FLAG_SD_OK          (1u << 1)  /* SD 卡挂载成功，正在写日志    */
#define QSM_FLAG_CROSS_CONFLICT (1u << 2)  /* 分级与在场人数交叉校验不一致 */
#define QSM_FLAG_BEAM_FAULT     (1u << 3)  /* 某路探头长时间不变，疑似故障 */

/* ------------------------------------------------------------------ */
/* 一次稳定边沿事件（去抖后）                                           */
/* ------------------------------------------------------------------ */
typedef struct {
    uint8_t  beam;    /* qsm_beam_t        */
    uint8_t  level;   /* 1 = 遮挡, 0 = 通光 */
    uint32_t t_ms;    /* 开机毫秒计数       */
} qsm_edge_t;

/* ------------------------------------------------------------------ */
/* 对外上报的完整状态                                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t t_ms;          /* 开机毫秒                          */
    uint16_t count_in;      /* 累计进入人数                      */
    uint16_t count_out;     /* 累计离开人数                      */
    int16_t  occupancy;     /* 在场人数 = in - out（夹紧 >= 0）   */
    uint32_t q50_occ_ms;    /* 50% 探头本次连续遮挡时长          */
    uint32_t q100_occ_ms;   /* 100% 探头本次连续遮挡时长         */
    uint8_t  grade;         /* qsm_grade_t                       */
    uint8_t  flags;         /* QSM_FLAG_*                        */
} qsm_state_t;

/* ------------------------------------------------------------------ */
/* 可调参数（STM32 侧集中放在 app_config.h，测试里直接构造）            */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t gate_coincide_ms;    /* 双束符合窗口，默认 300 ms          */
    uint32_t gate_timeout_ms;     /* 方向判定超时，默认 2000 ms         */
    uint32_t grade_sustain_ms;    /* 持续遮挡判定阈值，默认 5000 ms     */
    uint32_t grade_release_ms;    /* 降级所需的持续通光时长，默认 15000 ms */
    int16_t  occ_min_for_full;    /* 交叉校验：判"全满"所需最少在场人数，默认 3 */
} qsm_config_t;

/** 默认参数。与 README 表格一致；标定周只改调用方传入的值。 */
static inline void qsm_config_default(qsm_config_t *c)
{
    c->gate_coincide_ms = 300u;
    c->gate_timeout_ms  = 2000u;
    c->grade_sustain_ms = 5000u;
    c->grade_release_ms = 15000u;
    c->occ_min_for_full = 3;
}

#endif /* QSM_TYPES_H */
