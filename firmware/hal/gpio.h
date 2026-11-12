#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "peripherals.h"

void MX_GPIO_Init(void);

/*
 * Botón MENU (PC14) por INTERRUPCIÓN (EXTI línea 14, flanco de bajada).
 *
 * El pulsador conmuta a GND con pull-up interno, así que el flanco de bajada
 * es exactamente "pulsado". Al ser una interrupción, la pulsación se cuenta
 * aunque el programa esté metido en un `while(1)` que no llame a nadie: es lo
 * que permite salir de cualquier prueba de banco con el botón MENU.
 *
 * `GPIO_MenuBtnIRQ_Handler()` se llama desde EXTI15_10_IRQHandler() (en
 * src/main.c); la ISR solo incrementa un contador, no hace nada más.
 */

/** @brief Rutina de servicio: llamar desde EXTI15_10_IRQHandler(). */
void GPIO_MenuBtnIRQ_Handler(void);

/**
 * @brief Pide una salida SIN que haya habido flanco (orden `SALIR`/`MENU` de la consola).
 *
 * ⚠️ **Esto NO se puede hacer con `GPIO_MenuBtnIRQ_Handler()`**, y por ahí la orden `SALIR` no hacía
 * nada hasta el 2026-10-11: esa función despacha por `HAL_GPIO_EXTI_IRQHandler()`, y esa **solo llama
 * al callback si el flag de EXTI está puesto** — o sea, solo si acaba de haber un flanco real. En una
 * orden de consola no lo hay, así que el contador no se incrementaba y la prueba en curso seguía
 * corriendo aunque la consola acusara `>CMD_SALIR:1`.
 */
void GPIO_MenuBtnIRQ_Solicitar(void);

/**
 * @brief Pulsaciones de MENU contadas por la interrupción.
 * @return Número de flancos pendientes; al leerlos los consume (devuelve 0 si
 *         no había ninguno). Puede haber más de uno si nadie consultó a tiempo.
 */
uint8_t GPIO_MenuBtnIRQ_Pendiente(void);

#endif /* HAL_GPIO_H */
