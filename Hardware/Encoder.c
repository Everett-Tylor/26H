#include "Encoder.h"

/*
 * 左编码器：
 * PB6 -> TIM4_CH1
 * PB7 -> TIM4_CH2
 *
 * 右编码器：
 * PA6 -> TIM3_CH1
 * PA7 -> TIM3_CH2
 */

void Encoder_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_ICInitTypeDef TIM_ICInitStructure;

	RCC_APB2PeriphClockCmd(
		RCC_APB2Periph_GPIOA |
		RCC_APB2Periph_GPIOB,
		ENABLE
	);

	RCC_APB1PeriphClockCmd(
		RCC_APB1Periph_TIM3 |
		RCC_APB1Periph_TIM4,
		ENABLE
	);

	/* PA6、PA7：右编码器 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* PB6、PB7：左编码器 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);

	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = 65535;
	TIM_TimeBaseStructure.TIM_Prescaler = 0;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

	/* AB相编码器模式 */
	TIM_EncoderInterfaceConfig(
		TIM3,
		TIM_EncoderMode_TI12,
		TIM_ICPolarity_Rising,
		TIM_ICPolarity_Rising
	);

	TIM_EncoderInterfaceConfig(
		TIM4,
		TIM_EncoderMode_TI12,
		TIM_ICPolarity_Rising,
		TIM_ICPolarity_Rising
	);

	TIM_ICStructInit(&TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_ICFilter = 6;

	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInit(TIM3, &TIM_ICInitStructure);
	TIM_ICInit(TIM4, &TIM_ICInitStructure);

	TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
	TIM_ICInit(TIM3, &TIM_ICInitStructure);
	TIM_ICInit(TIM4, &TIM_ICInitStructure);

	TIM_SetCounter(TIM3, 0);
	TIM_SetCounter(TIM4, 0);

	TIM_Cmd(TIM3, ENABLE);
	TIM_Cmd(TIM4, ENABLE);
}

int16_t Encoder_GetLeft(void)
{
	int16_t Count;

	Count = (int16_t)TIM_GetCounter(TIM4);
	TIM_SetCounter(TIM4, 0);

	return Count * LEFT_ENCODER_DIRECTION;
}

int16_t Encoder_GetRight(void)
{
	int16_t Count;

	Count = (int16_t)TIM_GetCounter(TIM3);
	TIM_SetCounter(TIM3, 0);

	return Count * RIGHT_ENCODER_DIRECTION;
}