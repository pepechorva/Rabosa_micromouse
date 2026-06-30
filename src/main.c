//https://github.com/k3a/stm32f1-examples/blob/master/stm32cube-hal-blink.c
#if STM32F0
#include "stm32f0xx_hal.h"
#elif STM32F1
#include "stm32f1xx_hal.h"
#elif STM32F2
#include "stm32f2xx_hal.h"
#elif STM32F3
#include "stm32f3xx_hal.h"
#elif STM32F4
#include "stm32f4xx_hal.h"
#elif STM32F7
#include "stm32f7xx_hal.h"
#elif STM32L0
#include "stm32l0xx_hal.h"
#elif STM32L1
#include "stm32l1xx_hal.h"
#elif STM32L4
#include "stm32l4xx_hal.h"
#else
#error "Unsupported STM32 Family"
#endif

#define LEDINBOARD_PIN                GPIO_PIN_13
#define LEDINBOARD_GPIO_PORT          GPIOC
#define LEDINBOARD_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOC_CLK_ENABLE()

#define LED_PIN                       (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)
#define LED_GPIO_PORT                 GPIOA
#define LED_GPIO_CLK_ENABLE()         __HAL_RCC_GPIOA_CLK_ENABLE()

// Instancia global del Timer 2
TIM_HandleTypeDef htim2;

void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef clkinitstruct = {0};
  RCC_OscInitTypeDef oscinitstruct = {0};

  /* Configure PLL ------------------------------------------------------*/
  /* PLL configuration: PLLCLK = (HSI / 2) * PLLMUL = (8 / 2) * 16 = 64 MHz */
  /* PREDIV1 configuration: PREDIV1CLK = PLLCLK / HSEPredivValue = 64 / 1 = 64 MHz */
  /* Enable HSI and activate PLL with HSi_DIV2 as source */
  oscinitstruct.OscillatorType  = RCC_OSCILLATORTYPE_HSI;
  oscinitstruct.HSEState        = RCC_HSE_OFF;
  oscinitstruct.LSEState        = RCC_LSE_OFF;
  oscinitstruct.HSIState        = RCC_HSI_ON;
  oscinitstruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  oscinitstruct.HSEPredivValue    = RCC_HSE_PREDIV_DIV1;
  oscinitstruct.PLL.PLLState    = RCC_PLL_ON;
  oscinitstruct.PLL.PLLSource   = RCC_PLLSOURCE_HSI_DIV2;
  oscinitstruct.PLL.PLLMUL      = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&oscinitstruct)!= HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers */
  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2; // Reloj de periféricos APB1 = 64MHz / 2 = 32MHz. TIM2 hereda x2 -> 64MHz.
  if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2)!= HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }
}

// Configuración del Timer 2 y mapeo de los 4 canales PWM para PA0, PA1, PA2, PA3
void MX_TIM2_Init(void) {
    __HAL_RCC_TIM2_CLK_ENABLE();
    LED_GPIO_CLK_ENABLE();

    // Los pines PA0, PA1, PA2, PA3 deben configurarse como función alternativa push-pull (AF_PP)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);

    // Ajuste de periodo para el reloj configurado a 64MHz
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 63;        // Frecuencia de conteo = 64MHz / (63 + 1) = 1 MHz
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 1000;         // Resolución total = 1000 pasos (0 a 1000 para facilitar el porcentaje)
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_PWM_Init(&htim2);

    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;              // Ciclo de trabajo inicial al 0%
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    // Configura los 4 canales asociados a PA0 (CH1), PA1 (CH2), PA2 (CH3) y PA3 (CH4)
    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3);
    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4);
}

void GPIOC_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = LEDINBOARD_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LEDINBOARD_GPIO_PORT, &GPIO_InitStruct); 
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  LEDINBOARD_GPIO_CLK_ENABLE();
  GPIOC_Init();
  
  // Inicializamos el Timer 2 con sus configuraciones PWM
  MX_TIM2_Init();
  
  // Encendemos el LED integrado (lógica inversa de la Blue Pill)
  HAL_GPIO_WritePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN, GPIO_PIN_RESET);

  // Activamos las salidas PWM en los 4 canales
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

  int16_t dutyCycle = 0;
  int8_t step = 10; // Incrementos de ciclo de trabajo (Paso de 1%)

  while (1)
  {
    // Modifica dinámicamente el pulso en los registros correspondientes
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, dutyCycle);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, dutyCycle);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, dutyCycle);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, dutyCycle);

    dutyCycle += step;

    // Cambia de dirección al llegar a los extremos (0% o 100% de 1000 pasos)
    if (dutyCycle >= 1000 || dutyCycle <= 0) {
      step = -step; 
    }

    HAL_Delay(15); // Ajusta este tiempo para controlar la velocidad del "Blink" o desvanecimiento
  }
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}
