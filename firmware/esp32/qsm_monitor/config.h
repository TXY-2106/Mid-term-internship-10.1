#ifndef QSM_CONFIG_H
#define QSM_CONFIG_H

/* =========================================================================
 * 联网方式（改这一个数就行，.ino 不用动）
 *
 *   1 = AP     自建热点 —— 演示推荐
 *              不连任何现场 Wi-Fi，ESP32 自己开热点，
 *              地址永远是 http://192.168.4.1，手机/笔记本连上就能看。
 *              好处：换场地、现场没网、校园网要认证，全都不影响。
 *
 *   2 = STA    连现有 Wi-Fi —— 平时的开发调试模式
 *              需要填下面的 WIFI_SSID / WIFI_PASSWORD，
 *              IP 由路由器分配（想知道别人怎么访问，看串口打印的 IP）。
 *
 *   3 = APSTA  两者同时开 —— 最稳的演示方案（可选）
 *              既连现场 Wi-Fi（能上网、能远程看），
 *              又开热点（连不上现场网时用 192.168.4.1 兜底）。
 * ========================================================================= */
#define WIFI_MODE       1

/* ------------------------- 热点参数（模式 1 / 3 用） --------------------- */
#define AP_SSID         "QSM-Monitor"
#define AP_PASSWORD     "qsm12345"      /* 至少 8 位，否则开热点会失败 */

/* ------------------------- 现场 Wi-Fi（模式 2 / 3 用） ------------------- */
#define WIFI_SSID       "OnePlus Ace 3"
#define WIFI_PASSWORD   "exp1ore future"

/* STA 模式下想用固定 IP（不想每次去串口找 IP）就取消注释并改成你的网段。
 * 注意：IP 要和路由器同网段，且不能和别的设备冲突。 */
// #define STA_STATIC_IP   192, 168, 1, 50
// #define STA_GATEWAY     192, 168, 1, 1
// #define STA_SUBNET      255, 255, 255, 0
// #define STA_DNS         192, 168, 1, 1

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
