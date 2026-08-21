#include "BalanceOutput.h"
#include "stm32f10x.h"

/*
 * 舵机信号线：PA8（TIM1_CH1）
 *
 * PWM频率：50Hz
 * PWM周期：20ms
 *
 * 舵机中位：1500us
 * 最小脉宽：1000us
 * 最大脉宽：2000us
 */
#define SERVO_CENTER_US       1090
#define SERVO_MIN_US          500
#define SERVO_MAX_US          2500

/*
 * 常见180度舵机：
 * 1000us～2000us大约对应0～180度
 * 每度约为1000 / 180 = 5.56us
 */
#define SERVO_US_PER_DEGREE   5.56f

/*
 * 限制摆杆最大倾斜角度。
 * 初次测试先用5度，防止动作过大。
 */
#define SERVO_MAX_DELTA_DEG   90.25f

/*
 * 如果控制方向相反：
 * 将1.0f修改为-1.0f
 */
#define SERVO_DIRECTION       (-1.0f)

static volatile float LastOutput = 0.0f;


/**
  * @brief  浮点数限幅
  */
static float BalanceOutput_Limit(float Value,
                                 float Minimum,
                                 float Maximum)
{
    if (Value > Maximum)
    {
        Value = Maximum;
    }
    else if (Value < Minimum)
    {
        Value = Minimum;
    }

    return Value;
}


/**
  * @brief  设置舵机PWM高电平时间
  * @param  PulseUs：脉宽，单位us
  */
static void BalanceOutput_SetPulseUs(uint16_t PulseUs)
{
    if (PulseUs < SERVO_MIN_US)
    {
        PulseUs = SERVO_MIN_US;
    }
    else if (PulseUs > SERVO_MAX_US)
    {
        PulseUs = SERVO_MAX_US;
    }

    TIM_SetCompare1(TIM1, PulseUs);
}


/**
  * @brief  舵机PWM初始化
  * @note   PA8输出TIM1_CH1 PWM
  */
void BalanceOutput_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    /*
     * 打开GPIOA、复用功能和TIM1时钟
     */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_AFIO |
        RCC_APB2Periph_TIM1,
        ENABLE
    );

    /*
     * PA8配置为复用推挽输出
     */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /*
     * STM32主频为72MHz：
     *
     * 72MHz / 72 = 1MHz
     * 每个计数为1us
     *
     * 20000个计数为20ms
     * PWM频率为50Hz
     */
    TIM_TimeBaseStructInit(&TIM_TimeBaseInitStructure);

    TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;
    TIM_TimeBaseInitStructure.TIM_Period = 20000 - 1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;

    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);

    /*
     * 配置TIM1通道1为PWM模式
     */
    TIM_OCStructInit(&TIM_OCInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_Pulse = SERVO_CENTER_US;

    TIM_OC1Init(TIM1, &TIM_OCInitStructure);

    /*
     * 打开预装载
     */
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);

    /*
     * TIM1是高级定时器，必须开启主输出
     */
    TIM_CtrlPWMOutputs(TIM1, ENABLE);

    /*
     * 启动TIM1
     */
    TIM_Cmd(TIM1, ENABLE);

    /*
     * 上电先回到中位
     */
    LastOutput = 0.0f;
    BalanceOutput_SetPulseUs(SERVO_CENTER_US);
}


/**
  * @brief  根据PID输出控制舵机
  * @param  Output：目标倾斜角度，单位度
  */
void BalanceOutput_Set(float Output)
{
    float LimitedOutput;
    float PulseUs;

    /*
     * 将输出限制在±5度
     */
    LimitedOutput = BalanceOutput_Limit(
        Output,
        -SERVO_MAX_DELTA_DEG,
        SERVO_MAX_DELTA_DEG
    );

    /*
     * 角度转换成PWM脉宽
     */
    PulseUs =
        (float)SERVO_CENTER_US +
        SERVO_DIRECTION *
        LimitedOutput *
        SERVO_US_PER_DEGREE;

    BalanceOutput_SetPulseUs((uint16_t)PulseUs);

    LastOutput = LimitedOutput;
}


/**
  * @brief  停止滚球控制
  * @note   舵机回中，让摆杆恢复水平
  */
void BalanceOutput_Stop(void)
{
    LastOutput = 0.0f;

    BalanceOutput_SetPulseUs(SERVO_CENTER_US);
}


/**
  * @brief  获取最后一次输出角度
  */
float BalanceOutput_GetLast(void)
{
    return LastOutput;
}