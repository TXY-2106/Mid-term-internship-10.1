#ifndef QSM_CONFIG_H
#define QSM_CONFIG_H

/* ===================== 需要你改的地方 ===================== */
#define WIFI_SSID       "OnePlus Ace 3"
#define WIFI_PASSWORD   "exp1ore future"

/* 网页端口 */
#define WEB_PORT        80

/* ===================== 与 STM32 的串口 =====================
 * ESP32 用 Serial2（依据 hardware/硬件接线说明.md）：
 *   ESP32 GPIO16 (RX2) ← STM32 PA2 (USART2_TX)
 *   ESP32 GPIO17 (TX2) → STM32 PA3 (USART2_RX)
 * 两板必须共地，STM32 为 3.3 V 逻辑，可直连。
 *
 * 注意：STM32 的调试口在 USART1(PA9/PA10)，与这里无关，不会抢占。
 */
#define LINK_BAUD       115200
#define LINK_RX_PIN     16
#define LINK_TX_PIN     17

/* ===================== 历史缓冲 =====================
 * 1 帧/秒 × 3600 = 1 小时。内存占用约 3600 × 8 B ≈ 29 KB。
 * 若要更长历史，改用分钟聚合或写入闪存/服务器。
 */
#define HISTORY_SIZE    3600

/* ===================== 告警阈值 =====================
 * 分级 >= 该值 或 在场人数 >= 该值 时，网页显示告警。
 */
#define ALERT_GRADE     2      /* 0=较少 1=约半 2=全满 */
#define ALERT_OCC       8

#endif /* QSM_CONFIG_H */
