#include "hal/drv8833.h"

/* ------------------------------------------------------------------ */
void DRV8833_Init(DRV8833_Motor_t *motor)
{
    HAL_TIM_PWM_Start(motor->htim, motor->channel1);
    HAL_TIM_PWM_Start(motor->htim, motor->channel2);
    DRV8833_Coast(motor);
}

/* ------------------------------------------------------------------
 * Control de velocidad: rango -100 .. +100
 *   > 0 : sentido horario (channel1 PWM, channel2 LOW)
 *   < 0 : sentido antihorario (channel1 LOW, channel2 PWM)
 *   0   : Coast
 * ------------------------------------------------------------------ */
void DRV8833_Set_Speed(DRV8833_Motor_t *motor, int16_t speed)
{
    /* El montaje manda: si el motor va al revés, se invierte aquí y santo. Todo lo de arriba sigue
       hablando de «positivo = hacia delante». */
    if (motor->invertido != 0U) {
        speed = (int16_t)(-speed);
    }

    if (speed > 100)  speed = 100;
    if (speed < -100) speed = -100;

    uint32_t arr   = __HAL_TIM_GET_AUTORELOAD(motor->htim);
    uint32_t pulse = (uint32_t)((speed < 0 ? -speed : speed) * arr / 100);

    if (speed > 0) {
        __HAL_TIM_SET_COMPARE(motor->htim, motor->channel1, pulse);
        __HAL_TIM_SET_COMPARE(motor->htim, motor->channel2, 0);
    } else if (speed < 0) {
        __HAL_TIM_SET_COMPARE(motor->htim, motor->channel1, 0);
        __HAL_TIM_SET_COMPARE(motor->htim, motor->channel2, pulse);
    } else {
        DRV8833_Coast(motor);
    }
}

/* ------------------------------------------------------------------ */
void DRV8833_Brake(DRV8833_Motor_t *motor)
{
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(motor->htim);
    __HAL_TIM_SET_COMPARE(motor->htim, motor->channel1, arr);
    __HAL_TIM_SET_COMPARE(motor->htim, motor->channel2, arr);
}

/* ------------------------------------------------------------------ */
void DRV8833_Coast(DRV8833_Motor_t *motor)
{
    __HAL_TIM_SET_COMPARE(motor->htim, motor->channel1, 0);
    __HAL_TIM_SET_COMPARE(motor->htim, motor->channel2, 0);
}
