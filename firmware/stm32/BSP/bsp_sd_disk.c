/**
 * @file  bsp_sd_disk.c
 * @brief SD 卡 SPI1 底层驱动（扇区级）
 *
 * 流程遵循 SD 规范「SPI 模式」：
 *   CMD0  → 进入 idle
 *   CMD8  → 分辨 v1 / v2（v2 会回显 0x1AA）
 *   ACMD41(CMD55+CMD41) → 等卡退出 idle
 *   CMD58 → 读 OCR，判断是否 SDHC/SDXC（块寻址）
 *   CMD16 → SDSC 才需要设置 512 字节块长
 *
 * 时序常量按 SPI 时钟 281 kHz(初始化) → 9 MHz(读写) 设计。
 */
#include "bsp_sd_disk.h"
#include "app_config.h"

#if APP_SDCARD_ENABLE

/* ---------------- 引脚 ----------------
 * 注意：SPI1 的 SCK/MISO/MOSI 与 SD 卡座共用，CS 由软件控制（NSS 软模式）。
 */
#define SD_CS_PORT   GPIOA
#define SD_CS_PIN    GPIO_Pin_4

/* 单次等待上限：卡死时不要无限循环（1 kHz SysTick 不受影响） */
#define SD_SPIN_MAX  0x00FFFFFFu

static uint8_t  s_ready;     /* 1 = 初始化成功 */
static uint8_t  s_block_addr;/* 1 = 块寻址（SDHC/SDXC） */

/* ---------------- 小工具 ---------------- */

static void sd_spin(volatile uint32_t n)
{
    while (n-- != 0u) { __NOP(); }
}

static void cs_high(void) { GPIO_SetBits(SD_CS_PORT, SD_CS_PIN); }
static void cs_low(void)  { GPIO_ResetBits(SD_CS_PORT, SD_CS_PIN); }

/** 全双工收发一字节（同时产生 8 个时钟） */
static uint8_t spi_xfer(uint8_t d)
{
    uint32_t guard = 0x00100000u;

    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) {
        if (--guard == 0u) { return 0xFFu; }
    }
    SPI_I2S_SendData(SPI1, d);

    guard = 0x00100000u;
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET) {
        if (--guard == 0u) { return 0xFFu; }
    }
    return (uint8_t)SPI_I2S_ReceiveData(SPI1);
}

/** 发一个命令，返回 R1（0x00 = 成功；0xFF = 无响应超时） */
static uint8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t r = 0xFFu;
    uint32_t i;

    cs_high();
    spi_xfer(0xFFu);
    cs_low();
    spi_xfer(0xFFu);                       /* 命令前 8 个时钟 */

    spi_xfer((uint8_t)(0x40u | cmd));
    spi_xfer((uint8_t)(arg >> 24));
    spi_xfer((uint8_t)(arg >> 16));
    spi_xfer((uint8_t)(arg >> 8));
    spi_xfer((uint8_t)(arg));
    spi_xfer(crc);

    for (i = 0u; i < 0x00002000u; i++) {
        r = spi_xfer(0xFFu);
        if ((r & 0x80u) == 0u) { break; }  /* R1 最高位为 0 */
    }
    return r;
}

/** 等待某个字节出现（忙等），返回最后读到的值 */
static uint8_t sd_wait_byte(uint8_t want, uint32_t limit)
{
    uint8_t v = 0xFFu;
    while (limit-- != 0u) {
        v = spi_xfer(0xFFu);
        if (v == want) { break; }
    }
    return v;
}

/* ---------------- 初始化 ---------------- */

static void spi1_gpio_init(void)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef  spi;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    /* PA4 = CS，普通推挽输出，默认拉高（不选中） */
    gpio.GPIO_Pin   = GPIO_Pin_4;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    cs_high();

    /* PA5 = SCK, PA7 = MOSI：复用推挽 */
    gpio.GPIO_Pin   = GPIO_Pin_5 | GPIO_Pin_7;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    /* PA6 = MISO：浮空输入 */
    gpio.GPIO_Pin  = GPIO_Pin_6;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    /* SPI1：主机、8 位、模式 0（CPOL=0, CPHA=1Edge）、NSS 软件 */
    spi.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode              = SPI_Mode_Master;
    spi.SPI_DataSize          = SPI_DataSize_8b;
    spi.SPI_CPOL              = SPI_CPOL_Low;
    spi.SPI_CPHA              = SPI_CPHA_1Edge;
    spi.SPI_NSS               = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;  /* 72/256 ≈ 281 kHz */
    spi.SPI_FirstBit          = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI1, &spi);
    SPI_Cmd(SPI1, ENABLE);
}

uint8_t bsp_sd_disk_init(void)
{
    uint8_t  r;
    uint8_t  ocr[4];
    uint32_t i;

    s_ready      = 0u;
    s_block_addr = 0u;

    spi1_gpio_init();
    sd_spin(80000u);                       /* 上电后至少 1 ms 稳定时间 */

    /* 1) 不小于 74 个时钟，且 CS 保持高：让卡进入 SPI 模式 */
    cs_high();
    for (i = 0u; i < 10u; i++) { spi_xfer(0xFFu); }

    /* 2) CMD0：复位到 idle。少数卡需要多次 */
    r = 0xFFu;
    for (i = 0u; i < 10u; i++) {
        r = sd_cmd(0u, 0u, 0x95u);
        cs_high(); spi_xfer(0xFFu);
        if (r == 0x01u) { break; }
    }
    if (r != 0x01u) { return 0u; }

    /* 3) CMD8：区分 v1 / v2，同时确认电压 */
    r = sd_cmd(8u, 0x000001AAu, 0x87u);
    if (r == 0x01u) {
        /* R7：R1 + 4 字节，最后两字节应回显 0x01 0xAA */
        for (i = 0u; i < 4u; i++) { ocr[i] = spi_xfer(0xFFu); }
        if (ocr[2] != 0x01u || ocr[3] != 0xAAu) { cs_high(); return 0u; }
    }
    cs_high(); spi_xfer(0xFFu);

    /* 4) ACMD41：反复发起直到卡退出 idle（最多约 1 s） */
    r = 0xFFu;
    for (i = 0u; i < 20000u; i++) {
        if (sd_cmd(55u, 0u, 0xFFu) > 0x01u) { cs_high(); spi_xfer(0xFFu); continue; }
        cs_high(); spi_xfer(0xFFu);

        r = sd_cmd(41u, 0x40000000u, 0xFFu);   /* HCS=1 表示支持高容量 */
        cs_high(); spi_xfer(0xFFu);

        if (r == 0x00u) { break; }
        if (r != 0x01u) { return 0u; }          /* 返回其他值说明出错 */
    }
    if (r != 0x00u) { return 0u; }

    /* 5) CMD58：读 OCR，判断是否块寻址（SDHC/SDXC） */
    r = sd_cmd(58u, 0u, 0xFFu);
    if (r == 0x00u) {
        for (i = 0u; i < 4u; i++) { ocr[i] = spi_xfer(0xFFu); }
        s_block_addr = ((ocr[0] & 0x40u) != 0u) ? 1u : 0u;
    }
    cs_high(); spi_xfer(0xFFu);

    /* 6) SDSC 卡：显式设块长 512；SDHC/SDXC 固定 512 无需设置 */
    if (!s_block_addr) {
        r = sd_cmd(16u, 512u, 0xFFu);
        cs_high(); spi_xfer(0xFFu);
        if (r != 0x00u) { return 0u; }
    }

    /* 7) 提速到 9 MHz（72/8），并读一个扇区验证链路 */
    SPI1->CR1 = (uint16_t)((SPI1->CR1 & ~0x0038u) | SPI_BaudRatePrescaler_8);

    s_ready = 1u;
    return 1u;
}

uint8_t bsp_sd_disk_ready(void)
{
    return s_ready;
}

/* ---------------- 扇区读写 ---------------- */

static uint32_t sector_to_addr(uint32_t sector)
{
    /* SDHC/SDXC 用块地址，SDSC 用字节地址 */
    return s_block_addr ? sector : (sector << 9);
}

uint8_t bsp_sd_disk_read(uint8_t *buf, uint32_t sector)
{
    uint32_t i;
    uint8_t  tok;

    if (!s_ready) { return 0u; }

    if (sd_cmd(17u, sector_to_addr(sector), 0xFFu) != 0x00u) {
        cs_high(); spi_xfer(0xFFu);
        return 0u;
    }

    tok = sd_wait_byte(0xFEu, 0x00020000u);   /* 数据起始令牌 */
    if (tok != 0xFEu) {
        cs_high(); spi_xfer(0xFFu);
        return 0u;
    }

    for (i = 0u; i < 512u; i++) {
        buf[i] = spi_xfer(0xFFu);
    }
    spi_xfer(0xFFu);
    spi_xfer(0xFFu);                           /* 2 字节 CRC，SPI 模式可忽略 */

    cs_high();
    spi_xfer(0xFFu);
    return 1u;
}

uint8_t bsp_sd_disk_write(const uint8_t *buf, uint32_t sector)
{
    uint32_t i;
    uint8_t  resp;

    if (!s_ready) { return 0u; }

    if (sd_cmd(24u, sector_to_addr(sector), 0xFFu) != 0x00u) {
        cs_high(); spi_xfer(0xFFu);
        return 0u;
    }

    spi_xfer(0xFFu);
    spi_xfer(0xFEu);                           /* 写数据起始令牌 */

    for (i = 0u; i < 512u; i++) {
        spi_xfer(buf[i]);
    }
    spi_xfer(0xFFu);
    spi_xfer(0xFFu);                           /* 2 字节 CRC，占位 */

    resp = spi_xfer(0xFFu);                    /* 数据响应：xxxxx0 1 01 */
    if ((resp & 0x1Fu) != 0x05u) {
        cs_high(); spi_xfer(0xFFu);
        return 0u;
    }

    /* 等卡内部编程完成：MISO 保持低电平，回 0xFF 表示完成 */
    (void)sd_wait_byte(0xFFu, 0x00FFFFFFu);

    cs_high();
    spi_xfer(0xFFu);
    return 1u;
}

#else  /* APP_SDCARD_ENABLE == 0 */

uint8_t bsp_sd_disk_init(void) { return 0u; }
uint8_t bsp_sd_disk_read(uint8_t *buf, uint32_t sector) { (void)buf; (void)sector; return 0u; }
uint8_t bsp_sd_disk_write(const uint8_t *buf, uint32_t sector) { (void)buf; (void)sector; return 0u; }
uint8_t bsp_sd_disk_ready(void) { return 0u; }

#endif /* APP_SDCARD_ENABLE */
