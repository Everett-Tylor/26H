#include "Timer.h"

#include "Key.h"
#include "Control.h"
#include "LapTime.h"
#include "BallTask.h"
#include "CompetitionTask.h"

static volatile uint32_t Timer_Millis = 0;
static uint8_t Timer_Count10ms = 0;

void Timer_Init(void)
{
    SystemCoreClockUpdate();

    if (SysTick_Config(SystemCoreClock / 1000) != 0)
    {
        while (1)
        {
        }
    }

    NVIC_SetPriority(SysTick_IRQn, 1);
}

uint32_t Timer_GetMillis(void)
{
    return Timer_Millis;
}

void Timer_DelayMs(uint32_t TimeMs)
{
    uint32_t StartTime;

    StartTime = Timer_Millis;

    while ((Timer_Millis - StartTime) < TimeMs)
    {
    }
}

void Timer_SysTickISR(void)
{
    Timer_Millis++;

    Timer_Count10ms++;

    if (Timer_Count10ms >= 10)
    {
        Timer_Count10ms = 0;

        Key_Scan10ms();

        /*
         * 顺序不能乱：
         * 1. 先读取编码器并执行小车控制；
         * 2. 再检测终点；
         * 3. 再控制钢球；
         * 4. 最后运行总任务状态机。
         */
        Control_Update10ms();
LapTime_Update10ms();

/*
 * 先由比赛状态机更新目标位置，
 * 再且只执行一次钢球PID。
 */
CompetitionTask_Update10ms();
BallTask_Update10ms();
    }
}