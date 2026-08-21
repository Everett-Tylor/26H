#include "Gray.h"

/*
 * 从左到右：
 *
 * L2 -> PB12
 * L1 -> PB13
 * M  -> PB14
 * R1 -> PB15
 * R2 -> PB1
 */

volatile uint8_t Gray_State = 0;
volatile int16_t Gray_Error = 0;
volatile uint8_t Gray_LineLost = 0;

static int16_t Gray_LastError = 0;

static uint8_t Gray_IsBlack(GPIO_TypeDef *GPIOx, uint16_t Pin)
{
	uint8_t Level;

	Level = GPIO_ReadInputDataBit(GPIOx, Pin);

	if (Level == GRAY_BLACK_LEVEL)
	{
		return 1;
	}

	return 0;
}

void Gray_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(
		RCC_APB2Periph_GPIOB,
		ENABLE
	);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin =
		GPIO_Pin_1 |
		GPIO_Pin_12 |
		GPIO_Pin_13 |
		GPIO_Pin_14 |
		GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(GPIOB, &GPIO_InitStructure);

	Gray_State = 0;
	Gray_Error = 0;
	Gray_LineLost = 0;
	Gray_LastError = 0;
}

void Gray_Read(void)
{
	uint8_t L2;
	uint8_t L1;
	uint8_t M;
	uint8_t R1;
	uint8_t R2;

	int16_t Sum;
	uint8_t Count;

	L2 = Gray_IsBlack(GPIOB, GPIO_Pin_12);
	L1 = Gray_IsBlack(GPIOB, GPIO_Pin_13);
	M  = Gray_IsBlack(GPIOB, GPIO_Pin_14);
	R1 = Gray_IsBlack(GPIOB, GPIO_Pin_15);
	R2 = Gray_IsBlack(GPIOB, GPIO_Pin_1);

	Gray_State =
		(L2 << 4) |
		(L1 << 3) |
		(M  << 2) |
		(R1 << 1) |
		(R2 << 0);

	Sum = 0;
	Count = 0;

	/*
	 * 黑线在左边，误差为负；
	 * 黑线在右边，误差为正。
	 *
	 * 权值放大100倍，方便PD调节。
	 */
	if (L2)
	{
		Sum += -400;
		Count++;
	}

	if (L1)
	{
		Sum += -200;
		Count++;
	}

	if (M)
	{
		Sum += 0;
		Count++;
	}

	if (R1)
	{
		Sum += 200;
		Count++;
	}

	if (R2)
	{
		Sum += 400;
		Count++;
	}

	if (Count > 0)
	{
		Gray_Error = Sum / Count;
		Gray_LastError = Gray_Error;
		Gray_LineLost = 0;
	}
	else
	{
		/*
		 * 丢线后保持上一次方向，
		 * 让小车继续向原方向找线。
		 */
		Gray_LineLost = 1;

		if (Gray_LastError > 0)
		{
			Gray_Error = 500;
		}
		else if (Gray_LastError < 0)
		{
			Gray_Error = -500;
		}
		else
		{
			Gray_Error = 0;
		}
	}
}

int16_t Gray_GetError(void)
{
	return Gray_Error;
}

uint8_t Gray_IsLost(void)
{
	return Gray_LineLost;
}

