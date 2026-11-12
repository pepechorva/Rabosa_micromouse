#ifndef APP_BUTTONS_H
#define APP_BUTTONS_H

#include <stdint.h>

/*
 * Capa APP — botones de usuario (MENU / UP / DOWN / OK).
 *
 * Cableado: cada botón va entre su pin y GND, con el pull-up interno
 * activado. En reposo se lee 1; al pulsar se lee 0.
 *
 * Uso:
 *   1. BUTTONS_Init() una vez, después de MX_GPIO_Init().
 *   2. BUTTONS_Update() lo más a menudo posible (lo ideal, cada 1 ms).
 *   3. Consultar con BUTTONS_IsPressed() o BUTTONS_WasPressed().
 *
 * Ninguna función de este módulo bloquea: el antirrebote se resuelve midiendo tiempo con
 * HAL_GetTick(), no contando llamadas, para que siga funcionando aunque el bucle se retrase (un
 * `OLED_Update()` son ~50 ms de bus I2C).
 */

typedef enum {
    BTN_MENU = 0,
    BTN_UP   = 1,
    BTN_DOWN = 2,
    BTN_OK   = 3,
    BTN_COUNT
} Button_t;

/* Tiempo de antirrebote en milisegundos. */
#define BUTTONS_DEBOUNCE_MS   30U

/**
 * @brief Asocia cada botón a su puerto/pin del pinout y toma el estado
 *        inicial. Llamar una vez, después de MX_GPIO_Init().
 */
void BUTTONS_Init(void);

/**
 * @brief Aplica el antirrebote y detecta flancos de pulsación.
 *        Llamar lo más a menudo que se pueda (lo ideal, cada 1 ms).
 */
void BUTTONS_Update(void);

/**
 * @brief Estado estable del botón.
 * @return 1 si está pulsado, 0 si no.
 */
uint8_t BUTTONS_IsPressed(Button_t button);

/**
 * @brief Flanco de pulsación. Devuelve 1 una sola vez por pulsación:
 *        al consultarlo se limpia (patrón "consumir evento").
 * @return 1 si se acaba de pulsar, 0 si no.
 */
uint8_t BUTTONS_WasPressed(Button_t button);

/**
 * @brief Máscara con los botones pulsados ahora mismo.
 *        Bit 0 = MENU, bit 1 = UP, bit 2 = DOWN, bit 3 = OK.
 */
uint8_t BUTTONS_GetMask(void);

/**
 * @brief Pide salir de la prueba en curso, como si se hubiera pulsado MENU (orden `SALIR`).
 *
 * `BUTTONS_SalidaSolicitada()` mira el contador de la interrupción de MENU, así que marcarlo es
 * **exactamente** lo que hace el botón. Sirve para salir desde el PC, que es lo que permite guionizar
 * el banco sin tocar el robot.
 *
 * @note La orden solo se lee dentro de las pruebas que llaman a `CONSOLE_Update()` en su bucle
 *       (12, 18 y 20); en las demás hay que esperar a que salgan solas o pulsar MENU.
 */
void BUTTONS_SolicitarSalida(void);

/* ------------------------------------------------------------------
 * Numeración de los botones para telemetría y menú.
 *
 * Cada botón vale una POTENCIA DE DOS, igual que su bit en
 * BUTTONS_GetMask(): evento y máscara hablan el mismo idioma.
 *
 *   1 = MENU (PC14) · 2 = UP (PB9) · 4 = DOWN (PC15) · 8 = OK (PA12)
 *   0 = ningún botón (solo lo devuelve BUTTONS_GetEvent())
 *
 * Se derivan del propio enum con 1 << BTN_x, así que no hay dos listas que
 * mantener en paralelo: si se reordena Button_t, los valores se reordenan con él. */
#define BTN_NUM_NINGUNO   0U
#define BTN_NUM_MENU      (1U << BTN_MENU)    /* 1 */
#define BTN_NUM_UP        (1U << BTN_UP)      /* 2 */
#define BTN_NUM_DOWN      (1U << BTN_DOWN)    /* 4 */
#define BTN_NUM_OK        (1U << BTN_OK)      /* 8 */

/**
 * @brief Evento de pulsación en formato numérico (patrón "consumir evento"):
 *        devuelve el número del primer botón recién pulsado y lo consume.
 *
 * Si en la misma llamada hay dos botones pulsados, se devuelve el de menor
 * bit y el otro queda pendiente para la llamada siguiente (útil para el
 * menú: nunca se pierde una pulsación).
 *
 * @return 1, 2, 4 u 8 según el botón (potencia de dos = su bit en la máscara
 *         de BUTTONS_GetMask()), o 0 si no había ninguna pulsación pendiente.
 */
uint8_t BUTTONS_GetEvent(void);

/**
 * @brief ¿Se ha pulsado MENU? — camino por INTERRUPCIÓN (no por sondeo).
 *
 * MENU (PC14) es el único botón que va por interrupción (EXTI, flanco de
 * bajada, en `hal/gpio.c`). Motivo: el banco de pruebas son rutinas `while(1)`
 * que **no** llaman a BUTTONS_Update(), así que un botón por sondeo no
 * respondería dentro de una prueba. Con la interrupción, la pulsación se cuenta
 * siempre y basta con que la prueba pregunte aquí para terminar.
 *
 * ⚠️ **Esta es la ÚNICA vía de MENU**: `BUTTONS_GetEvent()` no lo devuelve, porque si lo hiciera se
 * comería la bandera antes de que el `while` de una prueba la viera y MENU dejaría de sacar de las
 * pruebas. Quien trate MENU como un botón más —el menú, para volver al nivel de arriba— llama aquí.
 *
 * @return 1 si había una pulsación pendiente (la consume), 0 si no.
 *         Un contacto mecánico puede contar varias veces: solo se acepta una
 *         pulsación cada BUTTONS_SALIDA_DEBOUNCE_MS.
 */
uint8_t BUTTONS_SalidaSolicitada(void);

/**
 * @brief Nombre del botón para pantalla y menú: "MENU", "UP", "DOWN", "OK".
 * @param numero Evento de BUTTONS_GetEvent() (BTN_NUM_*); otro valor ⇒ "".
 */
const char *BUTTONS_GetName(uint8_t numero);

/**
 * @brief Pin del botón para pantalla y menú: "PA12", "PC15", "PB9", "PC14".
 *        Los pines están definidos en include/pinout.h: si algún día se mueve
 *        un botón, hay que cambiar los dos sitios.
 */
const char *BUTTONS_GetPinName(uint8_t numero);

#endif /* APP_BUTTONS_H */
