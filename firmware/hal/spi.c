#include "hal/spi.h"

SPI_HandleTypeDef hspi2;

/* Pines de la IMU en el bus SPI2 (ver "Pinout Rabosa V1.0") */
#define SPI2_CS_PIN    GPIO_PIN_12   /* PB12 → CS   */
#define SPI2_SCK_PIN   GPIO_PIN_13   /* PB13 → SCK  */
#define SPI2_MISO_PIN  GPIO_PIN_14   /* PB14 → MISO */
#define SPI2_MOSI_PIN  GPIO_PIN_15   /* PB15 → MOSI */

/* ------------------------------------------------------------------ */
void MX_SPI2_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB12: CS como salida GPIO, inactivo en alto.
     * (El CS de la MPU6500 es activo en bajo y lo gobernamos por software.) */
    GPIO_InitStruct.Pin   = SPI2_CS_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, SPI2_CS_PIN, GPIO_PIN_SET);

    /* PB13 (SCK) y PB15 (MOSI): salidas alternate function push-pull */
    GPIO_InitStruct.Pin   = SPI2_SCK_PIN | SPI2_MOSI_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PB14 (MISO): entrada flotante */
    GPIO_InitStruct.Pin   = SPI2_MISO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    hspi2.Instance               = SPI2;
    hspi2.Init.Mode              = SPI_MODE_MASTER;
    hspi2.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;    /* SPI mode 0 (el del kernel Linux) */
    hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;     /* SPI mode 0 */
    hspi2.Init.NSS               = SPI_NSS_SOFT;
    /* PCLK1 (36 MHz) / 64 ≈ 562 kHz. IMPORTANTE: el SPI del MPU-9250
     * solo admite ~1 MHz (el MPU-6500 admite más); a 4.5 MHz no responde. */
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial     = 10U;   /* requerido por HAL en F1 */
    HAL_SPI_Init(&hspi2);
}

/* ------------------------------------------------------------------ */
void SPI2_CS_Low(void)
{
    HAL_GPIO_WritePin(GPIOB, SPI2_CS_PIN, GPIO_PIN_RESET);
}

/* ------------------------------------------------------------------ */
void SPI2_CS_High(void)
{
    HAL_GPIO_WritePin(GPIOB, SPI2_CS_PIN, GPIO_PIN_SET);
}

/* ------------------------------------------------------------------ */
HAL_StatusTypeDef SPI2_Transfer(uint8_t *tx, uint8_t *rx, uint16_t size)
{
    return HAL_SPI_TransmitReceive(&hspi2, tx, rx, size, HAL_MAX_DELAY);
}
