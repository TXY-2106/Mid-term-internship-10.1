/**
 * @file  serial_link.h
 * @brief 解析 STM32 上报的 $QSM 文本帧
 *
 * 帧格式（见 firmware/core/protocol.h）：
 *   $QSM,<seq>,<ms>,<datetime>,<in>,<out>,<occ>,<q50_ms>,<q100_ms>,<grade>,<flags>*<CRC8>\n
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
};

void        link_begin(uint32_t baud, int rxPin, int txPin);

/** 非阻塞读取；有新帧返回 true 并填充 out */
bool        link_poll(QsmState *out);

/** 累计收到的坏帧数（CRC 失败或格式错误），用于判断链路质量 */
uint32_t    link_badFrames(void);

/** 累计收到的好帧数 */
uint32_t    link_goodFrames(void);

/** 距上一帧正常帧的毫秒数（用于判断 STM32 是否掉线） */
uint32_t    link_ageMs(void);

#endif /* QSM_SERIAL_LINK_H */
