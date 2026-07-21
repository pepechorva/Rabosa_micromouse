#ifndef __ENCODER_H
#define __ENCODER_H

#include "peripherals.h"

/* Handles de los timers en modo encoder */
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

void ENC_Init(void);
int32_t ENC_GetCount1(void);   /* TIM2 – Motor 1 (PA0, PA1) */
int32_t ENC_GetCount2(void);   /* TIM3 – Motor 2 (PA6, PA7) */
void     ENC_ResetCount1(void);
void     ENC_ResetCount2(void);

/* Conteo por canal individual (software polling) */
void     ENC_UpdatePulses(void);         /* Llamar periódicamente */
uint32_t ENC_GetPulse1A(void);           /* PA0 – Motor1 canal A */
uint32_t ENC_GetPulse1B(void);           /* PA1 – Motor1 canal B */
uint32_t ENC_GetPulse2A(void);           /* PA6 – Motor2 canal A */
uint32_t ENC_GetPulse2B(void);           /* PA7 – Motor2 canal B */
void     ENC_ResetPulses(void);

#endif
