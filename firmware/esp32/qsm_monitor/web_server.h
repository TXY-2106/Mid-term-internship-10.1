/**
 * @file  web_server.h
 * @brief 托管仪表板与 HTTP API
 *
 *   GET /            仪表板网页（内嵌，无外部依赖）
 *   GET /api/state   最新一帧的状态 + 链路统计 + 告警阈值
 */
#ifndef QSM_WEB_SERVER_H
#define QSM_WEB_SERVER_H

#include <Arduino.h>
#include "serial_link.h"

void web_begin();

/** 每次收到新帧后调用，把状态推入历史缓冲 */
void web_update(const QsmState *s);

/** 主循环里调用，处理 HTTP 请求 */
void web_loop();

#endif /* QSM_WEB_SERVER_H */
