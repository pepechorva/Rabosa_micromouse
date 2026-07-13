//https://github.com/k3a/stm32f1-examples/blob/master/stm32cube-hal-blink.c
#include "peripherals.h"
#include "systemclock.h"
#include "timers.h"
#include "uart.h"
#include "i2c.h"
#include "telemetry.h"
#include "app.h"
#include "gpio.h"
#include "pwm.h"
#include "adc.h"

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  LEDINBOARD_GPIO_CLK_ENABLE();

  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_ADC1_Init();
  MX_USART3_UART_Init(115200);
  MX_I2C1_Init();

  HAL_GPIO_WritePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN, GPIO_PIN_RESET);
  PWM_StartAll();

  App_RunLoop2();

  return 0;
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}
