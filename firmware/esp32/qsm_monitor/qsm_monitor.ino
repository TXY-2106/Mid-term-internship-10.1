/**
 * @file  qsm_monitor.ino
 * @brief ESP32 网关：接收 STM32 上报 → 联网 → 托管网页仪表板
 *
 * 板级包：esp32 by Espressif Systems（本项目实测 3.2.1-cn 镜像版本）
 * 开发板：ESP32 Dev Module（ESP32-WROOM-32，CH340 串口）
 *
 * 联网方式在 config.h 的 WIFI_MODE 里选：
 *   1 = AP     自建热点，固定地址 http://192.168.4.1（演示推荐）
 *   2 = STA    连现有 Wi-Fi
 *   3 = APSTA  两者同时开（热点兜底）
 *
 * 职责边界（与 README 的"双板"设计一致）：
 *   - 本机只做联网与展示，不做任何计数判定；
 *   - Wi-Fi 断开或本机重启，都不影响 STM32 的采集与 SD 卡落盘。
 */
#include <WiFi.h>
#include <ESPmDNS.h>
#include "config.h"
#include "serial_link.h"
#include "web_server.h"

static QsmState g_state;

#if (WIFI_MODE == 2) || (WIFI_MODE == 3)
static uint32_t g_lastWifiTry = 0;
#endif

/* ===================== Wi-Fi ===================== */

#if (WIFI_MODE == 2) || (WIFI_MODE == 3)
static void wifi_sta_begin(void)
{
    Serial.printf("[WiFi] connecting to \"%s\" ...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    /* 已经能正常工作的配置就别改；只有用户主动取消了 STA_STATIC_IP 注释才用静态地址 */
#ifdef STA_STATIC_IP
    WiFi.config(IPAddress(STA_STATIC_IP),
                IPAddress(STA_GATEWAY),
                IPAddress(STA_SUBNET),
                IPAddress(STA_DNS));
#endif

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 15000) {
        delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] connected, open http://%s\n",
                      WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[WiFi] not connected yet, will keep retrying in background");
    }
}

static void wifi_sta_keepalive(void)
{
    if (WiFi.status() != WL_CONNECTED && (millis() - g_lastWifiTry) > 5000) {
        g_lastWifiTry = millis();
        WiFi.reconnect();
    }
}
#endif /* STA or APSTA */

static void wifi_ap_begin(void)
{
#if (WIFI_MODE == 1) || (WIFI_MODE == 3)
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("[WiFi] hotspot \"%s\" up, password \"%s\"\n", AP_SSID, AP_PASSWORD);
    Serial.printf("[WiFi] open http://%s\n", WiFi.softAPIP().toString().c_str());
#endif
}

/* 启动时打印一张"演示用"的横幅，现场照着念就行 */
static void print_banner(void)
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("  排队状态监测器 已启动");
#if (WIFI_MODE == 1) || (WIFI_MODE == 3)
    Serial.printf("  热点: %s   密码: %s\n", AP_SSID, AP_PASSWORD);
    Serial.printf("  打开: http://%s\n", WiFi.softAPIP().toString().c_str());
#endif
#if (WIFI_MODE == 2) || (WIFI_MODE == 3)
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("  现场Wi-Fi: http://%s\n", WiFi.localIP().toString().c_str());
    }
#endif
    Serial.println("  (手机提示\"无互联网\"时选\"保持连接\")");
    Serial.println("========================================");
    Serial.println();
}

/* ===================== Arduino 入口 ===================== */

void setup()
{
    Serial.begin(115200);
    delay(100);
    Serial.println("\n[QSM] gateway booting");

    link_begin(LINK_BAUD, LINK_RX_PIN, LINK_TX_PIN);

#if WIFI_MODE == 1
    WiFi.mode(WIFI_AP);
    wifi_ap_begin();
#elif WIFI_MODE == 3
    WiFi.mode(WIFI_AP_STA);
    wifi_ap_begin();
    wifi_sta_begin();
#else
    WiFi.mode(WIFI_STA);
    wifi_sta_begin();
#endif

    /* mDNS：额外送一个 http://qsm.local（Windows 可能需要装 Bonjour，
     * 不确定就还是用上面的 IP，IP 永远可用） */
    if (MDNS.begin("qsm")) {
        MDNS.addService("http", "tcp", WEB_PORT);
        Serial.println("[mDNS] http://qsm.local");
    }

    web_begin();
    print_banner();
    Serial.println("[QSM] http server started on port 80");
}

void loop()
{
    /* 1) 处理串口帧：可能一次来多帧，全部消化掉 */
    while (link_poll(&g_state)) {
        web_update(&g_state);
    }

    /* 2) 处理网页请求 */
    web_loop();

    /* 3) 仅 STA / APSTA 模式需要后台重连（AP 模式是热点本身，不存在掉线） */
#if (WIFI_MODE == 2) || (WIFI_MODE == 3)
    wifi_sta_keepalive();
#endif
}
