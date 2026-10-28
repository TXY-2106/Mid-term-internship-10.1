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
 * TODO(在 Keil 工程中补齐)：这里给出标准库调用骨架。
 * 需要在工程里加入 stm32f10x_i2c.c，并保证 PB6/PB7 配置为复用开漏。
 * 注意：F103 的硬件 I2C 存在已知 errata，若出现总线锁死，
 *       可改为软件模拟 I2C（本项目 SD 卡与 RTC 分开挂在不同总线，风险可控）。
 */
static uint8_t i2c_write_reg(uint8_t reg, uint8_t val)
{
    /* TODO: I2C_GenerateSTART / 发送地址 / 发送 reg / 发送 val / I2C_GenerateSTOP */
    (void)reg; (void)val;
    return 0u;   /* 骨架阶段返回失败，接入硬件后改为真实返回值 */
}

static uint8_t i2c_read_regs(uint8_t reg, uint8_t *buf, uint8_t n)
{
    /* TODO: 先写寄存器地址，再 restart 读 n 字节 */
    (void)reg; (void)buf; (void)n;
    return 0u;
}

uint8_t bsp_ds3231_init(void)
{
    GPIO_InitTypeDef  gpio;
    I2C_InitTypeDef   i2c;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode  = GPIO_Mode_AF_OD;          /* 复用开漏 */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    i2c.I2C_Mode                = I2C_Mode_I2C;
    i2c.I2C_DutyCycle           = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1         = 0x00;
    i2c.I2C_Ack                 = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    i2c.I2C_ClockSpeed          = 400000;
    I2C_Init(I2C1, &i2c);
    I2C_Cmd(I2C1, ENABLE);

    /* 探测：读状态寄存器，能读通即认为器件在线 */
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
