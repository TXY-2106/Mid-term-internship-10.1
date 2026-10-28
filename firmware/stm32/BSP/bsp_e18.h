/**
 * @file  bsp_e18.h
 * @brief 四路 E18-D80NK 的 1 kHz 采样与移位寄存器去抖
 *
 * 设计要点：
 *   - 采样在 SysTick 中断里完成（1 kHz），每路维护一个 8 bit 历史寄存器；
 *   - 连续 APP_DEBOUNCE_SAMPLES 个相同值才认定电平稳定，抑制抖动与环境光干扰；
 *   - 稳定电平发生变化的瞬间，把一个 qsm_edge_t 压入环形队列；
 *   - 中断里绝不调用 core 的状态机，只在主循环出队后处理（保持 ISR 短小）。
 */
#ifndef BSP_E18_H
#define BSP_E18_H

#include "qsm_types.h"

void    bsp_e18_init(void);

/** 由 SysTick 中断每秒调用 1000 次。 */
void    bsp_e18_sample_1khz(uint32_t now_ms);

/** 主循环取走一个稳定边沿。返回 1 表示取到，0 表示队列空。 */
uint8_t bsp_e18_pop(qsm_edge_t *out);

/** 当前去抖后的实时电平（1=遮挡），供调试与故障判断使用。 */
uint8_t bsp_e18_level(uint8_t beam);

#endif /* BSP_E18_H */
