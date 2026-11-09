/**
 * @file  app_main.c
 * @brief 固件主流程：初始化 → 时间片主循环 → 采集/判定/落盘/上报
 *
 * 任务节拍（全部由一个 1 ms 的 SysTick 驱动）：
 *   1 ms   SysTick 中断：g_ms++，调用 bsp_e18_sample_1khz() 采样四路探头
 *   10 ms  主循环：事件出队 → core 状态机 → 更新分级
 *   1 s    主循环：组帧发 ESP32 + 写状态 CSV
 *   10 s   主循环：f_sync 落盘
 *
 * 关键设计：ESP32 断线不影响采集与落盘（双板方案的立身之本）。
 */
#include "app_main.h"
#include "app_config.h"
#include <string.h>
#include "bsp_e18.h"
#include "bsp_ds3231.h"
#include "bsp_uart.h"
#include "bsp_sdcard.h"
#include "data_logger.h"
#include "hist_store.h"
#include "gate_counter.h"
#include "queue_grade.h"
#include "protocol.h"
#include "qsm_history.h"

/* ---------------- 全局状态 ---------------- */
static volatile uint32_t s_ms;          /* 1 ms 递增，仅 SysTick 修改 */
static qsm_config_t   s_cfg;
static gate_counter_t s_gate;
static queue_grade_t  s_grade;
static qsm_state_t    s_state;
static uint32_t       s_seq;
static uint8_t        s_rtc_ok;
#if APP_HISTORY_ENABLE
static qsm_history_t  s_hist;           /* 7 天 × 96 段的在场人数记录 */
#endif
static uint8_t        s_pred;           /* 预测排队人数 */
static uint8_t        s_pred_n;         /* 参与平均的天数 */

/* ---------------- SysTick：1 kHz ---------------- */
void SysTick_Handler(void)
{
    s_ms++;
    bsp_e18_sample_1khz(s_ms);     /* 采样 + 去抖，ISR 内只做这件事 */
}

static uint32_t now_ms(void) { return s_ms; }

/* ---------------- 初始化 ---------------- */
void app_init(void)
{
    qsm_config_default(&s_cfg);
    app_config_build(&s_cfg);      /* 用 app_config.h 的值覆盖默认 */

    gate_init(&s_gate, &s_cfg, APP_GATE_INVERT ? 1u : 0u);
    grade_init(&s_grade, &s_cfg);

    s_state.t_ms        = 0u;
    s_state.count_in    = 0u;
    s_state.count_out   = 0u;
    s_state.occupancy   = 0;
    s_state.q50_occ_ms  = 0u;
    s_state.q100_occ_ms = 0u;
    s_state.grade       = (uint8_t)QSM_GRADE_FEW;
    s_state.flags       = 0u;
    s_state.pred_occ    = 0u;
    s_state.pred_n      = 0u;
    s_seq               = 0u;
    s_pred              = 0u;
    s_pred_n            = 0u;
#if APP_HISTORY_ENABLE
    qsm_hist_init(&s_hist);
#endif

    /* 1 ms SysTick 作为唯一时基 */
    SystemCoreClockUpdate();
    if (SysTick_Config(SystemCoreClock / APP_SAMPLE_HZ) != 0u) {
        while (1) { }              /* 配置失败：停机，便于调试发现 */
    }

    bsp_uart_init();
    bsp_e18_init();

    s_rtc_ok = bsp_ds3231_init();
    if (!s_rtc_ok) {
        bsp_uart_debug("[QSM] DS3231 not found, timestamps disabled");
    }

    if (logger_init()) {
        bsp_uart_debug("[QSM] SD card mounted");
    } else {
        bsp_uart_debug("[QSM] SD card FAILED, logging disabled");
    }

#if APP_HISTORY_ENABLE && APP_HIST_PERSIST
    /* 用 SD 卡里最近 7 天的历史文件填回 RAM 表，掉电重启后预测立即可用 */
    if (s_rtc_ok) {
        char    dt[24];
        int32_t day = -1;
        uint16_t minute = 0u;

        if (bsp_ds3231_get_iso8601(dt, sizeof(dt)) &&
            qsm_hist_parse_iso(dt, &day, &minute) == 0) {
            uint8_t n = hist_store_begin(&s_hist, day);
            bsp_uart_debug(n > 0u ? "[QSM] history reloaded from SD"
                                  : "[QSM] no history file yet");
        }
    }
#endif

    bsp_uart_debug("[QSM] init done");
}

/* ---------------- 10 ms 任务：状态机推进 ---------------- */
static void task_fsm(uint32_t now)
{
    qsm_edge_t e;
    char dt[24];
    uint8_t have_dt = 0u;

    /* 取出所有待处理边沿 */
    while (bsp_e18_pop(&e)) {
        gate_on_edge(&s_gate, &e);
        grade_on_edge(&s_grade, &e);

        /* 每个边沿都写事件日志（A7 证据）；RTC 未就绪时不反复去读 */
        if (!have_dt && s_rtc_ok) {
            have_dt = bsp_ds3231_get_iso8601(dt, sizeof(dt));
        }
        logger_event(&e, have_dt ? dt : "-");
    }

    gate_tick(&s_gate, now);

    int16_t occ = gate_occupancy(&s_gate);
    grade_tick(&s_grade, now, occ);

    /* ---- 汇总为上报状态 ---- */
    s_state.t_ms        = now;
    s_state.count_in    = s_gate.count_in;
    s_state.count_out   = s_gate.count_out;
    s_state.occupancy   = occ;
    s_state.q50_occ_ms  = grade_probe_occ_ms(&s_grade, 0);
    s_state.q100_occ_ms = grade_probe_occ_ms(&s_grade, 1);
    s_state.grade       = s_grade.grade;
    s_state.pred_occ    = s_pred;
    s_state.pred_n      = s_pred_n;

    /* 四路探头当前电平，供网页逐支排查 */
    s_state.beams = 0u;
    for (uint8_t b = 0u; b < (uint8_t)QSM_BEAM_COUNT; b++) {
        if (bsp_e18_level(b) != 0u) {
            s_state.beams |= (uint8_t)(1u << b);
        }
    }

    s_state.flags = 0u;
    if (s_rtc_ok)                                 { s_state.flags |= QSM_FLAG_RTC_OK; }
    if (logger_ok())                              { s_state.flags |= QSM_FLAG_SD_OK; }
    if (s_grade.conflict)                         { s_state.flags |= QSM_FLAG_CROSS_CONFLICT; }
    if (s_pred_n > 0u)                            { s_state.flags |= QSM_FLAG_HIST_OK; }
}

/* ---------------- 调试输出：四路电平 + 计数 ---------------- */
static char *dbg_u32(char *p, uint32_t v)
{
    char tmp[10];
    uint8_t n = 0u;

    do {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v != 0u && n < sizeof(tmp));

    while (n > 0u) {
        *p++ = tmp[--n];
    }
    return p;
}

/** 每秒打印一行，形如：L=0101 in=12 out=3 occ=9
 *  四路顺序：入口A 入口B 队列50% 队列100%，1=遮挡（有人） */
static void debug_dump(void)
{
#if APP_DEBUG_UART_ENABLE
    char s[64];
    char *p = s;

    *p++ = 'L'; *p++ = '=';
    for (uint8_t i = 0u; i < QSM_BEAM_COUNT; i++) {
        *p++ = bsp_e18_level(i) ? '1' : '0';
    }
    *p++ = ' '; *p++ = 'i'; *p++ = 'n'; *p++ = '=';
    p = dbg_u32(p, s_state.count_in);
    *p++ = ' '; *p++ = 'o'; *p++ = 'u'; *p++ = 't'; *p++ = '=';
    p = dbg_u32(p, s_state.count_out);
    *p++ = ' '; *p++ = 'o'; *p++ = 'c'; *p++ = 'c'; *p++ = '=';
    p = dbg_u32(p, (uint32_t)(s_state.occupancy < 0 ? 0 : s_state.occupancy));
    *p++ = ' '; *p++ = 'p'; *p++ = 'r'; *p++ = 'e'; *p++ = 'd'; *p++ = '=';
    p = dbg_u32(p, s_pred);
    *p++ = '/';
    p = dbg_u32(p, s_pred_n);
    *p = '\0';

    bsp_uart_debug(s);
#endif
}

/* ---------------- 60 s 任务：历史记录与预测 ---------------- */
static void task_history(void)
{
#if APP_HISTORY_ENABLE
    char dt[24];
    int32_t day;
    uint16_t minute;
    int32_t occ;

    s_pred   = 0u;
    s_pred_n = 0u;

    /* 没有真实时间就无法判断"同一时段"，只能不预测 */
    if (!s_rtc_ok) {
        return;
    }
    if (!bsp_ds3231_get_iso8601(dt, sizeof(dt))) {
        return;
    }
    if (qsm_hist_parse_iso(dt, &day, &minute) != 0) {
        return;
    }

    occ = s_state.occupancy;
    if (occ < 0)   { occ = 0; }
    if (occ > 255) { occ = 255; }

    (void)qsm_hist_record(&s_hist, day, minute, (uint16_t)occ);
    (void)qsm_hist_predict(&s_hist, day, minute, &s_pred, &s_pred_n);

#if APP_HIST_PERSIST
    /* 落 SD 卡：每天一个 HYYMMDD.CSV，一分钟一行 "HH:MM,occ" */
    (void)hist_store_append(day, minute, (uint16_t)occ);
#endif
#endif
}

/* ---------------- 250 ms 任务：组帧上报（4 Hz，网页才有实时感） ---------------- */
static void task_report(void)
{
    char dt[24];
    char frame[128];
    uint8_t ok = 0u;

    /* RTC 未就绪时不再每秒去碰 I2C：没接 DS3231 也不会拖慢主循环 */
    if (s_rtc_ok) {
        ok = bsp_ds3231_get_iso8601(dt, sizeof(dt));
    }

    int n = qsm_frame_encode(&s_state, s_seq++, ok ? dt : 0, frame, sizeof(frame));
    if (n > 0) {
        bsp_uart_send(frame);      /* 发给 ESP32；断线也不影响本地流程 */
    }
}

/* ---------------- 1 s 任务：写状态 CSV + 调试行 ---------------- */
static uint8_t s_rtc_fail;

static void task_status(void)
{
    char    dt[24];
    uint8_t ok = 0u;

    if (s_rtc_ok) {
        ok = bsp_ds3231_get_iso8601(dt, sizeof(dt));
        if (ok) {
            s_rtc_fail = 0u;
        } else if (++s_rtc_fail >= 10u) {
            /* 连续 10 次读不到（多半是总线被从机拉死）：重新初始化 I2C1，
               自带 9 个 SCL 的总线恢复，不必再手动复位 STM32 */
            s_rtc_fail = 0u;
            s_rtc_ok   = bsp_ds3231_init();
            bsp_uart_debug(s_rtc_ok ? "[QSM] DS3231 recovered"
                                    : "[QSM] DS3231 lost, retrying later");
        }
    }

    logger_status(&s_state, ok ? dt : "-");
    debug_dump();                  /* 调试口打印四路电平，方便排查接线 */
}

/* ---------------- 下行命令：网页复位按钮 ---------------- */
static void app_reset(uint8_t cmd)
{
    if (cmd == QSM_CMD_RESET_COUNTS || cmd == QSM_CMD_RESET_ALL) {
        gate_init(&s_gate, &s_cfg, APP_GATE_INVERT ? 1u : 0u);
        grade_init(&s_grade, &s_cfg);

        s_state.count_in    = 0u;
        s_state.count_out   = 0u;
        s_state.occupancy   = 0;
        s_state.q50_occ_ms  = 0u;
        s_state.q100_occ_ms = 0u;
        s_state.grade       = (uint8_t)QSM_GRADE_FEW;
        s_state.pred_occ    = 0u;
        s_state.pred_n      = 0u;
        s_pred              = 0u;
        s_pred_n            = 0u;
    }

    if (cmd == QSM_CMD_RESET_HISTORY || cmd == QSM_CMD_RESET_ALL) {
#if APP_HISTORY_ENABLE
        qsm_hist_init(&s_hist);
        s_pred              = 0u;
        s_pred_n            = 0u;
        s_state.pred_occ    = 0u;
        s_state.pred_n      = 0u;
#if APP_HIST_PERSIST
        {
            char     dt[24];
            int32_t  day = -1;
            uint16_t minute = 0u;

            if (s_rtc_ok && bsp_ds3231_get_iso8601(dt, sizeof(dt)) &&
                qsm_hist_parse_iso(dt, &day, &minute) == 0) {
                (void)hist_store_clear(day);
            }
        }
#endif
#endif
    }

    bsp_uart_debug(cmd == QSM_CMD_RESET_HISTORY ? "[QSM] cmd: history cleared"
                                                : "[QSM] cmd: counters reset");
}

static void task_cmd(void)
{
    char    line[80];
    uint8_t cmd = 0u;

    if (bsp_uart_poll_cmd(line, sizeof(line)) &&
        qsm_cmd_parse(line, strlen(line), &cmd) == 0) {
        app_reset(cmd);
    }
}

/* ---------------- 主循环 ---------------- */
void app_run(void)
{
    uint32_t t_fsm = 0u, t_report = 0u, t_status = 0u, t_sync = 0u, t_hist = 0u;

    while (1) {
        uint32_t now = now_ms();

        task_cmd();                    /* 每轮查一次命令，无命令时立即返回 */

        if ((now - t_fsm) >= APP_TASK_FSM_MS) {
            t_fsm = now;
            task_fsm(now);
        }
        if ((now - t_report) >= APP_TASK_REPORT_MS) {
            t_report = now;
            task_report();
        }
        if ((now - t_status) >= APP_TASK_STATUS_MS) {
            t_status = now;
            task_status();
        }
        if ((now - t_sync) >= APP_TASK_LOGFLUSH_MS) {
            t_sync = now;
            logger_sync();
            hist_store_sync();
        }
        if ((now - t_hist) >= APP_TASK_HISTORY_MS) {
            t_hist = now;
            task_history();
        }
    }
}

/* ---------------- Keil 工程入口 ---------------- */
int main(void)
{
    app_init();
    app_run();
    return 0;
}
