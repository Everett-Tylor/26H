#include "MaixCAM.h"
#include "Timer.h"

#define MAIXCAM_RX_SIZE       32
#define MAIXCAM_OFFLINE_MS    2000


static char RxBuffer[MAIXCAM_RX_SIZE];
static volatile uint8_t RxIndex = 0;

static volatile int16_t BallPositionMm = 0;
static volatile uint8_t BallValid = 0;
static volatile uint32_t LastFrameMs = 0;

/* 串口调试计数 */
static volatile uint32_t RxByteCount = 0;
static volatile uint32_t ValidFrameCount = 0;
static volatile uint32_t ErrorFrameCount = 0;


/**
  * @brief  解析带正负号的整数
  */
static uint8_t MaixCAM_ParseNumber(
    const char *Text,
    int16_t *Value,
    const char **End
)
{
    int32_t Number = 0;
    int8_t Sign = 1;
    uint8_t HasDigit = 0;

    if (*Text == '-')
    {
        Sign = -1;
        Text++;
    }
    else if (*Text == '+')
    {
        Text++;
    }

    while (*Text >= '0' && *Text <= '9')
    {
        HasDigit = 1;

        Number = Number * 10 + (*Text - '0');

        /* 防止错误数据溢出 */
        if (Number > 1000)
        {
            return 0;
        }

        Text++;
    }

    if (HasDigit == 0)
    {
        return 0;
    }

    *Value = (int16_t)(Number * Sign);
    *End = Text;

    return 1;
}


/**
  * @brief  解析一帧MaixCAM数据
  * @note   正确格式：$BALL,位置毫米,有效标志
  *         示例：$BALL,-35,1
  */
static void MaixCAM_ParseFrame(char *Frame)
{
    const char *End;
    int16_t Position;
    uint8_t Valid;

    /*
     * 检查帧头：
     * $BALL,
     */
    if (
        Frame[0] != '$' ||
        Frame[1] != 'B' ||
        Frame[2] != 'A' ||
        Frame[3] != 'L' ||
        Frame[4] != 'L' ||
        Frame[5] != ','
    )
    {
        ErrorFrameCount++;
        return;
    }

    /*
     * 从Frame[6]开始解析位置
     */
    if (
        MaixCAM_ParseNumber(
            &Frame[6],
            &Position,
            &End
        ) == 0
    )
    {
        ErrorFrameCount++;
        return;
    }

    /*
     * 位置后面必须是逗号
     */
    if (End[0] != ',')
    {
        ErrorFrameCount++;
        return;
    }

    /*
     * 有效标志只能是0或1
     */
    if (End[1] == '0')
    {
        Valid = 0;
    }
    else if (End[1] == '1')
    {
        Valid = 1;
    }
    else
    {
        ErrorFrameCount++;
        return;
    }

    /*
     * 有效标志后面必须结束
     */
    if (End[2] != '\0')
    {
        ErrorFrameCount++;
        return;
    }

    BallPositionMm = Position;
    BallValid = Valid;
    LastFrameMs = Timer_GetMillis();

    ValidFrameCount++;
}


/**
  * @brief  USART1初始化
  * @note
  * MaixCAM A19/TX -> STM32 PA10/RX
  * MaixCAM A18/RX <- STM32 PA9/TX
  * MaixCAM GND    -> STM32 GND
  */
void MaixCAM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_USART1,
        ENABLE
    );

    /*
     * PA9：USART1发送
     */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /*
     * PA10：USART1接收
     */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_StructInit(&USART_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength =
        USART_WordLength_8b;
    USART_InitStructure.USART_StopBits =
        USART_StopBits_1;
    USART_InitStructure.USART_Parity =
        USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl =
        USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode =
        USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART1, &USART_InitStructure);

    /*
     * 清除可能残留的接收标志
     */
    USART_ClearFlag(USART1, USART_FLAG_RXNE);

    /*
     * 开启接收中断
     */
    USART_ITConfig(
        USART1,
        USART_IT_RXNE,
        ENABLE
    );

    NVIC_InitStructure.NVIC_IRQChannel =
        USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority =
        1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority =
        1;
    NVIC_InitStructure.NVIC_IRQChannelCmd =
        ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART1, ENABLE);

    RxIndex = 0;
    BallPositionMm = 0;
    BallValid = 0;
    LastFrameMs = 0;

    RxByteCount = 0;
    ValidFrameCount = 0;
    ErrorFrameCount = 0;
}


/**
  * @brief  USART1接收中断处理
  */
void MaixCAM_RxIRQHandler(void)
{
    char Data;

    if (
        USART_GetITStatus(
            USART1,
            USART_IT_RXNE
        ) == RESET
    )
    {
        return;
    }

    Data = (char)USART_ReceiveData(USART1);
    RxByteCount++;

    /*
     * 收到$表示新的一帧开始
     */
    if (Data == '$')
    {
        RxIndex = 0;
        RxBuffer[RxIndex++] = Data;
    }
    /*
     * 没有收到帧头之前，忽略其他字符
     */
    else if (RxIndex == 0)
    {
        return;
    }
    /*
     * 忽略回车
     */
    else if (Data == '\r')
    {
        return;
    }
    /*
     * 换行表示一帧结束
     */
    else if (Data == '\n')
    {
        RxBuffer[RxIndex] = '\0';

        MaixCAM_ParseFrame(RxBuffer);

        RxIndex = 0;
    }
    /*
     * 保存普通数据
     */
    else if (RxIndex < MAIXCAM_RX_SIZE - 1)
    {
        RxBuffer[RxIndex++] = Data;
    }
    /*
     * 缓冲区溢出，丢弃这一帧
     */
    else
    {
        RxIndex = 0;
        ErrorFrameCount++;
    }
}


int16_t MaixCAM_GetPositionMm(void)
{
    return BallPositionMm;
}


uint8_t MaixCAM_IsBallValid(void)
{
    return BallValid;
}


uint8_t MaixCAM_IsOnline(uint32_t NowMs)
{
    /*
     * 必须至少收到过一帧
     */
    if (ValidFrameCount == 0)
    {
        return 0;
    }

    if (NowMs - LastFrameMs > MAIXCAM_OFFLINE_MS)
    {
        return 0;
    }

    return 1;
}


uint32_t MaixCAM_GetRxByteCount(void)
{
    return RxByteCount;
}


uint32_t MaixCAM_GetValidFrameCount(void)
{
    return ValidFrameCount;
}


uint32_t MaixCAM_GetErrorFrameCount(void)
{
    return ErrorFrameCount;
}