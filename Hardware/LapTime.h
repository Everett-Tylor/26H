#ifndef __LAPTIME_H
#define __LAPTIME_H

#include "stm32f10x.h"

void LapTime_Init(void);
void LapTime_Start(void);
void LapTime_Stop(void);
void LapTime_Update10ms(void);

uint32_t LapTime_Get10ms(void);

uint8_t LapTime_IsRunning(void);
uint8_t LapTime_IsFinished(void);
uint8_t LapTime_HasLeftStart(void);
uint32_t LapTime_GetElapsedMs(void);
#endif