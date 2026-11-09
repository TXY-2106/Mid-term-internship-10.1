/**
 * @file    app_config.h
 * @brief   全部可调参数与引脚定义 —— 标定周只需改这个文件
 *
 * 目标芯片：STM32F103C8T6（Cortex-M3, 72 MHz, 64 KB Flash / 20 KB RAM）
 * 开发环境：Keil MDK5 + STM32F10x 标准外设库
 *
 * 引脚分配严格对应仓库 hardware/硬件接线说明.md：
 *
 *   PB12 E18 入口探头 A        PB6  I2C1_SCL  → DS3231
 *   PB13 E18 入口探头 B        PB7  I2C1_SDA  → DS3231
 *   PB14 E18 队列 50% 探头     PA4  SPI1_CS   → SD 卡座
 *   PB15 E18 队列 100% 探头    PA5  SPI1_SCK  → SD 卡座
 *   PA2  USART2_TX → ESP32 GPIO16(RX2)
 *   PA3  USART2_RX ← ESP32 GPIO17(TX2)
 *   PA9  USART1_TX → CH340 调试（可选）
 *   PA10 USART1_RX ← CH340 调试（可选）
 *   PA13/PA14  SWDIO / SWCLK（ST-Link，接 3.3V 引脚不接）
 *
 * 供电：整机由充电宝经 ESP32 Type-C 输入，ESP32 的 VIN 输出 5V 供
 *       STM32 / E18×4 / SD 卡座；STM32 的 3V3 供 DS3231 与四路上拉。
 *       注意：不要插 STM32 的 Type-C；不要把两块板的 3V3 并联。
 *
 * 四路探头不复用 EXTI：采用 SysTick 1 kHz 定时采样 + 移位寄存器去抖。
 * 轮询时序确定、抗噪好，且与"下降沿=开始遮挡"的电平语义一致。
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "stm32f10x.h"
#include "qsm_types.h"

/* ---------------- 采样与去抖 ---------------- */
#define APP_SAMPLE_HZ            1000u   /* SysTick 频率，同时作为采样率 */
#define APP_DEBOUNCE_SAMPLES     5u      /* 连续 5 个同值才认定变化 → 5 ms */
#define APP_E18_ACTIVE_LOW       1u      /* E18-D80NK：无遮挡 3.3V、有遮挡 0V */

/* ---------------- 任务周期（毫秒） ---------------- */
#define APP_TASK_FSM_MS          10u     /* 状态机推进与事件出队 */
#define APP_TASK_REPORT_MS       250u    /* 组帧发 ESP32：4 Hz，网页才有实时感 */
#define APP_TASK_STATUS_MS       1000u   /* 写状态 CSV + 调试行：1 Hz，不频繁动 SD */
#define APP_TASK_LOGFLUSH_MS     10000u  /* f_sync 落盘 */
#define APP_TASK_HISTORY_MS      60000u  /* 历史统计：每分钟记一次在场人数 */

/* ---------------- 队列分级参数（与 README 表格一致） ---------------- */
#define APP_GATE_COINCIDE_MS     300u    /* 双束符合窗口，参考 [1] */
#define APP_GATE_TIMEOUT_MS      2000u   /* 方向判定超时 */
#define APP_GRADE_SUSTAIN_MS     5000u   /* 持续遮挡判定为"有人排队" */
#define APP_GRADE_RELEASE_MS     15000u  /* 降级所需的持续通光时间 */
#define APP_OCC_MIN_FOR_FULL     3       /* 交叉校验：判全满所需最少在场人数 */

/* ---------------- 现场几何（供文档与标定参照） ----------------
 * 50%/100% 位置沿"从服务台往队尾"的方向计：
 *   50% 探头装在通道中点，100% 探头装在队尾（满员线）。
 * 这样队列从服务台向后生长时，先遮挡 50%、再遮挡 100%，与 README 分级规则一致。
 * 若把 100% 探头装在服务台前，它会长期被正在服务的人遮挡而恒为"全满"。
 */
#define APP_LANE_LEN_CM          600u    /* 队列通道标称长度（满员线） */
#define APP_Q50_FROM_COUNTER_CM  300u    /* = 50% × 通道长 */
#define APP_Q100_FROM_COUNTER_CM 600u    /* = 100% × 通道长，即队尾 */
#define APP_BEAM_HEIGHT_CM       100u    /* 四路探头离地高度 */
#define APP_GATE_BEAM_GAP_CM     10u     /* 入口双束沿行进方向错开距离 */

/* ---------------- 现场接线方向 ---------------- */
#define APP_GATE_INVERT          0u      /* 若发现"进""出"反了，改成 1 即可 */

/* ---------------- 串口 ----------------
 * USART2(PA2/PA3) 固定给 ESP32，帧协议走这里；
 * USART1(PA9/PA10) 作调试口，接 USB-TTL(CH340)。没有 CH340 时可关掉。
 */
#define APP_UART_BAUD            115200u
#define APP_DEBUG_UART_ENABLE    1u      /* 0 = 关闭调试输出，便于量产/省电 */

/* ---------------- 单探头台架测试 ----------------
 * 手里只有一支 E18 时置 1：四路逻辑探头全部读同一个引脚（PB12）。
 * 效果：挥一次手会同时产生"入口 A"与"入口 B"两个边沿 → 在场人数 +1；
 *       手在探头前停住 → "队列 50%/100%"同时持续遮挡 → 分级升到"全满"。
 * 用途：仅一支传感器时也能验证整条链路（计数 / 分级 / 协议 / 网页）。
 * ★ 正式部署必须置回 0，否则四路探头共用一个输入。
 */
#define APP_SINGLE_BEAM_TEST     0u

/* ---------------- 分阶段启用开关 ----------------
 * 现在三项都已接通：
 *   APP_RTC_ENABLE   1 → I2C1(PB6/PB7) 读 DS3231；时间戳与"同一时段"判断都要它
 *   APP_SDCARD_ENABLE 1 → SPI1(PA4–PA7) + Middlewares/FatFs，日志与历史文件落卡
 *   APP_HIST_PERSIST 1  → 每分钟的在场人数写进"每天一个"的 HYYMMDD.CSV，上电回读
 *
 * 任一项改成 0，对应源文件自动退化为空实现，不会产生编译/链接错误：
 *   关 SD 卡 → 预测只能靠本次上电后 RAM 里积累的数据
 *   关 RTC   → 没有真实时间，预测恒为 0（网页显示"历史不足"）
 */
#define APP_RTC_ENABLE           1u
#define APP_SDCARD_ENABLE        1u
#define APP_HIST_PERSIST         1u

/* ---------------- 历史人数预测 ----------------
 * 把在场人数按 15 分钟一段存进 RAM（7 天 × 96 段 = 672 B），
 * 预测值 = 过去 7 天同一时段的平均在场人数，随上报帧一起发给 ESP32 显示。
 *
 * ★ 需要真实时间：依赖 DS3231（APP_RTC_ENABLE = 1），否则无法判断"同一时段"，
 *   此时预测值恒为 0、网页显示"历史不足"。
 *   实测只需连续运行满一个自然日，之后就有预测值。
 */
#define APP_HISTORY_ENABLE       1u

/* ---------------- 日志文件（FatFs 8.3 命名） ---------------- */
#define APP_STATUS_CSV           "status.csv"
#define APP_EVENTS_CSV           "events.csv"

/** 由 app_config.h 生成 core 使用的参数结构体 */
static inline void app_config_build(qsm_config_t *c)
{
    c->gate_coincide_ms = APP_GATE_COINCIDE_MS;
    c->gate_timeout_ms  = APP_GATE_TIMEOUT_MS;
    c->grade_sustain_ms = APP_GRADE_SUSTAIN_MS;
    c->grade_release_ms = APP_GRADE_RELEASE_MS;
    c->occ_min_for_full = APP_OCC_MIN_FOR_FULL;
}

#endif /* APP_CONFIG_H */

