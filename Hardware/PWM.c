#include "stm32f10x.h"                  // Device header

//时钟-时基-输出比较OC-GPIO-启动

void PWM_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	/*RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);
	GPIO_PinRemapConfig(GPIO_PartialRemap1_TIM2,ENABLE);//重映射
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE);//解除调试端口
	*/
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	TIM_InternalClockConfig(TIM2);

	TIM_TimeBaseInitTypeDef TIM_TimebaseInitStructure;
	TIM_TimebaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;
	TIM_TimebaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;
	TIM_TimebaseInitStructure.TIM_Period=100-1;    //ARR
	TIM_TimebaseInitStructure.TIM_Prescaler=36-1;  //PSC
	TIM_TimebaseInitStructure.TIM_RepetitionCounter=0;
	TIM_TimeBaseInit(TIM2,&TIM_TimebaseInitStructure);
	
	TIM_OCInitTypeDef TIM_OCInitStruct;
	TIM_OCStructInit(&TIM_OCInitStruct);
	TIM_OCInitStruct.TIM_OCMode=TIM_OCMode_PWM1;
	TIM_OCInitStruct.TIM_OCPolarity=TIM_OCPolarity_High;
	TIM_OCInitStruct.TIM_OutputState=TIM_OutputState_Enable;
	TIM_OCInitStruct.TIM_Pulse=0;                //CCR
	TIM_OC3Init(TIM2,&TIM_OCInitStruct);//PA2是CH3通道
	
	TIM_Cmd(TIM2,ENABLE);
}


void PWM_SetCompare3(uint16_t Compare)
{
   TIM_SetCompare3(TIM2,Compare);
}


