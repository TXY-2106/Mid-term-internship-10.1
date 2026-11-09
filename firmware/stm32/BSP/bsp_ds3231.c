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

#else  /* APP_RTC_ENABLE == 0：空实现，时间戳字段输出 "-" */

uint8_t bsp_ds3231_init(void) { return 0u; }

uint8_t bsp_ds3231_get_iso8601(char *buf, uint8_t len)
{
    (void)buf;
    (void)len;
    return 0u;
}

#endif /* APP_RTC_ENABLE */
