#include "pwm.h"

#include "timers.h"

void PWM_StartAll(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
}

void PWM_SetDuties(uint16_t dutyPWM1, uint16_t dutyPWM2, uint16_t dutyPWM3, uint16_t dutyPWM4)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, dutyPWM1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, dutyPWM2);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, dutyPWM3);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, dutyPWM4);
}
