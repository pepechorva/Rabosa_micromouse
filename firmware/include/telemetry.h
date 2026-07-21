#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>

void Send_Teleplot_DutyValues(uint16_t dutyA8, uint16_t dutyA9, uint16_t dutyA10, uint16_t dutyA11);
void Send_Teleplot_MotorEncoder(int16_t enc1, int16_t speed1, int16_t enc2, int16_t speed2);
void Send_Teleplot_EncoderPulses(uint32_t p1a, uint32_t p1b, uint32_t p2a, uint32_t p2b);

#endif