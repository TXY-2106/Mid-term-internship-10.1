/**
 * @file  protocol.h
 * @brief STM32 → ESP32 串口帧编解码 + CRC8
 *
 * 帧格式（单行 ASCII，115200 8N1）：
 *   $QSM,<seq>,<ms>,<datetime>,<in>,<out>,<occ>,<q50_ms>,<q100_ms>,<grade>,<flags>,<pred>,<pred_n>,<beams>,<i2c>*<CRC8>\n
 *
 *   pred   —— 预测排队人数（近 7 天同一时段的平均在场人数）
 *   pred_n —— 参与平均的天数；0 表示历史不足，此时 pred 为 0
 *   beams  —— 四路探头当前电平位图（bit0=入口A, bit1=入口B,
 *             bit2=队列50%, bit3=队列100%）；1 = 遮挡（触发）
 *   i2c    —— I2C 总线扫描结果：bit0=0x68 有应答, bit1=0x57 有应答,
 *             bit2=其他地址有应答；0 = 总线上没有任何器件应答
 *   最后四个字段是后加的，解析端缺省按 0 处理，因此老帧仍能解析。
 *
 *   <CRC8> —— CRC-8/ATM(poly 0x07, init 0x00)，固定 2 位大写十六进制
 *
 * 示例：
 *   $QSM,1024,3735000,2026-10-28T12:31:04,37,29,8,6100,0,2,3,9,4,5,1*5A
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

/* ---------------- 下行命令帧（ESP32 → STM32） ----------------
 * 帧格式：$QSR,<cmd>*<CRC8>\n
 *   cmd = 1 只清零计数与分级；2 只清空历史；3 两者都做
 * CRC 规则与上行帧一致（覆盖 '$' 后到 '*' 前）。
 */
#define QSM_CMD_RESET_COUNTS   1u
#define QSM_CMD_RESET_HISTORY  2u
#define QSM_CMD_RESET_ALL      3u

/** 编码一条命令到 buf，返回写入字符数（含结尾 '\n'），<0 表示失败 */
int qsm_cmd_encode(uint8_t cmd, char *buf, size_t buflen);

/** 解析一条命令；@return 0 成功，-1 格式错，-2 CRC 错 */
int qsm_cmd_parse(const char *line, size_t len, uint8_t *cmd);

#endif /* QSM_PROTOCOL_H */
