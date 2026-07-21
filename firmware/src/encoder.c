#include "encoder.h"

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

/**
 * @brief Inicializa TIM2 en modo encoder para Motor 1 (PA0=CH1, PA1=CH2)
 */
static void MX_TIM2_Encoder_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA0, PA1 como entradas (señales de encoder) */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFF;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.RepetitionCounter = 0;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    TIM_Encoder_InitTypeDef sConfig = {0};
    sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC1Filter = 0;
    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC2Filter = 0;

    HAL_TIM_Encoder_Init(&htim2, &sConfig);
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
}

/**
 * @brief Inicializa TIM3 en modo encoder para Motor 2 (PA6=CH1, PA7=CH2)
 */
static void MX_TIM3_Encoder_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA6, PA7 como entradas (señales de encoder) */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 0;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 0xFFFF;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.RepetitionCounter = 0;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    TIM_Encoder_InitTypeDef sConfig = {0};
    sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC1Filter = 0;
    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC2Filter = 0;

    HAL_TIM_Encoder_Init(&htim3, &sConfig);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
}

void ENC_Init(void)
{
    MX_TIM2_Encoder_Init();
    MX_TIM3_Encoder_Init();
}

int32_t ENC_GetCount1(void)
{
    return (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
}

int32_t ENC_GetCount2(void)
{
    return (int32_t)__HAL_TIM_GET_COUNTER(&htim3);
}

/* ================================================================
 * Contadores por canal individual (software polling)
 * ================================================================ */
static volatile uint32_t pulse_1a = 0;
static volatile uint32_t pulse_1b = 0;
static volatile uint32_t pulse_2a = 0;
static volatile uint32_t pulse_2b = 0;

static uint8_t last_1a = 0;
static uint8_t last_1b = 0;
static uint8_t last_2a = 0;
static uint8_t last_2b = 0;

void ENC_UpdatePulses(void)
{
    uint8_t cur_1a = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t cur_1b = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t cur_2a = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t cur_2b = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7) == GPIO_PIN_SET) ? 1 : 0;

    if (cur_1a && !last_1a) pulse_1a++;
    if (cur_1b && !last_1b) pulse_1b++;
    if (cur_2a && !last_2a) pulse_2a++;
    if (cur_2b && !last_2b) pulse_2b++;

    last_1a = cur_1a;
    last_1b = cur_1b;
    last_2a = cur_2a;
    last_2b = cur_2b;
}

uint32_t ENC_GetPulse1A(void) { return pulse_1a; }
uint32_t ENC_GetPulse1B(void) { return pulse_1b; }
uint32_t ENC_GetPulse2A(void) { return pulse_2a; }
uint32_t ENC_GetPulse2B(void) { return pulse_2b; }

void ENC_ResetPulses(void)
{
    pulse_1a = 0; pulse_1b = 0;
    pulse_2a = 0; pulse_2b = 0;
    last_1a = 0;  last_1b = 0;
    last_2a = 0;  last_2b = 0;
}

void ENC_ResetCount1(void)
{
    __HAL_TIM_SET_COUNTER(&htim2, 0);
}

void ENC_ResetCount2(void)
{
    __HAL_TIM_SET_COUNTER(&htim3, 0);
}
