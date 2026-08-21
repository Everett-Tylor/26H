#ifndef __CONTROL_H
#define __CONTROL_H

#include "stm32f10x.h"

extern volatile int16_t Left_Speed;
extern volatile int16_t Right_Speed;

extern volatile int16_t Control_LeftTarget;
extern volatile int16_t Control_RightTarget;

extern volatile int16_t Control_LeftOutput;
extern volatile int16_t Control_RightOutput;

void Control_Init(void);

void Control_Start(void);
void Control_Stop(void);

void Control_SetBaseSpeed(int16_t Speed);

/* 设置左右速度PI */
void Control_SetSpeedPI(float Kp, float Ki);

/* 设置循迹PD */
void Control_SetTrackPD(float Kp, float Kd);

void Control_Update10ms(void);

uint8_t Control_IsRunning(void);

/*
 * 编码器行驶距离累计。
 */
void Control_ResetDistance(void);
uint32_t Control_GetLeftDistancePulse(void);
uint32_t Control_GetRightDistancePulse(void);
uint32_t Control_GetDistancePulse(void);


#endif