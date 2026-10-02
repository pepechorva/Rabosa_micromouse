#ifndef PINOUT_H
#define PINOUT_H

#define LEDINBOARD_PIN                GPIO_PIN_13
#define LEDINBOARD_GPIO_PORT          GPIOC
#define LEDINBOARD_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOC_CLK_ENABLE()

/* ------------------------------------------------------------------
 * PWM de motores — TIM1 canales 1..4, sobre GPIOA
 *
 *   PA8  = TIM1_CH1 = PWM1A → DRV8833 AIN1 (motor 1)
 *   PA9  = TIM1_CH2 = PWM1B → DRV8833 AIN2 (motor 1)
 *   PA10 = TIM1_CH3 = PWM2A → DRV8833 BIN1 (motor 2)
 *   PA11 = TIM1_CH4 = PWM2B → DRV8833 BIN2 (motor 2)
 *
 * OJO: la máscara se aplica SIEMPRE sobre GPIOA (ver hal/timers.c). No usarla
 * sobre GPIOB: PB8 es el emisor IR WALL L4 y PB9 es el botón UP.
 * El motor 1 (PA8/PA9) es el de la prueba App_Test_PWM_Rampa().
 * ------------------------------------------------------------------ */
#define PWM_PIN_MASK                  (GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11)

/* ------------------------------------------------------------------
 * Botones de usuario (MENU / UP / DOWN / OK)
 *
 * Cableado: botón entre el pin y GND, con pull-up interno activado.
 * Por eso la señal es ACTIVA A NIVEL BAJO.
 *
 * Asignación actual (2026-09-20: intercambiados OK↔MENU y UP↔DOWN):
 *   MENU = PC14 · UP = PB9 · DOWN = PC15 · OK = PA12
 *
 * OJO con los pines elegidos:
 *   - PB9 estaba configurado como salida en gpio.c; ahora es entrada.
 *   - PC14 y PC15 son OSC32_IN / OSC32_OUT: si la placa llevase montado el
 *     cristal de 32.768 kHz, MENU y DOWN quedan compartidos con él. El LSE no
 *     está habilitado, así que como entradas no molesta; pero si algún día se
 *     habilita el RTC habría que mover estos dos botones.
 *     Como ENTRADA no les afecta la limitación de corriente de PC13-PC15
 *     (esa limitación es solo para usarlos como salida).
 *   - PA12 es un pin limpio.
 * ------------------------------------------------------------------ */
#define BTN_MENU_PIN                  GPIO_PIN_14   /* PC14 */
#define BTN_MENU_PORT                 GPIOC
#define BTN_UP_PIN                    GPIO_PIN_9    /* PB9  */
#define BTN_UP_PORT                   GPIOB
#define BTN_DOWN_PIN                  GPIO_PIN_15   /* PC15 */
#define BTN_DOWN_PORT                 GPIOC
#define BTN_OK_PIN                    GPIO_PIN_12   /* PA12 */
#define BTN_OK_PORT                   GPIOA

/* Nivel que se lee cuando el botón está pulsado (pull-up → 0) */
#define BTN_PRESSED_STATE             GPIO_PIN_RESET

/* ------------------------------------------------------------------
 * I2C1 (pantalla OLED y lo que venga): PB6 = SCL · PB7 = SDA
 *
 * ⚠️ En I2C hacen falta pull-ups EXTERNOS: en el STM32F1 el pull-up interno
 * no se puede activar en modo alterno open-drain (el HAL ignora `Pull` en
 * GPIO_MODE_AF_OD; solo lo aplica en modo entrada). Ver hardware.md §4.9.
 * ------------------------------------------------------------------ */
#define I2C1_SCL_PIN                  GPIO_PIN_6
#define I2C1_SDA_PIN                  GPIO_PIN_7
#define I2C1_GPIO_PORT                GPIOB

/* ------------------------------------------------------------------
 * Emisores IR de pared (WALL L1..L4)
 *
 * Cada LED va con su transistor BC337 y una resistencia de 22 Ω en serie
 * (⇒ ≈75 mA por emisor; solo luce uno cada vez). La base se lleva con 1 kΩ
 * desde el GPIO: nivel ALTO = emisor encendido. Ver hardware.md §4.4.
 *
 * ⚠️ PB3 y PB4 son JTDO/NJTRST: funcionan como GPIO solo porque gpio.c
 * desactiva el JTAG (SWJ_NOJTAG). Si alguien lo vuelve a habilitar, L1 y L2
 * dejan de encender.
 * ------------------------------------------------------------------ */
#define IR_GPIO_PORT                  GPIOB
#define IR_E1_PIN                     GPIO_PIN_3    /* WALL L1 */
#define IR_E2_PIN                     GPIO_PIN_4    /* WALL L2 */
#define IR_E3_PIN                     GPIO_PIN_5    /* WALL L3 */
#define IR_E4_PIN                     GPIO_PIN_8    /* WALL L4 */
#define IR_TODOS_PIN                  (IR_E1_PIN | IR_E2_PIN | IR_E3_PIN | IR_E4_PIN)

/* ------------------------------------------------------------------
 * Entradas analógicas (ADC1)
 *
 *   PA2..PA5  WALL A1..A4   fototransistores IR de los sensores de pared
 *   PB0       ADC_BAT 1     tensión TOTAL del pack 2S   ─┐ divisores 100k/56k
 *   PB1       ADC_BAT 2     tensión de la CELDA 1       ─┘ (hardware.md §4.6)
 * ------------------------------------------------------------------ */

#endif