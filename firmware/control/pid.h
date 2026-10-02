/**
 * @file pid.h
 * @brief Regulador PID de la capa CONTROL, sin dependencias de hardware.
 *
 * El módulo no conoce ni las unidades ni el sistema controlado: entran una consigna y una medida en
 * la MISMA unidad, y sale una señal de control en la unidad de las ganancias.
 */

#ifndef CONTROL_PID_H
#define CONTROL_PID_H

#include <stdint.h>

/** @brief Configuración y estado de un PID. */
typedef struct {
    float kp;
    float ki;
    float kd;

    float salida_max;
    float i_max;
    float dt_max;
    float d_alpha;

    float integrador;
    float medida_anterior;
    float d_filtrada;
    uint8_t primera;
} PID_t;

/**
 * @brief Inicializa el PID con sus ganancias y los límites por defecto.
 *
 * @param pid PID a inicializar
 * @param kp Ganancia proporcional
 * @param ki Ganancia integral
 * @param kd Ganancia derivativa
 */
void PID_Init(PID_t *pid, float kp, float ki, float kd);

/**
 * @brief Cambia las ganancias sin tocar el estado, para sintonizar en caliente.
 *
 * @param pid PID a modificar
 * @param kp Ganancia proporcional
 * @param ki Ganancia integral
 * @param kd Ganancia derivativa
 */
void PID_SetGanancias(PID_t *pid, float kp, float ki, float kd);

/**
 * @brief Fija los topes de la salida y del término integral.
 *
 * @param pid PID a modificar
 * @param salida_max Tope de la señal de control
 * @param i_max Tope del término integral, para que no pueda pedir más que la salida
 */
void PID_SetLimites(PID_t *pid, float salida_max, float i_max);

/**
 * @brief Borra el integrador, la medida anterior y la derivada filtrada.
 *
 * @param pid PID a reiniciar
 */
void PID_Reset(PID_t *pid);

/**
 * @brief Calcula un paso del PID.
 *
 * El intervalo de tiempo condiciona el cálculo: si no es utilizable (cero, negativo o mayor que
 * `dt_max`) solo actúa el término proporcional. Así, un lazo que se ha quedado parado no dispara el
 * integrador al reanudarse.
 *
 * @param pid PID a actualizar
 * @param consigna Valor deseado de la variable controlada
 * @param medida Valor medido, en la misma unidad que la consigna
 * @param dt Segundos desde el paso anterior
 * @return Señal de control, recortada a ±`salida_max`
 */
float PID_Update(PID_t *pid, float consigna, float medida, float dt);

#endif /* CONTROL_PID_H */
