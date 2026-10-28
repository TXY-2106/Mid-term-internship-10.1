/**
 * @file  protocol.h
 * @brief STM32 → ESP32 串口帧编解码 + CRC8
 *
 * 帧格式（单行 ASCII，115200 8N1）：
 *   $QSM,<seq>,<ms>,<datetime>,<in>,<out>,<occ>,<q50_ms>,<q100_ms>,<grade>,<flags>*<CRC8>\n
 *
 * 示例：
 *   $QSM,1024,3735000,2026-10-28T12:31:04,37,29,8,6100,0,2,3*5E
 *
 * CRC8：多项式 0x07，初值 0x00，覆盖 '$' 之后到 '*' 之前（不含两端）。
 * 选择文本帧而非二进制，是为了能用任意串口助手肉眼调试。
 */
#ifndef QSM_PROTOCOL_H
#define QSM_PROTOCOL_H

#include "qsm_types.h"

/** CRC-8/ATM（poly 0x07, init 0x00, 不反射） */
uint8_t qsm_crc8(const uint8_t *data, size_t len);

/**
 * 编码一帧到 buf。
 * @param dt  形如 "2026-10-28T12:31:04"；传 NULL 时写 "-"（RTC 未就绪）
 * @return 写入的字符数（不含结尾 '\0'），<0 表示缓冲区不足。
 */
int qsm_frame_encode(const qsm_state_t *s,
                     uint32_t seq,
                     const char *dt,
                     char *buf,
                     size_t buflen);

/**
 * 解析一行（可含或不含结尾 '\n'）。
 * @return 0 成功；-1 格式错误；-2 CRC 校验失败。
 */
int qsm_frame_parse(const char *line,
                    qsm_state_t *s,
                    uint32_t *seq,
                    char *dt,
                    size_t dtlen);

#endif /* QSM_PROTOCOL_H */
