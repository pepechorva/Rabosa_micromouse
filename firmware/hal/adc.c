#include "hal/adc.h"

ADC_HandleTypeDef hadc1;

/* ------------------------------------------------------------------
 * Tiempo de muestreo: 239.5 ciclos.
 *
 * A 12 MHz de reloj de ADC son ~20 µs por conversión (+12.5 ciclos del SAR),
 * o sea 21 µs. Se elige el máximo porque TODAS las fuentes que leemos son de
 * impedancia alta:
 *   - los divisores de la batería: 100 kΩ ∥ 56 kΩ = 36 kΩ (ver hardware.md §4.6)
 *   - los fototransistores IR de los sensores de pared
 *
 * La impedancia máxima admisible vale  t_muestreo / (C_ADC · ln(2^14)) − R_ADC,
 * con C_ADC = 8 pF y R_ADC ≈ 1 kΩ en el F1:
 *   - 1.5 ciclos  (mínimo) → < 1 kΩ   ← no serviría para estos sensores
 *   - 13.5 ciclos (el que había) → ~13 kΩ
 *   - 239.5 ciclos (este)          → ~256 kΩ
 *
 * El coste es tiempo: 21 µs por canal. Leyendo los 6 canales son 126 µs, así
 * que sobra para los 10-100 Hz que usamos.
 * ------------------------------------------------------------------ */
#define ADC_TIEMPO_MUESTREO   ADC_SAMPLETIME_239CYCLES_5

/* ------------------------------------------------------------------ */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hadc->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* Reloj del ADC: PCLK2/6 = 72/6 = 12 MHz.
         * El F1 admite 14 MHz como máximo y el valor de reset del registro
         * ADCPRE es 00 (PCLK2/2 = 36 MHz), que está FUERA de especificación:
         * hay que configurarlo explícitamente. */
        __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6);

        GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

/* ------------------------------------------------------------------ */
void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance = ADC1;
    hadc1.Init.DataAlign            = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode         = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode   = DISABLE;
    hadc1.Init.DiscontinuousConvMode= DISABLE;
    hadc1.Init.NbrOfDiscConversion  = 1;
    hadc1.Init.ExternalTrigConv     = ADC_SOFTWARE_START;
    hadc1.Init.NbrOfConversion      = 1;
    HAL_ADC_Init(&hadc1);

    sConfig.Channel      = ADC_CHANNEL_2;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_TIEMPO_MUESTREO;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

/* ------------------------------------------------------------------ */
uint16_t ADC_ReadChannel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel      = channel;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_TIEMPO_MUESTREO;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint16_t value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return value;
}

/* ------------------------------------------------------------------ */
uint16_t ADC_ReadPin(ADC_Pin_t pin)
{
    switch (pin) {
        case ADC_PIN_A2: return ADC_ReadChannel(ADC_CHANNEL_2);
        case ADC_PIN_A3: return ADC_ReadChannel(ADC_CHANNEL_3);
        case ADC_PIN_A4: return ADC_ReadChannel(ADC_CHANNEL_4);
        case ADC_PIN_A5: return ADC_ReadChannel(ADC_CHANNEL_5);
        case ADC_PIN_B0: return ADC_ReadChannel(ADC_CHANNEL_8);
        case ADC_PIN_B1: return ADC_ReadChannel(ADC_CHANNEL_9);
        default:         return 0;
    }
}
