/**
 * @file setpoint.c
 * @brief Rampa de consigna de la capa CONTROL.
 */

#include "control/setpoint.h"

/**
 * @brief Arranca la rampa con una consigna inicial.
 *
 * @param sp Rampa a inicializar
 * @param inicial Consigna de partida, normalmente 0
 */
void SP_Init(Setpoint_t *sp, float inicial)
{
    if (sp == 0) {
        return;
    }

    sp->actual    = inicial;
    sp->objetivo  = inicial;
    sp->pendiente = SP_PENDIENTE_DEF_MM_S2;
}

/**
 * @brief Fija a dónde se quiere llegar; la rampa se encarga del resto.
 *
 * @param sp Rampa a modificar
 * @param objetivo Valor final de la consigna
 */
void SP_SetObjetivo(Setpoint_t *sp, float objetivo)
{
    if (sp == 0) {
        return;
    }

    sp->objetivo = objetivo;
}

/**
 * @brief Fija la pendiente máxima de la rampa.
 *
 * @param sp Rampa a modificar
 * @param pendiente Cambio máximo por segundo; un valor no positivo se ignora
 */
void SP_SetPendiente(Setpoint_t *sp, float pendiente)
{
    if (sp == 0) {
        return;
    }

    if (pendiente > 0.0f) {
        sp->pendiente = pendiente;
    }
}

/**
 * @brief Avanza la rampa un paso hacia el objetivo.
 *
 * @param sp Rampa a avanzar
 * @param dt Segundos desde el paso anterior
 * @return Consigna actual, ya acercada al objetivo
 */
float SP_Update(Setpoint_t *sp, float dt)
{
    float diferencia;
    float salto_maximo;

    if (sp == 0) {
        return 0.0f;
    }

    if (dt <= 0.0f) {
        return sp->actual;
    }

    diferencia   = sp->objetivo - sp->actual;
    salto_maximo = sp->pendiente * dt;

    if (diferencia > salto_maximo) {
        sp->actual += salto_maximo;
    } else if (diferencia < -salto_maximo) {
        sp->actual -= salto_maximo;
    } else {
        sp->actual = sp->objetivo;
    }

    return sp->actual;
}

/**
 * @brief Salta al objetivo sin rampa.
 *
 * @note Solo se usa para sintonizar el PID: un escalón de consigna es el peor caso para provocar la
 *       oscilación sostenida y medir la ganancia y el periodo críticos (Ziegler-Nichols).
 *
 * @param sp Rampa a saltar
 */
void SP_SaltoInstantaneo(Setpoint_t *sp)
{
    if (sp == 0) {
        return;
    }

    sp->actual = sp->objetivo;
}

/**
 * @brief Consulta la consigna actual sin avanzar la rampa.
 *
 * @param sp Rampa a consultar
 * @return Consigna actual
 */
float SP_GetActual(const Setpoint_t *sp)
{
    return (sp == 0) ? 0.0f : sp->actual;
}
