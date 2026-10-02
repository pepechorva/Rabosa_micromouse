#ifndef HW_IO_H
#define HW_IO_H

#include "peripherals.h"

/*
 * Capa HW: acceso directo a registros de E/S del microcontrolador.
 *
 * Proporciona macros y funciones para manipulación rápida de
 * pines GPIO sin pasar por la HAL de STM32, útil para operaciones
 * críticas en tiempo real.
 */

/* ------------------------------------------------------------------
 * Macros para acceso rápido a GPIO (evitando overhead de HAL)
 * ------------------------------------------------------------------ */

#define GPIO_SET(PORT, PIN)    ((PORT)->BSRR = (PIN))
#define GPIO_RESET(PORT, PIN)  ((PORT)->BSRR = ((uint32_t)(PIN) << 16))
#define GPIO_TOGGLE(PORT, PIN) ((PORT)->ODR ^= (PIN))
#define GPIO_READ(PORT, PIN)   (((PORT)->IDR & (PIN)) != 0)

/* ------------------------------------------------------------------
 * Funciones de inicialización de registros críticos
 * ------------------------------------------------------------------ */

/**
 * @brief Inicializa pines que requieren acceso rápido.
 *        Llamar después de MX_GPIO_Init().
 */
void IO_InitFastPins(void);

/**
 * @brief Escribe un patrón en varios pines simultáneamente.
 */
void IO_WriteGroup(GPIO_TypeDef *port, uint16_t pin_mask, uint16_t value);

#endif /* HW_IO_H */
