#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

void Key_Init(void);
void Key_Scan10ms(void);

uint8_t Key_GetStartPress(void);
uint8_t Key_GetBallShortPress(void);
uint8_t Key_GetBallLongPress(void);

#endif
