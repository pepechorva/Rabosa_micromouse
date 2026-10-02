/**
 * @file pid.c
 * @brief Regulador PID de la capa CONTROL.
 *
 * @note Por qué se hace así (anti-windup, derivada sobre la medida y derivada filtrada):
 *       `Documentacion/control-pid.md` §3.
 */

#include "control/pid.h"

#define PID_DT_MAX_DEF       0.5f
#define PID_SALIDA_MAX_DEF   100.0f
#define PID_I_MAX_DEF        50.0f
#define PID_D_ALPHA_DEF      0.2f

/**
 * @brief Actualiza la derivada filtrada de la medida.
 *
 * Se deriva la medida y no el error para que un cambio de consigna no produzca un pico de derivada;
 * se filtra porque la velocidad del encoder llega con ruido.
 *
 * @param pid PID a actualizar
 * @param medida Valor medido en este paso
 * @param dt Segundos desde el paso anterior
 * @return Derivada filtrada, en unidad de medida por segundo
 */
static float pid_update_derivative(PID_t *pid, float medida, float dt)
{
    uint8_t dt_utilizable = (uint8_t)((dt > 0.0f) && (dt <= pid->dt_max));
    float   derivative_raw = 0.0f;

    if (dt_utilizable) {
        if (!pid->primera) {
            derivative_raw = -(medida - pid->medida_anterior) / dt;
        }

        pid->d_filtrada += pid->d_alpha * (derivative_raw - pid->d_filtrada);
        pid->primera = 0U;
    }

    pid->medida_anterior = medida;

    return pid->d_filtrada;
}

/**
 * @brief Recorta la señal de control y deja al integrador solo el hueco que queda.
 *
 * Es el anti-windup: cuando la salida toca su tope, el término integral se recorta para que no se
 * quede cargado y el lazo recupere el mando en cuanto desaparece la saturación.
 *
 * @param pid PID a actualizar
 * @param salida Señal de control sin recortar
 * @param proporcional Término P ya calculado
 * @param derivativo Término D ya calculado
 * @return Señal de control recortada
 */
static float pid_clamp_output(PID_t *pid, float salida, float proporcional, float derivativo)
{
    if (salida > pid->salida_max) {
        salida = pid->salida_max;
        pid->integrador = salida - proporcional - derivativo;
    } else if (salida < -pid->salida_max) {
        salida = -pid->salida_max;
        pid->integrador = salida - proporcional - derivativo;
    }

    if (pid->integrador > pid->i_max) {
        pid->integrador = pid->i_max;
    }
    if (pid->integrador < -pid->i_max) {
        pid->integrador = -pid->i_max;
    }

    return salida;
}

/**
 * @brief Inicializa el PID con sus ganancias y los límites por defecto.
 *
 * @param pid PID a inicializar
 * @param kp Ganancia proporcional
 * @param ki Ganancia integral
 * @param kd Ganancia derivativa
 */
void PID_Init(PID_t *pid, float kp, float ki, float kd)
{
    if (pid == 0) {
        return;
    }

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;

    pid->salida_max = PID_SALIDA_MAX_DEF;
    pid->i_max      = PID_I_MAX_DEF;
    pid->dt_max     = PID_DT_MAX_DEF;
    pid->d_alpha    = PID_D_ALPHA_DEF;

    PID_Reset(pid);
}

/**
 * @brief Cambia las ganancias sin tocar el estado, para sintonizar en caliente.
 *
 * @param pid PID a modificar
 * @param kp Ganancia proporcional
 * @param ki Ganancia integral
 * @param kd Ganancia derivativa
 */
void PID_SetGanancias(PID_t *pid, float kp, float ki, float kd)
{
    if (pid == 0) {
        return;
    }

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;

    /* ⚠️ El integrador se resetea, y no es un detalle: guarda un **término de salida en %**, no un
       acumulador del error, así que al poner `Ki` a cero deja de acumular pero **conserva su último
       valor** y ese valor sigue sumándose a la salida. Sin este reset, «dejar solo la P» no deja solo
       la P: el lazo se queda con el duty que tenía.

       Se vio el 2026-10-01 sintonizando: con `KP = KI = KD = 0` el duty seguía en 17,6 % y 60 %, en
       lugar de irse a cero. */
    PID_Reset(pid);
}

/**
 * @brief Fija los topes de la salida y del término integral.
 *
 * @param pid PID a modificar
 * @param salida_max Tope de la señal de control
 * @param i_max Tope del término integral, para que no pueda pedir más que la salida
 */
void PID_SetLimites(PID_t *pid, float salida_max, float i_max)
{
    if (pid == 0) {
        return;
    }

    pid->salida_max = salida_max;
    pid->i_max      = i_max;
}

/**
 * @brief Borra el integrador, la medida anterior y la derivada filtrada.
 *
 * @param pid PID a reiniciar
 */
void PID_Reset(PID_t *pid)
{
    if (pid == 0) {
        return;
    }

    pid->integrador      = 0.0f;
    pid->medida_anterior = 0.0f;
    pid->d_filtrada      = 0.0f;
    pid->primera         = 1U;
}

/**
 * @brief Calcula un paso del PID.
 *
 * El intervalo de tiempo condiciona el cálculo: si no es utilizable (cero, negativo o mayor que
 * `dt_max`) solo actúa el término proporcional.
 *
 * @param pid PID a actualizar
 * @param consigna Valor deseado de la variable controlada
 * @param medida Valor medido, en la misma unidad que la consigna
 * @param dt Segundos desde el paso anterior
 * @return Señal de control, recortada a ±`salida_max`
 */
float PID_Update(PID_t *pid, float consigna, float medida, float dt)
{
    uint8_t dt_utilizable;
    float   error;
    float   proporcional;
    float   derivativo;

    if (pid == 0) {
        return 0.0f;
    }

    dt_utilizable = (uint8_t)((dt > 0.0f) && (dt <= pid->dt_max));

    error        = consigna - medida;
    proporcional = pid->kp * error;

    if (dt_utilizable) {
        pid->integrador += pid->ki * error * dt;
    }

    derivativo = pid->kd * pid_update_derivative(pid, medida, dt);

    return pid_clamp_output(pid, proporcional + pid->integrador + derivativo,
                            proporcional, derivativo);
}
