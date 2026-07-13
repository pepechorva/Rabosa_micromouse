
#include "drv8833.h"

/**
 * @brief Inicializa los canales PWM asociados al motor
 */
void DRV8833_Init(DRV8833_Motor_t *motor) {
    HAL_TIM_PWM_Start(motor->htim, motor->channel1);
    HAL_TIM_PWM_Start(motor->htim, motor->channel2);
    DRV8833_Coast(motor); // Inicia apagado por seguridad
}

/**
 * @brief Controla el motor mediante un valor de velocidad (-100 a 100)
 */
void DRV8833_Set_Speed(DRV8833_Motor_t *motor, int16_t speed) {
    // Limitar el rango de velocidad por seguridad
    if (speed > 100)  speed = 100;
    if (speed < -100) speed = -100;

    // Calcular el Duty Cycle basado en el periodo del Timer (ARR)
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(motor->htim);
    uint32_t pulse = (uint32_t)((speed < 0 ? -speed : speed) * arr / 100);

    if (speed > 0) {
        // Giro en sentido horario (AVANCE)
        __HAL_TIM_SET_COMPARE(motor->htim, motor->channel1, pulse);
        __HAL_TIM_SET_COMPARE(motor->htim, motor->channel2, 0);
    } 
    else if (speed < 0) {
        // Giro en sentido antihorario (REVERSA)
        __HAL_TIM_SET_COMPARE(motor->htim, motor->channel1, 0);
        __HAL_TIM_SET_COMPARE(motor->htim, motor->channel2, pulse);
    } 
    else {
        // Velocidad cero -> Apagado inercial
        DRV8833_Coast(motor);
    }
}

/**
 * @brief Freno rápido (Aplica GND en ambos extremos de la bobina)
 */
void DRV8833_Brake(DRV8833_Motor_t *motor) {
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(motor->htim);
    __HAL_TIM_SET_COMPARE(motor->htim, motor->channel1, arr);
    __HAL_TIM_SET_COMPARE(motor->htim, motor->channel2, arr);
}

/**
 * @brief Marcha por inercia / Desactivado (Alta impedancia)
 */
void DRV8833_Coast(DRV8833_Motor_t *motor) {
    __HAL_TIM_SET_COMPARE(motor->htim, motor->channel1, 0);
    __HAL_TIM_SET_COMPARE(motor->htim, motor->channel2, 0);
}
