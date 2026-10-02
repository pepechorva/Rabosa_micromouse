#ifndef APP_TRACE_H
#define APP_TRACE_H

#include <stdint.h>

/**
 * @file trace.h
 * @brief Traza en RAM para ver la respuesta de un lazo sin tener Teleplot enchufado (capa APP).
 *
 * Guarda unas cuantas muestras de tres valores —consigna, rueda izquierda y rueda derecha— en un
 * **anillo en RAM** y las vuelca después por telemetría. Existe por un problema muy concreto: para que
 * la sintonía valga, el robot tiene que **moverse en el suelo**, y ahí el cable de la UART estorba. Sin
 * esta traza, la oscilación del lazo hay que verla con el robot sobre la mesa, que no es lo mismo.
 *
 * @note La telemetría y las órdenes de la consola van por **el mismo USART3**, así que las dos cosas
 *       las atiende el mismo programa del PC; el OLED no tiene nada que ver (va por I²C).
 *
 * **Por qué RAM y no Flash**: el STM32F103 no tiene EEPROM. Emularla en Flash obliga a borrar páginas
 * de 1 KB (y la Flash del programa está al 80 %), desgasta la memoria y añade gestión de sectores. Con
 * 18 KB de RAM libres, un anillo de 3 KB cabe de sobra y se escribe sin consecuencias.
 *
 * @note La descarga se hace **a velocidad real** (`TRACE_PERIODO_MS` por muestra), no a toda la
 *       velocidad del puerto: así el eje de tiempo de Teleplot es el de verdad y el periodo de la
 *       oscilación se lee directamente en segundos, sin escalar nada.
 * @warning El anillo es **circular**: si se llena, lo que se descarga son las últimas muestras, que es
 *          justo lo que interesa cuando se acaba de tocar una ganancia.
 */

/** @brief Muestras que caben en el anillo. */
#define TRACE_MUESTRAS_MAX   512U

/** @brief Periodo con el que se graba una muestra y con el que se descarga [ms]. */
#define TRACE_PERIODO_MS     10U

/** @brief Los valores que guarda cada muestra. */
typedef enum {
    TRACE_SET = 0,   /**< Consigna que se estaba pidiendo                        */
    TRACE_IZQ,       /**< Velocidad medida de la rueda izquierda [mm/s]          */
    TRACE_DER,       /**< Velocidad medida de la rueda derecha [mm/s]            */
    TRACE_EXTRA,     /**< Cuarto canal: lo que necesite el módulo que traza      */
    TRACE_CANAL_N    /**< Número de canales: no es un canal válido               */
} Trace_Canal_t;

/** @brief Deja la traza vacía. */
void TRACE_Init(void);

/** @brief Vacía la traza y empieza a guardar de nuevo. */
void TRACE_Reiniciar(void);

/**
 * @brief Guarda una muestra.
 *
 * Se llama desde el lazo, no por tiempo: el ritmo lo pone quien llama (en la prueba 18, cada 10 ms).
 *
 * @param set Consigna que se estaba pidiendo [mm/s]
 * @param izq Velocidad medida de la rueda izquierda [mm/s]
 * @param der Velocidad medida de la rueda derecha [mm/s]
 */
void TRACE_Guarda(int16_t set, int16_t izq, int16_t der);

/**
 * @brief Guarda una muestra usando también el cuarto canal.
 *
 * Existe porque el *wall follower* necesita trazar otras cosas —las tres paredes y la decisión— y va
 * **sin cable**, así que la traza en RAM es su única forma de depuración. En vez de inventar un
 * segundo anillo se reaprovecha el mismo: son 1 KB de RAM y una etiqueta más en la descarga.
 *
 * @param set Valor del canal `TRACE_SET`
 * @param izq Valor del canal `TRACE_IZQ`
 * @param der Valor del canal `TRACE_DER`
 * @param extra Valor del canal `TRACE_EXTRA`
 */
void TRACE_GuardaExtra(int16_t set, int16_t izq, int16_t der, int16_t extra);

/**
 * @brief Cuántas muestras hay guardadas.
 *
 * @return Número de muestras (hasta `TRACE_MUESTRAS_MAX`)
 */
uint16_t TRACE_GetMuestras(void);

/**
 * @brief Lee una muestra del anillo.
 *
 * @param canal Canal a leer
 * @param indice Posición contada desde la muestra **más antigua** (0 = la primera)
 * @return Valor guardado, o 0 si el índice no existe
 */
int16_t TRACE_Get(Trace_Canal_t canal, uint16_t indice);

/**
 * @brief Vuelca la traza por telemetría, a velocidad real.
 *
 * Manda `>TR_SET`, `>TR_IZQ`, `>TR_DER` y `>TR_EXTRA` intercalados, una muestra cada
 * `TRACE_PERIODO_MS`, y cierra con `>TR_FIN:1`. **Bloquea** durante `TRACE_MUESTRAS_MAX ×
 * TRACE_PERIODO_MS` como mucho: con el anillo lleno, unos 5 segundos.
 */
void TRACE_Descargar(void);

#endif /* APP_TRACE_H */
