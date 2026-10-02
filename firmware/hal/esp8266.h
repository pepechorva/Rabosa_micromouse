#ifndef HAL_ESP8266_H
#define HAL_ESP8266_H

#include <stdint.h>

/**
 * @file esp8266.h
 * @brief Módulo ESP8266 gobernado por comandos AT (capa HAL).
 *
 * Va en HAL por el mismo motivo que `hal/drv8833.c`: es **un chip de fuera** al que se le habla por un
 * periférico del MCU (aquí la USART3). El *wall follower* no sabe que existe.
 *
 * ⚠️ **El módulo comparte la USART3 con la telemetría y la consola**, así que las tres cosas no pueden
 * hablar a la vez: mientras se manda un `AT` hay que **dejar de mandar telemetría**, o el ESP recibiría
 * `>WF_CELDAS:12` y lo tomaría por basura, y la consola se comería las respuestas del módulo. Por eso
 * quien use esto debe ser una prueba de banco que **no publique telemetría** y que muestre el
 * resultado en la pantalla.
 *
 * @note La recepción se apoya en el **anillo de `hal/uart.c`**, que ya está lleno por interrupción.
 *       No hace falta nada nuevo: el módulo solo lo vacía.
 * @note El ESP8266 es de 3,3 V y el STM32F103 también, así que van directos. Lo que **sí** hay que
 *       cruzar es TX con RX: es el fallo número uno y hace que no conteste absolutamente nada.
 * @see Procedimiento de la prueba en `Documentacion/pruebas-bringup.md`.
 */

/** @brief Caracteres que se guardan de la respuesta, para poder enseñarlos [caracteres]. */
#define ESP_RESPUESTA_MAX   320U

/** @brief Longitud máxima de una línea de respuesta [caracteres]. */
#define ESP_LINEA_MAX       80U

/** @brief Tiempo de espera de los comandos que contestan en milisegundos [ms]. */
#define ESP_TIMEOUT_CORTO   3000U

/** @brief Tiempo de espera para unirse a la red [ms]. El módulo tarda varios segundos. */
#define ESP_TIMEOUT_JOIN    20000U

/** @brief Tiempo de espera para abrir el socket con el PC [ms]. */
#define ESP_TIMEOUT_SOCKET  10000U

/**
 * @brief Tiempo de guarda del escape `+++` [ms].
 *
 * El escape del modo transparente exige **silencio** antes y después: si se manda pegado a otra cosa,
 * el módulo lo toma por datos y no sale. De ahí los dos silencios.
 */
#define ESP_ESCAPE_GUARDA_MS 600U

/** @brief Cómo acabó un comando. */
typedef enum {
    ESP_SIN_RESPUESTA = 0,   /**< Se agotó el tiempo sin ver `OK` ni `ERROR` */
    ESP_OK,                  /**< El módulo contestó `OK`                     */
    ESP_ERROR                /**< El módulo contestó `ERROR`                  */
} ESP_Resultado_t;

/**
 * @brief En qué paso de la secuencia del puente se está —o en cuál se quedó—.
 *
 * Existe porque «falló» no sirve para nada: hay que saber **cuál de los siete comandos** no cuajó, y
 * cada uno apunta a un sitio distinto (la contraseña, la IP del PC, el firmware AT...).
 */
typedef enum {
    ESP_PASO_NINGUNO = 0,
    ESP_PASO_AT,
    ESP_PASO_ECO,
    ESP_PASO_MODO,
    ESP_PASO_RED,
    ESP_PASO_MUX,
    ESP_PASO_TRANSPARENTE,
    ESP_PASO_SOCKET,
    ESP_PASO_ENVIAR,
    ESP_PASO_N
} ESP_Paso_t;

/**
 * @brief Callback que se llama con cada línea completa de la respuesta.
 *
 * Existe porque `AT+CWLAP` puede soltar **kilobytes** —una línea por red— y no caben en el buffer de
 * respuesta. Con la línea en la mano se puede ir guardando lo que interese sin límite.
 *
 * @param linea Línea recibida, sin el `\r\n` y con terminador
 */
typedef void (*ESP_LineaFn)(const char *linea);

/** @brief Deja el módulo listo: vacía el anillo de recepción y la respuesta guardada. */
void ESP_Init(void);

/**
 * @brief Registra el callback de líneas (o `NULL` para quitarlo).
 *
 * @param fn Función a llamar con cada línea completa
 */
void ESP_SetLineaCallback(ESP_LineaFn fn);

/**
 * @brief Manda un comando AT y espera a que conteste.
 *
 * Añade el `\r\n` al final: **el módulo no ejecuta nada sin él**, igual que la consola del robot.
 *
 * @param comando Comando sin el retorno de carro (`"AT+CWMODE=1"`)
 * @param timeout_ms Tiempo máximo de espera
 * @return Cómo acabó
 */
ESP_Resultado_t ESP_Comando(const char *comando, uint32_t timeout_ms);

/**
 * @brief Respuesta acumulada del último comando.
 *
 * @note Se corta en `ESP_RESPUESTA_MAX`: para respuestas largas (`AT+CWLAP`) lo que vale es el
 *       callback de líneas, no esto.
 * @return Cadena terminada en nulo
 */
const char *ESP_GetRespuesta(void);

/**
 * @brief Primera línea de la respuesta que **contenga** un texto.
 *
 * Es lo que se usa para sacar un dato concreto (`"+CWMODE:"`) sin recorrer la respuesta a mano.
 *
 * @param clave Texto a buscar
 * @return Puntero a la línea dentro de la respuesta, o `NULL` si no está
 */
const char *ESP_BuscarLinea(const char *clave);

/**
 * @brief Comprueba que el módulo contesta y le **quita el eco**.
 *
 * Es lo primero que hay que hacer con un ESP8266, y no por elegancia: **viene de fábrica con el eco
 * activo (`ATE1`)**, así que devuelve al robot todo lo que recibe. Con la telemetría compartiendo la
 * USART3, eso monta un bucle de realimentación —la telemetría vuelve, la consola la toma por órdenes,
 * contesta `CMD_ERR:4`, ese `CMD_ERR:4` vuelve a salir— que se realimenta solo a 115200.
 *
 * @return `ESP_OK` si contestó y se le pudo quitar el eco; `ESP_SIN_RESPUESTA` si no contestó al `AT`
 */
ESP_Resultado_t ESP_Preparar(void);

/**
 * @brief Paso en el que está —o en el que se quedó— la secuencia del puente.
 *
 * @return Último paso intentado
 */
ESP_Paso_t ESP_GetPaso(void);

/**
 * @brief Levanta el **puente transparente** con el PC: WiFi y de ahí en adelante la UART *es* el socket.
 *
 * Hace la secuencia entera —`AT`, modo estación, unirse a la red, conexión única, modo transparente,
 * abrir el socket y entrar— y **a partir de ese momento no hay que cambiar nada más en el firmware**:
 *
 * - lo que se escriba en la USART3 va al PC (`app/telemetry.c` sigue igual), y
 * - lo que el PC mande entra por la USART3 y lo ejecuta la consola (`app/console.c` sigue igual).
 *
 * O sea que la telemetría y las órdenes **dejan de necesitar el cable** sin tocar ninguna capa: el
 * módulo sustituye al adaptador USB-serie.
 *
 * @return `ESP_OK` si el puente quedó levantado; `ESP_ERROR` si algún paso falló; `ESP_SIN_RESPUESTA`
 *         si el módulo no contestó al primer `AT`
 * @note Las credenciales y la IP del PC **no están aquí**: salen de `wifi_secrets.h`, que no se sube al
 *       repositorio.
 * @warning Después de esto el módulo **no entiende comandos AT**: hay que salir con `ESP_PuenteParar()`.
 */
ESP_Resultado_t ESP_PuenteIniciar(void);

/**
 * @brief Cierra el puente y devuelve el módulo al modo de comandos.
 *
 * @note El escape es `+++` **sin retorno de carro** y con silencio antes y después; si se mandara con
 *       el `\r\n` de los comandos, el módulo lo tomaría por datos.
 */
void ESP_PuenteParar(void);

#endif /* HAL_ESP8266_H */
