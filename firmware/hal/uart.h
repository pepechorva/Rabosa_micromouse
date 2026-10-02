#ifndef HAL_UART_H
#define HAL_UART_H

#include "peripherals.h"

extern UART_HandleTypeDef huart3;

void MX_USART3_UART_Init(uint32_t baudrate);

/* Funciones helper para transmisión/recepción */
HAL_StatusTypeDef UART_SendString(const char *str);
HAL_StatusTypeDef UART_SendData(const uint8_t *data, uint16_t len);
HAL_StatusTypeDef UART_ReceiveByte(uint8_t *byte, uint32_t timeout);

/**
 * @brief Arranca la recepción por interrupción en un buffer circular de 64 B.
 *
 * @note Llamar UNA vez después de MX_USART3_UART_Init(). A partir de ahí los bytes que lleguen se
 *       van guardando solos (no se pierde nada aunque el bucle esté dentro de una prueba larga) y se
 *       sacan con UART_RxGet(). Si el buffer se llena, se descarta el byte MÁS VIEJO.
 * @note NO se usa el modo de recepción del HAL a propósito: ver el comentario de la ISR en
 *       `hal/uart.c` (un flag que se quedara puesto repetiría la interrupción para siempre).
 */
void UART_RxStart(void);

/**
 * @brief Cuerpo de la interrupción de USART3. La llama `USART3_IRQHandler()` (en `src/main.c`).
 *
 * @note Lee siempre el registro de datos, así que RXNE/ORE/NE/FE quedan limpios y la interrupción no
 *       puede repetirse en bucle.
 */
void UART_RxIRQHandler(void);

/** @return Bytes pendientes de leer en el buffer de recepción. */
uint16_t UART_RxAvailable(void);

/** @return Siguiente byte recibido, o -1 si no hay ninguno. */
int UART_RxGet(void);

#endif /* HAL_UART_H */
