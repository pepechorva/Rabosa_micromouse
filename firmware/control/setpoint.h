/**
 * @file setpoint.h
 * @brief Rampa de consigna de la capa CONTROL.
 *
 * Limita la pendiente de la consigna hasta un objetivo, para no pedir un escalón de velocidad a los
 * motores (el escalón pide un pico de corriente y hace patinar las ruedas, lo que falsea la
 * odometría). La fase de crucero no está aquí: eso es cosa de la misión, no de la consigna.
 */

#ifndef CONTROL_SETPOINT_H
#define CONTROL_SETPOINT_H

#include <stdint.h>

#define SP_PENDIENTE_DEF_MM_S2   600.0f

/** @brief Rampa de consigna: lleva `actual` hacia `objetivo` con una pendiente limitada. */
typedef struct {
    float actual;
    float objetivo;
    float pendiente;
} Setpoint_t;

/**
 * @brief Arranca la rampa con una consigna inicial.
 *
 * @param sp Rampa a inicializar
 * @param inicial Consigna de partida, normalmente 0
 */
void SP_Init(Setpoint_t *sp, float inicial);

/**
 * @brief Fija a dónde se quiere llegar; la rampa se encarga del resto.
 *
 * @param sp Rampa a modificar
 * @param objetivo Valor final de la consigna
 */
void SP_SetObjetivo(Setpoint_t *sp, float objetivo);

/**
 * @brief Fija la pendiente máxima de la rampa.
 *
 * @param sp Rampa a modificar
 * @param pendiente Cambio máximo por segundo; un valor no positivo se ignora
 */
void SP_SetPendiente(Setpoint_t *sp, float pendiente);

/**
 * @brief Avanza la rampa un paso hacia el objetivo.
 *
 * @param sp Rampa a avanzar
 * @param dt Segundos desde el paso anterior
 * @return Consigna actual, ya acercada al objetivo
 */
float SP_Update(Setpoint_t *sp, float dt);

/**
 * @brief Salta al objetivo sin rampa.
 *
 * @note Solo se usa para sintonizar el PID: un escalón de consigna es el peor caso para provocar la
 *       oscilación sostenida y medir la ganancia y el periodo críticos (Ziegler-Nichols).
 *
 * @param sp Rampa a saltar
 */
void SP_SaltoInstantaneo(Setpoint_t *sp);

/**
 * @brief Consulta la consigna actual sin avanzar la rampa.
 *
 * @param sp Rampa a consultar
 * @return Consigna actual
 */
float SP_GetActual(const Setpoint_t *sp);

#endif /* CONTROL_SETPOINT_H */
