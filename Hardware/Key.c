#include "Key.h"

/* PC13按键，一端接PC13，另一端接GND */

static volatile uint8_t Key_StartPress = 0;
static volatile uint8_t Key_BallShortPress = 0;
static volatile uint8_t Key_BallLongPress = 0;

static uint8_t Key_LastState = 1;
static uint8_t Key_StableState = 1;
static uint8_t Key_DebounceCount = 0;
static uint8_t Key_BallLastState = 1;
static uint8_t Key_BallStableState = 1;
static uint8_t Key_BallDebounceCount = 0;
static uint16_t Key_BallHoldCount = 0;
static uint8_t Key_BallLongSent = 0;

void Key_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(
		RCC_APB2Periph_GPIOC,
		ENABLE
	);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void Key_Scan10ms(void)
{
	uint8_t CurrentState;
	uint8_t BallState;

	CurrentState =
		GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13);

	if (CurrentState == Key_LastState)
	{
		if (Key_DebounceCount < 3)
		{
			Key_DebounceCount++;
		}
	}
	else
	{
		Key_DebounceCount = 0;
		Key_LastState = CurrentState;
	}

	/* 连续30ms相同，认为状态稳定 */
	if (
		Key_DebounceCount >= 3 &&
		CurrentState != Key_StableState
	)
	{
		Key_StableState = CurrentState;

		/* 按下为低电平 */
		if (Key_StableState == 0)
		{
			Key_StartPress = 1;
		}
	}

	BallState = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_14);

	if (BallState == Key_BallLastState)
	{
		if (Key_BallDebounceCount < 3)
		{
			Key_BallDebounceCount++;
		}
	}
	else
	{
		Key_BallDebounceCount = 0;
		Key_BallLastState = BallState;
	}

	if (Key_BallDebounceCount >= 3 && BallState != Key_BallStableState)
	{
		Key_BallStableState = BallState;

		if (Key_BallStableState == 0)
		{
			Key_BallHoldCount = 0;
			Key_BallLongSent = 0;
		}
		else if (Key_BallLongSent == 0)
		{
			Key_BallShortPress = 1;
		}
	}

	if (Key_BallStableState == 0)
	{
		if (Key_BallHoldCount < 1000)
		{
			Key_BallHoldCount++;
		}

		if (Key_BallHoldCount >= 100 && Key_BallLongSent == 0)
		{
			Key_BallLongSent = 1;
			Key_BallLongPress = 1;
		}
	}
}

uint8_t Key_GetStartPress(void)
{
	uint8_t Press;

	__disable_irq();

	Press = Key_StartPress;
	Key_StartPress = 0;

	__enable_irq();

	return Press;
}

uint8_t Key_GetBallShortPress(void)
{
	uint8_t Press;

	__disable_irq();
	Press = Key_BallShortPress;
	Key_BallShortPress = 0;
	__enable_irq();

	return Press;
}

uint8_t Key_GetBallLongPress(void)
{
	uint8_t Press;

	__disable_irq();
	Press = Key_BallLongPress;
	Key_BallLongPress = 0;
	__enable_irq();

	return Press;
}
