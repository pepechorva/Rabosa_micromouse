#include "sensor/imu.h"
#include "hal/spi.h"

/* ------------------------------------------------------------------
 * Registros MPU6500 (mapeo SPI, direcciones de 8 bits)
 * ------------------------------------------------------------------ */
#define MPU6500_WHO_AM_I        0x75U
#define MPU6500_PWR_MGMT_1      0x6BU
#define MPU6500_CONFIG          0x1AU
#define MPU6500_GYRO_CONFIG     0x1BU
#define MPU6500_ACCEL_CONFIG    0x1CU
#define MPU6500_ACCEL_XOUT_H    0x3BU

/* ------------------------------------------------------------------
 * Valores de configuración
 * ------------------------------------------------------------------ */
#define PWR_MGMT_1_RESET        0x80U   /* reset del dispositivo   */
#define PWR_MGMT_1_WAKE_CLKX    0x01U   /* despierta; CLKSEL=PLL X gyro */
#define GYRO_CONFIG_2000DPS     0x18U   /* ±2000 °/s               */
#define ACCEL_CONFIG_2G         0x00U   /* ±2 g                    */
#define CONFIG_DLPF_44HZ        0x03U   /* DLPF ≈ 44 Hz (acc) / 41 Hz (gyro) */
#define SIGNAL_PATH_RESET       0x68U   /* reset de la cadena (temp/acc/gyro) */
#define SIGNAL_PATH_RESET_ALL   0x07U   /* TEMP|ACCEL|GYRO reset (req. SPI)  */

/* Ráfaga de datos: ACCEL(6) + TEMP(2) + GYRO(6) desde 0x3B */
#define IMU_SENSOR_DATA_LEN     14U

/* ------------------------------------------------------------------
 * Escritura de un registro (bit 7 = 0)
 * ------------------------------------------------------------------ */
static HAL_StatusTypeDef imu_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { (uint8_t)(reg & 0x7FU), value };
    uint8_t rx[2] = { 0U, 0U };
    HAL_StatusTypeDef status;

    SPI2_CS_Low();
    status = SPI2_Transfer(tx, rx, 2U);
    SPI2_CS_High();
    return status;
}

/* ------------------------------------------------------------------
 * Lectura de uno o varios registros consecutivos (bit 7 = 1).
 * Mantiene CS bajo durante toda la ráfaga.
 * ------------------------------------------------------------------ */
static HAL_StatusTypeDef imu_read_regs(uint8_t reg, uint8_t *data, uint16_t len)
{
    uint8_t tx[IMU_SENSOR_DATA_LEN + 1U] = { 0U };
    uint8_t rx[IMU_SENSOR_DATA_LEN + 1U] = { 0U };
    HAL_StatusTypeDef status;

    if (len > IMU_SENSOR_DATA_LEN)
        return HAL_ERROR;

    tx[0] = (uint8_t)((reg & 0x7FU) | 0x80U);   /* lectura */

    SPI2_CS_Low();
    status = SPI2_Transfer(tx, rx, (uint16_t)(len + 1U));
    SPI2_CS_High();

    if (status == HAL_OK) {
        for (uint16_t i = 0U; i < len; i++)
            data[i] = rx[i + 1U];
    }
    return status;
}

/* ------------------------------------------------------------------ */
uint8_t IMU_WhoAmI(void)
{
    uint8_t id = 0U;
    imu_read_regs(MPU6500_WHO_AM_I, &id, 1U);
    return id;
}

/* ------------------------------------------------------------------ */
bool IMU_Detected(void)
{
    uint8_t id = IMU_WhoAmI();
    return (id == IMU_WHO_AM_I_MPU6500 || id == IMU_WHO_AM_I_MPU9250);
}

/* ------------------------------------------------------------------ */
bool IMU_Init(void)
{
    MX_SPI2_Init();

    /* Reset hardware del sensor y espera a que termine */
    imu_write_reg(MPU6500_PWR_MGMT_1, PWR_MGMT_1_RESET);
    HAL_Delay(100);

    /* Wake-up y reloj PLL (X gyro) para reducir el ruido del reloj */
    imu_write_reg(MPU6500_PWR_MGMT_1, PWR_MGMT_1_WAKE_CLKX);
    HAL_Delay(10);

    /* Reset de la cadena de señal: el driver del kernel Linux lo marca como
     * "required for SPI connection" en MPU6500/9250/9255 */
    imu_write_reg(SIGNAL_PATH_RESET, SIGNAL_PATH_RESET_ALL);
    HAL_Delay(100);

    /* Rangos de medida */
    imu_write_reg(MPU6500_GYRO_CONFIG, GYRO_CONFIG_2000DPS);   /* ±2000 °/s */
    imu_write_reg(MPU6500_ACCEL_CONFIG, ACCEL_CONFIG_2G);      /* ±2 g      */
    imu_write_reg(MPU6500_CONFIG, CONFIG_DLPF_44HZ);           /* DLPF      */

    return IMU_Detected();
}

/* ------------------------------------------------------------------ */
bool IMU_Read(IMU_Data_t *data)
{
    uint8_t buf[IMU_SENSOR_DATA_LEN];
    int16_t ax, ay, az, gx, gy, gz;
    const float accel_scale = 16384.0f;   /* LSB/g  (±2 g)   */
    const float gyro_scale  = 16.4f;      /* LSB/(°/s) (±2000) */

    if (data == NULL)
        return false;

    if (imu_read_regs(MPU6500_ACCEL_XOUT_H, buf, IMU_SENSOR_DATA_LEN) != HAL_OK)
        return false;

    ax = (int16_t)((buf[0] << 8) | buf[1]);
    ay = (int16_t)((buf[2] << 8) | buf[3]);
    az = (int16_t)((buf[4] << 8) | buf[5]);
    data->temp_raw = (int16_t)((buf[6] << 8) | buf[7]);
    gx = (int16_t)((buf[8] << 8) | buf[9]);
    gy = (int16_t)((buf[10] << 8) | buf[11]);
    gz = (int16_t)((buf[12] << 8) | buf[13]);

    data->ax = (float)ax / accel_scale;
    data->ay = (float)ay / accel_scale;
    data->az = (float)az / accel_scale;

    data->gx = (float)gx / gyro_scale;
    data->gy = (float)gy / gyro_scale;
    data->gz = (float)gz / gyro_scale;

    return true;
}
