#ifndef PINOUT_H
#define PINOUT_H

#define LEDINBOARD_PIN                GPIO_PIN_13
#define LEDINBOARD_GPIO_PORT          GPIOC
#define LEDINBOARD_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOC_CLK_ENABLE()

#define PWM_PIN_MASK                  (GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11)


#endif