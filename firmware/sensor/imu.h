#ifndef SENSOR_IMU_H
#define SENSOR_IMU_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Driver de la IMU MPU6500/MPU9250 conectada por SPI2 (capa SENSOR).
 *
 *   - WHO_AM_I: 0x70 → MPU-6500 · 0x71 → MPU-9250 (chip marcado
 *     habitualmente como "MPU-9250/6500/9255" en módulos GY-91).
 *   - Acelerómetro  ±2 g        → 16384 LSB/g
 *   - Giroscopio    ±2000 °/s   → 16.4 LSB/(°/s)
 *
 * Depende de la capa HAL (hal/spi.h). El volcado a UART lo realiza la
 * capa APP mediante TELEM_SendIMU() (USART3, formato Teleplot).
 */

#define IMU_WHO_AM_I_MPU6500    0x70U
#define IMU_WHO_AM_I_MPU9250    0x71U

/* Rangos configurados en IMU_Init() (mantener coherente con imu.c) */
#define IMU_ACCEL_FS_G          2
#define IMU_GYRO_FS_DPS         2000

/* Datos IMU escalados a unidades físicas */
typedef struct {
    float   ax, ay, az;   /* aceleración en g          */
    float   gx, gy, gz;   /* velocidad angular en °/s  */
    int16_t temp_raw;     /* ADC de temperatura (sin escalar) */
} IMU_Data_t;

/*
 * Inicializa SPI2 y configura la MPU6500/9250 (reset, wake, rangos y DLPF).
 * @return true si el WHO_AM_I corresponde a un MPU6500 o MPU9250.
 */
bool IMU_Init(void);

/*
 * Lee acelerómetro + giroscopio en ráfaga (0x3B..0x48) y los escala.
 * @param data  Estructura de salida con valores en g y °/s.
 * @return true si la transacción SPI fue correcta.
 */
bool IMU_Read(IMU_Data_t *data);

/* Lee el registro WHO_AM_I (0x70 MPU6500 / 0x71 MPU9250). */
uint8_t IMU_WhoAmI(void);

/* true si el WHO_AM_I corresponde a un MPU6500 (0x70) o MPU9250 (0x71). */
bool IMU_Detected(void);

#endif /* SENSOR_IMU_H */
