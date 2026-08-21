#ifndef __CONTROL_TIMER_H
#define __CONTROL_TIMER_H

#include "stm32f10x.h"

void ControlTimer_Init(void);

extern volatile uint8_t ControlFlag;

#endif