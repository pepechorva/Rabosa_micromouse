#ifndef HAL_DRV8833_H
#define HAL_DRV8833_H

#include "peripherals.h"

/**
 * @brief Estructura de configuración para un canal del DRV8833
 */
typedef struct {
    TIM_HandleTypeDef *htim;   /* Puntero al timer PWM (ej: &htim1) */
    uint32_t channel1;         /* Canal para AIN1 (ej: TIM_CHANNEL_1) */
    uint32_t channel2;         /* Canal para AIN2 (ej: TIM_CHANNEL_2) */

    /**
     * @brief 1 si este motor está montado **al revés** respecto a la convención del robot.
     *
     * La convención es **positivo = hacia delante**, la misma que usan los encoders (contar hacia
     * arriba al avanzar) y la odometría. Si un motor va montado en espejo, con `speed` positivo
     * giraría hacia atrás; este flag lo corrige **en el único sitio que escribe el PWM**, para que
     * todo lo de arriba pueda seguir hablando de «positivo = hacia delante» sin enterarse del montaje.
     *
     * @warning No es cosmético: un lazo cerrado con el signo cambiado **se realimenta al revés** y
     *          satura los dos motores en sentidos opuestos. Se vio el 2026-10-01 en la prueba 18
     *          (`>DUTY_IZQ:60` con `>VEL_IZQ:-101`).
     */
    uint8_t invertido;
} DRV8833_Motor_t;

void DRV8833_Init(DRV8833_Motor_t *motor);
void DRV8833_Set_Speed(DRV8833_Motor_t *motor, int16_t speed);
void DRV8833_Brake(DRV8833_Motor_t *motor);
void DRV8833_Coast(DRV8833_Motor_t *motor);

#endif /* HAL_DRV8833_H */
