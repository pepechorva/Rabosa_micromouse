#include "app.h"
#include "pwm.h"
#include "uart.h"
#include "telemetry.h"
#include "drv8833.h"
#include "timers.h"
#include "adc.h"
#include "encoder.h"

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

void App_RunLoop3(void)
{
    /* Configurar dos motores usando TIM1 CH1/CH2 y CH3/CH4 */
    DRV8833_Motor_t motor1 = { &htim1, TIM_CHANNEL_1, TIM_CHANNEL_2 };
    DRV8833_Motor_t motor2 = { &htim1, TIM_CHANNEL_3, TIM_CHANNEL_4 };

    DRV8833_Init(&motor1);
    DRV8833_Init(&motor2);

    /* 3 segundos = 3000 ms; con HAL_Delay(10) → 300 pasos */
    const uint16_t TOTAL_STEPS = 300U;
    int16_t speed = 0;
    uint32_t arr = 0;
    uint16_t d1a = 0, d1b = 0;

    while (1)
    {
        /* ---- Fase 1: Rampa progresiva 0 → +100 en 3s ---- */
        for (uint16_t i = 0; i < TOTAL_STEPS; i++)
        {
            speed = (int16_t)((i + 1) * 100 / TOTAL_STEPS);

            DRV8833_Set_Speed(&motor1, speed);
            DRV8833_Set_Speed(&motor2, speed);

            /* Telemetría */
            arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
            d1a = (uint16_t)(speed * arr / 100);
            d1b = 0;
            Send_Teleplot_DutyValues(d1a, d1b, d1a, d1b);

            HAL_Delay(10);
        }

        /* ---- Fase 2: Frenado brusco a 0 ---- */
        speed = 0;
        DRV8833_Brake(&motor1);
        DRV8833_Brake(&motor2);

        arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
        d1a = (uint16_t)(speed * arr / 100);
        d1b = 0;
        Send_Teleplot_DutyValues(d1a, d1b, d1a, d1b);
        HAL_Delay(10);

        /* ---- Fase 3: Aceleración brusca a -100 (reversa) ---- */
        speed = -100;
        DRV8833_Set_Speed(&motor1, speed);
        DRV8833_Set_Speed(&motor2, speed);

        arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
        d1a = 0;
        d1b = (uint16_t)(100 * arr / 100);
        Send_Teleplot_DutyValues(d1a, d1b, d1a, d1b);
        HAL_Delay(10);

        /* ---- Fase 4: Frenada progresiva -100 → 0 en 3s ---- */
        for (uint16_t i = 0; i < TOTAL_STEPS; i++)
        {
            speed = (int16_t)(-100 + ((i + 1) * 100 / TOTAL_STEPS));

            DRV8833_Set_Speed(&motor1, speed);
            DRV8833_Set_Speed(&motor2, speed);

            arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
            d1a = 0;
            d1b = (uint16_t)((-speed) * arr / 100);
            Send_Teleplot_DutyValues(d1a, d1b, d1a, d1b);

            HAL_Delay(10);
        }
    }
}

void App_RunLoop4(void)
{
    /* Configurar dos motores usando TIM1 CH1/CH2 y CH3/CH4 */
    DRV8833_Motor_t motor1 = { &htim1, TIM_CHANNEL_1, TIM_CHANNEL_2 };
    DRV8833_Motor_t motor2 = { &htim1, TIM_CHANNEL_3, TIM_CHANNEL_4 };

    DRV8833_Init(&motor1);
    DRV8833_Init(&motor2);
    ENC_Init();

    int32_t base = 0;
    uint32_t arr = 0;
    uint16_t d1a = 0, d1b = 0;

    while (1)
    {
        /* Avanzar 50 pulsos al 25% de velocidad */
        base = ENC_GetCount1();
        while (1)
        {
            int32_t delta = ENC_GetCount1() - base;
            if (delta >= 50) break;

            DRV8833_Set_Speed(&motor1, 25);
            DRV8833_Set_Speed(&motor2, 25);

            /* Telemetría */
            arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
            d1a = (uint16_t)(25UL * arr / 100);
            d1b = 0;
            Send_Teleplot_DutyValues(d1a, d1b, d1a, d1b);
            Send_Teleplot_MotorEncoder((int16_t)ENC_GetCount1(), 25, (int16_t)ENC_GetCount2(), 25);

            HAL_Delay(1);
        }

        /* Frenar y esperar 3 segundos */
        DRV8833_Brake(&motor1);
        DRV8833_Brake(&motor2);

        arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
        d1a = 0;
        d1b = 0;
        Send_Teleplot_DutyValues(d1a, d1b, d1a, d1b);
        Send_Teleplot_MotorEncoder((int16_t)ENC_GetCount1(), 0, (int16_t)ENC_GetCount2(), 0);

        HAL_Delay(3000);
    }
}

void App_RunLoop_Encoders(void)
{
    /* Solo inicializa los encoders, sin motores */
    ENC_Init();
    ENC_ResetPulses();

    while (1)
    {
        ENC_UpdatePulses();

        Send_Teleplot_EncoderPulses(
            ENC_GetPulse1A(),
            ENC_GetPulse1B(),
            ENC_GetPulse2A(),
            ENC_GetPulse2B()
        );

        HAL_Delay(1);
    }
}

void App_RunLoop_CalcularReductora(void)
{
    /* Solo inicializar encoders — SIN motores */
    ENC_Init();
    ENC_ResetCount1();
    ENC_ResetCount2();
    ENC_ResetPulses();

    int32_t last_enc1 = 0;
    int32_t last_enc2 = 0;
    uint32_t last_activity = HAL_GetTick();
    int32_t peak_enc1 = 0;
    int32_t peak_enc2 = 0;
    uint32_t pulses_1a = 0, pulses_1b = 0;
    uint32_t pulses_2a = 0, pulses_2b = 0;

    while (1)
    {
        ENC_UpdatePulses();

        int32_t enc1 = ENC_GetCount1();
        int32_t enc2 = ENC_GetCount2();

        /* Detectar movimiento en cualquier encoder */
        if (enc1 != last_enc1 || enc2 != last_enc2)
        {
            last_activity = HAL_GetTick();

            /* Pico máximo alcanzado (para ver el valor tras una vuelta) */
            if (enc1 > peak_enc1) peak_enc1 = enc1;
            if (enc2 > peak_enc2) peak_enc2 = enc2;

            last_enc1 = enc1;
            last_enc2 = enc2;
        }

        /* Timeout 5s sin movimiento → reiniciar todo */
        if ((HAL_GetTick() - last_activity) >= 5000U)
        {
            ENC_ResetCount1();
            ENC_ResetCount2();
            ENC_ResetPulses();
            last_enc1 = 0;
            last_enc2 = 0;
            peak_enc1 = 0;
            peak_enc2 = 0;
            pulses_1a = 0; pulses_1b = 0;
            pulses_2a = 0; pulses_2b = 0;
            last_activity = HAL_GetTick();
        }

        /* Leer pulsos individuales actualizados */
        pulses_1a = ENC_GetPulse1A();
        pulses_1b = ENC_GetPulse1B();
        pulses_2a = ENC_GetPulse2A();
        pulses_2b = ENC_GetPulse2B();

        /* Telemetría a teleplot */
        Send_Teleplot_EncoderPulses(pulses_1a, pulses_1b, pulses_2a, pulses_2b);

        /* Canal 1A = ENC1 (hardware), 1B = pico ENC1
           Canal 2A = ENC2 (hardware), 2B = pico ENC2 */
        Send_Teleplot_MotorEncoder(
            (int16_t)enc1,
            (int16_t)peak_enc1,
            (int16_t)enc2,
            (int16_t)peak_enc2
        );

        HAL_Delay(10);
    }
}

