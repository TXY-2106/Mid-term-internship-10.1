/**
 * @file  bsp_e18.c
 * @brief 1 kHz 采样 + 移位寄存器去抖实现
 */
#include "bsp_e18.h"
#include "app_config.h"

/* 环形队列容量取 2 的幂，便于用掩码回绕 */
#define E18_RING_SIZE 32u
#define E18_RING_MASK (E18_RING_SIZE - 1u)

static uint8_t    s_hist[QSM_BEAM_COUNT];    /* 位历史，低位为最新 */
static uint8_t    s_level[QSM_BEAM_COUNT];   /* 去抖后的电平：1=遮挡 */

static qsm_edge_t s_ring[E18_RING_SIZE];
static volatile uint16_t s_head;             /* 写指针，仅 ISR 修改 */
static volatile uint16_t s_tail;             /* 读指针，仅主循环修改 */

/** 把逻辑探头号映射到实际引脚（对应 hardware/硬件接线说明.md） */
static uint16_t e18_read_pin(uint8_t beam)
{
#if APP_SINGLE_BEAM_TEST
    /* 台架测试：只有一支探头时，四路逻辑探头都读 PB12。
       挥手 → A/B 两个边沿同时到达 → 计一次进出；
       手停在探头前 → Q50/Q100 同时持续遮挡 → 分级升到"全满"。 */
    (void)beam;
    return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12);
#else
    switch (beam) {
        case QSM_BEAM_IN_A:  return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12);
        case QSM_BEAM_IN_B:  return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13);
        case QSM_BEAM_Q50:   return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14);
        case QSM_BEAM_Q100:  return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15);
        default:             return 0u;
    }
#endif
}

void bsp_e18_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* 四路上拉输入：E18-D80NK 为 NPN 开集输出，黑线外部 10k 上拉到 3.3 V，
       MCU 内部上拉作为第二重保险，防止传感器掉电时浮空。
       依据接线说明：无遮挡 3.3V，有遮挡 0V。 */
    gpio.GPIO_Mode  = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Pin   = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOB, &gpio);

    for (uint8_t i = 0u; i < QSM_BEAM_COUNT; i++) {
        s_hist[i]  = 0u;
        s_level[i] = 0u;
    }
    s_head = 0u;
    s_tail = 0u;
}

void bsp_e18_sample_1khz(uint32_t now_ms)
{
    for (uint8_t b = 0u; b < QSM_BEAM_COUNT; b++) {
        uint8_t raw = (uint8_t)(e18_read_pin(b) ? 1u : 0u);

#if APP_E18_ACTIVE_LOW
        raw = (uint8_t)!raw;      /* 检测到物体时输出低电平 → 取反成"1=遮挡" */
#endif

        s_hist[b] = (uint8_t)((s_hist[b] << 1) | raw);

        const uint8_t mask = (uint8_t)((1u << APP_DEBOUNCE_SAMPLES) - 1u);
        uint8_t stable;

        if ((s_hist[b] & mask) == mask) {
            stable = 1u;                       /* 连续为高 */
        } else if ((s_hist[b] & mask) == 0u) {
            stable = 0u;                       /* 连续为低 */
        } else {
            continue;                          /* 处于跳变中，保持原状态 */
        }

        if (stable != s_level[b]) {
            s_level[b] = stable;

            uint16_t next = (uint16_t)((s_head + 1u) & E18_RING_MASK);
            if (next != s_tail) {              /* 队列满则丢弃本次事件 */
                s_ring[s_head].beam  = b;
                s_ring[s_head].level = stable;
                s_ring[s_head].t_ms  = now_ms;
                s_head = next;
            }
        }
    }
}

uint8_t bsp_e18_pop(qsm_edge_t *out)
{
    if (out == 0 || s_tail == s_head) {
        return 0u;
    }
    *out   = s_ring[s_tail];
    s_tail = (uint16_t)((s_tail + 1u) & E18_RING_MASK);
    return 1u;
}

uint8_t bsp_e18_level(uint8_t beam)
{
    return (beam < QSM_BEAM_COUNT) ? s_level[beam] : 0u;
}
