/**
 * @file control_loop.h
 * @brief El lazo de control dentro de la interrupción de TIM1 (capa APP).
 *
 * El bucle principal comparte hilo con la OLED y la telemetría, que son **bloqueantes**, y eso dejaba
 * al PID sin correr hasta **68,8 ms seguidos** (medido el 2026-10-11). Este módulo adelanta el lazo a
 * todo eso: corre al evento de actualización de TIM1 —uno por período de PWM, ~999 Hz— y hace las
 * tres cosas juntas: muestrear los encoders, calcular el PID y escribir el duty.
 *
 * Ver el diseño completo, con las medidas y el razonamiento de por qué va en APP y no en `drivers/`,
 * en `Documentacion/lazo-control.md`.
 *
 * @warning ⚠️ **Antes de frenar los motores hay que llamar a `LOOP_Desactivar()`.** Si el lazo sigue
 *          registrado, la interrupción vuelve a escribir el duty en el paso siguiente y **deshace el
 *          freno** — un duty de 0 es *coast*, no freno.
 */

#ifndef APP_CONTROL_LOOP_H
#define APP_CONTROL_LOOP_H

#include <stdint.h>

#include "control/controller.h"
#include "hal/drv8833.h"

/**
 * @brief Habilita la interrupción del lazo. Se llama una vez, desde `APP_InitAll()`.
 *
 * Deja la interrupción **funcionando pero sin regular nada**: hasta que alguien llame a
 * `LOOP_Activar()`, la interrupción solo muestrea los encoders y no toca el PWM.
 */
void LOOP_Init(void);

/**
 * @brief Pone a regular esas dos ruedas.
 *
 * Solo puede haber **un par registrado a la vez**: no tiene sentido regular dos pares de ruedas.
 *
 * @param izq Lazo de la rueda izquierda
 * @param der Lazo de la rueda derecha
 * @param motor_izq Driver de la rueda izquierda
 * @param motor_der Driver de la rueda derecha
 *
 * @note Los punteros tienen que seguir vivos mientras el lazo esté activo: las primitivas registran
 *       los suyos (que viven en la `Motion_t` del que llama) y la prueba 18 los suyos.
 */
void LOOP_Activar(Controller_t *izq, Controller_t *der,
                  DRV8833_Motor_t *motor_izq, DRV8833_Motor_t *motor_der);

/** @brief Suelta las ruedas: la interrupción vuelve a muestrear pero **no toca el PWM**. */
void LOOP_Desactivar(void);

/**
 * @brief Pausa la interrupción de control, para tocar el estado del lazo sin que se cuele un paso.
 *
 * Hace falta porque el bucle principal y la interrupción comparten los `Controller_t`. Con las
 * consignas no hay problema (un `float` alineado se escribe de una vez), pero **`CTRL_Parar()` no es
 * una escritura**: toca cinco cosas, y si la interrupción entra a mitad puede calcular un duty con el
 * integrador a medio resetear y escribirlo *después* del freno ⇒ **la parada no para**.
 *
 * @see `Documentacion/lazo-control.md` §5
 */
void LOOP_Pausa(void);

/** @brief Vuelve a permitir la interrupción de control. */
void LOOP_Reanuda(void);

/** @brief El cuerpo de la interrupción. Lo llama `TIM1_UP_IRQHandler()` en `src/main.c`. */
void LOOP_IrqHandler(void);

/**
 * @brief Un paso de muestreo suelto, con el mismo `dt` que usa la interrupción.
 *
 * Existe para el banco: hay pruebas que **resetean los contadores** y necesitan vaciar la ventana del
 * estimador en ese mismo instante (ver `cal_encoders_referencias()`), cosa que la interrupción no
 * puede garantizar sola porque no se sabe cuándo va a entrar. Se llama con `LOOP_Pausa()` puesto.
 */
void LOOP_Muestrear(void);

/**
 * @brief Cuántos pasos lleva dados el lazo desde el arranque.
 *
 * Sirve para comprobar de un vistazo que la interrupción **está corriendo**: si el número no sube,
 * la interrupción no está llegando.
 */
uint32_t LOOP_GetPasos(void);

#endif /* APP_CONTROL_LOOP_H */
