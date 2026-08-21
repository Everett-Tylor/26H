#include "stm32f10x.h"

#include "Motor.h"
#include "Encoder.h"
#include "Gray.h"
#include "Key.h"
#include "Control.h"
#include "Timer.h"
#include "OLED.h"
#include "LapTime.h"
#include "MaixCAM.h"
#include "BallTask.h"
#include "CompetitionTask.h"

/* 第2问运行状态 */
static uint8_t Task2_Running = 0;
static uint8_t Task2_Finished = 0;

/* 第2问最终成绩 */
static uint32_t Task2_FinalTimeMs = 0;

/**
  * @brief  计算int16_t绝对值
  */
static int16_t Main_AbsInt16(int16_t Value)
{
    if (Value < 0)
    {
        return (int16_t)(-Value);
    }

    return Value;
}

/**
  * @brief  OLED显示带正负号的毫米数
  * @param  Line：行，1~4
  * @param  Column：列
  * @param  Value：位置，单位mm
  */
static void OLED_ShowSignedMm(
    uint8_t Line,
    uint8_t Column,
    int16_t Value
)
{
    if (Value < 0)
    {
        OLED_ShowChar(Line, Column, '-');
    }
    else
    {
        OLED_ShowChar(Line, Column, '+');
    }

    OLED_ShowNum(
        Line,
        Column + 1,
        Main_AbsInt16(Value),
        3
    );
}

/**
  * @brief  显示时间
  * @note   格式为MM:SS.CC
  */
static void OLED_ShowTime(
    uint8_t Line,
    uint8_t Column,
    uint32_t TimeMs
)
{
    uint32_t TotalCentiseconds;
    uint32_t TotalSeconds;

    uint8_t Minutes;
    uint8_t Seconds;
    uint8_t Centiseconds;

    TotalCentiseconds = TimeMs / 10;
    TotalSeconds = TotalCentiseconds / 100;

    Minutes = (uint8_t)(TotalSeconds / 60);
    Seconds = (uint8_t)(TotalSeconds % 60);
    Centiseconds = (uint8_t)(TotalCentiseconds % 100);

    OLED_ShowNum(Line, Column, Minutes, 2);
    OLED_ShowChar(Line, Column + 2, ':');
    OLED_ShowNum(Line, Column + 3, Seconds, 2);
    OLED_ShowChar(Line, Column + 5, '.');
    OLED_ShowNum(Line, Column + 6, Centiseconds, 2);
}

/**
  * @brief  显示第2问界面
  */
static void OLED_ShowTask2(void)
{
    uint32_t DisplayTimeMs;

    if (Task2_Running)
    {
        DisplayTimeMs = LapTime_GetElapsedMs();
    }
    else
    {
        DisplayTimeMs = Task2_FinalTimeMs;
    }

    OLED_ShowString(1, 1, "TASK 2");

    if (Task2_Running)
    {
        OLED_ShowString(1, 9, "RUN ");
    }
    else if (Task2_Finished)
    {
        OLED_ShowString(1, 9, "DONE");
    }
    else
    {
        OLED_ShowString(1, 9, "WAIT");
    }

    OLED_ShowString(2, 1, "TIME:");
    OLED_ShowTime(2, 7, DisplayTimeMs);

    OLED_ShowString(3, 1, "L:");
    OLED_ShowNum(
        3,
        3,
        Main_AbsInt16(Left_Speed),
        4
    );

    OLED_ShowString(3, 9, "R:");
    OLED_ShowNum(
        3,
        11,
        Main_AbsInt16(Right_Speed),
        4
    );

    if (Task2_Running)
    {
        OLED_ShowString(4, 1, "PC13:STOP      ");
    }
    else
    {
        OLED_ShowString(4, 1, "PC13:START     ");
    }
}

/**
  * @brief  显示第3～6问的任务时间
  */
static void OLED_ShowCompetitionTime(void)
{
    OLED_ShowTime(
        1,
        9,
        CompetitionTask_GetElapsedMs()
    );
}

/**
  * @brief  显示第3～6问界面
  */
static void OLED_ShowCompetitionTask(void)
{
    CompetitionTask_State State;
    CompetitionTask_Number TaskNumber;

    int16_t Position;
    int16_t Target;
    int16_t MaximumError;

    uint32_t DistancePulse;

    TaskNumber = CompetitionTask_GetSelected();
    State = CompetitionTask_GetState();

    Position = CompetitionTask_GetPositionMm();
    Target = CompetitionTask_GetTargetMm();

    MaximumError =
        CompetitionTask_GetMaximumErrorMm();

    DistancePulse =
        CompetitionTask_GetDistancePulse();

    /*
     * 第一行：题号、状态、时间
     */
    OLED_ShowChar(1, 1, 'T');
    OLED_ShowNum(1, 2, TaskNumber, 1);

    if (State == COMPETITION_RUNNING)
    {
        OLED_ShowString(1, 4, "RUN ");
    }
    else if (State == COMPETITION_FINISHED)
    {
        OLED_ShowString(1, 4, "DONE");
    }
    else if (State == COMPETITION_FAULT)
    {
        OLED_ShowString(1, 4, "ERR ");
    }
    else
    {
        OLED_ShowString(1, 4, "WAIT");
    }

    OLED_ShowCompetitionTime();

    /*
     * 第二行：钢球当前位置和目标位置
     */
    OLED_ShowString(2, 1, "P:");
    OLED_ShowSignedMm(2, 3, Position);

    OLED_ShowString(2, 8, "T:");
    OLED_ShowSignedMm(2, 10, Target);

    OLED_ShowString(2, 14, "   ");

    /*
     * 第三行：根据题号显示不同内容
     */
    if (TaskNumber == COMPETITION_TASK_4)
    {
        /*
         * 第4问显示编码器累计距离
         */
        OLED_ShowString(3, 1, "DIS:");

        OLED_ShowNum(
            3,
            5,
            DistancePulse % 100000UL,
            5
        );

        OLED_ShowString(3, 10, "       ");
    }
    else if (TaskNumber == COMPETITION_TASK_3)
    {
        /*
         * 第3问显示滚球阶段
         */
        OLED_ShowString(3, 1, "STAGE:");

        if (CompetitionTask_GetTask3Stage() == 0)
        {
            OLED_ShowString(3, 7, "TO +5CM  ");
        }
        else if (CompetitionTask_GetTask3Stage() == 1)
        {
            OLED_ShowString(3, 7, "TO -5CM  ");
        }
        else
        {
            OLED_ShowString(3, 7, "FINISHED ");
        }
    }
    else
    {
        /*
         * 第5、6问显示最大位置误差
         */
        OLED_ShowString(3, 1, "MAX ERR:");

        OLED_ShowNum(
            3,
            9,
            Main_AbsInt16(MaximumError),
            3
        );

        OLED_ShowString(3, 12, "mm  ");
    }

    /*
     * 第四行：操作提示或故障信息
     */
    if (State == COMPETITION_FAULT)
    {
        if (MaixCAM_IsBallValid() == 0)
        {
            OLED_ShowString(4, 1, "CAM/BALL LOST   ");
        }
        else
        {
            OLED_ShowString(4, 1, "TASK TIMEOUT    ");
        }
    }
    else if (State == COMPETITION_RUNNING)
    {
        OLED_ShowString(4, 1, "PC14:STOP       ");
    }
    else
    {
        OLED_ShowString(4, 1, "S:START L:SELECT");
    }
}

/**
  * @brief  启动第2问
  */
static void Task2_Start(void)
{
    /*
     * 第3～6问正在运行时，不允许启动第2问
     */
    if (CompetitionTask_IsRunning())
    {
        return;
    }

    Task2_Finished = 0;
    Task2_FinalTimeMs = 0;

    /*
     * LapTime_Start()负责：
     * 1. 计时清零；
     * 2. 清除上一圈完成标志；
     * 3. 等待小车离开起点后检测终点。
     */
    LapTime_Start();

    /*
     * 设置第2问基础速度
     */
    Control_SetBaseSpeed(54);

    /*
     * Control_Start()内部会清零编码器累计距离
     */
    Control_Start();

    Task2_Running = 1;
}

/**
  * @brief  手动停止第2问
  */
static void Task2_ManualStop(void)
{
    if (Task2_Running == 0)
    {
        return;
    }

    /*
     * 停止前保存时间
     */
    Task2_FinalTimeMs = LapTime_GetElapsedMs();

    Control_Stop();
    LapTime_Stop();

    Task2_Running = 0;
    Task2_Finished = 0;
}

/**
  * @brief  第2问跑完一圈自动完成
  */
static void Task2_AutoFinish(void)
{
    if (Task2_Running == 0)
    {
        return;
    }

    /*
     * 先保存最终圈速，再停止计时
     */
    Task2_FinalTimeMs = LapTime_GetElapsedMs();

    Control_Stop();
    LapTime_Stop();

    Task2_Running = 0;
    Task2_Finished = 1;
}

int main(void)
{
    uint32_t OLED_LastUpdate;

    /*
     * 电机和传感器初始化
     */
    Motor_Init();
    Encoder_Init();
    Gray_Init();
    Key_Init();

    /*
     * 小车控制和计时初始化
     */
    Control_Init();
    LapTime_Init();

    /*
     * MaixCAM和滚球控制初始化
     */
    MaixCAM_Init();
    BallTask_Init();
    CompetitionTask_Init();

    /*
     * OLED初始化
     */
    OLED_Init();
    OLED_Clear();

    /*
     * 必须最后启动SysTick定时中断
     */
    Timer_Init();

    /*
     * 小车基础速度
     */
    Control_SetBaseSpeed(65);

    /*
     * 左右轮速度PI参数
     */
    Control_SetSpeedPI(
        5.0f,
        0.05f
    );

    /*
     * 灰度循迹PD参数
     */
    Control_SetTrackPD(
        0.070f,
        0.025f
    );

    /*
     * 开机默认显示第2问
     */
    OLED_ShowTask2();

    OLED_LastUpdate = Timer_GetMillis();

    while (1)
    {

        /*
         * PC13短按：
         * 启动或手动停止第2问
         */
        if (Key_GetStartPress())
        {
            /*
             * 第3～6问运行时，PC13无效
             */
            if (CompetitionTask_IsRunning() == 0)
            {
                if (Task2_Running)
                {
                    Task2_ManualStop();
                }
                else
                {
                    Task2_Start();
                }

                OLED_Clear();
                OLED_ShowTask2();
            }
        }

        /*
         * PC14长按：
         * 第3、4、5、6问循环选择
         */
        if (Key_GetBallLongPress())
        {
            /*
             * 第2问运行时不允许切换题目
             */
            if (Task2_Running == 0)
            {
                CompetitionTask_SelectNext();

                /*
                 * 进入第3～6问界面后，
                 * 清除第2问完成显示状态。
                 */
                Task2_Finished = 0;

                OLED_Clear();
                OLED_ShowCompetitionTask();
            }
        }

        /*
         * PC14短按：
         * 启动或停止当前选择的第3～6问
         */
        if (Key_GetBallShortPress())
        {
            /*
             * 第2问运行时，PC14无效
             */
            if (Task2_Running == 0)
            {
                if (CompetitionTask_IsRunning())
                {
                    CompetitionTask_Stop();
                }
                else
                {
                    CompetitionTask_Start();
                }

                OLED_Clear();
                OLED_ShowCompetitionTask();
            }
        }

        /*
         * 第2问检测到跑完一圈，
         * 自动保存时间并停车
         */
        if (Task2_Running &&
            LapTime_IsFinished())
        {
            Task2_AutoFinish();

            OLED_Clear();
            OLED_ShowTask2();
        }

        /*
         * 每100ms刷新一次OLED
         */
        if ((Timer_GetMillis() -
             OLED_LastUpdate) >= 100)
        {
            OLED_LastUpdate = Timer_GetMillis();

            if (Task2_Running ||
                Task2_Finished)
            {
                OLED_ShowTask2();
            }
            else
            {
                OLED_ShowCompetitionTask();
            }
        }
    }
}