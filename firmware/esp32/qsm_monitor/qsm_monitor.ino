/**
 * @file  qsm_monitor.ino
 * @brief ESP32 网关：接收 STM32 上报 → 连 Wi-Fi → 托管网页仪表板
 *
 * 板级包：esp32 by Espressif Systems（本项目实测 3.2.1-cn 镜像版本）
 * 开发板：ESP32 Dev Module（ESP32-WROOM-32，CH340 串口）
 *
 * 职责边界（与 README 的"双板"设计一致）：
 *   - 本机只做联网与展示，不做任何计数判定；
 *   - Wi-Fi 断开或本机重启，都不影响 STM32 的采集与 SD 卡落盘。
 */
#include <WiFi.h>
#include "config.h"
#include "serial_link.h"
#include "web_server.h"

static QsmState g_state;
static uint32_t g_lastWifiTry = 0;

static void wifi_connect_blocking_once()
{
    Serial.printf("[WiFi] connecting to %s ...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 15000) {
        delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[WiFi] connected, IP = ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("[WiFi] not connected yet, will keep retrying in background");
    }
}

void setup()
{
    Serial.begin(115200);
    delay(100);
    Serial.println("\n[QSM] gateway booting");

    link_begin(LINK_BAUD, LINK_RX_PIN, LINK_TX_PIN);
    wifi_connect_blocking_once();
    web_begin();

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

    /* 3) Wi-Fi 掉线时后台重连（不阻塞采集与展示） */
    if (WiFi.status() != WL_CONNECTED && (millis() - g_lastWifiTry) > 5000) {
        g_lastWifiTry = millis();
        WiFi.reconnect();
    }
}
