#include "app.h"
#include "pwm.h"
#include "uart.h"
#include "telemetry.h"
#include "drv8833.h"
#include "timers.h"
#include "adc.h"

void App_RunLoop(void)
{
    /* Configurar dos motores usando TIM1 CH1/CH2 y CH3/CH4 */
    DRV8833_Motor_t motor1 = { &htim1, TIM_CHANNEL_1, TIM_CHANNEL_2 };
    DRV8833_Motor_t motor2 = { &htim1, TIM_CHANNEL_3, TIM_CHANNEL_4 };

    DRV8833_Init(&motor1);
    DRV8833_Init(&motor2);

    int16_t speed1 = 0;
    int16_t speed2 = 0;
    int8_t activeLED = 1;
    int8_t direction = 1;
    uint32_t moveCounter = 0;

    while (1)
    {
        /* Decaimiento suave */
        speed1 = (int16_t)(speed1 * 85 / 100);
        speed2 = (int16_t)(speed2 * 85 / 100);

        moveCounter++;
        if (moveCounter >= 12)
        {
            moveCounter = 0;
            activeLED += direction;

            if (activeLED >= 4) {
                activeLED = 4;
                direction = -1;
            }
            else if (activeLED <= 1) {
                activeLED = 1;
                direction = 1;
            }
        }

        /* Asignar pulsos máximos a cada canal alternadamente
           speed rango: -100..100 */
        if (activeLED == 1) speed1 = 100;      // Motor1 avance
        if (activeLED == 2) speed1 = -100;     // Motor1 reversa
        if (activeLED == 3) speed2 = 100;      // Motor2 avance
        if (activeLED == 4) speed2 = -100;     // Motor2 reversa

        /* Aplicar velocidades */
        DRV8833_Set_Speed(&motor1, speed1);
        DRV8833_Set_Speed(&motor2, speed2);

        /* Enviar telemetría: convertir a valores de duty (0..ARR)
           para mostrar en la herramienta existente */
        uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
        uint16_t d1a = 0, d1b = 0, d2a = 0, d2b = 0;
        if (speed1 > 0) d1a = (uint16_t)(speed1 * arr / 100);
        else if (speed1 < 0) d1b = (uint16_t)((-speed1) * arr / 100);
        if (speed2 > 0) d2a = (uint16_t)(speed2 * arr / 100);
        else if (speed2 < 0) d2b = (uint16_t)((-speed2) * arr / 100);

        Send_Teleplot_DutyValues(d1a, d1b, d2a, d2b);

        HAL_Delay(10);
    }
}

void App_RunLoop2(void)
{
    while (1)
    {
        uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
        if (arr == 0U)
        {
            arr = 1U;
        }

        uint16_t adcA2 = ADC_ReadPin(ADC_PIN_A2);
        uint16_t adcA3 = ADC_ReadPin(ADC_PIN_A3);
        uint16_t adcA4 = ADC_ReadPin(ADC_PIN_A4);
        uint16_t adcA5 = ADC_ReadPin(ADC_PIN_A5);

        uint16_t dutyPWM1 = (uint16_t)((adcA2 * arr) / 4095U);
        uint16_t dutyPWM2 = (uint16_t)((adcA3 * arr) / 4095U);
        uint16_t dutyPWM3 = (uint16_t)((adcA4 * arr) / 4095U);
        uint16_t dutyPWM4 = (uint16_t)((adcA5 * arr) / 4095U);

        PWM_SetDuties(dutyPWM1, dutyPWM2, dutyPWM3, dutyPWM4);

        HAL_Delay(10);
    }
}
