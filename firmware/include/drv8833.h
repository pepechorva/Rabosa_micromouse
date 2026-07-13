#ifndef DRV8833_H_
#define DRV8833_H_

#include "stm32f1xx_hal.h"

/**
 * @brief Estructura de configuración para un canal del DRV8833
 */
typedef struct {
    TIM_HandleTypeDef *htim;  // Puntero al temporizador HAL (ej: &htim1)
    uint32_t channel1;        // Canal HAL para AIN1 (ej: TIM_CHANNEL_1)
    uint32_t channel2;        // Canal HAL para AIN2 (ej: TIM_CHANNEL_2)
} DRV8833_Motor_t;

/* Prototipos de funciones */
void DRV8833_Init(DRV8833_Motor_t *motor);
void DRV8833_Set_Speed(DRV8833_Motor_t *motor, int16_t speed);
void DRV8833_Brake(DRV8833_Motor_t *motor);
void DRV8833_Coast(DRV8833_Motor_t *motor);

#endif /* DRV8833_H_ */
