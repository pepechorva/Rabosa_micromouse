#ifndef HAL_SPI_H
#define HAL_SPI_H

#include "peripherals.h"

extern SPI_HandleTypeDef hspi2;

/*
 * SPI2 maestro para la IMU MPU6500.
 *
 *   CS   = PB12 (GPIO, gestionado por software)
 *   SCK  = PB13
 *   MISO = PB14
 *   MOSI = PB15
 *
 * Configuración: master, 8 bits, MSB first, SPI mode 0, ~562 kHz.
 * (El MPU-9250 solo soporta ~1 MHz de SPI; a más velocidad no responde.)
 */
void MX_SPI2_Init(void);

/* Gestión del CS (chip select) por GPIO */
void SPI2_CS_Low(void);
void SPI2_CS_High(void);

/*
 * Transferencia full-duplex bloqueante.
 * @param tx  Buffer de datos a transmitir.
 * @param rx  Buffer donde recibir (puede ser distinto de tx).
 * @param size Número de bytes a intercambiar.
 * @return HAL_OK o código de error.
 */
HAL_StatusTypeDef SPI2_Transfer(uint8_t *tx, uint8_t *rx, uint16_t size);

#endif /* HAL_SPI_H */
