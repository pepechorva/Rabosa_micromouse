#include "hal/uart.h"
#include <stdio.h>
#include <stdarg.h>

UART_HandleTypeDef huart3;

/* Recepción por interrupción, a mano y sin el modo del HAL.

   Motivo (verificado en `stm32f1xx_hal_uart.c` de framework-stm32cubef1): `UART_Receive_IT()` hace
   `return` SIN leer el registro de datos si `RxState != HAL_UART_STATE_BUSY_RX`, así que si el flag
   RXNE se queda puesto la interrupción se repite indefinidamente y el bucle principal no avanza
   (síntoma: la pantalla se queda congelada y los botones no responden). Aquí la ISR lee SIEMPRE el
   registro de datos, así que sus flags quedan limpios y no hay estados que se puedan desincronizar
   con el periférico.

   Los índices son uint16_t para que el anillo no necesite distinguir «lleno» de «vacío» con un
   contador que dé la vuelta: si el consumidor se retrasa 65 535 bytes, la diferencia se lee mal, pero
   eso es más de 5 s a 115200 baudios sin que nadie llame a UART_RxGet(). */
#define UART_RX_RING    64U
#define UART_RX_MASCARA (UART_RX_RING - 1U)

static volatile uint8_t  rx_anillo[UART_RX_RING];
static volatile uint16_t rx_escritura;
static volatile uint16_t rx_lectura;

/* ------------------------------------------------------------------ */
void MX_USART3_UART_Init(uint32_t baudrate)
{
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB10 = TX, PB11 = RX */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = GPIO_PIN_10;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = GPIO_PIN_11;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = baudrate;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
}

/* ------------------------------------------------------------------ */
HAL_StatusTypeDef UART_SendString(const char *str)
{
    return HAL_UART_Transmit(&huart3, (uint8_t *)str,
                             strlen(str), HAL_MAX_DELAY);
}

/* ------------------------------------------------------------------ */
HAL_StatusTypeDef UART_SendData(const uint8_t *data, uint16_t len)
{
    return HAL_UART_Transmit(&huart3, (uint8_t *)data, len, HAL_MAX_DELAY);
}

/* ------------------------------------------------------------------ */
HAL_StatusTypeDef UART_ReceiveByte(uint8_t *byte, uint32_t timeout)
{
    return HAL_UART_Receive(&huart3, byte, 1, timeout);
}

/* ------------------------------------------------------------------ */
void UART_RxStart(void)
{
    rx_escritura = 0U;
    rx_lectura   = 0U;

    HAL_NVIC_SetPriority(USART3_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(USART3_IRQn);

    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
}

/* ------------------------------------------------------------------ */
void UART_RxIRQHandler(void)
{
    uint32_t sr = USART3->SR;

    if ((sr & (USART_SR_RXNE | USART_SR_ORE)) != 0U) {
        uint8_t byte = (uint8_t)(USART3->DR & 0xFFU);

        if ((uint16_t)(rx_escritura - rx_lectura) >= UART_RX_RING) {
            rx_lectura++;
        }

        rx_anillo[rx_escritura & UART_RX_MASCARA] = byte;
        rx_escritura++;
    }
}

/* ------------------------------------------------------------------ */
uint16_t UART_RxAvailable(void)
{
    return (uint16_t)(rx_escritura - rx_lectura);
}

/* ------------------------------------------------------------------ */
int UART_RxGet(void)
{
    int byte;

    if (rx_lectura == rx_escritura) {
        return -1;
    }

    byte = (int)rx_anillo[rx_lectura & UART_RX_MASCARA];
    rx_lectura++;

    return byte;
}
