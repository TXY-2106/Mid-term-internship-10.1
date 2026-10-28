/**
 * @file  serial_link.cpp
 * @brief STM32 → ESP32 帧解析实现
 */
#include "serial_link.h"
#include <string.h>

static char     s_buf[128];
static uint8_t  s_len;
static uint32_t s_bad, s_good;
static uint32_t s_lastGoodMs;

/* CRC-8/ATM，与 firmware/core/protocol.c 保持一致 */
static uint8_t crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* 取第 idx 个逗号分隔字段 */
static bool field(const char *s, size_t n, uint8_t idx, const char **out, size_t *outLen)
{
    size_t start = 0;
    uint8_t cur = 0;
    for (size_t i = 0; i <= n; i++) {
        if (i == n || s[i] == ',') {
            if (cur == idx) {
                *out = &s[start];
                *outLen = i - start;
                return true;
            }
            cur++;
            start = i + 1;
        }
    }
    return false;
}

static bool toU32(const char *p, size_t n, uint32_t *v)
{
    if (n == 0 || n > 10) return false;
    uint32_t r = 0;
    for (size_t i = 0; i < n; i++) {
        if (p[i] < '0' || p[i] > '9') return false;
        r = r * 10 + (uint32_t)(p[i] - '0');
    }
    *v = r;
    return true;
}

static bool parseFrame(const char *line, size_t len, QsmState *s)
{
    if (len < 5 || line[0] != '$') return false;

    const char *star = (const char *)memchr(line, '*', len);
    if (star == nullptr) return false;

    size_t bodyEnd = (size_t)(star - line);
    size_t crcLen  = len - bodyEnd - 1;
    if (crcLen == 0 || crcLen > 2) return false;

    uint32_t want = 0;
    if (!toU32(star + 1, crcLen, &want)) return false;
    if (crc8((const uint8_t *)&line[1], bodyEnd - 1) != (uint8_t)want) return false;

    const char *body = &line[1];
    size_t bodyLen = bodyEnd - 1;

    const char *f; size_t fl;
    if (!field(body, bodyLen, 0, &f, &fl) || fl != 3 || memcmp(f, "QSM", 3) != 0) return false;

    uint32_t v;

#define NEED_FIELD(idx, target, type)                                    \
    do {                                                                 \
        if (!field(body, bodyLen, (idx), &f, &fl)) return false;         \
        if (!toU32(f, fl, &v)) return false;                             \
        (target) = (type)v;                                              \
    } while (0)

    NEED_FIELD(1,  s->seq,       uint32_t);
    NEED_FIELD(2,  s->uptimeMs,  uint32_t);
    NEED_FIELD(4,  s->countIn,   uint16_t);
    NEED_FIELD(5,  s->countOut,  uint16_t);
    NEED_FIELD(6,  s->occupancy, int16_t);
    NEED_FIELD(7,  s->q50Ms,     uint32_t);
    NEED_FIELD(8,  s->q100Ms,    uint32_t);
    NEED_FIELD(9,  s->grade,     uint8_t);
    NEED_FIELD(10, s->flags,     uint8_t);
#undef NEED_FIELD

    if (!field(body, bodyLen, 3, &f, &fl)) return false;
    size_t k = fl < sizeof(s->datetime) - 1 ? fl : sizeof(s->datetime) - 1;
    memcpy(s->datetime, f, k);
    s->datetime[k] = '\0';

    s->valid = true;
    return true;
}

void link_begin(uint32_t baud, int rxPin, int txPin)
{
    Serial2.begin(baud, SERIAL_8N1, rxPin, txPin);
    s_len = 0;
    s_bad = s_good = 0;
    s_lastGoodMs = 0;
}

bool link_poll(QsmState *out)
{
    while (Serial2.available()) {
        char c = (char)Serial2.read();

        if (c == '$') {                 // 帧头：重新开始收集
            s_len = 0;
            s_buf[s_len++] = c;
            continue;
        }
        if (s_len == 0) continue;       // 还没等到帧头

        if (c == '\n' || c == '\r') {
            if (s_len > 1) {
                s_buf[s_len] = '\0';
                QsmState tmp;
                memset(&tmp, 0, sizeof(tmp));
                if (parseFrame(s_buf, s_len, &tmp)) {
                    s_good++;
                    s_lastGoodMs = millis();
                    *out = tmp;
                    s_len = 0;
                    return true;
                }
                s_bad++;
            }
            s_len = 0;
            continue;
        }

        if (s_len < sizeof(s_buf) - 1) {
            s_buf[s_len++] = c;
        } else {
            s_len = 0;                  // 超长，丢弃
            s_bad++;
        }
    }
    return false;
}

uint32_t link_badFrames(void)  { return s_bad; }
uint32_t link_goodFrames(void) { return s_good; }
uint32_t link_ageMs(void)      { return s_lastGoodMs ? (millis() - s_lastGoodMs) : 0xFFFFFFFFu; }
