#ifndef __ENCODER_H
#define __ENCODER_H

#include "stm32f10x.h"

/*
 * 小车向前时编码器必须为正。
 * 如果某一侧为负，把对应的1改成-1。
 */
#define LEFT_ENCODER_DIRECTION     1
#define RIGHT_ENCODER_DIRECTION    1

void Encoder_Init(void);

int16_t Encoder_GetLeft(void);
int16_t Encoder_GetRight(void);

#endif