/**
 * @file control_loop.c
 * @brief El lazo de control dentro de la interrupción de TIM1.
 *
 * Corre al **evento de actualización de TIM1**, que es el mismo que genera el PWM de los motores: uno
 * por período, o sea a ~999 Hz. Hace las tres cosas **juntas y en este orden**:
 *
 *   1. `update_encoder_readings()`  — lee TIM2/TIM3 y estima la velocidad de cada rueda
 *   2. `CTRL_Update()` × 2          — rampa de consigna + PID
 *   3. `DRV8833_Set_Speed()` × 2    — escribe los CCR del PWM
 *
 * ⚠️ **Las tres juntas, no solo el muestreo.** Si el PID se quedara en el bucle principal seguiría
 * esperando a la OLED y no se habría arreglado nada; y si el estimador se quedara allí, la ventana de
 * la media móvil volvería a durar lo que tarde el bucle (ver `sensor/encoder.h`).
 *
 * El porqué de cada decisión (el timer, el `dt` constante, el registro de controladores y las
 * secciones críticas) está en `Documentacion/lazo-control.md`.
 */

#include "app/control_loop.h"

#include "hal/pwm.h"
#include "hal/timers.h"
#include "sensor/encoder.h"

/**
 * @brief Período del lazo, en segundos.
 *
 * `PSC` = 71 sobre un reloj de 72 MHz da 1 µs por cuenta, y `ARR` = 1000 => el evento de actualización
 * llega cada **1001 cuentas**, o sea a 999 Hz.
 *
 * ⚠️ Es una **constante a propósito**, no una medida. Los instantes del evento los fija el hardware y
 * son exactos *aunque la interrupción se retrase* —que es justo lo que pasa cuando la OLED tiene el
 * bus ocupado—. Cronometrarlo con TIM4 metería el **retardo de entrada** (el jitter) dentro del propio
 * `dt`, que es exactamente lo que se quiere dejar fuera. Que 1001 valga por 1000 es un 0,1 %, y el
 * integrador lo absorbe.
 */
#define LOOP_DT_S         (1001.0e-6f)

/**
 * @brief Prioridad de la interrupción de control.
 *
 * Por encima del botón MENU (3) y de la UART (5): el lazo no se puede quedar esperando a que nadie
 * termine. Una pulsación de botón o un byte del puerto pueden esperar microsegundos; un período de PWM
 * sin regular, no.
 */
#define LOOP_PRIORIDAD    1U

static Controller_t    *lazo_izq;
static Controller_t    *lazo_der;
static DRV8833_Motor_t *motor_izq;
static DRV8833_Motor_t *motor_der;

static volatile uint32_t pasos;

/* ------------------------------------------------------------------ */
void LOOP_Init(void)
{
    pasos = 0U;

    HAL_NVIC_SetPriority(TIM1_UP_IRQn, LOOP_PRIORIDAD, 0U);
    HAL_NVIC_EnableIRQ(TIM1_UP_IRQn);

    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);
}

/* ------------------------------------------------------------------ */
void LOOP_Activar(Controller_t *izq, Controller_t *der,
                  DRV8833_Motor_t *m_izq, DRV8833_Motor_t *m_der)
{
    LOOP_Pausa();

    lazo_izq  = izq;
    lazo_der  = der;
    motor_izq = m_izq;
    motor_der = m_der;

    LOOP_Reanuda();
}

/* ------------------------------------------------------------------ */
void LOOP_Desactivar(void)
{
    LOOP_Pausa();

    lazo_izq  = NULL;
    lazo_der  = NULL;
    motor_izq = NULL;
    motor_der = NULL;

    LOOP_Reanuda();
}

/* ------------------------------------------------------------------ */
void LOOP_Pausa(void)
{
    __HAL_TIM_DISABLE_IT(&htim1, TIM_IT_UPDATE);
}

/* ------------------------------------------------------------------ */
void LOOP_Reanuda(void)
{
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);
}

/* ------------------------------------------------------------------ */
uint32_t LOOP_GetPasos(void)
{
    return pasos;
}

/* ------------------------------------------------------------------ */
void LOOP_Muestrear(void)
{
    update_encoder_readings(LOOP_DT_S);
}

/* ------------------------------------------------------------------ */
void LOOP_IrqHandler(void)
{
    uint16_t t0 = TIM_Cuentas();

    /* El flag se limpia **siempre y lo primero**, como en la ISR de la UART: una interrupción que
       pueda salir sin limpiarlo se repite sin fin y cuelga el robot entero. */
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);

    update_encoder_readings(LOOP_DT_S);

    /* Sin nadie registrado esto solo muestrea: las pruebas en lazo abierto mandan ellas el duty y no
       pueden tener a nadie escribiéndolo por detrás. */
    if (lazo_izq != NULL) {
        DRV8833_Set_Speed(motor_izq,
                          (int16_t)CTRL_Update(lazo_izq,
                                               get_encoder_left_speed() * 1000.0f,
                                               LOOP_DT_S));
        DRV8833_Set_Speed(motor_der,
                          (int16_t)CTRL_Update(lazo_der,
                                               get_encoder_right_speed() * 1000.0f,
                                               LOOP_DT_S));
    }

    pasos++;

    TIM_Cronometrar(TIM_TRAMO_ISR, t0);
}
