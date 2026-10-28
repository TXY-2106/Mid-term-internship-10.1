/** @file app_main.h  固件主流程入口 */
#ifndef APP_MAIN_H
#define APP_MAIN_H

/** 初始化全部外设与 core 状态 */
void app_init(void);

/** 进入主循环（不返回） */
void app_run(void);

#endif /* APP_MAIN_H */
