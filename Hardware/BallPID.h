#ifndef __BALL_PID_H
#define __BALL_PID_H

#include "stm32f10x.h"

void BallPID_Init(void);

void BallPID_SetParameter(
    float Kp,
    float Ki,
    float Kd
);

void BallPID_Reset(void);

float BallPID_Calculate(
    int16_t TargetMm,
    int16_t PositionMm
);

#endif