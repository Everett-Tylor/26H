#ifndef __BALLTASK_H
#define __BALLTASK_H

#include "stm32f10x.h"

/* 钢球任务编号 */
typedef enum
{
    BALL_TASK_3 = 3,
    BALL_TASK_4 = 4,
    BALL_TASK_5 = 5,
    BALL_TASK_6 = 6
} BallTask_Number;

void BallTask_Init(void);
void BallTask_SelectNext(void);

/*
 * 保留int16_t参数，以兼容你原来的
 * CompetitionTask.c和main.c。
 */
void BallTask_Start(int16_t InitialTargetMm);

void BallTask_Stop(void);
void BallTask_Update10ms(void);

uint8_t BallTask_IsRunning(void);
uint8_t BallTask_HasFault(void);

BallTask_Number BallTask_GetSelected(void);

int16_t BallTask_GetTargetMm(void);
uint32_t BallTask_GetElapsedMs(void);

void BallTask_SetTask6TargetMm(int16_t NewTargetMm);
void BallTask_SetTargetMm(int16_t NewTargetMm);
#endif