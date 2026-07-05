#include "i2c.h"

void MX_I2C1_Init(void)
{
    /* Preparado para futuras expansiones de I2C. */
    __HAL_RCC_I2C1_CLK_ENABLE();
}
