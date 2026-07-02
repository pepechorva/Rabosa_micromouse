#ifndef __PERIPHERALS_H
#define __PERIPHERALS_H

#include <stdint.h>

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

#define PWM_PIN_MASK                  (GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11)

extern TIM_HandleTypeDef htim1;
extern UART_HandleTypeDef huart2;

void MX_GPIO_Init(void);

#endif
