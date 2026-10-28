/**
 * @file  web_server.cpp
 * @brief HTTP 服务与 JSON 序列化
 */
#include "web_server.h"
#include "web_page.h"
#include "config.h"
#include <WebServer.h>
#include <WiFi.h>

static WebServer  s_server(WEB_PORT);
static QsmState   s_state;
static bool       s_hasState = false;

static void handleRoot()
{
    s_server.send_P(200, "text/html; charset=utf-8", QSM_PAGE);
}

static void handleState()
{
    char buf[320];
    if (!s_hasState) {
        snprintf(buf, sizeof(buf), "{\"valid\":false,\"good\":%lu,\"bad\":%lu,\"alert_grade\":%d,\"alert_occ\":%d}",
                 (unsigned long)link_goodFrames(), (unsigned long)link_badFrames(),
                 ALERT_GRADE, ALERT_OCC);
    } else {
        snprintf(buf, sizeof(buf),
            "{\"valid\":true,\"seq\":%lu,\"ms\":%lu,\"datetime\":\"%s\","
            "\"in\":%u,\"out\":%u,\"occ\":%d,\"q50\":%lu,\"q100\":%lu,"
            "\"grade\":%u,\"flags\":%u,\"good\":%lu,\"bad\":%lu,\"age\":%lu,"
            "\"alert_grade\":%d,\"alert_occ\":%d}",
            (unsigned long)s_state.seq, (unsigned long)s_state.uptimeMs, s_state.datetime,
            s_state.countIn, s_state.countOut, s_state.occupancy,
            (unsigned long)s_state.q50Ms, (unsigned long)s_state.q100Ms,
            s_state.grade, s_state.flags,
            (unsigned long)link_goodFrames(), (unsigned long)link_badFrames(),
            (unsigned long)link_ageMs(),
            ALERT_GRADE, ALERT_OCC);
    }

    s_server.sendHeader("Cache-Control", "no-store");
    s_server.send(200, "application/json", buf);
}

void web_begin()
{
    s_server.on("/", handleRoot);
    s_server.on("/api/state", handleState);
    s_server.onNotFound([]() { s_server.send(404, "text/plain", "not found"); });
    s_server.begin();
}

void web_update(const QsmState *s)
{
    s_state = *s;
    s_hasState = true;
}

void web_loop()
{
    s_server.handleClient();
}
