#ifndef __PID_H
#define __PID_H

typedef struct
{
    float Kp;
    float Ki;
    float Kd;

    float Error;
    float LastError;
    float Integral;

    float Output;
    float OutputMax;
    float IntegralMax;
} PID_TypeDef;

void PID_Init(
    PID_TypeDef *PID,
    float Kp,
    float Ki,
    float Kd,
    float OutputMax,
    float IntegralMax
);

float PID_PositionCalc(
    PID_TypeDef *PID,
    float Target,
    float Actual
);

float PID_PDCalc(
    PID_TypeDef *PID,
    float Error
);

void PID_Clear(PID_TypeDef *PID);

#endif