/*
 * main.c – Punto de entrada del firmware Rabosa MVR.
 *
 * La inicialización y el bucle principal se delegan en la
 * capa APP (app/application.c), que orquesta todas las demás.
 *
 * Arquitectura por capas:
 *   APP → CONTROL → FILTER → SENSOR → HAL → HW
 */

#include "peripherals.h"
#include "hal/systemclock.h"
#include "hal/gpio.h"
#include "hal/uart.h"
#include "app/control_loop.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    LEDINBOARD_GPIO_CLK_ENABLE();

    /* La APP se encarga de inicializar todo y ejecutar el bucle */
    extern void APP_InitAll(void);
    extern void APP_Run(void);

    APP_InitAll();

    /* Prender el LED para indicar que arrancó */
    HAL_GPIO_WritePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN, GPIO_PIN_RESET);

    //while(1){;}

    APP_Run();

    /* Nunca debe llegar aquí */
    return 0;
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/**
 * @brief Interrupción del botón MENU (PC14).
 *
 * Es lo que permite que el botón responda siempre, incluso dentro del bucle de una prueba de banco.
 * El trabajo lo hace `hal/gpio.c`, que solo incrementa un contador.
 */
void EXTI15_10_IRQHandler(void)
{
    GPIO_MenuBtnIRQ_Handler();
}

/**
 * @brief Interrupción de USART3: recepción de los comandos que llegan por telemetría.
 *
 * El trabajo lo hace `hal/uart.c`, que guarda el byte en un buffer circular. Aquí no se llama al HAL
 * a propósito: su modo de recepción puede dejar un flag que repita la interrupción sin fin.
 */
void USART3_IRQHandler(void)
{
    UART_RxIRQHandler();
}

/**
 * @brief Interrupción de actualización de TIM1: el paso del lazo de control.
 *
 * Corre **una vez por período de PWM** (999 Hz) y es lo que **adelanta al bucle principal**: la
 * pantalla y la telemetría pueden tardar lo que quieran, que el robot sigue regulando. El trabajo lo
 * hace `app/control_loop.c`, y el porqué está en `Documentacion/lazo-control.md`.
 */
void TIM1_UP_IRQHandler(void)
{
    LOOP_IrqHandler();
}

/**
 * @brief Fallo del procesador (acceso inválido, división entre cero, pila rota…).
 *
 * Parpadea el LED onboard a ~3 Hz en vez de quedarse en un `while (1)` mudo: así se distingue «el
 * firmware se ha colgado» de «la pantalla no responde», que se veían igual desde fuera.
 */
void HardFault_Handler(void)
{
    for (;;) {
        HAL_GPIO_TogglePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN);
        for (volatile uint32_t i = 0U; i < 400000U; i++) {
            ;
        }
    }
}

void NMI_Handler(void) {}
void MemManage_Handler(void) { while (1); }
void BusFault_Handler(void)  { while (1); }
void UsageFault_Handler(void){ while (1); }
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}
