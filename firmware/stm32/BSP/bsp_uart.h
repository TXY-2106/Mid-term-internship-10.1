/**
 * @file  bsp_uart.h
 * @brief USART2 → ESP32（协议帧）、USART1 → CH340（调试日志）
 *
 * 引脚以 hardware/硬件接线说明.md 为准：
 *   PA2/PA3 = USART2，接 ESP32 GPIO16/GPIO17
 *   PA9/PA10 = USART1，接 USB-TTL(CH340)，可选
 */
#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>

void bsp_uart_init(void);

/** 发送一帧（含结尾 '\n'）到 ESP32（USART2）。帧长 < 64 B，115200 下约 5 ms。 */
void bsp_uart_send(const char *frame);

/** 调试串口（USART1）输出一行。APP_DEBUG_UART_ENABLE=0 时为空操作。 */
void bsp_uart_debug(const char *s);

/** 粗略判断 ESP32 是否还在（收到其心跳/无校验错误）。骨架阶段恒返回 1。 */
uint8_t bsp_uart_link_ok(void);

/**
 * 取一条来自 ESP32 的完整命令行（以 '\n' 结束）。
 * 由 USART2 接收中断累积，主循环轮询调用，不阻塞。
 * @param line 输出缓冲；@param len 缓冲长度
 * @return 1 = 取到一行；0 = 暂无完整行
 */
uint8_t bsp_uart_poll_cmd(char *line, uint16_t len);

#endif /* BSP_UART_H */
