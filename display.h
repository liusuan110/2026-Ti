/*
 * display.h
 * 显示页面管理
 * 每个页面对应一个任务 (5~10)
 */

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "config.h"

/* 初始化显示模块 */
void Display_init(void);

/*
 * 刷新指定页面
 * page_id: PAGE_INFO ~ PAGE_FFT
 * 内部根据 page_id 调用对应的测量和绘制逻辑
 */
void Display_refresh(PageID page_id);

/* Vpp/Vrms 子模式切换 (任务7, KEY2 触发) */
void Display_toggleVppSubMode(void);

#endif /* __DISPLAY_H__ */
