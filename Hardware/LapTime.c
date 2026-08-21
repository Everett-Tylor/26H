#include "LapTime.h"
#include "Gray.h"
#include "Control.h"
#include "Motor.h"

/* 连续20ms检测到终点线，确认停车 */
#define FINISH_CONFIRM_TICKS      3

/* 连续100ms没有压横线，确认已经离开发车线 */
#define LEAVE_CONFIRM_TICKS       10

/* 发车1秒后才允许识别终点，防止刚启动就停车 */
#define MIN_LAP_TICKS             1800

/* 至少2路灰度传感器同时检测到黑线 */
#define FINISH_BLACK_COUNT        2


static volatile uint32_t LapTime_10ms = 0;

static uint8_t LapTime_Running = 0;
static uint8_t LapTime_Finished = 0;
static uint8_t LapTime_LeftStart = 0;

static uint8_t LeaveLine_Count = 0;
static uint8_t FinishLine_Count = 0;


/**
  * @brief  统计五路灰度中检测到黑线的数量
  * @note   默认Gray_State低5位中，1表示检测到黑线
  */
static uint8_t LapTime_GetBlackCount(void)
{
    uint8_t State;
    uint8_t BlackCount;

    State = Gray_State & 0x1F;
    BlackCount = 0;

    /*
     * 如果你的灰度模块是：
     * 白底输出1、黑线输出0，
     * 则把上面的代码改成：
     *
     * State = (~Gray_State) & 0x1F;
     */

    while (State != 0)
    {
        if ((State & 0x01) != 0)
        {
            BlackCount++;
        }

        State >>= 1;
    }

    return BlackCount;
}


/**
  * @brief  判断当前是否压到起终点横线
  */
static uint8_t LapTime_IsFinishLine(void)
{
    if (LapTime_GetBlackCount() >= FINISH_BLACK_COUNT)
    {
        return 1;
    }

    return 0;
}


/**
  * @brief  圈时及终点检测初始化
  */
void LapTime_Init(void)
{
    LapTime_10ms = 0;

    LapTime_Running = 0;
    LapTime_Finished = 0;
    LapTime_LeftStart = 0;

    LeaveLine_Count = 0;
    FinishLine_Count = 0;
}


/**
  * @brief  开始新一圈
  */
void LapTime_Start(void)
{
    LapTime_10ms = 0;

    LapTime_Running = 1;
    LapTime_Finished = 0;
    LapTime_LeftStart = 0;

    LeaveLine_Count = 0;
    FinishLine_Count = 0;
}


/**
  * @brief  手动停止计时
  */
void LapTime_Stop(void)
{
    LapTime_Running = 0;
}


/**
  * @brief  圈时及终点检测
  * @note   必须每10ms调用一次
  */
void LapTime_Update10ms(void)
{
    if (LapTime_Running == 0)
    {
        return;
    }

    LapTime_10ms++;

    /*
     * 第一阶段：确认小车已经离开发车线。
     * 连续100ms没有检测到横线，才允许识别终点。
     */
    if (LapTime_LeftStart == 0)
    {
        if (LapTime_IsFinishLine() == 0)
        {
            if (LeaveLine_Count < LEAVE_CONFIRM_TICKS)
            {
                LeaveLine_Count++;
            }

            if (LeaveLine_Count >= LEAVE_CONFIRM_TICKS)
            {
                LapTime_LeftStart = 1;
                FinishLine_Count = 0;
            }
        }
        else
        {
            LeaveLine_Count = 0;
        }

        return;
    }

    /*
     * 第二阶段：重新经过起终点横线。
     * 发车至少1秒，并且至少2路连续20ms压到黑线，
     * 立即停止小车。
     */
    if ((LapTime_10ms >= MIN_LAP_TICKS) &&
        (LapTime_IsFinishLine() != 0))
    {
        if (FinishLine_Count < FINISH_CONFIRM_TICKS)
        {
            FinishLine_Count++;
        }

        if (FinishLine_Count >= FINISH_CONFIRM_TICKS)
        {
            LapTime_Running = 0;
            LapTime_Finished = 1;

            Control_Stop();
					Motor_Brake();
        }
    }
    else
    {
        FinishLine_Count = 0;
    }
}


uint32_t LapTime_Get10ms(void)
{
    return LapTime_10ms;
}


uint8_t LapTime_IsRunning(void)
{
    return LapTime_Running;
}


uint8_t LapTime_IsFinished(void)
{
    return LapTime_Finished;
}


uint8_t LapTime_HasLeftStart(void)
{
    return LapTime_LeftStart;
}

/**
  * @brief  获取本圈经过时间
  * @retval 时间，单位ms
  */
uint32_t LapTime_GetElapsedMs(void)
{
    return LapTime_10ms * 10UL;
}