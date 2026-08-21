#include "BallPID.h"

/*
 * PID允许输出的最大角度。
 * 限制倾斜幅度，防止钢球速度过快。
 */
#define BALL_RIGHT_OUTPUT_MAX        12.0f
#define BALL_LEFT_OUTPUT_MAX         5.0f
/*
 * 距离目标超过40mm时，
 * 才使用最小启动角克服静摩擦。
 */
#define BALL_NEAR_TARGET_MM          40.0f

/*
 * 左右最小启动角不同：
 * 向右较弱，所以右侧启动角大一些；
 * 向左过猛，所以左侧启动角小一些。
 */
#define BALL_RIGHT_START_MIN_DEG     8.5f
#define BALL_LEFT_START_MIN_DEG      2.5f

/*
 * 补偿水管、舵机和摩擦造成的左右不对称。
 *
 * 当前假设：
 * Output > 0：钢球向右运动
 * Output < 0：钢球向左运动
 */
#define BALL_RIGHT_GAIN              1.15f
#define BALL_LEFT_GAIN               0.40f

/*
 * 进入目标附近后限制P项，
 * 防止比例项继续猛烈推动钢球。
 */
#define BALL_BRAKE_RANGE_MM          25.0f
#define BALL_NEAR_P_MAX              1.5f

/*
 * 当位置误差和速度同时较小时，
 * 进入目标保持状态。
 */
#define BALL_HOLD_ERROR_MM           5.0f
#define BALL_HOLD_SPEED              2.0f
#define BALL_HOLD_OUTPUT_MAX         1.0f

/*
 * 位置滤波系数。
 *
 * 数值越大越平滑，但是响应会稍慢。
 * 0.70表示保留70%旧值、加入30%新值。
 */
#define BALL_POSITION_FILTER         0.70f

/*
 * 速度滤波系数。
 * 用来减小MaixCAM坐标跳动对微分项的影响。
 */
#define BALL_SPEED_FILTER            0.40f

#define BALL_LEFT_BRAKE_KD           2.20f
#define BALL_LEFT_BRAKE_RANGE_MM     45.0f

/*
 * PID参数。
 *
 * 注意：
 * 当前BallSpeed的单位是“每10ms变化的毫米数”，
 * 没有除以0.01秒，所以Kd使用1.20左右。
 */
static float Kp = 0.06f;
static float Ki = 0.000f;
static float Kd = 1.20f;

static float Integral;

static float FilteredPosition;
static float LastPosition;
static float FilteredSpeed;

static uint8_t FirstCalculate;


/**
  * @brief  将数值限制在指定范围内
  */
static float BallPID_Limit(
    float Value,
    float Minimum,
    float Maximum
)
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
  * @brief  计算浮点数绝对值
  */
static float BallPID_Abs(float Value)
{
    if (Value < 0.0f)
    {
        return -Value;
    }

    return Value;
}


/**
  * @brief  初始化钢球PID
  */
void BallPID_Init(void)
{
    BallPID_Reset();
}


/**
  * @brief  设置PID参数
  */
void BallPID_SetParameter(
    float NewKp,
    float NewKi,
    float NewKd
)
{
    Kp = NewKp;
    Ki = NewKi;
    Kd = NewKd;

    BallPID_Reset();
}


/**
  * @brief  清除PID历史数据
  */
void BallPID_Reset(void)
{
    Integral = 0.0f;

    FilteredPosition = 0.0f;
    LastPosition = 0.0f;
    FilteredSpeed = 0.0f;

    FirstCalculate = 1;
}


/**
  * @brief  计算钢球PID输出
  * @param  TargetMm：目标位置，单位mm
  * @param  PositionMm：当前位置，单位mm
  * @retval 水管需要倾斜的角度
  */
float BallPID_Calculate(
    int16_t TargetMm,
    int16_t PositionMm
)
{
    float Error;
    float AbsError;

    float RawSpeed;
    float POutput;
    float IOutput;
    float DOutput;
    float Output;

    /*
     * 第一次计算时没有历史位置，
     * 直接用当前位置初始化滤波器。
     */
    if (FirstCalculate)
    {
        FilteredPosition = (float)PositionMm;
        LastPosition = FilteredPosition;

        FilteredSpeed = 0.0f;
        FirstCalculate = 0;
    }
    else
    {
        /*
         * 对摄像头位置进行低通滤波，
         * 减小坐标跳动。
         */
        FilteredPosition =
            BALL_POSITION_FILTER *
            FilteredPosition +
            (1.0f - BALL_POSITION_FILTER) *
            (float)PositionMm;

        /*
         * 计算每个10ms周期的位置变化。
         *
         * 正值：钢球正在向坐标正方向运动；
         * 负值：钢球正在向坐标负方向运动。
         */
        RawSpeed =
            FilteredPosition -
            LastPosition;

        LastPosition = FilteredPosition;

        /*
         * 对速度再次滤波，
         * 防止微分项忽正忽负。
         */
        FilteredSpeed =
            BALL_SPEED_FILTER *
            FilteredSpeed +
            (1.0f - BALL_SPEED_FILTER) *
            RawSpeed;
    }

    /*
     * 位置误差：
     * 正值表示需要向右移动；
     * 负值表示需要向左移动。
     */
    Error =
        (float)TargetMm -
        FilteredPosition;

    AbsError = BallPID_Abs(Error);

    /*
     * 积分计算。
     *
     * 当前Ki为0，积分暂时不会参与输出，
     * 但保留接口方便后续调节。
     */
    Integral += Error * 0.01f;

    Integral = BallPID_Limit(
        Integral,
        -100.0f,
        100.0f
    );

    /*
     * 分别计算P、I、D输出。
     */
    POutput = Kp * Error;
    IOutput = Ki * Integral;

    /*
     * 速度阻尼项必须使用负号。
     *
     * 当钢球向右滚动时，
     * FilteredSpeed为正，
     * DOutput产生向左的制动力。
     */
    /*
 * 从右边返回-50mm时单独加强刹车。
 *
 * TargetMm < 0：当前目标是左侧-50mm
 * FilteredSpeed < 0：钢球正在向左运动
 * Error > -45：距离左侧目标已经不足45mm
 *
 * 此时DOutput为正，使水管向右倾斜，对向左运动的球制动。
 */
if (
    (TargetMm < 0) &&
    (FilteredSpeed < 0.0f) &&
    (Error > -BALL_LEFT_BRAKE_RANGE_MM)
)
{
    DOutput =
        -BALL_LEFT_BRAKE_KD *
        FilteredSpeed;
}
else
{
    DOutput =
        -Kd *
        FilteredSpeed;
}

    /*
     * 接近目标25mm以内时，
     * 只限制继续推动钢球的P项。
     *
     * 不限制D项，保留反向刹车能力。
     */
    if (AbsError <= BALL_BRAKE_RANGE_MM)
    {
        POutput = BallPID_Limit(
            POutput,
            -BALL_NEAR_P_MAX,
            BALL_NEAR_P_MAX
        );
    }

    /*
     * 位置推动量 + 积分量 + 速度制动力。
     */
    Output =
        POutput +
        IOutput +
        DOutput;

    /*
     * 目标附近而且钢球速度已经较低时，
     * 进入轻微保持状态。
     *
     * 此时不再使用较强的速度制动，
     * 避免球停止后又被反方向推出去。
     */
    if (
        (AbsError <= BALL_HOLD_ERROR_MM) &&
        (BallPID_Abs(FilteredSpeed) <=
         BALL_HOLD_SPEED)
    )
    {
        Output =
            POutput +
            IOutput;

        /*
 * 左右分别限制最大倾角：
 * 右边保持原力度，左边限制为7°，防止折返时冲过头。
 */
Output = BallPID_Limit(
    Output,
    -BALL_LEFT_OUTPUT_MAX,
    BALL_RIGHT_OUTPUT_MAX
);
/*
 * 临时测试：
 * 目标为左侧，且球正在向左运动时，
 * 无条件输出+5°向右刹车。
 */
if (
    (TargetMm == -50) &&
    (FilteredSpeed < -0.10f)
)
{
    Output = 20.0f;
}

return Output;
        return Output;
    }

    /*
 * 根据目标方向调节推动力度。
 * Error > 0：目标在右边，使用右侧力度。
 * Error < 0：目标在左边，降低左侧力度。
 *
 * 当输出已经反向制动时，不削弱制动力。
 */
if ((Error > 0.0f) && (Output > 0.0f))
{
    Output *= BALL_RIGHT_GAIN;
}
else if ((Error < 0.0f) && (Output < 0.0f))
{
    Output *= BALL_LEFT_GAIN;
}

    /*
     * 只有距离目标超过40mm时，
     * 才强制使用最小启动角。
     *
     * 目标附近绝对不能强制最小角度，
     * 否则钢球会不断被推出目标位置。
     */
    if (Error > BALL_NEAR_TARGET_MM)
    {
        if (Output < BALL_RIGHT_START_MIN_DEG)
        {
            Output = BALL_RIGHT_START_MIN_DEG;
        }
    }
    else if (Error < -BALL_NEAR_TARGET_MM)
    {
        if (Output > -BALL_LEFT_START_MIN_DEG)
        {
            Output = -BALL_LEFT_START_MIN_DEG;
        }
    }

    /*
     * 最终输出限制为±6°。
     */
    /*
 * 左右分别限制最大倾角：
 * 右边保持原力度，左边限制为7°，防止折返时冲过头。
 */
Output = BallPID_Limit(
    Output,
    -BALL_LEFT_OUTPUT_MAX,
    BALL_RIGHT_OUTPUT_MAX
);

    return Output;
}