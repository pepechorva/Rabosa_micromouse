#include "hw/io.h"

/* ================================================================== */
void IO_InitFastPins(void)
{
    /* Por ahora, función placeholder.
     * Aquí se pueden cachear direcciones de registros para acceso
     * ultrarrápido en lazo de control.
     */
}

/* ================================================================== */
void IO_WriteGroup(GPIO_TypeDef *port, uint16_t pin_mask, uint16_t value)
{
    /* Escribe en ODR solo los bits indicados en pin_mask */
    uint32_t odr = port->ODR;
    odr &= ~pin_mask;
    odr |= (value & pin_mask);
    port->ODR = odr;
}
