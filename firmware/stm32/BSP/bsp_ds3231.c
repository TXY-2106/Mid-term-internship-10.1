/**
 * @file  bsp_ds3231.c
 * @brief DS3231 读取与 ISO-8601 格式化
 */
#include "bsp_ds3231.h"
#include "app_config.h"

#if APP_RTC_ENABLE

/* 极简 BCD 转换与两位补零，避免引入 printf */
static uint8_t bcd_to_bin(uint8_t v) { return (uint8_t)(((v >> 4) * 10u) + (v & 0x0Fu)); }

static void put2(char *buf, uint8_t *pos, uint8_t v)
{
    buf[(*pos)++] = (char)('0' + (v / 10u));
    buf[(*pos)++] = (char)('0' + (v % 10u));
}

static void put4(char *buf, uint8_t *pos, uint16_t v)
{
    buf[(*pos)++] = (char)('0' + (v / 1000u));
    buf[(*pos)++] = (char)('0' + ((v / 100u) % 10u));
    buf[(*pos)++] = (char)('0' + ((v / 10u) % 10u));
    buf[(*pos)++] = (char)('0' + (v % 10u));
}

/* ---------------- I2C1 底层 ----------------
 * PB6 = SCL, PB7 = SDA（复用开漏，模块自带 4.7 kΩ 上拉时也可用内部弱上拉）。
 * 所有等待都带超时：总线被拉死时返回失败而不是卡住主循环。
 * F103 硬件 I2C 有已知 errata，因此在 init 前先做一次"9 个时钟"的
 * 总线恢复（把从机从半途状态打回空闲），再复位 I2C 外设。
 */
#define I2C_TIMEOUT   0x00040000u

static uint8_t i2c_wait_event(uint32_t event)
{
    uint32_t to = I2C_TIMEOUT;

    while (I2C_CheckEvent(I2C1, event) == ERROR) {
        if (--to == 0u) {
            return 0u;
        }
    }
    return 1u;
}

/** 总线恢复：SDA 被从机拉低时，手动发 9 个 SCL 脉冲 + 一个 STOP */
static void i2c_bus_recover(void)
{
    GPIO_InitTypeDef gpio;
    uint8_t i;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode  = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    GPIO_SetBits(GPIOB, GPIO_Pin_6 | GPIO_Pin_7);
    for (i = 0u; i < 9u; i++) {
        GPIO_ResetBits(GPIOB, GPIO_Pin_6);
        GPIO_SetBits(GPIOB, GPIO_Pin_6);
    }
    /* 制造一个 STOP：SCL 高时 SDA 由低变高 */
    GPIO_ResetBits(GPIOB, GPIO_Pin_7);
    GPIO_SetBits(GPIOB, GPIO_Pin_6);
    GPIO_SetBits(GPIOB, GPIO_Pin_7);
}

static uint8_t i2c_send_addr(uint8_t addr)
{
    I2C_Send7bitAddress(I2C1, addr, I2C_Direction_Transmitter);
    return i2c_wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED);
}

static uint8_t i2c_send_byte(uint8_t v)
{
    I2C_SendData(I2C1, v);
    return i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED);
}

static uint8_t i2c_write_reg(uint8_t reg, uint8_t val)
{
    I2C_GenerateSTART(I2C1, ENABLE);
    if (!i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT)) { return 0u; }
    if (!i2c_send_addr((uint8_t)(DS3231_I2C_ADDR << 1))) { return 0u; }
    if (!i2c_send_byte(reg)) { return 0u; }
    if (!i2c_send_byte(val)) { return 0u; }

    I2C_GenerateSTOP(I2C1, ENABLE);
    return 1u;
}

static uint8_t i2c_read_regs(uint8_t reg, uint8_t *buf, uint8_t n)
{
    uint8_t i;

    if (buf == 0 || n == 0u || n > 16u) {
        return 0u;
    }

    /* 1) 写寄存器地址（不 STOP，直接 restart） */
    I2C_GenerateSTART(I2C1, ENABLE);
    if (!i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT)) { return 0u; }
    if (!i2c_send_addr((uint8_t)(DS3231_I2C_ADDR << 1))) { return 0u; }
    if (!i2c_send_byte(reg)) { return 0u; }

    /* 2) 重复起始 + 读地址 */
    I2C_GenerateSTART(I2C1, ENABLE);
    if (!i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT)) { return 0u; }

    I2C_AcknowledgeConfig(I2C1, ENABLE);
    I2C_Send7bitAddress(I2C1, (uint8_t)(DS3231_I2C_ADDR << 1), I2C_Direction_Receiver);
    if (!i2c_wait_event(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)) { return 0u; }

    /* 3) 读 n 字节：最后一个字节前给 NACK */
    for (i = 0u; i < n; i++) {
        if ((uint8_t)(i + 1u) == n) {
            I2C_AcknowledgeConfig(I2C1, DISABLE);
        }
        if (!i2c_wait_event(I2C_EVENT_MASTER_BYTE_RECEIVED)) {
            I2C_AcknowledgeConfig(I2C1, ENABLE);
            return 0u;
        }
        buf[i] = I2C_ReceiveData(I2C1);
    }

    I2C_GenerateSTOP(I2C1, ENABLE);
    I2C_AcknowledgeConfig(I2C1, ENABLE);   /* 恢复默认，供下次使用 */
    return 1u;
}

uint8_t bsp_ds3231_init(void)
{
    GPIO_InitTypeDef  gpio;
    I2C_InitTypeDef   i2c;

    /* 先恢复总线，再复位 I2C 外设，避免上一次运行留下的半途状态 */
    i2c_bus_recover();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode  = GPIO_Mode_AF_OD;          /* 复用开漏 */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    I2C_SoftwareResetCmd(I2C1, ENABLE);
    I2C_SoftwareResetCmd(I2C1, DISABLE);

    i2c.I2C_Mode                = I2C_Mode_I2C;
    i2c.I2C_DutyCycle           = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1         = 0x00;
    i2c.I2C_Ack                 = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    i2c.I2C_ClockSpeed          = 100000;       /* 100 kHz，长排线更稳 */
    I2C_Init(I2C1, &i2c);
    I2C_Cmd(I2C1, ENABLE);

    /* 探测：读状态寄存器 0x0F，能读通即认为器件在线 */
    uint8_t st = 0u;
    return i2c_read_regs(0x0Fu, &st, 1u);
}

uint8_t bsp_ds3231_is_valid(void)
{
    uint8_t raw[7];
    uint8_t status = 0u;
    uint8_t year;
    uint8_t month;
    uint8_t day;

    if (!i2c_read_regs(0x00u, raw, 7u)) {
        return 0u;
    }
    /* OSF（0x0F 的 bit7）置位 = 曾经掉电停振，时间不可信 */
    if (!i2c_read_regs(0x0Fu, &status, 1u)) {
        return 0u;
    }
    if ((status & 0x80u) != 0u) {
        return 0u;
    }

    year  = bcd_to_bin(raw[6]);
    month = bcd_to_bin(raw[5] & 0x1Fu);
    day   = bcd_to_bin(raw[4] & 0x3Fu);

    if (year < 20u) {                   /* 2000–2019：从未校时 */
        return 0u;
    }
    if (month < 1u || month > 12u) {
        return 0u;
    }
    if (day < 1u || day > 31u) {
        return 0u;
    }
    return 1u;
}

uint8_t bsp_ds3231_get_iso8601(char *buf, uint8_t len)
{
    uint8_t raw[7];

    if (buf == 0 || len < 20u) {
        return 0u;
    }
    if (!i2c_read_regs(0x00u, raw, 7u)) {       /* 秒 分 时 星期 日 月 年 */
        return 0u;
    }

    uint8_t sec  = bcd_to_bin(raw[0] & 0x7Fu);
    uint8_t min  = bcd_to_bin(raw[1]);
    uint8_t hour = bcd_to_bin(raw[2] & 0x3Fu);  /* 忽略 12/24 小时位 */
    uint8_t day  = bcd_to_bin(raw[4]);
    uint8_t mon  = bcd_to_bin(raw[5] & 0x1Fu);
    uint16_t yr  = (uint16_t)(2000u + bcd_to_bin(raw[6]));

    uint8_t p = 0u;
    put4(buf, &p, yr);   buf[p++] = '-';
    put2(buf, &p, mon);  buf[p++] = '-';
    put2(buf, &p, day);  buf[p++] = 'T';
    put2(buf, &p, hour); buf[p++] = ':';
    put2(buf, &p, min);  buf[p++] = ':';
    put2(buf, &p, sec);
    buf[p] = '\0';
    (void)sec;

    return (uint8_t)(p == 19u);
}

/* ---------------- 校时（网页一键写入） ---------------- */

static uint8_t bin_to_bcd(uint8_t v) { return (uint8_t)(((v / 10u) << 4) | (v % 10u)); }

/** 0 = 周日 … 6 = 周六（Sakamoto） */
static uint8_t weekday_from_ymd(uint16_t y, uint8_t m, uint8_t d)
{
    static const uint8_t t[12] = { 0u, 3u, 2u, 5u, 0u, 3u, 5u, 1u, 4u, 6u, 2u, 4u };
    uint16_t yy = (m < 3u) ? (uint16_t)(y - 1u) : y;
    uint16_t r = (uint16_t)(yy + yy / 4u - yy / 100u + yy / 400u + t[m - 1u] + d);

    return (uint8_t)(r % 7u);
}

static uint8_t parse_two(const char *s)
{
    return (uint8_t)(((uint8_t)(s[0] - '0')) * 10u + (uint8_t)(s[1] - '0'));
}

uint8_t bsp_ds3231_set_iso8601(const char *iso)
{
    uint16_t y;
    uint8_t  mo, d, h, mi, sec, wd;
    uint8_t  raw[7];
    uint8_t  st = 0u;
    uint8_t  i;

    if (iso == NULL) { return 0u; }

    /* 只接受 YYYY-MM-DDThh:mm:ss，逐位校验，避免把脏数据写进芯片 */
    for (i = 0u; i < 19u; i++) {
        char c = iso[i];
        if (i == 4u || i == 7u) {
            if (c != '-') { return 0u; }
        } else if (i == 10u) {
            if (c != 'T' && c != ' ') { return 0u; }
        } else if (i == 13u || i == 16u) {
            if (c != ':') { return 0u; }
        } else if (c < '0' || c > '9') {
            return 0u;
        }
    }

    y   = (uint16_t)(parse_two(&iso[0]) * 100u + parse_two(&iso[2]));
    mo  = parse_two(&iso[5]);
    d   = parse_two(&iso[8]);
    h   = parse_two(&iso[11]);
    mi  = parse_two(&iso[14]);
    sec = parse_two(&iso[17]);

    if (y < 2020u || y > 2099u || mo < 1u || mo > 12u || d < 1u || d > 31u ||
        h > 23u || mi > 59u || sec > 59u) {
        return 0u;
    }

    wd = (uint8_t)(weekday_from_ymd(y, mo, d) + 1u);   /* DS3231: 1=周日 */

    raw[0] = bin_to_bcd(sec);
    raw[1] = bin_to_bcd(mi);
    raw[2] = bin_to_bcd(h);
    raw[3] = wd;
    raw[4] = bin_to_bcd(d);
    raw[5] = bin_to_bcd(mo);
    raw[6] = bin_to_bcd((uint8_t)(y % 100u));

    for (i = 0u; i < 7u; i++) {
        if (!i2c_write_reg(i, raw[i])) { return 0u; }
    }

    /* 清 OSF（状态寄存器 bit7）：芯片停振过会被置位，不清的话某些模块会继续"冻结" */
    if (i2c_read_regs(0x0Fu, &st, 1u)) {
        st &= (uint8_t)~0x80u;
        (void)i2c_write_reg(0x0Fu, st);
    }

    return 1u;
}

/* ---------------- 总线扫描（排查接线用） ----------------
 * 这个扫描在 task_status() 里每 10 s 跑一次，**必须严格限时**。
 * 如果总线被拉死（SDA/SCL 短路、模块没供电把线钳住），
 * I2C_TIMEOUT(0x40000) 每次等待要几百毫秒，112 个地址就是几十秒，
 * 会把主循环整个冻住 → 网页上表现为"收不到任何数据"。
 * 所以这里：单次探测用短超时，整轮扫描再用总预算封顶。
 */
#define I2C_PROBE_TO    500u      /* 单次等待上限，约 0.3 ms */
#define I2C_SCAN_BUDGET 20000u    /* 整轮扫描总预算，约 10-20 ms */

/** 带总预算的等待：预算耗尽就放弃，保证扫描不会拖死主循环 */
static uint8_t i2c_wait_event_to(uint32_t event, uint32_t *budget)
{
    uint32_t to = I2C_PROBE_TO;

    while (I2C_CheckEvent(I2C1, event) == ERROR) {
        if (--to == 0u || *budget == 0u) {
            return 0u;
        }
        (*budget)--;
    }
    return 1u;
}

/** 探测一个 7 位地址是否有人应答：只发地址、不发数据 */
static uint8_t i2c_probe(uint8_t addr, uint32_t *budget)
{
    uint32_t to = I2C_PROBE_TO;

    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) != RESET) {
        if (--to == 0u || *budget == 0u) {
            return 0u;
        }
        (*budget)--;
    }

    I2C_GenerateSTART(I2C1, ENABLE);
    if (!i2c_wait_event_to(I2C_EVENT_MASTER_MODE_SELECT, budget)) {
        I2C_GenerateSTOP(I2C1, ENABLE);
        return 0u;
    }

    I2C_Send7bitAddress(I2C1, (uint8_t)(addr << 1), I2C_Direction_Transmitter);

    to = I2C_PROBE_TO;
    while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == ERROR) {
        if (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) != RESET) {   /* 从机 NACK */
            I2C_ClearFlag(I2C1, I2C_FLAG_AF);
            I2C_GenerateSTOP(I2C1, ENABLE);
            return 0u;
        }
        if (--to == 0u || *budget == 0u) {
            I2C_ClearFlag(I2C1, I2C_FLAG_AF);
            I2C_GenerateSTOP(I2C1, ENABLE);
            return 0u;
        }
        (*budget)--;
    }

    I2C_GenerateSTOP(I2C1, ENABLE);
    return 1u;
}

uint8_t bsp_ds3231_scan(uint8_t *addrs, uint8_t max_found)
{
    static const uint8_t first[] = { 0x68u, 0x57u, 0x50u, 0x69u, 0x76u, 0x77u };
    uint32_t budget = I2C_SCAN_BUDGET;
    uint8_t  n = 0u;
    uint8_t  i;
    uint8_t  a;

    if (addrs == 0 || max_found == 0u) { return 0u; }

    /* 先探最可能的地址：命中 0x68 立刻返回（正常情况只有零点几毫秒） */
    for (i = 0u; i < (uint8_t)sizeof(first); i++) {
        if (i2c_probe(first[i], &budget)) {
            if (n < max_found) { addrs[n] = first[i]; }
            n++;
            if (first[i] == 0x68u) { return n; }
        }
        if (budget == 0u) { return (n > max_found) ? max_found : n; }
    }

    /* 常见地址都没应答时，再做一次全地址扫描（预算封顶） */
    for (a = 0x08u; a <= 0x77u; a++) {
        if (budget == 0u) { break; }
        if (i2c_probe(a, &budget)) {
            if (n < max_found) { addrs[n] = a; }
            n++;
        }
    }

    return (n > max_found) ? max_found : n;
}

#else  /* APP_RTC_ENABLE == 0：空实现，时间戳字段输出 "-" */

uint8_t bsp_ds3231_init(void) { return 0u; }

uint8_t bsp_ds3231_is_valid(void) { return 0u; }

uint8_t bsp_ds3231_scan(uint8_t *addrs, uint8_t max_found)
{
    (void)addrs;
    (void)max_found;
    return 0u;
}

uint8_t bsp_ds3231_get_iso8601(char *buf, uint8_t len)
{
    (void)buf;
    (void)len;
    return 0u;
}

uint8_t bsp_ds3231_set_iso8601(const char *iso)
{
    (void)iso;
    return 0u;
}

#endif /* APP_RTC_ENABLE */
