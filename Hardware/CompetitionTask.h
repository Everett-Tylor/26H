#ifndef __COMPETITION_TASK_H
#define __COMPETITION_TASK_H

#include "stm32f10x.h"

typedef enum
{
    COMPETITION_TASK_3 = 3,
    COMPETITION_TASK_4 = 4,
    COMPETITION_TASK_5 = 5,
    COMPETITION_TASK_6 = 6
} CompetitionTask_Number;

typedef enum
{
    COMPETITION_READY = 0,
    COMPETITION_RUNNING,
    COMPETITION_FINISHED,
    COMPETITION_FAULT
} CompetitionTask_State;

void CompetitionTask_Init(void);

void CompetitionTask_SelectNext(void);
void CompetitionTask_Start(void);
void CompetitionTask_Stop(void);

void CompetitionTask_Update10ms(void);

CompetitionTask_Number CompetitionTask_GetSelected(void);
CompetitionTask_State CompetitionTask_GetState(void);

uint8_t CompetitionTask_IsRunning(void);
uint8_t CompetitionTask_IsFinished(void);
uint8_t CompetitionTask_HasFault(void);

uint8_t CompetitionTask_GetTask3Stage(void);

int16_t CompetitionTask_GetTargetMm(void);
int16_t CompetitionTask_GetPositionMm(void);
int16_t CompetitionTask_GetMaximumErrorMm(void);

uint32_t CompetitionTask_GetElapsedMs(void);
uint32_t CompetitionTask_GetDistancePulse(void);

#endif