#ifndef HAL_I2C_H
#define HAL_I2C_H

#include "peripherals.h"

/* Handle I2C expuesto */
extern I2C_HandleTypeDef hi2c1;

/* Velocidad del bus. Con pull-ups débiles (o cables largos) bajarla a 50000
 * suele arreglar los fallos de comunicación. */
#define I2C1_CLOCK_HZ   100000U

/* Inicialización del periférico I2C1 (PB6-SCL, PB7-SDA) */
void MX_I2C1_Init(void);

/**
 * @brief Escanea el bus buscando dispositivos que respondan.
 * @param dirs_7b Array donde guardar las direcciones de 7 bits encontradas
 * @param max     Número máximo de direcciones a guardar
 * @return Cuántas direcciones se han guardado (hasta `max`)
 */
uint8_t I2C_Scan(uint8_t *dirs_7b, uint8_t max);

/**
 * @brief Lee el nivel de SCL y SDA con los pines como entradas FLOTANTES.
 *
 * Es el diagnóstico de cableado y pull-ups: en reposo las dos líneas deben
 * estar a nivel alto (3.3 V). Si alguna lee 0, está tirada a GND; si las dos
 * leen 0, nadie está subiendo el bus (faltan los pull-ups).
 *
 * ⚠️ Llamar solo con el bus en reposo: reconfigura los pines y los deja otra
 * vez en modo alterno open-drain al terminar.
 *
 * @return bit0 = SCL · bit1 = SDA  (1 = nivel alto)
 */
uint8_t I2C_BusLevels(void);

/* ------------------------------------------------------------------
 * Operaciones I2C maestro con timeout
 *
 * ⚠️ `dev_addr` es la dirección de 8 BITS tal como la espera el HAL, es decir
 * la de 7 bits desplazada a la izquierda: un SSD1306 en 0x3C se pasa como
 * 0x78. (Con la dirección de 7 bits el HAL no encuentra al dispositivo.)
 * ------------------------------------------------------------------ */

/**
 * @brief Escribe un byte en un registro de un dispositivo I2C.
 * @param dev_addr Dirección del dispositivo (8 bits: 7 bits << 1)
 * @param reg_addr Dirección del registro interno
 * @param data     Byte a escribir
 * @return HAL_OK o código de error
 */
HAL_StatusTypeDef I2C_WriteReg(uint16_t dev_addr, uint8_t reg_addr, uint8_t data);

/**
 * @brief Lee un byte de un registro de un dispositivo I2C.
 * @param dev_addr Dirección del dispositivo (8 bits: 7 bits << 1)
 * @param reg_addr Dirección del registro interno
 * @param data     Puntero donde almacenar el byte leído
 * @return HAL_OK o código de error
 */
HAL_StatusTypeDef I2C_ReadReg(uint16_t dev_addr, uint8_t reg_addr, uint8_t *data);

/**
 * @brief Escribe múltiples bytes comenzando en un registro.
 */
HAL_StatusTypeDef I2C_WriteMulti(uint16_t dev_addr, uint8_t reg_addr,
                                 const uint8_t *data, uint16_t len);

/**
 * @brief Lee múltiples bytes comenzando en un registro.
 */
HAL_StatusTypeDef I2C_ReadMulti(uint16_t dev_addr, uint8_t reg_addr,
                                uint8_t *data, uint16_t len);

/**
 * @brief Escritura directa sin registro (para dispositivos tipo OLED).
 * @param dev_addr Dirección del dispositivo (8 bits: 7 bits << 1)
 * @param data     Bytes a enviar (el primero es el byte de control del OLED)
 * @param len      Número de bytes
 */
HAL_StatusTypeDef I2C_WriteRaw(uint16_t dev_addr, const uint8_t *data, uint16_t len);

#endif /* HAL_I2C_H */
