#ifndef __MAIXCAM_H
#define __MAIXCAM_H

#include "stm32f10x.h"

void MaixCAM_Init(void);
void MaixCAM_RxIRQHandler(void);

int16_t MaixCAM_GetPositionMm(void);

uint8_t MaixCAM_IsBallValid(void);
uint8_t MaixCAM_IsOnline(uint32_t NowMs);

/* 调试信息 */
uint32_t MaixCAM_GetRxByteCount(void);
uint32_t MaixCAM_GetValidFrameCount(void);
uint32_t MaixCAM_GetErrorFrameCount(void);

#endif