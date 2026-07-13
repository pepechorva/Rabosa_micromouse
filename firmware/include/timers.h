#ifndef __TIMERS_H
#define __TIMERS_H

#include "peripherals.h"

void MX_TIM1_Init(void);
// Exponer el handle del timer para módulos que lo necesiten (ej: drv8833)
extern TIM_HandleTypeDef htim1;

#endif
