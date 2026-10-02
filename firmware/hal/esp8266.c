/**
 * @file esp8266.c
 * @brief Módulo ESP8266 por AT (capa HAL).
 *
 * @see Por qué va en HAL y por qué comparte la USART3 con la telemetría: `hal/esp8266.h`.
 */

#include "hal/esp8266.h"
#include "hal/systemclock.h"
#include "hal/uart.h"
#include "wifi_secrets.h"

#include <stdio.h>
#include <string.h>

/** @brief Respuesta del último comando, para poder enseñarla. */
static char     esp_respuesta[ESP_RESPUESTA_MAX];

/** @brief Caracteres guardados en `esp_respuesta`. */
static uint16_t esp_respuesta_n;

/** @brief Línea que se está montando. */
static char     esp_linea[ESP_LINEA_MAX];

/** @brief Caracteres que lleva la línea en curso. */
static uint16_t esp_linea_n;

/** @brief Callback de líneas completas, o `NULL`. */
static ESP_LineaFn esp_linea_fn;

/** @brief Paso de la secuencia del puente en el que se está, o en el que se quedó. */
static ESP_Paso_t esp_paso = ESP_PASO_NINGUNO;

/**
 * @brief Tiempo máximo que se dedica a tirar lo que haya en el anillo de recepción [ms].
 *
 * ⚠️ **El tope no es un adorno.** Un ESP8266 al que le falta alimentación se queda en **bucle de
 * arranque** y suelta su registro de arranque de forma continua; con un `while (UART_RxGet() >= 0)`
 * sin tope, el bucle **nunca ve el anillo vacío** y el firmware se queda colgado ahí para siempre —con
 * los botones muertos y la pantalla congelada en lo último que se pintó—. Se vacía durante un tiempo
 * acotado y se sigue: lo que quede ya lo tirará el propio comando al leerlo.
 */
#define ESP_DRENA_MS    20U

/**
 * @brief Tira lo que haya en el anillo de recepción, con tope de tiempo.
 *
 * @param ms Tiempo máximo que se le dedica [ms]; si el anillo se vacía antes, se sale antes
 */
static void esp_drena(uint32_t ms)
{
    uint32_t t_inicio = HAL_GetTick();

    while ((HAL_GetTick() - t_inicio) <= ms) {
        if (UART_RxGet() < 0) {
            return;   /* anillo vacío: ya está */
        }
    }
}

/**
 * @brief Añade un carácter a la respuesta guardada, sin pasarse del tope.
 *
 * @param c Carácter a añadir
 */
static void esp_apunta(char c)
{
    if (esp_respuesta_n < (ESP_RESPUESTA_MAX - 1U)) {
        esp_respuesta[esp_respuesta_n] = c;
        esp_respuesta_n++;
        esp_respuesta[esp_respuesta_n] = '\0';
    }
}

/**
 * @brief Dime si una cadena acaba con un texto.
 *
 * @param texto Cadena a mirar
 * @param sufijo Texto buscado al final
 * @return 1 si acaba así
 */
static uint8_t esp_termina_con(const char *texto, const char *sufijo)
{
    size_t n_texto  = strlen(texto);
    size_t n_sufijo = strlen(sufijo);

    if (n_texto < n_sufijo) {
        return 0U;
    }

    return (strcmp(&texto[n_texto - n_sufijo], sufijo) == 0) ? 1U : 0U;
}

/**
 * @brief Cierra la línea en curso y avisa al callback.
 *
 * @note El `OK` y el `ERROR` se buscan **aquí, por línea**, y no en la respuesta entera: `AT+CWLAP`
 *       suelta más de lo que cabe en el buffer, así que mirar el final de la respuesta acumulada
 *       fallaría justo cuando más redes hay.
 */
static void esp_cierra_linea(void)
{
    esp_linea[esp_linea_n] = '\0';

    if (esp_linea_fn != 0) {
        esp_linea_fn(esp_linea);
    }

    esp_linea_n = 0U;
}

/**
 * @brief Deja el módulo listo.
 */
void ESP_Init(void)
{
    esp_respuesta_n  = 0U;
    esp_respuesta[0] = '\0';
    esp_linea_n      = 0U;
    esp_linea[0]     = '\0';
    esp_linea_fn     = 0;

    /* Los bytes que hayan llegado mientras no se escuchaba (basura del arranque del módulo) se tiran:
       si se quedaran, el primer comando leería la respuesta de otro. */
    esp_drena(ESP_DRENA_MS);
}

/**
 * @brief Registra el callback de líneas.
 */
void ESP_SetLineaCallback(ESP_LineaFn fn)
{
    esp_linea_fn = fn;
}

/**
 * @brief Manda un comando AT y espera a que conteste.
 */
ESP_Resultado_t ESP_Comando(const char *comando, uint32_t timeout_ms)
{
    ESP_Resultado_t resultado = ESP_SIN_RESPUESTA;
    uint32_t        t_inicio;
    int             crudo;

    if (comando == 0) {
        return ESP_SIN_RESPUESTA;
    }

    /* Se vacía el anillo **antes** de hablar: un OK que se hubiera quedado de antes daría el comando
       por bueno sin que el módulo hubiera contestado. */
    esp_drena(ESP_DRENA_MS);

    esp_respuesta_n  = 0U;
    esp_respuesta[0] = '\0';
    esp_linea_n      = 0U;

    UART_SendString(comando);
    UART_SendString("\r\n");

    t_inicio = HAL_GetTick();

    while ((resultado == ESP_SIN_RESPUESTA) && ((HAL_GetTick() - t_inicio) < timeout_ms)) {
        /* El tope de tiempo va también en el bucle de dentro: si el módulo suelta bytes **sin parar**
           (bucle de arranque), este bucle no vería nunca el anillo vacío y no se saldría de aquí. */
        while ((resultado == ESP_SIN_RESPUESTA) &&
               ((HAL_GetTick() - t_inicio) < timeout_ms) &&
               ((crudo = UART_RxGet()) >= 0)) {
            char c = (char)crudo;

            esp_apunta(c);

            if (c == '\n') {
                /* El `OK` y el `ERROR` se comprueban **antes** de avisar al callback: el callback va a
                   guardar la línea, y no tiene por qué saber de protocolo. */
                if (esp_termina_con(esp_linea, "OK") != 0U) {
                    resultado = ESP_OK;
                } else if (esp_termina_con(esp_linea, "ERROR") != 0U) {
                    resultado = ESP_ERROR;
                }
            }

            if (c == '\n') {
                esp_cierra_linea();
            } else if (c != '\r') {
                if (esp_linea_n < (ESP_LINEA_MAX - 1U)) {
                    esp_linea[esp_linea_n] = c;
                    esp_linea_n++;

                    /* ⚠️ **El terminador se escribe en CADA byte, no solo al cerrar la línea.** Sin
                       esto la cadena arrastra lo que hubiera de la línea anterior, y la comprobación
                       del `OK` lee basura pegada al final: la respuesta `ATE0` + `OK` quedaba como
                       `"OKE0"` y el `OK` no se veía. Pasó el 2026-10-02, y **solo se notó porque la
                       pantalla enseña la respuesta**: el módulo contestaba `OK` y el robot decía
                       `FALLO EN ATE0`.

                       ⚠️ Y costaba verlo porque con `AT` **sí funcionaba**: las dos líneas miden dos
                       caracteres y el terminador viejo caía justo en su sitio. Un fallo que depende
                       de la longitud de la respuesta anterior es de los que se esconden solos. */
                    esp_linea[esp_linea_n] = '\0';
                }
            }
        }

        if (resultado == ESP_SIN_RESPUESTA) {
            HAL_Delay(1);
        }
    }

    return resultado;
}

/**
 * @brief Respuesta acumulada del último comando.
 */
const char *ESP_GetRespuesta(void)
{
    return esp_respuesta;
}

/**
 * @brief Primera línea de la respuesta que contenga un texto.
 */
const char *ESP_BuscarLinea(const char *clave)
{
    const char *p = esp_respuesta;

    if (clave == 0) {
        return 0;
    }

    while (*p != '\0') {
        const char *fin = p;

        while ((*fin != '\0') && (*fin != '\n')) {
            fin++;
        }

        if (strstr(p, clave) != 0) {
            return p;
        }

        p = (*fin == '\0') ? fin : (fin + 1);
    }

    return 0;
}

/**
 * @brief Comprueba que el módulo contesta y le quita el eco.
 */
ESP_Resultado_t ESP_Preparar(void)
{
    esp_paso = ESP_PASO_AT;

    if (ESP_Comando("AT", ESP_TIMEOUT_CORTO) != ESP_OK) {
        return ESP_SIN_RESPUESTA;
    }

    /* ⚠️ **Quitarle el eco es obligatorio, no cosmético.** El AT del ESP8266 viene con `ATE1` de
       fábrica y devuelve al robot todo lo que recibe: la telemetría volvería por el anillo de
       recepción, la consola la tomaría por órdenes y el robot se realimentaría **solo**, a 115200,
       hasta colgarse. Pasó el 2026-10-02. */
    esp_paso = ESP_PASO_ECO;

    if (ESP_Comando("ATE0", ESP_TIMEOUT_CORTO) != ESP_OK) {
        return ESP_ERROR;
    }

    return ESP_OK;
}

/**
 * @brief Paso en el que está —o en el que se quedó— la secuencia del puente.
 */
ESP_Paso_t ESP_GetPaso(void)
{
    return esp_paso;
}

/**
 * @brief Levanta el puente transparente con el PC.
 */
ESP_Resultado_t ESP_PuenteIniciar(void)
{
    char            comando[96];
    ESP_Resultado_t resultado;

    /* 1 y 2. Que conteste y sin eco. */
    resultado = ESP_Preparar();

    if (resultado != ESP_OK) {
        return resultado;
    }

    esp_paso = ESP_PASO_MODO;

    /* 3. Modo **estación**: sin esto no se puede unir a ninguna red. */
    if (ESP_Comando("AT+CWMODE=1", ESP_TIMEOUT_CORTO) != ESP_OK) {
        return ESP_ERROR;
    }

    esp_paso = ESP_PASO_RED;

    /* 4. A la red. Si ya estaba unido, contesta `OK` igual, así que no hay que distinguir. */
    snprintf(comando, sizeof(comando), "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID, WIFI_CLAVE);

    if (ESP_Comando(comando, ESP_TIMEOUT_JOIN) != ESP_OK) {
        return ESP_ERROR;
    }

    esp_paso = ESP_PASO_MUX;

    /* 5. **Una** conexión. */
    if (ESP_Comando("AT+CIPMUX=0", ESP_TIMEOUT_CORTO) != ESP_OK) {
        return ESP_ERROR;
    }

    esp_paso = ESP_PASO_TRANSPARENTE;

    /* 6. **Modo transparente**: es el ajuste que hace que el módulo deje de hablar AT y se comporte
          como un cable. */
    if (ESP_Comando("AT+CIPMODE=1", ESP_TIMEOUT_CORTO) != ESP_OK) {
        return ESP_ERROR;
    }

    esp_paso = ESP_PASO_SOCKET;

    /* 7. Abrir el socket con el PC. A partir de aquí el módulo ya tiene a dónde mandar lo que le llegue
          por la UART, pero todavía obedece comandos. */
    snprintf(comando, sizeof(comando), "AT+CIPSTART=\"TCP\",\"%s\",%d", WIFI_SERVIDOR,
             (int)WIFI_PUERTO);

    if (ESP_Comando(comando, ESP_TIMEOUT_SOCKET) != ESP_OK) {
        return ESP_ERROR;
    }

    esp_paso = ESP_PASO_ENVIAR;

    /* 8. Entrar en transparente. El módulo contesta `OK` y luego un `>` de invitación, **y a partir de
          el `>` la UART es el socket**. */
    if (ESP_Comando("AT+CIPSEND", ESP_TIMEOUT_CORTO) != ESP_OK) {
        return ESP_ERROR;
    }

    /* Se tira el `>` y lo que haya quedado: si no, se quedaría en la línea que está montando la
       consola y **la primera orden del PC llegaría con un `>` delante**, que la haría desconocida. */
    esp_drena(ESP_DRENA_MS);

    return ESP_OK;
}

/**
 * @brief Cierra el puente y devuelve el módulo al modo de comandos.
 */
void ESP_PuenteParar(void)
{
    HAL_Delay(ESP_ESCAPE_GUARDA_MS);

    UART_SendString("+++");

    HAL_Delay(ESP_ESCAPE_GUARDA_MS);

    esp_drena(ESP_DRENA_MS);
}
