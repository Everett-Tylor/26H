#include "CompetitionTask.h"

#include "BallTask.h"
#include "MaixCAM.h"
#include "Control.h"
#include "LapTime.h"
#include "Timer.h"

/*
 * 第4问A到B的编码器累计值。
 *
 * 标定方法：
 * 1. 小车放在A点；
 * 2. 启动后运行到B点；
 * 3. OLED或调试器读取编码器平均累计值；
 * 4. 将该数值填写到这里。
 *
 * 下面的5000只是临时占位，不能直接作为比赛值。
 */
#define TASK4_AB_STOP_PULSE       5000UL

/*
 * 接近B点时开始减速。
 * 默认在目标距离的80%处减速。
 */
#define TASK4_SLOW_PERCENT        80UL

#define TASK4_NORMAL_SPEED        36
#define TASK4_SLOW_SPEED          13

/*
 * 第3～6问允许误差：±1cm，即±10mm。
 */
#define BALL_ALLOW_ERROR_MM       10

/*
 * 第3问：
 * 到+5cm附近连续稳定150ms后立即折返；
 * 到-5cm附近连续稳定300ms后完成。
 */
#define TASK3_POSITIVE_STABLE     15
#define TASK3_NEGATIVE_STABLE     30
#define TASK3_TIMEOUT_MS          10000UL

/*
 * 第4问必须在8秒内通过B。
 * 超时后仍然停车，状态记为FAULT。
 */
#define TASK4_TIMEOUT_MS          8000UL

/*
 * 第5、6问一圈必须小于30秒。
 */
#define LAP_TIMEOUT_MS            30000UL

/*
 * 视觉长时间丢失保护。
 */
#define BALL_LOST_TIMEOUT_MS      500UL

static volatile CompetitionTask_Number Selected;
static volatile CompetitionTask_State State;

static volatile uint8_t Task3Stage;
static volatile int16_t TargetMm;
static volatile int16_t MaximumErrorMm;

static uint32_t StartMs;
static uint32_t FrozenElapsedMs;
static uint32_t LostStartMs;

static uint16_t StableCounter;

/**
  * @brief  计算有符号数绝对值
  */
static int16_t CompetitionTask_Abs(int16_t Value)
{
    if (Value < 0)
    {
        return (int16_t)(-Value);
    }

    return Value;
}

/**
  * @brief  更新最大位置误差
  */
static void CompetitionTask_UpdateMaximumError(void)
{
    int16_t Error;

    Error = CompetitionTask_Abs(
        (int16_t)(TargetMm - MaixCAM_GetPositionMm())
    );

    if (Error > MaximumErrorMm)
    {
        MaximumErrorMm = Error;
    }
}

/**
  * @brief  判断钢球是否位于目标±1cm范围
  */
static uint8_t CompetitionTask_BallIsStable(void)
{
    int16_t Error;

    Error = CompetitionTask_Abs(
        (int16_t)(TargetMm - MaixCAM_GetPositionMm())
    );

    if (Error <= BALL_ALLOW_ERROR_MM)
    {
        return 1;
    }

    return 0;
}

/**
  * @brief  正常完成任务
  */
static void CompetitionTask_Finish(void)
{
    FrozenElapsedMs = Timer_GetMillis() - StartMs;

    Control_Stop();
    LapTime_Stop();
    

    State = COMPETITION_FINISHED;
}

/**
  * @brief  故障停车
  */
static void CompetitionTask_FaultStop(void)
{
    FrozenElapsedMs = Timer_GetMillis() - StartMs;

    Control_Stop();
    LapTime_Stop();
    BallTask_Stop();

    State = COMPETITION_FAULT;
}

void CompetitionTask_Init(void)
{
    Selected = COMPETITION_TASK_3;
    State = COMPETITION_READY;

    Task3Stage = 0;
    TargetMm = 0;
    MaximumErrorMm = 0;

    StartMs = 0;
    FrozenElapsedMs = 0;
    LostStartMs = 0;
    StableCounter = 0;
}

/**
  * @brief  第3、4、5、6问循环选择
  */
void CompetitionTask_SelectNext(void)
{
    if (State == COMPETITION_RUNNING)
    {
        return;
    }

    if (Selected >= COMPETITION_TASK_6)
    {
        Selected = COMPETITION_TASK_3;
    }
    else
    {
        Selected =
            (CompetitionTask_Number)((uint8_t)Selected + 1);
    }

    State = COMPETITION_READY;
    Task3Stage = 0;
    MaximumErrorMm = 0;
    FrozenElapsedMs = 0;

    if (Selected == COMPETITION_TASK_6)
    {
        /*
         * 未启动时显示当前视觉位置。
         * 真正启动时还会重新锁定一次。
         */
        TargetMm = MaixCAM_GetPositionMm();
    }
    else
    {
        TargetMm = 0;
    }
}

/**
  * @brief  启动当前题目
  */
void CompetitionTask_Start(void)
{
    uint32_t NowMs;

    if (State == COMPETITION_RUNNING)
    {
        return;
    }

    NowMs = Timer_GetMillis();

    /*
     * 启动前必须有有效视觉坐标。
     */
    if ((MaixCAM_IsOnline(NowMs) == 0) ||
        (MaixCAM_IsBallValid() == 0))
    {
        State = COMPETITION_FAULT;
        return;
    }

    StartMs = NowMs;
    FrozenElapsedMs = 0;
    LostStartMs = 0;

    StableCounter = 0;
    MaximumErrorMm = 0;
    Task3Stage = 0;

    /*
     * 第3问启动后直接控制到+5cm。
     */
    if (Selected == COMPETITION_TASK_3)
    {
        TargetMm = 50;

        BallTask_Start(TargetMm);

        /*
         * 第3问小车必须保持静止。
         */
        Control_Stop();
        LapTime_Stop();
    }
    /*
     * 第4、5问钢球保持在O点。
     */
    else if ((Selected == COMPETITION_TASK_4) ||
             (Selected == COMPETITION_TASK_5))
    {
        TargetMm = 0;

        BallTask_Start(TargetMm);

        Control_SetBaseSpeed(TASK4_NORMAL_SPEED);
        Control_Start();

        if (Selected == COMPETITION_TASK_5)
        {
            LapTime_Start();
        }
    }
    /*
     * 第6问将启动瞬间的钢球位置锁定为目标位置。
     * 不是固定+3cm。
     */
    else
    {
        TargetMm = MaixCAM_GetPositionMm();

        BallTask_Start(TargetMm);

        Control_SetBaseSpeed(TASK4_NORMAL_SPEED);
        Control_Start();
        LapTime_Start();
    }

    State = COMPETITION_RUNNING;
}

/**
  * @brief  手动停止
  */
void CompetitionTask_Stop(void)
{
    if (State == COMPETITION_RUNNING)
    {
        FrozenElapsedMs = Timer_GetMillis() - StartMs;
    }

    Control_Stop();
    LapTime_Stop();
    BallTask_Stop();

    State = COMPETITION_READY;
}

/**
  * @brief  第3问状态机
  */
static void CompetitionTask_UpdateTask3(uint32_t ElapsedMs)
{
    /*
     * 阶段0：O点向+5cm运行。
     */
    if (Task3Stage == 0)
    {
        TargetMm = 50;
        BallTask_SetTargetMm(TargetMm);

        if (CompetitionTask_BallIsStable())
        {
            StableCounter++;

            /*
             * 到达+5cm后立即折返。
             */
            if (StableCounter >= TASK3_POSITIVE_STABLE)
            {
                StableCounter = 0;
                Task3Stage = 1;

                TargetMm = -50;
                BallTask_SetTargetMm(TargetMm);
            }
        }
        else
        {
            StableCounter = 0;
        }
    }
    /*
     * 阶段1：+5cm向-5cm运行。
     */
    else
    {
        TargetMm = -50;
        BallTask_SetTargetMm(TargetMm);

        if (CompetitionTask_BallIsStable())
        {
            StableCounter++;

            /*
             * 在-5cm附近连续稳定300ms后完成。
             */
            if (StableCounter >= TASK3_NEGATIVE_STABLE)
            {
                Task3Stage = 2;
                CompetitionTask_Finish();
            }
        }
        else
        {
            StableCounter = 0;
        }
    }

    if (ElapsedMs >= TASK3_TIMEOUT_MS)
    {
        CompetitionTask_FaultStop();
    }
		
}

/**
  * @brief  第4问状态机
  */
static void CompetitionTask_UpdateTask4(uint32_t ElapsedMs)
{
    uint32_t DistancePulse;
    uint32_t SlowPulse;

    TargetMm = 0;
    BallTask_SetTargetMm(0);

    DistancePulse = Control_GetDistancePulse();

    SlowPulse =
        TASK4_AB_STOP_PULSE *
        TASK4_SLOW_PERCENT / 100UL;

    /*
     * 接近B点时减速，降低停车惯性。
     */
    if (DistancePulse >= SlowPulse)
    {
        Control_SetBaseSpeed(TASK4_SLOW_SPEED);
    }

    /*
     * 编码器累计达到标定值，表示通过B位置。
     */
    if (DistancePulse >= TASK4_AB_STOP_PULSE)
    {
        CompetitionTask_Finish();
        return;
    }

    if (ElapsedMs >= TASK4_TIMEOUT_MS)
    {
        CompetitionTask_FaultStop();
    }
}

/**
  * @brief  第5、6问状态机
  */
static void CompetitionTask_UpdateLapTask(uint32_t ElapsedMs)
{
    /*
     * LapTime检测离开A后，再次检测停车线。
     */
    if (LapTime_IsFinished())
    {
        CompetitionTask_Finish();
        return;
    }

    if (ElapsedMs >= LAP_TIMEOUT_MS)
    {
        CompetitionTask_FaultStop();
    }
}

void CompetitionTask_Update10ms(void)
{
    uint32_t NowMs;
    uint32_t ElapsedMs;

    if (State != COMPETITION_RUNNING)
    {
        return;
    }

    NowMs = Timer_GetMillis();
    ElapsedMs = NowMs - StartMs;

    /*
     * 视觉掉线保护。
     */
    if ((MaixCAM_IsOnline(NowMs) == 0) ||
        (MaixCAM_IsBallValid() == 0))
    {
        if (LostStartMs == 0)
        {
            LostStartMs = NowMs;
        }
        else if ((NowMs - LostStartMs) >=
                 BALL_LOST_TIMEOUT_MS)
        {
            CompetitionTask_FaultStop();
        }

        return;
    }

    LostStartMs = 0;

    CompetitionTask_UpdateMaximumError();

    if (Selected == COMPETITION_TASK_3)
    {
        CompetitionTask_UpdateTask3(ElapsedMs);
    }
    else if (Selected == COMPETITION_TASK_4)
    {
        CompetitionTask_UpdateTask4(ElapsedMs);
    }
    else
    {
        CompetitionTask_UpdateLapTask(ElapsedMs);
    }
		/*
     * 比赛流程更新完目标后，
     * 每10ms执行一次钢球PID控制。
     */
    
}

CompetitionTask_Number CompetitionTask_GetSelected(void)
{
    return Selected;
}

CompetitionTask_State CompetitionTask_GetState(void)
{
    return State;
}

uint8_t CompetitionTask_IsRunning(void)
{
    return (uint8_t)(State == COMPETITION_RUNNING);
}

uint8_t CompetitionTask_IsFinished(void)
{
    return (uint8_t)(State == COMPETITION_FINISHED);
}

uint8_t CompetitionTask_HasFault(void)
{
    return (uint8_t)(State == COMPETITION_FAULT);
}

uint8_t CompetitionTask_GetTask3Stage(void)
{
    return Task3Stage;
}

int16_t CompetitionTask_GetTargetMm(void)
{
    return TargetMm;
}

int16_t CompetitionTask_GetPositionMm(void)
{
    return MaixCAM_GetPositionMm();
}

int16_t CompetitionTask_GetMaximumErrorMm(void)
{
    return MaximumErrorMm;
}

uint32_t CompetitionTask_GetElapsedMs(void)
{
    if (State == COMPETITION_RUNNING)
    {
        return Timer_GetMillis() - StartMs;
    }

    return FrozenElapsedMs;
}

uint32_t CompetitionTask_GetDistancePulse(void)
{
    return Control_GetDistancePulse();
}