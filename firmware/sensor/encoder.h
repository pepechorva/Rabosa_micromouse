#ifndef SENSOR_ENCODER_H
#define SENSOR_ENCODER_H

#include <stdint.h>
#include <stdbool.h>
#include "peripherals.h"

/*
 * Encoders de cuadratura para motores N20.
 *   Motor 1 → TIM2 (PA0=CH1, PA1=CH2)
 *   Motor 2 → TIM3 (PA6=CH1, PA7=CH2)
 *
 * La API sigue el patrón de Bulebots/mmlib:
 *   - update_encoder_readings() se llama periódicamente (p.ej. cada systick)
 *   - max_likelihood_counter_diff() maneja el overflow del contador hardware
 *   - Las funciones get_encoder_* proporcionan: diferencia de pulsos, total
 *     acumulado, distancia en micrómetros y velocidad en m/s.
 */

#define MICROMETERS_PER_METER   1000000L
#define SYSTICK_FREQUENCY_HZ    1000    /* 1 ms tick */

/* ------------------------------------------------------------------
 * Datos del motor + encoder (especificación del fabricante, 2026-09-21)
 *   Motor:    GA12-N20-S0310D-E (B1215), 6 V 1500 RPM
 *   Reductora GA12 de engranajes rectos con relación 1:10
 *   Encoder:  magnético de 2 canales, 7 PPR sobre el eje del MOTOR
 *   Rueda:    **Ø24 mm con la goma puesta** (llanta Ø20 + goma Ø20 interior / Ø24 exterior)
 *
 * El modo encoder es TI12 (x4), así que cada vuelta de RUEDA da:
 *      4 x PPR x reducción = 4 x 7 x 10 = 280 cuentas
 * y cada cuenta serían π x 24 mm / 280 ≈ 0,269 mm.
 *
 * Y el nominal se queda corto: empujando el robot 1 000 mm con las **ruedas nuevas** salieron **287,0 µm
 * por cuenta** en la pasada recta, un **6,6 %** más que los 269,3 teóricos. Eso dice que el **diámetro
 * real en reposo es mayor de 24 mm** (287 implica un Ø efectivo de 24,7 ⇒ ≈25,4 sin aplastar): lo más
 * probable es que el **calibre comprima la goma blanda** al medirla. Manda la medida, y es la que trae
 * `ENCODER_UM_POR_CUENTA_DEF`.
 * ------------------------------------------------------------------ */
#define ENCODER_PPR_MOTOR         7.0f     /* pulsos por vuelta del eje del motor (por canal) */
#define ENCODER_REDUCCION         10.0f    /* relación de la reductora GA12 */
#define ENCODER_CUENTAS_POR_VUELTA (4.0f * ENCODER_PPR_MOTOR * ENCODER_REDUCCION)
#define RUEDA_DIAMETRO_MM         24.0f    /* Ø de la rueda CON GOMA medido con el calibre (comprime la goma) */
#define ENCODER_UM_POR_CUENTA \
    (3.14159265f * RUEDA_DIAMETRO_MM * 1000.0f / ENCODER_CUENTAS_POR_VUELTA)

/** @brief Factor con el que arranca el firmware, en µm por cuenta: el **medido**, no el nominal.
 *
 * Salió de empujar el robot 1 000 mm entre dos marcas del suelo, en pasadas que solo valen si las
 * cuentas de las dos ruedas salen parecidas (si el robot gira, la media no es el avance y el número sale
 * alto). Con las **ruedas nuevas** (30-sep) la pasada recta dio **287,0**; frente a los 269,3 del nominal
 * de Ø24 hay un 6,6 %, que es lo que apunta a un diámetro real mayor.
 *
 * @note Se ajusta en caliente con `set_micrometers_per_count()`, y hay que **repetirlo cada vez que
 *       cambie la rueda**: diámetro, grosor de goma o aplastamiento.
 */
#define ENCODER_UM_POR_CUENTA_DEF 287.0f

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

/* ------------------------------------------------------------------
 * Inicialización del hardware (TIM encoder mode)
 * ------------------------------------------------------------------ */
void ENC_Init(void);

/* ------------------------------------------------------------------
 * Lectura raw del contador hardware
 * ------------------------------------------------------------------ */
int32_t ENC_GetCount1(void);   /* TIM2 counter (Motor 1) */
int32_t ENC_GetCount2(void);   /* TIM3 counter (Motor 2) */
void    ENC_ResetCount1(void);
void    ENC_ResetCount2(void);

/* ------------------------------------------------------------------
 * Conteo por flanco individual (software polling) – compatibilidad
 * ------------------------------------------------------------------ */
void     ENC_UpdatePulses(void);
uint32_t ENC_GetPulse1A(void);
uint32_t ENC_GetPulse1B(void);
uint32_t ENC_GetPulse2A(void);
uint32_t ENC_GetPulse2B(void);
void     ENC_ResetPulses(void);

/* ------------------------------------------------------------------
 * API estilo mmlib – para usar con motores N20
 * ------------------------------------------------------------------ */

/* Configuración: micrómetros por pulso del encoder (calibrable) */
void    set_micrometers_per_count(float value);
float   get_micrometers_per_count(void);

/* Diferencia de pulsos respecto a la última lectura */
int32_t get_encoder_left_diff_count(void);
int32_t get_encoder_right_diff_count(void);

/* Total acumulado de pulsos */
int32_t get_encoder_left_total_count(void);
int32_t get_encoder_right_total_count(void);

/* Distancia recorrida en micrómetros */
int32_t get_encoder_left_micrometers(void);
int32_t get_encoder_right_micrometers(void);

/* Distancia promedio (media de ambos encoders) en micrómetros */
int32_t get_encoder_average_micrometers(void);

/* Velocidad en metros por segundo */
float get_encoder_left_speed(void);
float get_encoder_right_speed(void);

/* Manejo de overflow del contador (para contadores uint16_t) */
int32_t max_likelihood_counter_diff(uint16_t now, uint16_t before);

/* Actualización periódica: leer hardware, calcular diferencias,
 * distancia y velocidad. Llamar a la frecuencia definida por
 * SYSTICK_FREQUENCY_HZ. */
void update_encoder_readings(void);

#endif /* SENSOR_ENCODER_H */
