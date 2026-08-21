#include "PID.h"

static float PID_Limit(float Value, float Max)
{
    if (Value > Max)
    {
        Value = Max;
    }
    else if (Value < -Max)
    {
        Value = -Max;
    }

    return Value;
}

void PID_Init(
    PID_TypeDef *PID,
    float Kp,
    float Ki,
    float Kd,
    float OutputMax,
    float IntegralMax
)
{
    PID->Kp = Kp;
    PID->Ki = Ki;
    PID->Kd = Kd;

    PID->Error = 0.0f;
    PID->LastError = 0.0f;
    PID->Integral = 0.0f;

    PID->Output = 0.0f;
    PID->OutputMax = OutputMax;
    PID->IntegralMax = IntegralMax;
}

float PID_PositionCalc(
    PID_TypeDef *PID,
    float Target,
    float Actual
)
{
    PID->Error = Target - Actual;

    PID->Integral += PID->Error;

    if (PID->IntegralMax > 0.0f)
    {
        PID->Integral = PID_Limit(
            PID->Integral,
            PID->IntegralMax
        );
    }

    PID->Output =
        PID->Kp * PID->Error +
        PID->Ki * PID->Integral +
        PID->Kd * (PID->Error - PID->LastError);

    PID->Output = PID_Limit(
        PID->Output,
        PID->OutputMax
    );

    PID->LastError = PID->Error;

    return PID->Output;
}

float PID_PDCalc(
    PID_TypeDef *PID,
    float Error
)
{
    PID->Error = Error;

    PID->Output =
        PID->Kp * PID->Error +
        PID->Kd * (PID->Error - PID->LastError);

    PID->Output = PID_Limit(
        PID->Output,
        PID->OutputMax
    );

    PID->LastError = PID->Error;

    return PID->Output;
}

void PID_Clear(PID_TypeDef *PID)
{
    PID->Error = 0.0f;
    PID->LastError = 0.0f;
    PID->Integral = 0.0f;
    PID->Output = 0.0f;
}