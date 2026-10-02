#ifndef APP_TELEMETRY_H
#define APP_TELEMETRY_H

#include <stdint.h>

/*
 * Telemetría: envío de datos en formato Teleplot.
 * Esta capa pertenece a APP porque empaqueta y formatea los datos
 * para su transmisión, independientemente del medio físico (UART).
 */

void TELEM_SendDutyValues(uint16_t d1a, uint16_t d1b, uint16_t d2a, uint16_t d2b);
void TELEM_SendMotorEncoder(int16_t enc1, int16_t speed1, int16_t enc2, int16_t speed2);
void TELEM_SendEncoderPulses(uint32_t p1a, uint32_t p1b, uint32_t p2a, uint32_t p2b);

/* Funciones adicionales */
void TELEM_SendString(const char *label, const char *value);
void TELEM_SendFloat(const char *label, float value);
void TELEM_SendInt(const char *label, int32_t value);
void TELEM_SendIRValues(const uint16_t *ir, uint8_t count);
void TELEM_SendIMU(float ax, float ay, float az, float gx, float gy, float gz);

/* Evento de botón ya numerado (1 = MENU · 2 = UP · 4 = DOWN · 8 = OK).
 * Ver BUTTONS_GetEvent() en app/buttons.h. */
void TELEM_SendButton(uint8_t numero);

#endif /* APP_TELEMETRY_H */
