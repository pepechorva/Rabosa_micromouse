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

// Configura el oscilador externo HSE y el multiplicador PLL para alcanzar los 72 MHz máximos
void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef clkinitstruct = {0};
  RCC_OscInitTypeDef oscinitstruct = {0};

  /* Habilita el cristal externo de 8MHz (HSE) y multiplica x9: 8MHz * 9 = 72 MHz */
  oscinitstruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
  oscinitstruct.HSEState            = RCC_HSE_ON;
  oscinitstruct.HSEPredivValue      = RCC_HSE_PREDIV_DIV1;
  oscinitstruct.LSEState            = RCC_LSE_OFF;
  oscinitstruct.HSIState            = RCC_HSI_OFF;
  oscinitstruct.PLL.PLLState        = RCC_PLL_ON;
  oscinitstruct.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
  oscinitstruct.PLL.PLLMUL          = RCC_PLL_MUL9; 
  if (HAL_RCC_OscConfig(&oscinitstruct) != HAL_OK)
  {
    /* Error de inicialización */
    while(1);
  }

  /* Elige el PLL como reloj de sistema y configura los divisores de buses */
  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;  // AHB Clock = 72 MHz (Máximo)
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;   // APB2 Clock = 72 MHz (Máximo)
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;   // APB1 Clock = 36 MHz (Límite máximo permitido de 36MHz)
  
  if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2) != HAL_OK)
  {
    /* Error de inicialización */
    while(1);
  }
}

// Configuración del Timer 2 y mapeo de los 4 canales PWM para PA0, PA1, PA2, PA3 a 72MHz
void MX_TIM2_Init(void) {
    __HAL_RCC_TIM2_CLK_ENABLE();
    LED_GPIO_CLK_ENABLE();

    // Los pines PA0, PA1, PA2, PA3 se configuran como función alternativa push-pull (AF_PP)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);

    /* El reloj base de TIM2 en APB1 se multiplica automáticamente x2 si APB1 div != 1.
       Por lo tanto, la frecuencia de entrada del Timer es: 36 MHz * 2 = 72 MHz. */
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 71;        // Frecuencia de conteo = 72MHz / (71 + 1) = 1 MHz
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 1000;         // Resolución total = 1000 pasos (Frecuencia PWM = 1MHz / 1000 = 1 kHz)
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_PWM_Init(&htim2);

    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;              // Ciclo de trabajo inicial al 0%
    sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;
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
  
  // Inicializamos el Timer 2 con las nuevas configuraciones para 72MHz
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

  uint16_t dutyLedA0 = 0;
  uint16_t dutyLedA1 = 0;
  uint16_t dutyLedA2 = 0;
  uint16_t dutyLedA3 = 0;
  
  int8_t activeLED= 1;
  int8_t direction = 1;
  uint32_t moveCounter = 0;

  while (1)
  {
    // 1. ATENUACIÓN CONSTANTE (Corre a máxima velocidad en cada ciclo del bucle)
    // Multiplicar por 0.85 desgasta el brillo suavemente. Ajusta a 0.80 si quieres menos estela.
    dutyLedA0 = (uint16_t)(dutyLedA0 * 0.85);
    dutyLedA1 = (uint16_t)(dutyLedA1 * 0.85);
    dutyLedA2 = (uint16_t)(dutyLedA2 * 0.85);
    dutyLedA3 = (uint16_t)(dutyLedA3 * 0.85);

    // 2. CONTROL DE MOVIMIENTO (Solo movemos el "foco" cada X ciclos)
    // Esto da tiempo a que los LEDs anteriores muestren su rampa de apagado
    moveCounter++;
    if (moveCounter >= 12) // Ajusta este número para cambiar la velocidad del viaje (ej: 8 más rápido, 15 más lento)
    {
      moveCounter = 0; // Reiniciamos el contador de movimiento

      // Movemos la posición del LED activo
      activeLED += direction;

      // Si llega a los extremos, rebotamos invirtiendo el sentido
      if (activeLED >= 4) {
        activeLED = 4;
        direction = -1;
      }
      else if (activeLED <= 1) {
        activeLED = 1;
        direction = 1;
      }
    }

    // 3. SEÑALAMOS EL LED ACTIVO (Siempre forzamos al actual al 100%)
    if (activeLED == 1) dutyLedA0 = 1000;
    if (activeLED == 2) dutyLedA1 = 1000;
    if (activeLED == 3) dutyLedA2 = 1000;
    if (activeLED == 4) dutyLedA3 = 1000;

    // 4. ESCRIBIMOS EN LOS REGISTROS DEL HARDWARE
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, dutyLedA0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, dutyLedA1);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, dutyLedA2);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, dutyLedA3);

    // 5. PEQUEÑO RETARDO BASE (Mantiene estable el refresco visual de los PWM)
    HAL_Delay(10); 
  }
}


void SysTick_Handler(void)
{
  HAL_IncTick();
}
