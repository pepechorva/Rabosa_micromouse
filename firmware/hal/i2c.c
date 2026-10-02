#include "hal/i2c.h"
#include "pinout.h"

I2C_HandleTypeDef hi2c1;

/* Tiempo de espera por defecto para operaciones I2C */
#define I2C_TIMEOUT_MS  100

/* ------------------------------------------------------------------ */
void MX_I2C1_Init(void)
{
    __HAL_RCC_I2C1_CLK_ENABLE();

    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = I2C1_CLOCK_HZ;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

/* ------------------------------------------------------------------ */
HAL_StatusTypeDef I2C_WriteReg(uint16_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    return HAL_I2C_Mem_Write(&hi2c1, dev_addr, reg_addr,
                             I2C_MEMADD_SIZE_8BIT, &data, 1, I2C_TIMEOUT_MS);
}

/* ------------------------------------------------------------------ */
HAL_StatusTypeDef I2C_ReadReg(uint16_t dev_addr, uint8_t reg_addr, uint8_t *data)
{
    return HAL_I2C_Mem_Read(&hi2c1, dev_addr, reg_addr,
                            I2C_MEMADD_SIZE_8BIT, data, 1, I2C_TIMEOUT_MS);
}

/* ------------------------------------------------------------------ */
HAL_StatusTypeDef I2C_WriteMulti(uint16_t dev_addr, uint8_t reg_addr,
                                 const uint8_t *data, uint16_t len)
{
    return HAL_I2C_Mem_Write(&hi2c1, dev_addr, reg_addr,
                             I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, len,
                             I2C_TIMEOUT_MS);
}

/* ------------------------------------------------------------------ */
HAL_StatusTypeDef I2C_ReadMulti(uint16_t dev_addr, uint8_t reg_addr,
                                uint8_t *data, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1, dev_addr, reg_addr,
                            I2C_MEMADD_SIZE_8BIT, data, len, I2C_TIMEOUT_MS);
}

/* ------------------------------------------------------------------ */
HAL_StatusTypeDef I2C_WriteRaw(uint16_t dev_addr, const uint8_t *data, uint16_t len)
{
    return HAL_I2C_Master_Transmit(&hi2c1, dev_addr, (uint8_t *)data,
                                   len, I2C_TIMEOUT_MS);
}

/* ==================================================================
 * Diagnóstico del bus
 * ================================================================== */
uint8_t I2C_Scan(uint8_t *dirs_7b, uint8_t max)
{
    uint8_t guardados = 0;

    /* Rango útil de direcciones de 7 bits: 0x08..0x77 */
    for (uint8_t dir = 0x08; dir <= 0x77; dir++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(dir << 1), 2, 20) != HAL_OK) {
            continue;
        }
        if (guardados < max) {
            dirs_7b[guardados] = dir;
            guardados++;
        }
    }

    return guardados;
}

/* ------------------------------------------------------------------ */
uint8_t I2C_BusLevels(void)
{
    GPIO_InitTypeDef gi = {0};
    uint8_t niveles = 0;

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Entradas FLOTANTES: así se ve quién sube el bus (los pull-ups del módulo,
     * resistencias externas… o nadie). Con el pin en AF_OD no se puede leer. */
    gi.Pin   = I2C1_SCL_PIN | I2C1_SDA_PIN;
    gi.Mode  = GPIO_MODE_INPUT;
    gi.Pull  = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C1_GPIO_PORT, &gi);

    HAL_Delay(1);

    if (HAL_GPIO_ReadPin(I2C1_GPIO_PORT, I2C1_SCL_PIN) == GPIO_PIN_SET) niveles |= 0x01;
    if (HAL_GPIO_ReadPin(I2C1_GPIO_PORT, I2C1_SDA_PIN) == GPIO_PIN_SET) niveles |= 0x02;

    /* Devolver los pines al modo alterno del periférico I2C */
    gi.Mode = GPIO_MODE_AF_OD;
    HAL_GPIO_Init(I2C1_GPIO_PORT, &gi);

    return niveles;
}
