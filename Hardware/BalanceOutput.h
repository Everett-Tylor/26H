#ifndef __BALANCE_OUTPUT_H
#define __BALANCE_OUTPUT_H

/*
 * Output单位：角度
 * 0.0f：舵机中位，摆杆水平
 * 正负值：向两个方向倾斜
 */
void BalanceOutput_Init(void);
void BalanceOutput_Set(float Output);
void BalanceOutput_Stop(void);
float BalanceOutput_GetLast(void);

#endif
