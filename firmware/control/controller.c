/**
 * @file controller.c
 * @brief Lazo de velocidad de una rueda: rampa de consigna + PID.
 */

#include "control/controller.h"

/**
 * @brief Prepara el lazo, arrancándolo parado.
 *
 * @param c Lazo a inicializar
 * @param kp Ganancia proporcional
 * @param ki Ganancia integral
 * @param kd Ganancia derivativa
 * @param duty_max Tope de duty que puede pedir el PID, en %
 */
void CTRL_Init(Controller_t *c, float kp, float ki, float kd, float duty_max)
{
    if (c == 0) {
        return;
    }

    PID_Init(&c->pid, kp, ki, kd);
    PID_SetLimites(&c->pid, duty_max, duty_max);

    SP_Init(&c->consigna, 0.0f);

    c->duty   = 0.0f;
    c->medida = 0.0f;
    c->parado = 1U;
}

/**
 * @brief Cambia las ganancias sin recompilar, para sintonizar en caliente.
 *
 * @param c Lazo a modificar
 * @param kp Ganancia proporcional
 * @param ki Ganancia integral
 * @param kd Ganancia derivativa
 */
void CTRL_SetGanancias(Controller_t *c, float kp, float ki, float kd)
{
    if (c == 0) {
        return;
    }

    PID_SetGanancias(&c->pid, kp, ki, kd);
}

/**
 * @brief Fija la pendiente de la rampa de consigna.
 *
 * @param c Lazo a modificar
 * @param mm_s2 Cambio máximo de velocidad por segundo
 */
void CTRL_SetRampa(Controller_t *c, float mm_s2)
{
    if (c == 0) {
        return;
    }

    SP_SetPendiente(&c->consigna, mm_s2);
}

/**
 * @brief Pide una velocidad; la rampa lleva la consigna hasta ella.
 *
 * @param c Lazo a modificar
 * @param mm_s Velocidad deseada
 */
void CTRL_SetConsigna(Controller_t *c, float mm_s)
{
    if (c == 0) {
        return;
    }

    SP_SetObjetivo(&c->consigna, mm_s);
    c->parado = 0U;
}

/**
 * @brief Pide una velocidad de golpe, sin rampa.
 *
 * @param c Lazo a modificar
 * @param mm_s Velocidad deseada
 */
void CTRL_SetConsignaEscalon(Controller_t *c, float mm_s)
{
    if (c == 0) {
        return;
    }

    SP_SetObjetivo(&c->consigna, mm_s);
    SP_SaltoInstantaneo(&c->consigna);
    c->parado = 0U;
}

/**
 * @brief Para el lazo: salida a cero, PID reiniciado y consigna a cero.
 *
 * @param c Lazo a parar
 */
void CTRL_Parar(Controller_t *c)
{
    if (c == 0) {
        return;
    }

    c->parado = 1U;
    c->duty   = 0.0f;

    PID_Reset(&c->pid);
    SP_SetObjetivo(&c->consigna, 0.0f);
    SP_SaltoInstantaneo(&c->consigna);
}

/**
 * @brief Indica si el lazo está parado.
 *
 * @param c Lazo a consultar
 * @return 1 si está parado
 */
uint8_t CTRL_EstaParado(const Controller_t *c)
{
    return (c == 0) ? 1U : c->parado;
}

/**
 * @brief Calcula un paso del lazo: avanza la rampa y aplica el PID.
 *
 * @param c Lazo a actualizar
 * @param medida_mm_s Velocidad medida de la rueda
 * @param dt Segundos desde el paso anterior
 * @return Duty a aplicar, en %
 */
float CTRL_Update(Controller_t *c, float medida_mm_s, float dt)
{
    float consigna_mm_s;

    if (c == 0) {
        return 0.0f;
    }

    c->medida = medida_mm_s;

    if (c->parado) {
        PID_Reset(&c->pid);
        c->duty = 0.0f;
        return 0.0f;
    }

    consigna_mm_s = SP_Update(&c->consigna, dt);
    c->duty       = PID_Update(&c->pid, consigna_mm_s, medida_mm_s, dt);

    return c->duty;
}

/**
 * @brief Consulta la última salida calculada.
 *
 * @param c Lazo a consultar
 * @return Duty en %
 */
float CTRL_GetDuty(const Controller_t *c)
{
    return (c == 0) ? 0.0f : c->duty;
}

/**
 * @brief Consulta la consigna en curso.
 *
 * @param c Lazo a consultar
 * @return Consigna en mm/s
 */
float CTRL_GetConsigna(const Controller_t *c)
{
    return (c == 0) ? 0.0f : SP_GetActual(&c->consigna);
}

/**
 * @brief Consulta la última medida.
 *
 * @param c Lazo a consultar
 * @return Velocidad en mm/s
 */
float CTRL_GetMedida(const Controller_t *c)
{
    return (c == 0) ? 0.0f : c->medida;
}
