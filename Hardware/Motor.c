#include "Motor.h"

/*
 * 左电机：
 * PWMA -> PA0  TIM2_CH1
 * AIN1 -> PA4
 * AIN2 -> PA5
 *
 * 右电机：
 * PWMB -> PA1  TIM2_CH2
 * BIN1 -> PB10
 * BIN2 -> PB11
 *
 * STBY -> PB0
 */

static int16_t Motor_Limit(int16_t Speed)
{
	if (Speed > MOTOR_MAX_OUTPUT)
	{
		Speed = MOTOR_MAX_OUTPUT;
	}
	else if (Speed < -MOTOR_MAX_OUTPUT)
	{
		Speed = -MOTOR_MAX_OUTPUT;
	}

	return Speed;
}

void Motor_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;

	RCC_APB2PeriphClockCmd(
		RCC_APB2Periph_GPIOA |
		RCC_APB2Periph_GPIOB |
		RCC_APB2Periph_AFIO,
		ENABLE
	);

	RCC_APB1PeriphClockCmd(
		RCC_APB1Periph_TIM2,
		ENABLE
	);

	/* PA0、PA1：PWM输出 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* PA4、PA5：左电机方向 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* PB0、PB10、PB11 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin =
		GPIO_Pin_0 |
		GPIO_Pin_10 |
		GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/*
	 * PWM频率：
	 * 72MHz / 3600 = 20kHz
	 */
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = 3599;
	TIM_TimeBaseStructure.TIM_Prescaler = 0;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	TIM_OCStructInit(&TIM_OCInitStructure);

	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 0;

	TIM_OC1Init(TIM2, &TIM_OCInitStructure);
	TIM_OC2Init(TIM2, &TIM_OCInitStructure);

	TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
	TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);
	TIM_ARRPreloadConfig(TIM2, ENABLE);

	TIM_Cmd(TIM2, ENABLE);

	/* 使能TB6612 */
	GPIO_SetBits(GPIOB, GPIO_Pin_0);

	Motor_Stop();
}

void Motor_SetLeft(int16_t Speed)
{
	uint16_t PWM;

	Speed *= LEFT_MOTOR_DIRECTION;
	Speed = Motor_Limit(Speed);

	if (Speed > 0)
	{
		GPIO_SetBits(GPIOA, GPIO_Pin_4);
		GPIO_ResetBits(GPIOA, GPIO_Pin_5);

		PWM = (uint16_t)Speed;
	}
	else if (Speed < 0)
	{
		GPIO_ResetBits(GPIOA, GPIO_Pin_4);
		GPIO_SetBits(GPIOA, GPIO_Pin_5);

		PWM = (uint16_t)(-Speed);
	}
	else
	{
		GPIO_ResetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_5);
		PWM = 0;
	}

	/* 0～1000映射到0～3599 */
	TIM_SetCompare1(
		TIM2,
		(uint16_t)((uint32_t)PWM * 3599 / 1000)
	);
}

void Motor_SetRight(int16_t Speed)
{
	uint16_t PWM;

	Speed *= RIGHT_MOTOR_DIRECTION;
	Speed = Motor_Limit(Speed);

	if (Speed > 0)
	{
		GPIO_SetBits(GPIOB, GPIO_Pin_10);
		GPIO_ResetBits(GPIOB, GPIO_Pin_11);

		PWM = (uint16_t)Speed;
	}
	else if (Speed < 0)
	{
		GPIO_ResetBits(GPIOB, GPIO_Pin_10);
		GPIO_SetBits(GPIOB, GPIO_Pin_11);

		PWM = (uint16_t)(-Speed);
	}
	else
	{
		GPIO_ResetBits(
			GPIOB,
			GPIO_Pin_10 | GPIO_Pin_11
		);

		PWM = 0;
	}

	TIM_SetCompare2(
		TIM2,
		(uint16_t)((uint32_t)PWM * 3599 / 1000)
	);
}

void Motor_SetSpeed(int16_t LeftSpeed, int16_t RightSpeed)
{
	Motor_SetLeft(LeftSpeed);
	Motor_SetRight(RightSpeed);
}

void Motor_Stop(void)
{
	Motor_SetSpeed(0, 0);
}

void Motor_Brake(void)
{
	TIM_SetCompare1(TIM2, 0);
	TIM_SetCompare2(TIM2, 0);

	GPIO_SetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_5);
	GPIO_SetBits(GPIOB, GPIO_Pin_10 | GPIO_Pin_11);
}