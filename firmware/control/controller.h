/**
 * @file controller.h
 * @brief Lazo de velocidad de una rueda: rampa de consigna + PID (capa CONTROL).
 *
 * El módulo no conoce el hardware: entra la velocidad medida y sale el duty. Leer el encoder y
 * escribir el driver es tarea de la capa APP, que durante el bring-up es la única autorizada a
 * saltar capas.
 *
 * Unidades: velocidad en mm/s y duty en % (−100…100).
 *
 * @note Sintonía y ejemplo de uso: `Documentacion/control-pid.md` y la prueba 18 de
 *       `Documentacion/pruebas-bringup.md` §14.
 */

#ifndef CONTROL_CONTROLLER_H
#define CONTROL_CONTROLLER_H

#include <stdint.h>
#include "control/pid.h"
#include "control/setpoint.h"

/** @brief Lazo de velocidad de una rueda. */
typedef struct {
    PID_t      pid;
    Setpoint_t consigna;
    float      duty;
    float      medida;
    uint8_t    parado;
} Controller_t;

/**
 * @brief Prepara el lazo, arrancándolo parado.
 *
 * @param c Lazo a inicializar
 * @param kp Ganancia proporcional
 * @param ki Ganancia integral
 * @param kd Ganancia derivativa
 * @param duty_max Tope de duty que puede pedir el PID, en %; durante la sintonía conviene bajarlo
 */
void CTRL_Init(Controller_t *c, float kp, float ki, float kd, float duty_max);

/**
 * @brief Cambia las ganancias sin recompilar, para sintonizar en caliente.
 *
 * @param c Lazo a modificar
 * @param kp Ganancia proporcional
 * @param ki Ganancia integral
 * @param kd Ganancia derivativa
 */
void CTRL_SetGanancias(Controller_t *c, float kp, float ki, float kd);

/**
 * @brief Fija la pendiente de la rampa de consigna.
 *
 * @param c Lazo a modificar
 * @param mm_s2 Cambio máximo de velocidad por segundo
 */
void CTRL_SetRampa(Controller_t *c, float mm_s2);

/**
 * @brief Pide una velocidad; la rampa lleva la consigna hasta ella.
 *
 * @param c Lazo a modificar
 * @param mm_s Velocidad deseada
 */
void CTRL_SetConsigna(Controller_t *c, float mm_s);

/**
 * @brief Pide una velocidad de golpe, sin rampa.
 *
 * @note Solo para sintonizar (Ziegler-Nichols); en funcionamiento normal se usa CTRL_SetConsigna().
 *
 * @param c Lazo a modificar
 * @param mm_s Velocidad deseada
 */
void CTRL_SetConsignaEscalon(Controller_t *c, float mm_s);

/**
 * @brief Para el lazo: salida a cero, PID reiniciado y consigna a cero.
 *
 * @param c Lazo a parar
 */
void CTRL_Parar(Controller_t *c);

/**
 * @brief Indica si el lazo está parado.
 *
 * @param c Lazo a consultar
 * @return 1 si está parado
 */
uint8_t CTRL_EstaParado(const Controller_t *c);

/**
 * @brief Calcula un paso del lazo: avanza la rampa y aplica el PID.
 *
 * @param c Lazo a actualizar
 * @param medida_mm_s Velocidad medida de la rueda
 * @param dt Segundos desde el paso anterior
 * @return Duty a aplicar, en %
 */
float CTRL_Update(Controller_t *c, float medida_mm_s, float dt);

/**
 * @brief Consulta la última salida calculada.
 *
 * @param c Lazo a consultar
 * @return Duty en %
 */
float CTRL_GetDuty(const Controller_t *c);

/**
 * @brief Consulta la consigna en curso.
 *
 * @param c Lazo a consultar
 * @return Consigna en mm/s
 */
float CTRL_GetConsigna(const Controller_t *c);

/**
 * @brief Consulta la última medida.
 *
 * @param c Lazo a consultar
 * @return Velocidad en mm/s
 */
float CTRL_GetMedida(const Controller_t *c);

#endif /* CONTROL_CONTROLLER_H */
