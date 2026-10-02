/**
 * @file reset.h
 * @brief Causa del último reinicio del MCU, leída de las banderas del RCC (capa HAL).
 *
 * Un robot que se reinicia al chocar contra la pared se ve igual que uno que se cuelga por software, y
 * sin este dato no se sabe si hay que mirar el cableado o el firmware. El STM32 deja puesto un bit en
 * `RCC->CSR` según de dónde vino el reinicio, y esa es la única forma de distinguirlos sin osciloscopio.
 *
 * @note Este firmware **no usa watchdog**, así que en la práctica solo salen dos causas: la tensión
 *       (dado tensión o caída del riel) y el pin NRST (botón de reset, ruido o un roce en el cableado).
 * @note La bandera hay que leerla **una vez por arranque** y antes de que nadie la borre: por eso
 *       `RESET_LeerCausa()` se llama nada más empezar `APP_InitAll()`.
 * @note Tabla de códigos en `Documentacion/pruebas-bringup.md` §3.
 */

#ifndef HAL_RESET_H
#define HAL_RESET_H

#include "peripherals.h"

/** @brief Causa del último reinicio; los números viajan por telemetría (`>RESET:n`) y no se cambian. */
typedef enum {
    RESET_CAUSA_DESCONOCIDA  = 0, /**< Sin banderas: ya se había leído o el reinicio fue por otra vía */
    RESET_CAUSA_TENSION      = 1, /**< Se ha dado tensión, o el riel se ha caído (brownout)         */
    RESET_CAUSA_NRST         = 2, /**< Pin NRST: botón de reset, ruido o roce en el cableado        */
    RESET_CAUSA_SOFTWARE     = 3, /**< `NVIC_SystemReset()` pedido por el firmware                  */
    RESET_CAUSA_WATCHDOG     = 4, /**< IWDG/WWDG: el firmware dejó de refrescarlo                   */
    RESET_CAUSA_BAJO_CONSUMO = 5  /**< Salida del modo de bajo consumo                              */
} ResetCausa_t;

/**
 * @brief Lee las banderas de reinicio del RCC, las interpreta y las borra.
 *
 * @warning Llamar **una sola vez** y lo antes posible: al borrar las banderas, una segunda llamada
 *          devuelve `RESET_CAUSA_DESCONOCIDA`.
 */
void RESET_LeerCausa(void);

/**
 * @brief Devuelve la causa capturada por `RESET_LeerCausa()`.
 *
 * @return Causa del último reinicio
 */
ResetCausa_t RESET_GetCausa(void);

#endif /* HAL_RESET_H */
