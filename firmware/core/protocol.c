/**
 * @file  protocol.c
 * @brief 帧编解码实现。不依赖任何 libc 的 printf/strtok，便于在 MCU 上裁剪。
 */
#include "protocol.h"

uint8_t qsm_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00u;
    for (size_t i = 0u; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0u; b < 8u; b++) {
            if (crc & 0x80u) {
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* ---------------- 极简无符号整数写入 ---------------- */
static size_t put_uint(char *buf, size_t cap, size_t pos, uint32_t v, uint8_t min_digits)
{
    char tmp[10];
    size_t n = 0u;

    do {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v != 0u && n < sizeof(tmp));

    while (n < min_digits && n < sizeof(tmp)) {
        tmp[n++] = '0';
    }
    if (pos + n + 1u > cap) {
        return 0u;   /* 空间不足，由调用方检查 */
    }
    while (n > 0u) {
        buf[pos++] = tmp[--n];
    }
    return pos;
}

static size_t put_char(char *buf, size_t cap, size_t pos, char c)
{
    if (pos + 1u > cap) {
        return 0u;
    }
    buf[pos] = c;
    return pos + 1u;
}

static size_t put_str(char *buf, size_t cap, size_t pos, const char *s)
{
    if (s == NULL) { s = "-"; }
    while (*s != '\0') {
        pos = put_char(buf, cap, pos, *s++);
        if (pos == 0u) { return 0u; }
    }
    return pos;
}

/** CRC 固定写成两位大写十六进制（如 *5E），解析端也按十六进制读 */
static size_t put_hex8(char *buf, size_t cap, size_t pos, uint8_t v)
{
    static const char hex[] = "0123456789ABCDEF";

    if (pos + 3u > cap) {
        return 0u;
    }
    buf[pos++] = hex[(v >> 4) & 0x0Fu];
    buf[pos++] = hex[v & 0x0Fu];
    return pos;
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    return -1;
}

static int parse_hex8(const char *p, size_t n, uint8_t *out)
{
    uint8_t v = 0u;

    if (n == 0u || n > 2u) {
        return -1;
    }
    for (size_t i = 0u; i < n; i++) {
        int d = hex_digit(p[i]);
        if (d < 0) {
            return -1;
        }
        v = (uint8_t)((v << 4) | (uint8_t)d);
    }
    *out = v;
    return 0;
}

int qsm_frame_encode(const qsm_state_t *s,
                     uint32_t seq,
                     const char *dt,
                     char *buf,
                     size_t buflen)
{
    if (s == NULL || buf == NULL || buflen < 16u) {
        return -1;
    }

    size_t pos = 0u;
    pos = put_char(buf, buflen, pos, '$');
    pos = put_str(buf, buflen, pos, "QSM,");
    if (pos == 0u) { return -1; }

    pos = put_uint(buf, buflen, pos, seq,          1u); if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, ',');             if (pos == 0u) { return -1; }
    pos = put_uint(buf, buflen, pos, s->t_ms,      1u); if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, ',');             if (pos == 0u) { return -1; }
    pos = put_str (buf, buflen, pos, dt);              if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, ',');             if (pos == 0u) { return -1; }
    pos = put_uint(buf, buflen, pos, s->count_in,  1u); if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, ',');             if (pos == 0u) { return -1; }
    pos = put_uint(buf, buflen, pos, s->count_out, 1u); if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, ',');             if (pos == 0u) { return -1; }
    pos = put_uint(buf, buflen, pos, (uint32_t)(s->occupancy < 0 ? 0 : s->occupancy), 1u);
    if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, ',');             if (pos == 0u) { return -1; }
    pos = put_uint(buf, buflen, pos, s->q50_occ_ms,  1u); if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, ',');             if (pos == 0u) { return -1; }
    pos = put_uint(buf, buflen, pos, s->q100_occ_ms, 1u); if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, ',');             if (pos == 0u) { return -1; }
    pos = put_uint(buf, buflen, pos, s->grade,      1u); if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, ',');             if (pos == 0u) { return -1; }
    pos = put_uint(buf, buflen, pos, s->flags,      1u); if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, ',');             if (pos == 0u) { return -1; }
    pos = put_uint(buf, buflen, pos, s->pred_occ,   1u); if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, ',');             if (pos == 0u) { return -1; }
    pos = put_uint(buf, buflen, pos, s->pred_n,     1u); if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, ',');             if (pos == 0u) { return -1; }
    pos = put_uint(buf, buflen, pos, s->beams,      1u); if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, ',');             if (pos == 0u) { return -1; }
    pos = put_uint(buf, buflen, pos, s->i2c_seen,   1u); if (pos == 0u) { return -1; }

    /* CRC 覆盖 '$' 之后到 '*' 之前 */
    uint8_t crc = qsm_crc8((const uint8_t *)&buf[1], pos - 1u);

    pos = put_char(buf, buflen, pos, '*'); if (pos == 0u) { return -1; }
    pos = put_hex8(buf, buflen, pos, crc);  if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, '\n'); if (pos == 0u) { return -1; }
    buf[pos] = '\0';
    return (int)pos;
}

/* ---------------- 解析 ---------------- */
static int parse_uint(const char *p, size_t n, uint32_t *out)
{
    if (n == 0u || n > 10u) {
        return -1;
    }
    uint32_t v = 0u;
    for (size_t i = 0u; i < n; i++) {
        if (p[i] < '0' || p[i] > '9') {
            return -1;
        }
        v = v * 10u + (uint32_t)(p[i] - '0');
    }
    *out = v;
    return 0;
}

/** 把 s[0..n) 的第 idx 个字段复制出来；返回 0 成功 */
static int field_at(const char *s, size_t n, uint8_t idx,
                    const char **out, size_t *outlen)
{
    size_t start = 0u;
    uint8_t cur  = 0u;

    for (size_t i = 0u; i <= n; i++) {
        if (i == n || s[i] == ',') {
            if (cur == idx) {
                *out    = &s[start];
                *outlen = i - start;
                return 0;
            }
            cur++;
            start = i + 1u;
        }
    }
    return -1;
}

int qsm_frame_parse(const char *line,
                    qsm_state_t *s,
                    uint32_t *seq,
                    char *dt,
                    size_t dtlen)
{
    if (line == NULL || s == NULL || seq == NULL) {
        return -1;
    }
    if (line[0] != '$') {
        return -1;
    }

    /* 找 '*' 与 '\n' */
    size_t star = 0u;
    size_t end  = 0u;
    for (size_t i = 1u; line[i] != '\0'; i++) {
        if (line[i] == '*') { star = i; }
        else if (line[i] == '\n' || line[i] == '\r') { end = i; break; }
    }
    if (star == 0u) {
        return -1;
    }
    if (end == 0u) {
        for (size_t i = star + 1u; line[i] != '\0'; i++) { end = i + 1u; }
    }

    /* CRC 校验：'$' 后到 '*' 前 */
    uint8_t want = 0u;
    size_t crc_len = (end > star + 1u) ? (end - star - 1u) : 0u;
    if (crc_len == 0u || crc_len > 2u) {
        return -1;
    }
    if (parse_hex8(&line[star + 1u], crc_len, &want) != 0) {
        return -1;
    }
    if (qsm_crc8((const uint8_t *)&line[1], star - 1u) != want) {
        return -2;
    }

    /* 负载 = '$' 后到 '*' 前，首字段应为 "QSM" */
    const char *body = &line[1];
    size_t body_len  = star - 1u;

    const char *fp; size_t fl;
    if (field_at(body, body_len, 0u, &fp, &fl) != 0 ||
        fl != 3u || fp[0] != 'Q' || fp[1] != 'S' || fp[2] != 'M') {
        return -1;
    }

    uint32_t v = 0u;

    if (field_at(body, body_len, 1u, &fp, &fl) != 0 || parse_uint(fp, fl, &v) != 0) { return -1; }
    *seq = v;

    if (field_at(body, body_len, 2u, &fp, &fl) != 0 || parse_uint(fp, fl, &v) != 0) { return -1; }
    s->t_ms = v;

    if (field_at(body, body_len, 3u, &fp, &fl) != 0) { return -1; }
    if (dt != NULL && dtlen > 0u) {
        size_t k = (fl < dtlen - 1u) ? fl : (dtlen - 1u);
        for (size_t i = 0u; i < k; i++) { dt[i] = fp[i]; }
        dt[k] = '\0';
    }

    if (field_at(body, body_len, 4u, &fp, &fl) != 0 || parse_uint(fp, fl, &v) != 0) { return -1; }
    s->count_in = (uint16_t)v;

    if (field_at(body, body_len, 5u, &fp, &fl) != 0 || parse_uint(fp, fl, &v) != 0) { return -1; }
    s->count_out = (uint16_t)v;

    if (field_at(body, body_len, 6u, &fp, &fl) != 0 || parse_uint(fp, fl, &v) != 0) { return -1; }
    s->occupancy = (int16_t)v;

    if (field_at(body, body_len, 7u, &fp, &fl) != 0 || parse_uint(fp, fl, &v) != 0) { return -1; }
    s->q50_occ_ms = v;

    if (field_at(body, body_len, 8u, &fp, &fl) != 0 || parse_uint(fp, fl, &v) != 0) { return -1; }
    s->q100_occ_ms = v;

    if (field_at(body, body_len, 9u, &fp, &fl) != 0 || parse_uint(fp, fl, &v) != 0) { return -1; }
    s->grade = (uint8_t)v;

    if (field_at(body, body_len, 10u, &fp, &fl) != 0 || parse_uint(fp, fl, &v) != 0) { return -1; }
    s->flags = (uint8_t)v;

    /* 预测字段是后加的：缺省为 0，缺字段时不影响整帧解析 */
    s->pred_occ = 0u;
    s->pred_n   = 0u;
    if (field_at(body, body_len, 11u, &fp, &fl) == 0 &&
        parse_uint(fp, fl, &v) == 0) {
        s->pred_occ = (uint8_t)v;
    }
    if (field_at(body, body_len, 12u, &fp, &fl) == 0 &&
        parse_uint(fp, fl, &v) == 0) {
        s->pred_n = (uint8_t)v;
    }

    /* 探头电平位图也是后加的：缺省 0 */
    s->beams = 0u;
    if (field_at(body, body_len, 13u, &fp, &fl) == 0 &&
        parse_uint(fp, fl, &v) == 0 && v <= 0x0Fu) {
        s->beams = (uint8_t)v;
    }

    /* I2C 扫描结果同为可选字段，缺省 0 */
    s->i2c_seen = 0u;
    if (field_at(body, body_len, 14u, &fp, &fl) == 0 &&
        parse_uint(fp, fl, &v) == 0 && v <= 0x07u) {
        s->i2c_seen = (uint8_t)v;
    }

    return 0;
}

/* ---------------- 下行命令帧 ---------------- */
int qsm_cmd_encode(uint8_t cmd, char *buf, size_t buflen)
{
    if (buf == NULL || buflen < 12u) {
        return -1;
    }

    size_t pos = 0u;
    pos = put_char(buf, buflen, pos, '$');
    pos = put_str (buf, buflen, pos, "QSR,");
    if (pos == 0u) { return -1; }
    pos = put_uint(buf, buflen, pos, cmd, 1u);
    if (pos == 0u) { return -1; }

    uint8_t crc = qsm_crc8((const uint8_t *)&buf[1], pos - 1u);

    pos = put_char(buf, buflen, pos, '*'); if (pos == 0u) { return -1; }
    pos = put_hex8(buf, buflen, pos, crc); if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, '\n'); if (pos == 0u) { return -1; }
    buf[pos] = '\0';
    return (int)pos;
}

int qsm_cmd_parse(const char *line, size_t len, uint8_t *cmd)
{
    if (line == NULL || cmd == NULL || len < 8u) {
        return -1;
    }
    if (line[0] != '$') {
        return -1;
    }

    /* 找 '*' */
    size_t star = 0u;
    for (size_t i = 1u; i < len; i++) {
        if (line[i] == '*') { star = i; break; }
        if (line[i] == '\n' || line[i] == '\r') { break; }
    }
    if (star == 0u || star < 5u) {
        return -1;
    }

    /* CRC 两位（允许只写一位） */
    size_t crc_len = 0u;
    for (size_t i = star + 1u; i < len && crc_len < 2u; i++) {
        if (line[i] == '\n' || line[i] == '\r') { break; }
        crc_len++;
    }
    uint8_t want = 0u;
    if (crc_len == 0u || parse_hex8(&line[star + 1u], crc_len, &want) != 0) {
        return -1;
    }
    if (qsm_crc8((const uint8_t *)&line[1], star - 1u) != want) {
        return -2;
    }

    const char *body = &line[1];
    size_t body_len  = star - 1u;
    const char *fp; size_t fl;

    if (field_at(body, body_len, 0u, &fp, &fl) != 0 || fl != 3u ||
        fp[0] != 'Q' || fp[1] != 'S' || fp[2] != 'R') {
        return -1;
    }
    if (field_at(body, body_len, 1u, &fp, &fl) != 0) {
        return -1;
    }

    uint32_t v = 0u;
    if (parse_uint(fp, fl, &v) != 0 || v < 1u || v > 3u) {
        return -1;
    }
    *cmd = (uint8_t)v;
    return 0;
}

/* ---------------- 校时帧 ---------------- */

/**
 * 校验 "$...*CRC" 结构：成功时把 '*' 的下标写入 *star。
 * @return 0 成功；-1 格式错；-2 CRC 错
 */
static int frame_check(const char *line, size_t len, size_t *star)
{
    size_t  s = 0u;
    size_t  cl = 0u;
    uint8_t want = 0u;

    if (line == NULL || star == NULL || len < 8u || line[0] != '$') {
        return -1;
    }
    for (size_t i = 1u; i < len; i++) {
        if (line[i] == '*') { s = i; break; }
        if (line[i] == '\n' || line[i] == '\r') { break; }
    }
    if (s == 0u || s < 5u) {
        return -1;
    }
    for (size_t i = s + 1u; i < len && cl < 2u; i++) {
        if (line[i] == '\n' || line[i] == '\r') { break; }
        cl++;
    }
    if (cl == 0u || parse_hex8(&line[s + 1u], cl, &want) != 0) {
        return -1;
    }
    if (qsm_crc8((const uint8_t *)&line[1], s - 1u) != want) {
        return -2;
    }
    *star = s;
    return 0;
}

/** "YYYY-MM-DDThh:mm:ss" 的字符位置检查（不校验真实日期） */
static int iso_digits_ok(const char *iso)
{
    for (size_t i = 0u; i < 19u; i++) {
        char c = iso[i];
        if (i == 4u || i == 7u) {
            if (c != '-') { return 0; }
        } else if (i == 10u) {
            if (c != 'T' && c != ' ') { return 0; }
        } else if (i == 13u || i == 16u) {
            if (c != ':') { return 0; }
        } else if (c < '0' || c > '9') {
            return 0;
        }
    }
    return 1;
}

int qsm_time_encode(const char *iso, char *buf, size_t buflen)
{
    size_t  pos = 0u;
    uint8_t crc;

    if (iso == NULL || buf == NULL || buflen < 32u || !iso_digits_ok(iso)) {
        return -1;
    }

    pos = put_char(buf, buflen, pos, '$');
    pos = put_str (buf, buflen, pos, "QST,");
    if (pos == 0u) { return -1; }
    for (size_t i = 0u; i < 19u; i++) {
        pos = put_char(buf, buflen, pos, iso[i]);
        if (pos == 0u) { return -1; }
    }

    crc = qsm_crc8((const uint8_t *)&buf[1], pos - 1u);

    pos = put_char(buf, buflen, pos, '*');  if (pos == 0u) { return -1; }
    pos = put_hex8(buf, buflen, pos, crc);  if (pos == 0u) { return -1; }
    pos = put_char(buf, buflen, pos, '\n'); if (pos == 0u) { return -1; }
    buf[pos] = '\0';

    return (int)pos;
}

int qsm_time_parse(const char *line, size_t len, char *iso, size_t isolen)
{
    size_t       star = 0u;
    int          r;
    const char  *fp;
    size_t       fl;
    const char  *body;
    size_t       body_len;

    if (iso == NULL || isolen < 20u) {
        return -1;
    }

    r = frame_check(line, len, &star);
    if (r != 0) {
        return r;
    }

    body     = &line[1];
    body_len = star - 1u;

    if (field_at(body, body_len, 0u, &fp, &fl) != 0 || fl != 3u ||
        fp[0] != 'Q' || fp[1] != 'S' || fp[2] != 'T') {
        return -1;
    }
    if (field_at(body, body_len, 1u, &fp, &fl) != 0 || fl != 19u) {
        return -1;
    }

    for (size_t i = 0u; i < 19u; i++) {
        iso[i] = fp[i];
    }
    iso[19] = '\0';

    return iso_digits_ok(iso) ? 0 : -1;
}
