#include "hal/timers.h"
#include "pinout.h"

TIM_HandleTypeDef htim1;

/* ------------------------------------------------------------------
 * TIM1: PWM para motores (canales 1-4)
 *   Prescaler 71 → tick de 1 µs (72 MHz / 72)
 *   Period 1000  → PWM a 1 kHz
 * ------------------------------------------------------------------ */
void MX_TIM1_Init(void)
{
    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Pines PWM: PA8-PA11 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = PWM_PIN_MASK;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 71;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = 1000;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    HAL_TIM_PWM_Init(&htim1);

    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_4);
}

/* ------------------------------------------------------------------
 * TIM4: base de tiempos del cronómetro
 *   Prescaler 143 → 500 kHz (72 MHz / 144) ⇒ 2 µs por cuenta
 *   Period 65535  → el contador da la vuelta cada 131 ms
 *
 * A 1 MHz los 16 bits solo dan 65,5 ms de margen, y una vuelta del lazo con pantalla y telemetría
 * dentro se acerca a los 65 ms: la medida saldría mal justo cuando más interesa. Con 500 kHz sobra
 * margen y los 2 µs de resolución no se notan en tramos de milisegundos.
 * ------------------------------------------------------------------ */
#define TIM4_PRESCALER     143U

/* El contador va a 500 kHz (72 MHz / 144), así que **cada cuenta son 2 µs**: para pasar a
   microsegundos hay que **multiplicar** por esto, no dividir.

   ⚠️ La primera versión de esto dividía, y el error no salta a la vista: los tres tramos salían
   **cuatro veces más cortos** de lo que son (cuentas/2 en vez de cuentas×2) y los números seguían
   siendo plausibles. Se descubrió cruzando dos referencias externas: el `CCR` de la I²C leído por
   SWD decía 100 kHz exactos —así que una pantalla completa tenía que tardar ~50 ms, y no los
   13,2 ms que salían— y un `AT+CIPSTART` del ESP (~40 B a 115200 = 3,5 ms) daba 826 µs. Las dos
   apuntaban al mismo factor 4. */
#define TIM4_US_POR_CUENTA 2U

TIM_HandleTypeDef htim4;

static uint32_t tramo_ultimo[TIM_TRAMO_N];
static uint32_t tramo_maximo[TIM_TRAMO_N];

void MX_TIM4_Init(void)
{
    __HAL_RCC_TIM4_CLK_ENABLE();

    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = TIM4_PRESCALER;
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = 0xFFFFU;
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim4);

    HAL_TIM_Base_Start(&htim4);
}

uint16_t TIM_Cuentas(void)
{
    return (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
}

uint32_t TIM_Cronometrar(TIM_Tramo_t tramo, uint16_t inicio)
{
    uint32_t us;

    if (tramo >= TIM_TRAMO_N) {
        return 0U;
    }

    /* La resta en uint16_t resuelve sola la vuelta del contador (ver el Doxygen de la cabecera). */
    us = (uint32_t)(uint16_t)(TIM_Cuentas() - inicio) * TIM4_US_POR_CUENTA;

    tramo_ultimo[tramo] = us;

    if (us > tramo_maximo[tramo]) {
        tramo_maximo[tramo] = us;
    }

    return us;
}

uint32_t TIM_GetUltimo(TIM_Tramo_t tramo)
{
    return (tramo < TIM_TRAMO_N) ? tramo_ultimo[tramo] : 0U;
}

uint32_t TIM_GetMaximo(TIM_Tramo_t tramo)
{
    return (tramo < TIM_TRAMO_N) ? tramo_maximo[tramo] : 0U;
}

void TIM_ResetMaximos(void)
{
    for (uint8_t i = 0U; i < (uint8_t)TIM_TRAMO_N; i++) {
        tramo_maximo[i] = 0U;
    }
}
