#ifndef __PWM_H
#define __PWM_H

#include "peripherals.h"

void PWM_StartAll(void);
void PWM_SetDuties(uint16_t dutyPWM1, uint16_t dutyPWM2, uint16_t dutyPWM3, uint16_t dutyPWM4);

#endif
