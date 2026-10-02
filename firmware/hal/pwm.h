#ifndef HAL_PWM_H
#define HAL_PWM_H

#include "peripherals.h"

/* Handles de timers PWM expuestos */
extern TIM_HandleTypeDef htim1;

void PWM_StartAll(void);
void PWM_SetDuties(uint16_t duty1, uint16_t duty2, uint16_t duty3, uint16_t duty4);
void PWM_SetDuty(uint32_t channel, uint16_t duty);

#endif /* HAL_PWM_H */
