#ifndef __ADC_H
#define __ADC_H

#include "peripherals.h"

typedef enum {
    ADC_PIN_A2 = 2,
    ADC_PIN_A3 = 3,
    ADC_PIN_A4 = 4,
    ADC_PIN_A5 = 5,
    ADC_PIN_B0 = 8,
    ADC_PIN_B1 = 9
} ADC_Pin_t;

void MX_ADC1_Init(void);
uint16_t ADC_ReadChannel(uint32_t channel);
uint16_t ADC_ReadPin(ADC_Pin_t pin);

#endif
