/**
 * @file  serial_link.h
 * @brief 解析 STM32 上报的 $QSM 文本帧
 *
 * 帧格式（见 firmware/core/protocol.h）：
 *   $QSM,<seq>,<ms>,<datetime>,<in>,<out>,<occ>,<q50_ms>,<q100_ms>,<grade>,<flags>,<pred>,<pred_n>,<beams>,<i2c>*<CRC8>\n
 *
 * pred/pred_n/beams/i2c 都是后加的字段，老固件缺失时按 0 处理，不影响解析。
 *
 * 注意：CRC 算法必须与 firmware/core/protocol.c 中的 qsm_crc8 完全一致
 *       （CRC-8/ATM：poly 0x07，init 0x00，不反射）。两边改动需同步。
 */
#ifndef QSM_SERIAL_LINK_H
#define QSM_SERIAL_LINK_H

#include <Arduino.h>

struct QsmState {
    bool     valid;
    uint32_t seq;
    uint32_t uptimeMs;
    char     datetime[24];
    uint16_t countIn;
    uint16_t countOut;
    int16_t  occupancy;
    uint32_t q50Ms;
    uint32_t q100Ms;
    uint8_t  grade;      // 0=较少 1=约半 2=全满
    uint8_t  flags;
    uint8_t  predOcc;    // 预测排队人数（近 7 天同时段平均）
    uint8_t  predN;      // 参与平均的天数，0=历史不足
    uint8_t  beams;      // 四路探头电平位图：bit0=入口A bit1=入口B bit2=50% bit3=100%
    uint8_t  i2cSeen;    // I2C 扫描：bit0=0x68应答 bit1=0x57应答 bit2=其他器件
};

void        link_begin(uint32_t baud, int rxPin, int txPin);

/** 非阻塞读取；有新帧返回 true 并填充 out */
bool        link_poll(QsmState *out);

/** 累计收到的原始字节数：>0 但没有好帧 = 接线通但波特率/解析有问题；
 *  一直为 0 = STM32 没在发（供电、TX/RX 接反、没共地）。 */
uint32_t    link_rxBytes(void);

/** 累计收到的坏帧数（CRC 失败或格式错误），用于判断链路质量 */
uint32_t    link_badFrames(void);

/** 累计收到的好帧数 */
uint32_t    link_goodFrames(void);

/** 距上一帧正常帧的毫秒数（用于判断 STM32 是否掉线） */
uint32_t    link_ageMs(void);

/**
 * 向 STM32 下发命令帧 $QSR,<cmd>*<CRC8>\n
 *   cmd = 1 清零计数；2 清空历史；3 两者都做
 * @return true = 已写入串口
 */
bool        link_send_cmd(uint8_t cmd);

/**
 * 把 "YYYY-MM-DDThh:mm:ss"（19 字符）写入 STM32 的 DS3231：
 * 发 $QST,<iso>*<CRC8>\n，STM32 收到后写芯片并清 OSF。
 * @return true = 已写入串口
 */
bool        link_send_time(const char *iso);

#endif /* QSM_SERIAL_LINK_H */
