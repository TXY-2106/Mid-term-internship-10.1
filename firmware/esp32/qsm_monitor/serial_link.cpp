/**
 * @file  serial_link.cpp
 * @brief STM32 → ESP32 帧解析实现
 */
#include "serial_link.h"
#include <stdio.h>
#include <string.h>

static char     s_buf[128];
static uint8_t  s_len;
static uint32_t s_bad, s_good, s_rx;   /* s_rx: 从 STM32 收到的原始字节数 */
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

/* CRC 在帧里是 2 位十六进制，和 STM32 端 put_hex8() 对应 */
static int hexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool toHex8(const char *p, size_t n, uint8_t *v)
{
    uint8_t r = 0;
    if (n == 0 || n > 2) return false;
    for (size_t i = 0; i < n; i++) {
        int d = hexDigit(p[i]);
        if (d < 0) return false;
        r = (uint8_t)((r << 4) | (uint8_t)d);
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

    uint8_t want = 0;
    if (!toHex8(star + 1, crcLen, &want)) return false;
    if (crc8((const uint8_t *)&line[1], bodyEnd - 1) != want) return false;

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

    // 预测字段是后加的：缺省 0，老固件发的帧照样能解析
    s->predOcc = 0;
    s->predN   = 0;
    if (field(body, bodyLen, 11, &f, &fl) && toU32(f, fl, &v)) {
        s->predOcc = (uint8_t)v;
    }
    if (field(body, bodyLen, 12, &f, &fl) && toU32(f, fl, &v)) {
        s->predN = (uint8_t)v;
    }

    // 探头电平位图同样是后加的：缺省 0
    s->beams = 0;
    if (field(body, bodyLen, 13, &f, &fl) && toU32(f, fl, &v) && v <= 0x0Fu) {
        s->beams = (uint8_t)v;
    }

    // I2C 扫描摘要（bit0=0x68 bit1=0x57 bit2=其他）：缺省 0
    s->i2cSeen = 0;
    if (field(body, bodyLen, 14, &f, &fl) && toU32(f, fl, &v) && v <= 0x07u) {
        s->i2cSeen = (uint8_t)v;
    }

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
    s_rx = 0;
    s_lastGoodMs = 0;
}

bool link_poll(QsmState *out)
{
    while (Serial2.available()) {
        char c = (char)Serial2.read();
        s_rx++;

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

uint32_t link_rxBytes(void)   { return s_rx; }
uint32_t link_badFrames(void)  { return s_bad; }
uint32_t link_goodFrames(void) { return s_good; }
uint32_t link_ageMs(void)      { return s_lastGoodMs ? (millis() - s_lastGoodMs) : 0xFFFFFFFFu; }

bool link_send_cmd(uint8_t cmd)
{
    char    body[16];
    char    line[24];
    uint8_t crc;
    int     n;

    if (cmd < 1 || cmd > 3) {
        return false;
    }

    n = snprintf(body, sizeof(body), "QSR,%u", (unsigned)cmd);
    if (n <= 0 || (size_t)n + 6u > sizeof(line)) {
        return false;
    }

    crc = crc8((const uint8_t *)body, (size_t)n);

    snprintf(line, sizeof(line), "$%s*%02X\n", body, (unsigned)crc);
    Serial2.print(line);                  // STM32 侧 USART2 RX 中断累积后解析
    Serial.print("[QSM] cmd sent: ");
    Serial.print(line);
    return true;
}

