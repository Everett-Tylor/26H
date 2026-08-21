#include "BallTask.h"
#include "BallPID.h"
#include "BalanceOutput.h"
#include "MaixCAM.h"
#include "Timer.h"

/* 第三问参数 */
#define TASK3_POSITIVE_MM          50
#define TASK3_NEGATIVE_MM         -50
#define TASK3_ALLOW_ERROR_MM       10

#define TASK3_POSITIVE_STABLE_MS   100
#define TASK3_NEGATIVE_STABLE_MS   300
#define TASK3_TIMEOUT_MS           5000

#define BALL_LOST_MS               500

typedef enum
{
    TASK3_GO_POSITIVE = 0,
    TASK3_GO_NEGATIVE,
    TASK3_HOLD_NEGATIVE
} Task3_Stage;

static BallTask_Number Selected = BALL_TASK_3;

static volatile uint8_t Running;
static volatile uint8_t Fault;

static uint32_t StartMs;
static volatile uint32_t ElapsedMs;
static uint32_t LostStartMs;

static int16_t TargetMm;
static int16_t Task6TargetMm = 30;

static Task3_Stage Stage3;
static uint16_t StageStableMs;

/**
  * @brief 判断钢球是否进入目标允许范围
  */
static uint8_t BallTask_IsInRange(
    int16_t PositionMm,
    int16_t TargetPositionMm
)
{
    int16_t Error;

    Error = PositionMm - TargetPositionMm;

    if (Error < 0)
    {
        Error = -Error;
    }

    if (Error <= TASK3_ALLOW_ERROR_MM)
    {
        return 1;
    }

    return 0;
}

void BallTask_Init(void)
{
    Selected = BALL_TASK_3;

    Running = 0;
    Fault = 0;

    TargetMm = 0;
    ElapsedMs = 0;

    Stage3 = TASK3_GO_POSITIVE;
    StageStableMs = 0;

    BallPID_Init();
    BalanceOutput_Init();
}

void BallTask_SelectNext(void)
{
    if (Running)
    {
        return;
    }

    if (Selected >= BALL_TASK_6)
    {
        Selected = BALL_TASK_3;
    }
    else
    {
        Selected =
            (BallTask_Number)((uint8_t)Selected + 1);
    }

    Fault = 0;
    ElapsedMs = 0;

    if (Selected == BALL_TASK_6)
    {
        TargetMm = Task6TargetMm;
    }
    else
    {
        TargetMm = 0;
    }
}

void BallTask_Start(int16_t InitialTargetMm)
{
    Fault = 0;

    StartMs = Timer_GetMillis();
    ElapsedMs = 0;
    LostStartMs = 0;

    if (InitialTargetMm > 150)
    {
        InitialTargetMm = 150;
    }
    else if (InitialTargetMm < -150)
    {
        InitialTargetMm = -150;
    }

    TargetMm = InitialTargetMm;

    BallPID_Reset();
    Running = 1;
}

void BallTask_Stop(void)
{
    if (Running)
    {
        ElapsedMs = Timer_GetMillis() - StartMs;
    }

    Running = 0;

    BallPID_Reset();
    BalanceOutput_Stop();
}

/**
  * @brief 第三问状态机
  * @note  0mm -> +50mm -> -50mm并保持
  */
static void BallTask_Task3Update(
    int16_t PositionMm
)
{
    if (Stage3 == TASK3_GO_POSITIVE)
    {
        TargetMm = TASK3_POSITIVE_MM;

        /*
         * 在+40～+60mm内连续保持100ms，
         * 然后开始折返。
         */
        if (
            BallTask_IsInRange(
                PositionMm,
                TASK3_POSITIVE_MM
            )
        )
        {
            StageStableMs += 10;

            if (
                StageStableMs >=
                TASK3_POSITIVE_STABLE_MS
            )
            {
                Stage3 = TASK3_GO_NEGATIVE;
                StageStableMs = 0;

                TargetMm = TASK3_NEGATIVE_MM;

                /*
                 * 改变目标时清除旧的PID状态，
                 * 防止之前的控制量影响折返。
                 */
                BallPID_Reset();
            }
        }
        else
        {
            StageStableMs = 0;
        }
    }
    else if (Stage3 == TASK3_GO_NEGATIVE)
    {
        TargetMm = TASK3_NEGATIVE_MM;

        /*
         * 在-60～-40mm内连续保持300ms，
         * 认为已经到达并稳定。
         */
        if (
            BallTask_IsInRange(
                PositionMm,
                TASK3_NEGATIVE_MM
            )
        )
        {
            StageStableMs += 10;

            if (
                StageStableMs >=
                TASK3_NEGATIVE_STABLE_MS
            )
            {
                Stage3 = TASK3_HOLD_NEGATIVE;
                StageStableMs = 0;
            }
        }
        else
        {
            StageStableMs = 0;
        }
    }
    else
    {
        /*
         * 第三问完成后继续闭环保持-50mm。
         * 这里不能调用BallTask_Stop()，
         * 否则舵机回平，小球会滑走。
         */
        TargetMm = TASK3_NEGATIVE_MM;
    }
}

void BallTask_Update10ms(void)
{
    uint32_t NowMs;
    int16_t PositionMm;
    float Output;

    if (Running == 0)
    {
        return;
    }

    NowMs = Timer_GetMillis();
    ElapsedMs = NowMs - StartMs;

    /*
 * 摄像头短暂丢帧时保持上一次舵机输出，
 * 不立即回中，也不立即清空PID。
 */
if ((MaixCAM_IsOnline(NowMs) == 0) ||
    (MaixCAM_IsBallValid() == 0))
{
    if (LostStartMs == 0)
    {
        LostStartMs = NowMs;
    }
    else if ((NowMs - LostStartMs) >= BALL_LOST_MS)
    {
        /*
         * 连续丢失超过限定时间才停止。
         */
        Fault = 1;
        BallTask_Stop();
    }

    return;
}

    LostStartMs = 0;

    PositionMm = MaixCAM_GetPositionMm();

    /*
     * TargetMm由CompetitionTask.c统一设置。
     * 此处只负责执行PID控制，不能再次修改目标。
     */
    Output = BallPID_Calculate(
        TargetMm,
        PositionMm
    );

    BalanceOutput_Set(Output);
}

uint8_t BallTask_IsRunning(void)
{
    return Running;
}

uint8_t BallTask_HasFault(void)
{
    return Fault;
}

BallTask_Number BallTask_GetSelected(void)
{
    return Selected;
}

int16_t BallTask_GetTargetMm(void)
{
    return TargetMm;
}

uint32_t BallTask_GetElapsedMs(void)
{
    if (Running)
    {
        return Timer_GetMillis() - StartMs;
    }

    return ElapsedMs;
}

void BallTask_SetTask6TargetMm(
    int16_t NewTargetMm
)
{
    if (NewTargetMm > 50)
    {
        NewTargetMm = 50;
    }
    else if (NewTargetMm < -50)
    {
        NewTargetMm = -50;
    }

    Task6TargetMm = NewTargetMm;

    if (
        Selected == BALL_TASK_6 &&
        Running == 0
    )
    {
        TargetMm = Task6TargetMm;
    }
}

void BallTask_SetTargetMm(int16_t NewTargetMm)
{
    if (NewTargetMm > 150)
    {
        NewTargetMm = 150;
    }
    else if (NewTargetMm < -150)
    {
        NewTargetMm = -150;
    }

     /*
     * 只有目标真正改变时才复位PID。
     * 不能每10ms都复位，否则Kd完全不起作用。
     */
    if (NewTargetMm != TargetMm)
    {
        TargetMm = NewTargetMm;
        BallPID_Reset();
    }
}