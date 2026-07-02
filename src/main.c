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

#include <stdio.h>

#define LEDINBOARD_PIN                GPIO_PIN_13
#define LEDINBOARD_GPIO_PORT          GPIOC
#define LEDINBOARD_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOC_CLK_ENABLE()

#define PWM_PIN_MASK                  (GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11)

// Instancias globales de los periféricos usados
TIM_HandleTypeDef htim1;
UART_HandleTypeDef huart2;

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
    while(1);
  }

  /* Elige el PLL como reloj de sistema y configura los divisores de buses */
  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2) != HAL_OK)
  {
    while(1);
  }
}

// Configuración del Timer 1 para los 4 canales PWM en PA8..PA11
void MX_TIM1_Init(void)
{
    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = PWM_PIN_MASK;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 71;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = 1000;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    HAL_TIM_PWM_Init(&htim1);

    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3);
    HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_4);
}

// Configuración de UART2 en PA2 (TX) y PA3 (RX) a 115200 baudios
void MX_USART2_UART_Init(uint32_t baudrate)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = baudrate;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Pin = LEDINBOARD_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LEDINBOARD_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_INPUT;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

static void Send_Teleplot_DutyValues(uint16_t dutyA8, uint16_t dutyA9, uint16_t dutyA10, uint16_t dutyA11)
{
    char buffer[96];
    int len = snprintf(buffer, sizeof(buffer),
                       ">M8:%u\r\n"
                       ">M9:%u\r\n"
                       ">MA:%u\r\n"
                       ">MB:%u\r\n",
                       dutyA8, dutyA9, dutyA10, dutyA11);
    if (len > 0) {
        HAL_UART_Transmit(&huart2, (uint8_t *)buffer, (uint16_t)len, HAL_MAX_DELAY);
    }
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  LEDINBOARD_GPIO_CLK_ENABLE();

  MX_GPIO_Init();
  MX_TIM1_Init();

  MX_USART2_UART_Init(115200);

  HAL_GPIO_WritePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN, GPIO_PIN_RESET);

  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

  uint16_t dutyLedA8 = 0;
  uint16_t dutyLedA9 = 0;
  uint16_t dutyLedA10 = 0;
  uint16_t dutyLedA11 = 0;

  int8_t activeLED = 1;
  int8_t direction = 1;
  uint32_t moveCounter = 0;

  while (1)
  {
    dutyLedA8 = (uint16_t)(dutyLedA8 * 0.85);
    dutyLedA9 = (uint16_t)(dutyLedA9 * 0.85);
    dutyLedA10 = (uint16_t)(dutyLedA10 * 0.85);
    dutyLedA11 = (uint16_t)(dutyLedA11 * 0.85);

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

    if (activeLED == 1) dutyLedA8 = 1000;
    if (activeLED == 2) dutyLedA9 = 1000;
    if (activeLED == 3) dutyLedA10 = 1000;
    if (activeLED == 4) dutyLedA11 = 1000;

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, dutyLedA8);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, dutyLedA9);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, dutyLedA10);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, dutyLedA11);

    Send_Teleplot_DutyValues(dutyLedA8, dutyLedA9, dutyLedA10, dutyLedA11);

    HAL_Delay(10);
  }
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}
