#include "Control.h"
#include "Motor.h"
#include "Encoder.h"
#include "Gray.h"

/*
 * 初始参数，不是最终参数。
 *
 * 调参顺序：
 * 1. TRACK_KP、TRACK_KD先设0，只调速度PI
 * 2. 速度稳定后再调循迹PD
 */
#define SPEED_KP_DEFAULT       5.0f
#define SPEED_KI_DEFAULT       0.05f

#define TRACK_KP_DEFAULT       0.070f
#define TRACK_KD_DEFAULT       0.025f

#define CONTROL_BASE_SPEED     80

#define SPEED_TARGET_MAX       90
#define SPEED_INTEGRAL_MAX     600.0f

#define TRACK_OUTPUT_MAX       38

#define LEFT_SPEED_TRIM   0
#define RIGHT_SPEED_TRIM  0

volatile int16_t Left_Speed = 0;
volatile int16_t Right_Speed = 0;

volatile int16_t Control_LeftTarget = 0;
volatile int16_t Control_RightTarget = 0;

volatile int16_t Control_LeftOutput = 0;
volatile int16_t Control_RightOutput = 0;

static volatile uint8_t Control_Running = 0;

static int16_t Control_BaseSpeed = CONTROL_BASE_SPEED;
/* 软启动过程中的实际目标速度 */
static int16_t Control_CurrentBaseSpeed = 0;

static float Speed_Kp = SPEED_KP_DEFAULT;
static float Speed_Ki = SPEED_KI_DEFAULT;

static float Track_Kp = TRACK_KP_DEFAULT;
static float Track_Kd = TRACK_KD_DEFAULT;

static float Left_Integral = 0;
static float Right_Integral = 0;

static int16_t Track_LastError = 0;

static volatile uint32_t LeftDistancePulse = 0;
static volatile uint32_t RightDistancePulse = 0;


static int16_t Control_LimitInt16(
	int16_t Value,
	int16_t Minimum,
	int16_t Maximum
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

static float Control_LimitFloat(
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

static int16_t Control_SpeedPI(
	int16_t Target,
	int16_t Actual,
	float *Integral
)
{
	float Error;
	float Output;

	Error = (float)(Target - Actual);

	*Integral += Error;

	*Integral = Control_LimitFloat(
		*Integral,
		-SPEED_INTEGRAL_MAX,
		SPEED_INTEGRAL_MAX
	);

	Output =
		Speed_Kp * Error +
		Speed_Ki * (*Integral);

	Output = Control_LimitFloat(
		Output,
		-MOTOR_MAX_OUTPUT,
		MOTOR_MAX_OUTPUT
	);

	return (int16_t)Output;
}

static uint16_t Control_AbsSpeed(int16_t Value)
{
    if (Value < 0)
    {
        return (uint16_t)(-Value);
    }

    return (uint16_t)Value;
}

void Control_ResetDistance(void)
{
    LeftDistancePulse = 0;
    RightDistancePulse = 0;
}

uint32_t Control_GetLeftDistancePulse(void)
{
    return LeftDistancePulse;
}

uint32_t Control_GetRightDistancePulse(void)
{
    return RightDistancePulse;
}

uint32_t Control_GetDistancePulse(void)
{
    return
        (LeftDistancePulse + RightDistancePulse) / 2;
}

void Control_Init(void)
{
	
	Left_Speed = 0;
	Right_Speed = 0;

	Control_LeftTarget = 0;
	Control_RightTarget = 0;

	Control_LeftOutput = 0;
	Control_RightOutput = 0;

	Control_BaseSpeed = CONTROL_BASE_SPEED;
  Control_CurrentBaseSpeed = 0;
	
	Left_Integral = 0;
	Right_Integral = 0;

	Track_LastError = 0;
	Control_Running = 0;
	
	
    /* 初始化编码器累计距离 */
    Control_ResetDistance();


	Motor_Stop();
}

void Control_Start(void)
{
	Control_ResetDistance();
	Left_Integral = 0;
	Right_Integral = 0;

	Control_CurrentBaseSpeed = 0;
	
	Track_LastError = Gray_GetError();

	Control_Running = 1;
}

void Control_Stop(void)
{
	Control_Running = 0;
	Control_CurrentBaseSpeed = 0;

	Control_LeftTarget = 0;
	Control_RightTarget = 0;

	Control_LeftOutput = 0;
	Control_RightOutput = 0;

	Left_Integral = 0;
	Right_Integral = 0;

	Motor_Stop();
}

void Control_SetBaseSpeed(int16_t Speed)
{
	Speed = Control_LimitInt16(
		Speed,
		0,
		SPEED_TARGET_MAX
	);

	Control_BaseSpeed = Speed;
}

void Control_SetSpeedPI(float Kp, float Ki)
{
	Speed_Kp = Kp;
	Speed_Ki = Ki;

	Left_Integral = 0;
	Right_Integral = 0;
}

void Control_SetTrackPD(float Kp, float Kd)
{
	Track_Kp = Kp;
	Track_Kd = Kd;

	Track_LastError = Gray_GetError();
}

void Control_Update10ms(void)
{
	int16_t TrackError;
	int16_t TrackDerivative;
	int16_t TrackOutput;

	/* 每10ms读取一次编码器增量 */
	Left_Speed = Encoder_GetLeft();
	Right_Speed = Encoder_GetRight();
	
	if (Control_Running)
{
    LeftDistancePulse += Control_AbsSpeed(Left_Speed);
    RightDistancePulse += Control_AbsSpeed(Right_Speed);
}

	/* 每10ms读取一次灰度 */
	Gray_Read();

	if (Control_Running == 0)
	{
		Motor_Stop();
		return;
	}
	
	/* 软启动：每10ms增加1，约200ms达到速度20 */
  if (Control_CurrentBaseSpeed < Control_BaseSpeed)
  {
    Control_CurrentBaseSpeed++;
  }
  else if (Control_CurrentBaseSpeed > Control_BaseSpeed)
  {
    Control_CurrentBaseSpeed = Control_BaseSpeed;
  }

	TrackError = Gray_GetError();
	TrackDerivative = TrackError - Track_LastError;

	TrackOutput =
		(int16_t)
		(
			Track_Kp * TrackError +
			Track_Kd * TrackDerivative
		);

	TrackOutput = Control_LimitInt16(
		TrackOutput,
		-TRACK_OUTPUT_MAX,
		TRACK_OUTPUT_MAX
	);

	
	

	/*
	 * 黑线偏右：
	 * 左轮加速，右轮减速，小车向右转。
	 */
	Control_LeftTarget =
    Control_CurrentBaseSpeed
    + TrackOutput
    + LEFT_SPEED_TRIM;

Control_RightTarget =
    Control_CurrentBaseSpeed
    - TrackOutput
    + RIGHT_SPEED_TRIM;

	Control_LeftTarget = Control_LimitInt16(
		Control_LeftTarget,
		0,
		SPEED_TARGET_MAX
	);

	Control_RightTarget = Control_LimitInt16(
		Control_RightTarget,
		0,
		SPEED_TARGET_MAX
	);

	Control_LeftOutput = Control_SpeedPI(
		Control_LeftTarget,
		Left_Speed,
		&Left_Integral
	);

	Control_RightOutput = Control_SpeedPI(
		Control_RightTarget,
		Right_Speed,
		&Right_Integral
	);

	Motor_SetSpeed(
		Control_LeftOutput,
		Control_RightOutput
	);

	Track_LastError = TrackError;
}

uint8_t Control_IsRunning(void)
{
	return Control_Running;
}