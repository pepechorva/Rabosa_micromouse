#include "app/buttons.h"
#include "hal/gpio.h"
#include "peripherals.h"
#include "pinout.h"

/* ==================================================================
 * Estado interno por botón
 * ================================================================== */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       raw;          /* última lectura cruda */
    uint8_t       stable;       /* estado ya filtrado (0/1) */
    uint32_t      t_cambio;     /* instante del último cambio de la lectura cruda */
    uint8_t       pressed_edge; /* 1 = pulsación pendiente de consumir */
} button_t;

static button_t buttons[BTN_COUNT];

/** @brief Ventana de antirrebote de la vía de interrupción del botón MENU. */
#define BUTTONS_SALIDA_DEBOUNCE_MS   250U

/**
 * @brief Lee el pin de un botón.
 *
 * @param b Botón a leer
 * @return 1 si está pulsado
 */
static uint8_t read_raw(const button_t *b)
{
    return (HAL_GPIO_ReadPin(b->port, b->pin) == BTN_PRESSED_STATE) ? 1U : 0U;
}

/**
 * @brief Asocia cada botón a su puerto y pin, y toma el estado inicial.
 *
 * Arranca con el estado real del pin para no generar un flanco falso si alguien mantiene un botón
 * pulsado al encender. Llamar una vez, después de MX_GPIO_Init().
 */
void BUTTONS_Init(void)
{
    buttons[BTN_MENU].port = BTN_MENU_PORT;
    buttons[BTN_MENU].pin  = BTN_MENU_PIN;

    buttons[BTN_UP].port   = BTN_UP_PORT;
    buttons[BTN_UP].pin    = BTN_UP_PIN;

    buttons[BTN_DOWN].port = BTN_DOWN_PORT;
    buttons[BTN_DOWN].pin  = BTN_DOWN_PIN;

    buttons[BTN_OK].port   = BTN_OK_PORT;
    buttons[BTN_OK].pin    = BTN_OK_PIN;

    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        buttons[i].raw          = read_raw(&buttons[i]);
        buttons[i].stable       = buttons[i].raw;
        buttons[i].t_cambio     = HAL_GetTick();
        buttons[i].pressed_edge = 0;
    }
}

/**
 * @brief Aplica el antirrebote y detecta los flancos de pulsación.
 *
 * El antirrebote se mide en MILISEGUNDOS, no en número de llamadas: así funciona aunque el bucle
 * principal vaya más lento de lo previsto (un `OLED_Update()` son ~50 ms de bus I2C y, mientras se
 * dibuja, nadie llama aquí). Llamar lo más a menudo que se pueda (lo ideal, cada 1 ms).
 * El botón MENU se excluye aquí: lo cuenta su interrupción.
 */
void BUTTONS_Update(void)
{
    uint32_t ahora = HAL_GetTick();

    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        button_t *b = &buttons[i];
        uint8_t   now = read_raw(b);

        if (now != b->raw) {
            b->raw     = now;
            b->t_cambio = ahora;
            continue;
        }

        if ((now != b->stable) && ((ahora - b->t_cambio) >= BUTTONS_DEBOUNCE_MS)) {
            b->stable = now;

            if (now && (i != BTN_MENU)) {
                b->pressed_edge = 1;
            }
        }
    }
}

/**
 * @brief Estado estable de un botón.
 *
 * @param button Botón a consultar
 * @return 1 si está pulsado
 */
uint8_t BUTTONS_IsPressed(Button_t button)
{
    if (button >= BTN_COUNT) {
        return 0;
    }
    return buttons[button].stable;
}

/**
 * @brief Consume el flanco de pulsación de un botón, si lo había.
 *
 * @param button Botón a consultar
 * @return 1 una sola vez por pulsación
 */
uint8_t BUTTONS_WasPressed(Button_t button)
{
    if (button >= BTN_COUNT) {
        return 0;
    }

    uint8_t edge = buttons[button].pressed_edge;
    buttons[button].pressed_edge = 0;
    return edge;
}

/**
 * @brief Máscara de los botones pulsados en este momento.
 *
 * @return Bits 0..3: MENU, UP, DOWN y OK
 */
uint8_t BUTTONS_GetMask(void)
{
    uint8_t mask = 0;

    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        if (buttons[i].stable) {
            mask |= (uint8_t)(1U << i);
        }
    }
    return mask;
}

/**
 * @brief Devuelve la primera pulsación pendiente **por sondeo** y la consume.
 *
 * Si hay dos botones pulsados a la vez se sirve el de bit más bajo y el otro queda para la llamada
 * siguiente, de modo que una pulsación nunca se pierde.
 *
 * ⚠️ **MENU NO sale por aquí, y es a propósito.** MENU va por interrupción y su bandera la consume
 * `BUTTONS_SalidaSolicitada()`, que es lo que espera el `while` de las pruebas. Si `GetEvent()`
 * consultara esa bandera —como hizo hasta el 2026-10-11— se la **comía antes** de que el bucle la
 * viera, porque los bucles de prueba llaman a `GetEvent()` en el cuerpo y al `while` después. Medido:
 * `SALIR` por consola no sacaba de la prueba 20 y el MENU físico salía solo algunas veces, según el
 * orden de las llamadas. Quien necesite MENU como botón tiene `BUTTONS_SalidaSolicitada()`.
 *
 * @return Potencia de dos del botón (2 UP · 4 DOWN · 8 OK), o 0 si no había ninguna
 */
uint8_t BUTTONS_GetEvent(void)
{
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        if (buttons[i].pressed_edge) {
            buttons[i].pressed_edge = 0;
            return (uint8_t)(1U << i);
        }
    }

    return BTN_NUM_NINGUNO;
}

/**
 * @brief ¿Se ha pulsado MENU? Vía por interrupción, no por sondeo.
 *
 * El banco de pruebas son rutinas que no llaman a BUTTONS_Update(), así que un botón por sondeo no
 * respondería dentro de una prueba; contándolo por interrupción, basta con que la prueba pregunte
 * aquí para poder salir. Un mismo contacto mecánico puede provocar varios flancos, así que solo se
 * acepta una pulsación por ventana.
 *
 * @return 1 si había una pulsación pendiente (la consume)
 *
 * @warning **La bandera se LATCHA, y no latcharla era el fallo hasta el 2026-10-11.**
 *          `GPIO_MenuBtnIRQ_Pendiente()` **borra el contador al leerlo**, así que la versión anterior
 *          perdía pulsaciones por dos caminos: (a) si el antirrebote rechazaba la consulta, la
 *          pulsación ya se había borrado y se perdía para siempre; y (b) cualquier otra consulta de la
 *          misma vuelta del bucle se llevaba el contador antes de que el `while` de la prueba
 *          preguntara. Con el latch, el flanco **espera** a que lo recoja quien pregunta.
 */
uint8_t BUTTONS_SalidaSolicitada(void)
{
    static uint8_t  pendiente = 0U;
    static uint32_t t_ultima  = 0U;
    uint32_t        ahora;

    /* Lo que haya contado la interrupción se guarda: leerlo borra el contador del HAL, y esta función
       puede ser la segunda en preguntar en la misma vuelta. */
    if (GPIO_MenuBtnIRQ_Pendiente() != 0U) {
        pendiente = 1U;
    }

    if (pendiente == 0U) {
        return 0U;
    }

    ahora = HAL_GetTick();

    if ((t_ultima != 0U) && ((ahora - t_ultima) < BUTTONS_SALIDA_DEBOUNCE_MS)) {
        pendiente = 0U;   /* es el rebote del contacto que ya se dio por bueno */
        return 0U;
    }

    pendiente = 0U;
    t_ultima  = ahora;
    return 1U;
}

/**
 * @brief Pide salir de la prueba en curso, como si se hubiera pulsado MENU (orden `SALIR`).
 *
 * Se marca **la misma bandera que marca la interrupción del botón**, así que el camino es idéntico:
 * `BUTTONS_SalidaSolicitada()` no distingue de dónde vino.
 *
 * @warning **Hasta el 2026-10-11 esto no hacía nada.** Llamaba a `GPIO_MenuBtnIRQ_Handler()`, que
 *          despacha por `HAL_GPIO_EXTI_IRQHandler()` — y esa **solo llama al callback si el flag de
 *          EXTI está puesto**, cosa que en un `SALIR` por consola no ocurre porque no hay flanco. El
 *          contador no se incrementaba: la orden quedaba en el acuse `>CMD_SALIR:1` y la prueba
 *          seguía corriendo. Ver `GPIO_MenuBtnIRQ_Solicitar()`.
 */
void BUTTONS_SolicitarSalida(void)
{
    GPIO_MenuBtnIRQ_Solicitar();
}

/**
 * @brief Nombre del botón, para pantalla y menú.
 *
 * @param numero Evento de BUTTONS_GetEvent()
 * @return "MENU", "UP", "DOWN", "OK", o "" si el número no es válido
 */
const char *BUTTONS_GetName(uint8_t numero)
{
    switch (numero) {
        case BTN_NUM_MENU: return "MENU";
        case BTN_NUM_UP:   return "UP";
        case BTN_NUM_DOWN: return "DOWN";
        case BTN_NUM_OK:   return "OK";
        default:           return "";
    }
}

/**
 * @brief Pin del botón, para pantalla y menú.
 *
 * @param numero Evento de BUTTONS_GetEvent()
 * @return "PA12", "PC15", "PB9", "PC14", o "" si el número no es válido
 */
const char *BUTTONS_GetPinName(uint8_t numero)
{
    switch (numero) {
        case BTN_NUM_MENU: return "PC14";   /* BTN_MENU_PIN */
        case BTN_NUM_UP:   return "PB9";    /* BTN_UP_PIN   */
        case BTN_NUM_DOWN: return "PC15";   /* BTN_DOWN_PIN */
        case BTN_NUM_OK:   return "PA12";   /* BTN_OK_PIN   */
        default:           return "";
    }
}
