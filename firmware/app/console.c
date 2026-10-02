#include "app/console.h"
#include "app/application.h"
#include "app/telemetry.h"
#include "app/trace.h"
#include "hal/uart.h"
#include "sensor/battery.h"
#include "pinout.h"

#include <stdio.h>
#include <string.h>

/* Longitud máxima de una orden. Con 4 campos de sobra ("BAT AVISO_CELDA 3450" son 20 caracteres) y
   un poco de margen para el retorno de carro de los terminales que lo mandan aparte. */
#define CONSOLE_LINEA_MAX   40U

/**
 * @brief Bytes que se leen del anillo como mucho en cada llamada a `CONSOLE_Update()`.
 *
 * ⚠️ **El tope no es una optimización, es lo que impide un cuelgue.** El anillo de recepción lo
 * comparte la consola con cualquier cosa que se conecte a la USART3 —un módulo ESP8266, por
 * ejemplo—, y si algo suelta bytes **de forma continua** un `while (UART_RxGet() >= 0)` sin tope no
 * sale nunca de aquí. Y como esto se llama desde el bucle del menú, el síntoma que produce es «los
 * botones no hacen nada», que apunta a cualquier sitio menos a la consola.
 *
 * Pasó el 2026-10-01 con un ESP8266 que reiniciaba en bucle por falta de alimentación. Lo que no se
 * lea en una llamada se lee en la siguiente: el anillo guarda 64 B y esto se llama a 1 kHz.
 */
#define CONSOLE_BYTES_MAX   32U

static char    linea[CONSOLE_LINEA_MAX];
static uint8_t n_linea;

/* ------------------------------------------------------------------ */
static char *console_token(char **cursor)
{
    char *inicio = *cursor;
    char *fin;

    while ((*inicio == ' ') || (*inicio == '\t')) {
        inicio++;
    }

    if (*inicio == '\0') {
        *cursor = inicio;
        return NULL;
    }

    fin = inicio;
    while ((*fin != '\0') && (*fin != ' ') && (*fin != '\t')) {
        fin++;
    }

    if (*fin != '\0') {
        *fin = '\0';
        fin++;
    }

    *cursor = fin;

    return inicio;
}

/* ------------------------------------------------------------------ */
static void console_mayusculas(char *texto)
{
    for (; *texto != '\0'; texto++) {
        if ((*texto >= 'a') && (*texto <= 'z')) {
            *texto = (char)(*texto - 'a' + 'A');
        }
    }
}

/**
 * @brief Pasa el texto de una orden a milivoltios.
 *
 * @param texto Lo que escribió el usuario
 * @param mv Milivoltios resultantes
 * @return 1 si el texto es un número válido, 0 si no
 *
 * @note Se aceptan las dos formas cómodas de escribir una tensión: **con punto, en voltios**
 *       (`7.4` ⇒ 7400 mV) y **sin punto, en milivoltios** (`7400` ⇒ 7400 mV). Así no hay que teclear
 *       ceros de más ni recordar cuál es cuál: el punto ya dice que son voltios.
 */
static uint8_t console_texto_a_mv(const char *texto, uint16_t *mv)
{
    uint32_t entero     = 0U;
    uint32_t decimales  = 0U;
    uint32_t escala     = 100U;   /* 3 decimales: décimas, centésimas, milésimas */
    uint8_t  hay_punto  = 0U;
    uint8_t  n_digitos  = 0U;

    for (const char *p = texto; *p != '\0'; p++) {
        if (*p == '.') {
            if (hay_punto) {
                return 0U;
            }
            hay_punto = 1U;
            continue;
        }

        if ((*p < '0') || (*p > '9')) {
            return 0U;
        }

        n_digitos++;

        if (hay_punto) {
            if (escala == 0U) {
                return 0U;   /* más de tres decimales: no aporta nada a un umbral */
            }
            decimales += (uint32_t)(*p - '0') * escala;
            escala /= 10U;
        } else {
            entero = (entero * 10U) + (uint32_t)(*p - '0');
            if (entero > 99999U) {
                return 0U;
            }
        }
    }

    if (n_digitos == 0U) {
        return 0U;
    }

    *mv = (uint16_t)(hay_punto ? ((entero * 1000U) + decimales) : entero);

    return (*mv <= (uint16_t)(BAT_V_UMBRAL_MAX * 1000.0f)) ? 1U : 0U;
}

/**
 * @brief Pasa el texto de una orden a número decimal.
 *
 * Acepta un signo opcional, dígitos y hasta tres decimales (`0.35`, `12`, `0.0`). No valen las reglas
 * de `console_texto_a_mv()`: esto es para las ganancias del lazo, que no son tensiones y se escriben
 * siempre con punto.
 *
 * @param texto Lo que escribió el usuario
 * @param valor Número resultante
 * @return 1 si el texto es un número válido, 0 si no
 */
static uint8_t console_texto_a_float(const char *texto, float *valor)
{
    uint32_t entero    = 0U;
    uint32_t decimales = 0U;
    uint32_t escala    = 100U;   /* 3 decimales: décimas, centésimas, milésimas */
    uint8_t  hay_punto = 0U;
    uint8_t  n_digitos = 0U;
    uint8_t  negativo  = 0U;

    if ((*texto == '-') || (*texto == '+')) {
        negativo = (uint8_t)(*texto == '-');
        texto++;
    }

    for (; *texto != '\0'; texto++) {
        if (*texto == '.') {
            if (hay_punto) {
                return 0U;
            }
            hay_punto = 1U;
            continue;
        }

        if ((*texto < '0') || (*texto > '9')) {
            return 0U;
        }

        n_digitos++;

        if (hay_punto) {
            if (escala == 0U) {
                return 0U;   /* más de tres decimales: no aporta nada a una ganancia */
            }
            decimales += (uint32_t)(*texto - '0') * escala;
            escala /= 10U;
        } else {
            entero = (entero * 10U) + (uint32_t)(*texto - '0');
            if (entero > 99999U) {
                return 0U;
            }
        }
    }

    if (n_digitos == 0U) {
        return 0U;
    }

    *valor = (float)entero + ((float)decimales / 1000.0f);

    if (negativo != 0U) {
        *valor = -*valor;
    }

    return 1U;
}

/* ------------------------------------------------------------------ */
/**
 * @brief Publica el estado de la batería: umbrales en uso, factor del divisor y tensión medida.
 */
static void console_publicar_bateria(void)
{
    char etiqueta[24];

    for (uint8_t i = 0U; i < (uint8_t)BAT_UMBRAL_N; i++) {
        snprintf(etiqueta, sizeof(etiqueta), "BAT_%s", BATTERY_UMBRAL_NOMBRES[i]);
        TELEM_SendFloat(etiqueta, BATTERY_GetUmbral((Battery_Umbral_t)i));
    }

    TELEM_SendFloat("BAT_K", BATTERY_GetKDivisor());
    TELEM_SendFloat("VBAT", BATTERY_GetPackV());
}

/* ------------------------------------------------------------------ */
static Battery_Umbral_t console_buscar_umbral(const char *nombre)
{
    for (uint8_t i = 0U; i < (uint8_t)BAT_UMBRAL_N; i++) {
        if (strcmp(nombre, BATTERY_UMBRAL_NOMBRES[i]) == 0) {
            return (Battery_Umbral_t)i;
        }
    }

    return BAT_UMBRAL_N;
}

/* ------------------------------------------------------------------ */
static void console_comando_bat(char *resto)
{
    char             *token = console_token(&resto);
    Battery_Umbral_t  cual;
    uint16_t          mv;

    if (token == NULL) {
        console_publicar_bateria();
        return;
    }

    console_mayusculas(token);

    if (strcmp(token, "LIST") == 0) {
        console_publicar_bateria();
        return;
    }

    /* Calibrado: se le dice la tensión REAL del pack y el módulo recalcula el factor del divisor */
    if (strcmp(token, "CAL") == 0) {
        token = console_token(&resto);

        if ((token == NULL) || (console_texto_a_mv(token, &mv) == 0U)) {
            TELEM_SendInt("CMD_ERR", 2);   /* 2 = valor no válido */
            return;
        }

        if (BATTERY_CalibrarConPack((float)mv / 1000.0f) == 0U) {
            TELEM_SendInt("CMD_ERR", 5);   /* 5 = no se pudo calibrar */
            return;
        }

        console_publicar_bateria();
        return;
    }

    cual = console_buscar_umbral(token);
    if (cual >= BAT_UMBRAL_N) {
        TELEM_SendInt("CMD_ERR", 1);   /* 1 = umbral desconocido */
        return;
    }

    token = console_token(&resto);
    if ((token == NULL) || (console_texto_a_mv(token, &mv) == 0U)) {
        TELEM_SendInt("CMD_ERR", 2);   /* 2 = valor no válido */
        return;
    }

    if (BATTERY_SetUmbral(cual, (float)mv / 1000.0f) == 0U) {
        TELEM_SendInt("CMD_ERR", 3);   /* 3 = rechazado (rango o incoherente) */
        return;
    }

    console_publicar_bateria();
}

/* ------------------------------------------------------------------ */
/**
 * @brief Publica la configuración del lazo de velocidad (prueba 18).
 *
 * Las mismas etiquetas que manda la prueba al arrancar, para que el registro de Teleplot tenga la
 * ganancia con la que se hizo cada tramo de la sintonía.
 */
static void console_publicar_pid(void)
{
    TELEM_SendFloat("PID_KP", APP_PID_GetGanancia(APP_PID_GANANCIA_KP));
    TELEM_SendFloat("PID_KI", APP_PID_GetGanancia(APP_PID_GANANCIA_KI));
    TELEM_SendFloat("PID_KD", APP_PID_GetGanancia(APP_PID_GANANCIA_KD));
    TELEM_SendFloat("PID_VEL", APP_PID_GetVelocidad());
}

/**
 * @brief Orden `PID`: sintoniza el lazo de velocidad sin recompilar.
 *
 * Formas: `PID` o `PID LIST` (publica), `PID KP|KI|KD <valor>` y `PID VEL <mm/s>`. El valor se
 * comprueba en la APP, que es la dueña de los ajustes; aquí solo se avisa del rechazo.
 *
 * @param resto Lo que queda de la orden tras la palabra `PID`
 */
static void console_comando_pid(char *resto)
{
    char             *token    = console_token(&resto);
    char             *valor_tx = NULL;
    float             valor    = 0.0f;
    App_PidGanancia_t cual     = APP_PID_GANANCIA_N;
    uint8_t           es_vel   = 0U;

    if (token == NULL) {
        console_publicar_pid();
        return;
    }

    console_mayusculas(token);

    if (strcmp(token, "LIST") == 0) {
        console_publicar_pid();
        return;
    }

    if (strcmp(token, "KP") == 0) {
        cual = APP_PID_GANANCIA_KP;
    } else if (strcmp(token, "KI") == 0) {
        cual = APP_PID_GANANCIA_KI;
    } else if (strcmp(token, "KD") == 0) {
        cual = APP_PID_GANANCIA_KD;
    } else if (strcmp(token, "VEL") == 0) {
        es_vel = 1U;
    } else {
        TELEM_SendInt("CMD_ERR", 1);   /* 1 = parámetro desconocido */
        return;
    }

    valor_tx = console_token(&resto);

    if ((valor_tx == NULL) || (console_texto_a_float(valor_tx, &valor) == 0U)) {
        TELEM_SendInt("CMD_ERR", 2);   /* 2 = valor no válido */
        return;
    }

    if (((es_vel != 0U) && (APP_PID_SetVelocidad(valor) == 0U)) ||
        ((es_vel == 0U) && (APP_PID_SetGanancia(cual, valor) == 0U))) {
        TELEM_SendInt("CMD_ERR", 3);   /* 3 = rechazado (fuera de rango) */
        return;
    }

    console_publicar_pid();
}

/* ------------------------------------------------------------------ */
/** @brief Estado del LED de la placa, para la orden `LED`. */
static uint8_t console_led_encendido;

/**
 * @brief Publica el estado del LED de la placa.
 */
static void console_publicar_led(void)
{
    TELEM_SendInt("LED", (int32_t)console_led_encendido);
}

/**
 * @brief Orden `LED`: enciende, apaga o conmuta el LED de la placa (PC13).
 *
 * Es la **orden de prueba de vida de la consola**: no toca nada del robot, se ve a simple vista y no
 * necesita Teleplot. Si `LED 1` no lo enciende, lo que falla es la **recepción** (el cable, el puerto,
 * o que el programa del PC no manda el salto de línea al final), no la orden que se estaba probando.
 *
 * @param resto Lo que queda de la orden tras la palabra `LED`
 */
static void console_comando_led(char *resto)
{
    char  *token = console_token(&resto);
    float  valor;

    if (token == NULL) {
        console_publicar_led();
        return;
    }

    console_mayusculas(token);

    if (strcmp(token, "TOGGLE") == 0) {
        console_led_encendido = (uint8_t)(!console_led_encendido);
    } else if (console_texto_a_float(token, &valor) != 0U) {
        console_led_encendido = (valor > 0.5f) ? 1U : 0U;
    } else {
        TELEM_SendInt("CMD_ERR", 2);   /* 2 = valor no válido */
        return;
    }

    /* El LED de la placa es **activo a bajo**: RESET enciende y SET apaga. */
    HAL_GPIO_WritePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN,
                      (console_led_encendido != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    console_publicar_led();
}

/* ------------------------------------------------------------------ */
/**
 * @brief Publica la petición de movimiento aceptada.
 *
 * Se publica la acción y las repeticiones **aceptadas**, y no lo que se pidió: así el registro de
 * Teleplot dice exactamente lo que va a hacer el robot.
 *
 * @param cual Acción aceptada
 * @param repeticiones Cuántas veces se va a repetir
 */
static void console_publicar_mov(App_Movimiento_t cual, uint8_t repeticiones)
{
    TELEM_SendInt("MOV_CMD", (int32_t)cual);
    TELEM_SendInt("MOV_REP", (int32_t)repeticiones);
}

/**
 * @brief Orden `MOV`: manda un movimiento a la prueba 20 (primitivas de `app/motion.c`).
 *
 * Las acciones son las de los botones de la prueba —`OK` avanza una celda, `UP` gira 90° a la
 * izquierda y `DOWN` 90° a la derecha— y además `BACK`, que **retrocede** una celda. Detrás se puede
 * poner un contador para encadenarlas: `MOV OK 4` son cuatro celdas seguidas.
 *
 * @note `BACK` **no tiene botón**: la prueba no tiene un cuarto botón libre, así que el retroceso solo
 *       se puede pedir por consola. Existe para poder hacer ida y vuelta —`MOV OK 2` y `MOV BACK 2`
 *       devuelven el robot al sitio— sin que se vaya cruzando la mesa y acabando contra el monitor.
 *
 * @note El contador cuenta **movimientos independientes**, no uno más largo: `MOV OK 4` llama cuatro
 *       veces a `MOTION_Avanzar(180)`, que es lo que mide si el error se acumula. `MOTION_Avanzar(720)`
 *       sería otra prueba distinta.
 * @note La orden **solo vale dentro de la prueba 20**, que es la única que recoge la petición. En
 *       cualquier otro sitio se rechaza con el código 3, porque aceptarla y no hacer nada haría
 *       creer que se ha perdido.
 * @param resto Lo que queda de la orden tras la palabra `MOV`
 */
static void console_comando_mov(char *resto)
{
    char             *token = console_token(&resto);
    char             *texto_rep;
    App_Movimiento_t  cual;
    float             valor;
    uint8_t           repeticiones = 1U;

    if (token == NULL) {
        TELEM_SendInt("CMD_ERR", 1);   /* 1 = parámetro desconocido */
        return;
    }

    console_mayusculas(token);

    if (strcmp(token, "OK") == 0) {
        cual = APP_MOV_AVANZAR;
    } else if (strcmp(token, "BACK") == 0) {
        cual = APP_MOV_RETROCEDER;
    } else if (strcmp(token, "UP") == 0) {
        cual = APP_MOV_GIRAR_IZQ;
    } else if (strcmp(token, "DOWN") == 0) {
        cual = APP_MOV_GIRAR_DER;
    } else {
        TELEM_SendInt("CMD_ERR", 1);   /* 1 = parámetro desconocido */
        return;
    }

    texto_rep = console_token(&resto);

    if (texto_rep != NULL) {
        if ((console_texto_a_float(texto_rep, &valor) == 0U) || !(valor >= 1.0f) ||
            (valor > (float)APP_MOV_REP_MAX)) {
            TELEM_SendInt("CMD_ERR", 2);   /* 2 = valor no válido */
            return;
        }

        repeticiones = (uint8_t)valor;
    }

    if (APP_MOV_Pedir(cual, repeticiones) == 0U) {
        TELEM_SendInt("CMD_ERR", 3);   /* 3 = rechazado: la prueba 20 no está en marcha */
        return;
    }

    console_publicar_mov(cual, repeticiones);
}

/* ------------------------------------------------------------------ */
static void console_ejecutar(char *orden)
{
    char *token = console_token(&orden);

    if (token == NULL) {
        return;
    }

    console_mayusculas(token);

    if (strcmp(token, "BAT") == 0) {
        console_comando_bat(orden);
        return;
    }

    if (strcmp(token, "PID") == 0) {
        console_comando_pid(orden);
        return;
    }

    if (strcmp(token, "MOV") == 0) {
        console_comando_mov(orden);
        return;
    }

    /* La traza se pide cuando ya hay alguien escuchando en el otro extremo del puerto: de nada sirve
       volcarla mientras el robot está en el suelo y sin cable. */
    if (strcmp(token, "TRACE") == 0) {
        TRACE_Descargar();
        return;
    }

    if (strcmp(token, "LED") == 0) {
        console_comando_led(orden);
        return;
    }

    if ((strcmp(token, "HELP") == 0) || (strcmp(token, "?") == 0)) {
        console_publicar_bateria();
        console_publicar_pid();
        console_publicar_led();
        return;
    }

    TELEM_SendInt("CMD_ERR", 4);   /* 4 = orden desconocida */
}

/* ------------------------------------------------------------------ */
void CONSOLE_Init(void)
{
    n_linea = 0U;
    linea[0] = '\0';

    UART_RxStart();
}

/* ------------------------------------------------------------------ */
void CONSOLE_Update(void)
{
    for (uint16_t i = 0U; i < CONSOLE_BYTES_MAX; i++) {
        int byte = UART_RxGet();

        if (byte < 0) {
            return;   /* anillo vacío: ya está */
        }

        if ((byte == '\n') || (byte == '\r')) {
            if (n_linea > 0U) {
                linea[n_linea] = '\0';

                /* Se ignora lo que empiece por `>`, que es **telemetría** y no una orden. No es una
                   precaución de estilo, es lo que impide un **bucle de realimentación**: el ESP8266
                   viene de fábrica con el **eco** activo (`ATE1`), así que devuelve al robot todo lo
                   que recibe. Sin esto, la propia telemetría vuelve por el anillo de recepción, la
                   consola la toma por órdenes, contesta `CMD_ERR:4`, ese `CMD_ERR:4` sale otra vez...
                   y se realimenta solo **a 115200 durante todo el rato** (pasado el 2026-10-02). */
                if (linea[0] != '>') {
                    console_ejecutar(linea);
                }

                n_linea = 0U;
            }
            continue;
        }

        if (n_linea < (CONSOLE_LINEA_MAX - 1U)) {
            linea[n_linea] = (char)byte;
            n_linea++;
        } else {
            n_linea = 0U;   /* línea demasiado larga: se descarta y se espera a la siguiente */
        }
    }
}
