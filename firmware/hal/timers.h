#ifndef HAL_TIMERS_H
#define HAL_TIMERS_H

#include "peripherals.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim4;

void MX_TIM1_Init(void);

/* ------------------------------------------------------------------
 * Cronómetro de tramos (TIM4)
 *
 * Sirve para **medir**, no para generar nada: no hay interrupción ni salida en ningún pin. Existe
 * porque el retardo del lazo de control tiene varias fuentes y hacía falta saber cuál pesa — el
 * filtro de la velocidad (algorítmico), el periodo del lazo, y las llamadas que **bloquean**: la
 * telemetría por UART y el volcado del framebuffer de la OLED por I²C.
 *
 * Los máximos **no se publican solos**: se piden con la orden `LAZO` de la consola. Publicarlos
 * desde dentro del lazo sería meter telemetría —o sea, retardo— justo en lo que se está midiendo.
 * ------------------------------------------------------------------ */

/** @brief Tramo cronometrado. Su máximo se consulta con `TIM_GetMaximo()`. */
typedef enum {
    TIM_TRAMO_LAZO = 0,  /**< Una vuelta del **bucle principal** (trabajo + espera)           */
    TIM_TRAMO_OLED,      /**< Un `OLED_Update()`: los 512 B del framebuffer por I²C           */
    TIM_TRAMO_UART,      /**< Un `UART_SendData()`: una línea de telemetría                    */
    TIM_TRAMO_ISR,       /**< Un paso del lazo dentro de la interrupción de TIM1               */
    TIM_TRAMO_N
} TIM_Tramo_t;

/** @brief Arranca TIM4 como base de tiempos de 2 µs. En `APP_InitAll()`, antes de todo lo medible. */
void MX_TIM4_Init(void);

/**
 * @brief Lectura cruda del contador de tiempos, en cuentas de 2 µs.
 *
 * @return El contador de 16 bits; **da la vuelta cada 131 ms**
 */
uint16_t TIM_Cuentas(void);

/**
 * @brief Cierra un tramo: mide lo que ha durado y actualiza su máximo.
 *
 * La resta se hace en `uint16_t`, así que **la vuelta del contador se resuelve sola** para cualquier
 * tramo de menos de 131 ms. Un tramo más largo que eso no se puede medir con este contador.
 *
 * @param tramo Cuál de los tramos se cierra
 * @param inicio Lectura de `TIM_Cuentas()` tomada al empezar
 * @return Duración en microsegundos
 */
uint32_t TIM_Cronometrar(TIM_Tramo_t tramo, uint16_t inicio);

/** @brief Duración del último tramo cerrado, en µs. */
uint32_t TIM_GetUltimo(TIM_Tramo_t tramo);

/** @brief Duración máxima vista desde el último reset, en µs. */
uint32_t TIM_GetMaximo(TIM_Tramo_t tramo);

/** @brief Pone los máximos a cero (lo hace la orden `LAZO` al acabar de publicarlos). */
void TIM_ResetMaximos(void);

#endif /* HAL_TIMERS_H */
