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
 * @brief Pulsaciones de MENU contadas por la interrupción.
 * @return Número de flancos pendientes; al leerlos los consume (devuelve 0 si
 *         no había ninguno). Puede haber más de uno si nadie consultó a tiempo.
 */
uint8_t GPIO_MenuBtnIRQ_Pendiente(void);

#endif /* HAL_GPIO_H */
