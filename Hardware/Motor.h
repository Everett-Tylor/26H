#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

/*
 * 电机方向反了，只修改对应方向：
 * 1  正向
 * -1 反向
 */
#define LEFT_MOTOR_DIRECTION     -1
#define RIGHT_MOTOR_DIRECTION    -1

#define MOTOR_MAX_OUTPUT         1000

void Motor_Init(void);

void Motor_SetLeft(int16_t Speed);
void Motor_SetRight(int16_t Speed);
void Motor_SetSpeed(int16_t LeftSpeed, int16_t RightSpeed);

void Motor_Stop(void);
void Motor_Brake(void);

#endif