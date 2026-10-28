/**
 * @file  bsp_uart.c
 * @brief 串口初始化与发送
 *
 * 引脚与角色严格对应 hardware/硬件接线说明.md：
 *   USART2: PA2=TX → ESP32 GPIO16(RX2)
 *           PA3=RX ← ESP32 GPIO17(TX2)     ← 协议帧走这里
 *   USART1: PA9=TX, PA10=RX → USB-TTL(CH340)  ← 调试输出（可选）
 *
 * 注意：接线说明里 USART2 是固定给 ESP32 的，调试口改用空闲的 USART1。
 *       没有 CH340 时把 APP_DEBUG_UART_ENABLE 置 0 即可，不影响采集与落盘。
 */
#include "bsp_uart.h"
#include "app_config.h"

static void uart_send_str(USART_TypeDef *u, const char *s)
{
    while (*s != '\0') {
        while (USART_GetFlagStatus(u, USART_FLAG_TXE) == RESET) { }
        USART_SendData(u, (uint16_t)(uint8_t)(*s++));
    }
    while (USART_GetFlagStatus(u, USART_FLAG_TC) == RESET) { }
}

void bsp_uart_init(void)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* ---- USART2 → ESP32：PA2 复用推挽, PA3 浮空输入 ---- */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Pin   = GPIO_Pin_2;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Pin  = GPIO_Pin_3;
    GPIO_Init(GPIOA, &gpio);

    /* ---- USART1 → 调试（CH340）：PA9 / PA10 ---- */
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Pin  = GPIO_Pin_9;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Pin  = GPIO_Pin_10;
    GPIO_Init(GPIOA, &gpio);

    usart.USART_BaudRate            = APP_UART_BAUD;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART2, &usart);     /* ESP32 链路 */
    USART_Cmd(USART2, ENABLE);

    USART_Init(USART1, &usart);     /* 调试口 */
    USART_Cmd(USART1, ENABLE);
}

void bsp_uart_send(const char *frame)
{
    uart_send_str(USART2, frame);   /* → ESP32 */
}

void bsp_uart_debug(const char *s)
{
#if APP_DEBUG_UART_ENABLE
    uart_send_str(USART1, s);       /* → CH340 */
    uart_send_str(USART1, "\r\n");
#else
    (void)s;
#endif
}

uint8_t bsp_uart_link_ok(void)
{
    /* TODO: 让 ESP32 每秒回一个 'K'，这里用接收超时判断链路。
       骨架阶段恒为 1，不影响采集与落盘。 */
    return 1u;
}
