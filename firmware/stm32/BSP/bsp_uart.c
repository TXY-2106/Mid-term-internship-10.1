/**
 * @file  bsp_uart.c
 * @brief 串口初始化、发送与 USART2 接收（ESP32 → STM32 的命令）
 *
 * 引脚与角色严格对应 hardware/硬件接线说明.md：
 *   USART2: PA2=TX → ESP32 GPIO16(RX2)
 *           PA3=RX ← ESP32 GPIO17(TX2)     ← 协议帧/命令都走这里
 *   USART1: PA9=TX, PA10=RX → USB-TTL(CH340)  ← 调试输出（可选）
 *
 * 发送改为带上限的轮询：串口异常时不会把主循环卡死。
 * 接收用 RXNE 中断 + 128 B 环形缓冲，主循环只做非阻塞搬运。
 */
#include "bsp_uart.h"
#include "app_config.h"

/* ---------------- 发送：带超时，绝不无限等待 ---------------- */
#define UART_TX_GUARD  200000u   /* 72 MHz 下约几十 ms，远小于任何任务周期 */

static uint8_t uart_putc(USART_TypeDef *u, uint8_t c)
{
    uint32_t guard = UART_TX_GUARD;

    while (USART_GetFlagStatus(u, USART_FLAG_TXE) == RESET) {
        if (--guard == 0u) {
            return 0u;          /* 发送器一直不空：放弃这一帧，别卡住主循环 */
        }
    }
    USART_SendData(u, (uint16_t)c);
    return 1u;
}

static void uart_send_str(USART_TypeDef *u, const char *s)
{
    while (*s != '\0') {
        if (!uart_putc(u, (uint8_t)(*s++))) {
            return;
        }
    }
    uint32_t guard = UART_TX_GUARD;
    while (USART_GetFlagStatus(u, USART_FLAG_TC) == RESET) {
        if (--guard == 0u) {
            return;
        }
    }
}

/* ---------------- 接收：环形缓冲 + 行累积 ---------------- */
#define UART_RX_RING   128u
#define UART_LINE_MAX  80u

static volatile uint8_t  s_ring[UART_RX_RING];
static volatile uint16_t s_head;
static volatile uint16_t s_tail;
static char              s_acc[UART_LINE_MAX];
static uint16_t          s_acc_len;

void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        uint8_t  b    = (uint8_t)USART_ReceiveData(USART2);   /* 读 DR 即清标志 */
        uint16_t next = (uint16_t)((s_head + 1u) % UART_RX_RING);

        if (next != s_tail) {
            s_ring[s_head] = b;
            s_head = next;
        }
    }
}

uint8_t bsp_uart_poll_cmd(char *line, uint16_t len)
{
    if (line == NULL || len == 0u) {
        return 0u;
    }

    while (s_tail != s_head) {
        uint8_t b = s_ring[s_tail];

        s_tail = (uint16_t)((s_tail + 1u) % UART_RX_RING);

        if (b == '\r') {
            continue;
        }
        if (b == '\n') {
            if (s_acc_len > 0u) {
                uint16_t i = 0u;
                while (i + 1u < len && i < s_acc_len) {
                    line[i] = s_acc[i];
                    i++;
                }
                line[i] = '\0';
                s_acc_len = 0u;
                return 1u;
            }
            s_acc_len = 0u;
            continue;
        }

        if (s_acc_len + 1u < sizeof(s_acc)) {
            s_acc[s_acc_len++] = (char)b;
        } else {
            s_acc_len = 0u;     /* 超长行丢弃重来，避免卡在半条命令上 */
        }
    }
    return 0u;
}

/* ---------------- 初始化 ---------------- */
void bsp_uart_init(void)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef  nvic;

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

    /* ---- USART2 接收中断：网页发来的复位命令 ---- */
    s_head    = 0u;
    s_tail    = 0u;
    s_acc_len = 0u;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    nvic.NVIC_IRQChannel                   = USART2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2u;   /* 低于 1 kHz 采样的 SysTick */
    nvic.NVIC_IRQChannelSubPriority        = 0u;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
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
    return 1u;
}
