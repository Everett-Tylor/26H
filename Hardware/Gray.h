#ifndef __GRAY_H
#define __GRAY_H

#include "stm32f10x.h"

/*
 * 大多数数字灰度模块：
 * 检测到黑线输出0。
 *
 * 如果你的模块检测黑线输出1，
 * 把这里改成1。
 */
#define GRAY_BLACK_LEVEL    1

extern volatile uint8_t Gray_State;
extern volatile int16_t Gray_Error;
extern volatile uint8_t Gray_LineLost;

void Gray_Init(void);
void Gray_Read(void);

int16_t Gray_GetError(void);
uint8_t Gray_IsLost(void);
void Gray_Update(void);
#endif