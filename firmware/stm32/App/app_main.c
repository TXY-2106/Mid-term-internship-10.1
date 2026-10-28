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
#include "bsp_e18.h"
#include "bsp_ds3231.h"
#include "bsp_uart.h"
#include "bsp_sdcard.h"
#include "data_logger.h"
#include "gate_counter.h"
#include "queue_grade.h"
#include "protocol.h"

/* ---------------- 全局状态 ---------------- */
static volatile uint32_t s_ms;          /* 1 ms 递增，仅 SysTick 修改 */
static qsm_config_t   s_cfg;
static gate_counter_t s_gate;
static queue_grade_t  s_grade;
static qsm_state_t    s_state;
static uint32_t       s_seq;
static uint8_t        s_rtc_ok;

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
    s_seq               = 0u;

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

        /* 每个边沿都写事件日志（A7 证据） */
        if (!have_dt) {
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

    s_state.flags = 0u;
    if (s_rtc_ok)                                 { s_state.flags |= QSM_FLAG_RTC_OK; }
    if (logger_ok())                              { s_state.flags |= QSM_FLAG_SD_OK; }
    if (s_grade.conflict)                         { s_state.flags |= QSM_FLAG_CROSS_CONFLICT; }
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
    char s[48];
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
    *p = '\0';

    bsp_uart_debug(s);
#endif
}

/* ---------------- 1 s 任务：组帧上报 + 写状态行 ---------------- */
static void task_report(void)
{
    char dt[24];
    char frame[128];
    uint8_t ok = bsp_ds3231_get_iso8601(dt, sizeof(dt));

    int n = qsm_frame_encode(&s_state, s_seq++, ok ? dt : 0, frame, sizeof(frame));
    if (n > 0) {
        bsp_uart_send(frame);      /* 发给 ESP32；断线也不影响本地流程 */
    }
    logger_status(&s_state, ok ? dt : "-");

    debug_dump();                  /* 调试口打印四路电平，方便排查接线 */
}

/* ---------------- 主循环 ---------------- */
void app_run(void)
{
    uint32_t t_fsm = 0u, t_report = 0u, t_sync = 0u;

    while (1) {
        uint32_t now = now_ms();

        if ((now - t_fsm) >= APP_TASK_FSM_MS) {
            t_fsm = now;
            task_fsm(now);
        }
        if ((now - t_report) >= APP_TASK_REPORT_MS) {
            t_report = now;
            task_report();
        }
        if ((now - t_sync) >= APP_TASK_LOGFLUSH_MS) {
            t_sync = now;
            logger_sync();
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
