#include "hal/pwm.h"

/* ------------------------------------------------------------------ */
void PWM_StartAll(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
}

/* ------------------------------------------------------------------ */
void PWM_SetDuties(uint16_t duty1, uint16_t duty2, uint16_t duty3, uint16_t duty4)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, duty2);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, duty3);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, duty4);
}

/* ------------------------------------------------------------------ */
void PWM_SetDuty(uint32_t channel, uint16_t duty)
{
    __HAL_TIM_SET_COMPARE(&htim1, channel, duty);
}
