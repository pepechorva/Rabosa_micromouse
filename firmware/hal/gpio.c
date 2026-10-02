#include "hal/gpio.h"
#include "pinout.h"

/**
 * @brief Configura el botón MENU como interrupción externa por flanco de bajada.
 *
 * MENU va por interrupción en vez de por sondeo porque el banco de pruebas son rutinas que no llaman
 * a BUTTONS_Update(): así la pulsación se registra siempre y la prueba en curso puede terminar.
 *
 * @note La línea 14 comparte vector con PA12 (OK) y PC15 (DOWN), pero solo se habilita la de MENU.
 */
static void gpio_init_boton_menu_irq(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin  = BTN_MENU_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BTN_MENU_PORT, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 3U, 0U);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* ------------------------------------------------------------------ */
void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* Libera PB3 (JTDO) y PB4 (NJTRST) del control del JTAG.
     * Necesario para los emisores IR WALL L1/L2 (PB3/PB4), que si no no
     * pueden usarse como salida. De paso libera PA15 (JTDI), que queda libre.
     * OJO: NO usar __HAL_AFIO_REMAP_SWJ_DISABLE(), que deshabilitaría también
     * SWD (PA13/PA14) y se perdería el ST-Link. */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    /* LED onboard (PC13) */
    GPIO_InitStruct.Pin   = LEDINBOARD_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LEDINBOARD_GPIO_PORT, &GPIO_InitStruct);

    /* PA0-PA3: GPIO Input (encoders/buttons) */
    GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA12: botón OK (entrada con pull-up, activo a nivel bajo) */
    GPIO_InitStruct.Pin   = BTN_OK_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(BTN_OK_PORT, &GPIO_InitStruct);

    /* PA13, PA14 y PA15 NO se tocan:
     *   - PA13/PA14 son SWDIO/SWCLK. Reconfigurarlos como GPIO dejaría sin
     *     depurador al programa en ejecución.
     *   - PA15 (JTDI) queda libre: al desactivar JTAG pasa a ser un GPIO
     *     normal, pero ya no lo usa ningún botón. */

    /* PA4-PA7: Analog (sensores IR) */
    GPIO_InitStruct.Pin   = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB0-PB1: Analog */
    GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PB2: BOOT1 (no se usa como GPIO) */
    GPIO_InitStruct.Pin   = GPIO_PIN_2;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PB3, PB4, PB5: emisores IR WALL L1/L2/L3 → salida, apagados.
     * PB3 y PB4 solo funcionan como salida si el JTAG está desactivado
     * (ya se ha hecho al principio de esta función). */
    GPIO_InitStruct.Pin   = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);

    /* PB6-PB7: I2C1 SCL/SDA (pantalla OLED).
     * ⚠️ OJO: el pull-up interno NO se puede activar aquí. El HAL del F1 solo
     * aplica `Pull` en modo entrada, así que en AF_OD se ignora: el bus
     * NECESITA pull-ups externos (2× 4.7 kΩ a 3.3 V). Ver hardware.md §4.9. */
    GPIO_InitStruct.Pin   = I2C1_SCL_PIN | I2C1_SDA_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C1_GPIO_PORT, &GPIO_InitStruct);

    /* PB8: emisor IR WALL L4 → salida, apagado */
    GPIO_InitStruct.Pin   = GPIO_PIN_8;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);

    /* PB9: botón UP (entrada con pull-up). Antes estaba como salida. */
    GPIO_InitStruct.Pin   = BTN_UP_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(BTN_UP_PORT, &GPIO_InitStruct);

    /* PB10/PB11 NO se tocan: son USART3 TX/RX y los configura
     * MX_USART3_UART_Init() en modo alterno. */

    /* PB12: AF PP (SPI/other) */
    GPIO_InitStruct.Pin   = GPIO_PIN_12;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PB13: AF PP (SPI SCK) */
    GPIO_InitStruct.Pin   = GPIO_PIN_13;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PB14: AF Input (SPI MISO) */
    GPIO_InitStruct.Pin   = GPIO_PIN_14;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_INPUT;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PB15: AF PP (SPI MOSI) */
    GPIO_InitStruct.Pin   = GPIO_PIN_15;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = BTN_DOWN_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(BTN_DOWN_PORT, &GPIO_InitStruct);

    gpio_init_boton_menu_irq();
}

static volatile uint8_t menu_btn_irq_pendientes = 0U;

/**
 * @brief Atiende la interrupción del botón MENU.
 *
 * No lee el pin ni cuenta rebotes: solo registra el flanco. Interpretarlo es tarea de la capa APP
 * (`app/buttons.c`).
 */
void GPIO_MenuBtnIRQ_Handler(void)
{
    HAL_GPIO_EXTI_IRQHandler(BTN_MENU_PIN);
}

/**
 * @brief Pulsaciones de MENU contadas por la interrupción.
 *
 * @return Flancos pendientes; al consultarlos los consume
 */
uint8_t GPIO_MenuBtnIRQ_Pendiente(void)
{
    uint8_t pendientes = menu_btn_irq_pendientes;

    menu_btn_irq_pendientes = 0U;
    return pendientes;
}

/**
 * @brief Callback del HAL para las interrupciones de pin.
 *
 * Se sobrescribe aquí (en la librería es una función débil) para no tocar los ficheros generados.
 *
 * @param GPIO_Pin Pin que ha provocado la interrupción
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == BTN_MENU_PIN) {
        if (menu_btn_irq_pendientes < 0xFFU) {
            menu_btn_irq_pendientes++;
        }
    }
}
