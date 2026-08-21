#ifndef __TIMER_H
#define __TIMER_H

#include "stm32f10x.h"

void Timer_Init(void);

uint32_t Timer_GetMillis(void);
void Timer_DelayMs(uint32_t TimeMs);

void Timer_SysTickISR(void);

#endif