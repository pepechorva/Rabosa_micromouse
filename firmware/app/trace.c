/**
 * @file trace.c
 * @brief Traza en RAM (capa APP): un anillo de muestras y su descarga por telemetría.
 *
 * @see Por qué RAM y no Flash, y por qué la descarga va a velocidad real: `app/trace.h`.
 */

#include "app/trace.h"
#include "app/telemetry.h"
#include "hal/systemclock.h"

/**
 * @brief El anillo, un array por canal.
 *
 * Se guarda por canales (y no por muestras) para que cada canal quede contiguo en memoria: así la
 * lectura de una serie entera no salta de un sitio a otro.
 */
static int16_t  traza[TRACE_CANAL_N][TRACE_MUESTRAS_MAX];

/** @brief Muestras guardadas (crece hasta `TRACE_MUESTRAS_MAX`). */
static uint16_t traza_n;

/** @brief Dónde va la siguiente muestra: cuando el anillo se llena, es también la más antigua. */
static uint16_t traza_pos;

/**
 * @brief Posición en el array de la muestra `indice`, contada desde la más antigua.
 *
 * Mientras el anillo no se ha llenado, la más antigua está en la posición 0. Una vez lleno, la más
 * antigua es la que se va a sobreescribir, o sea `traza_pos`.
 *
 * @param indice Posición contada desde la muestra más antigua
 * @return Posición en el array de ese canal
 */
static uint16_t trace_slot(uint16_t indice)
{
    uint16_t base = (traza_n < TRACE_MUESTRAS_MAX) ? 0U : traza_pos;

    return (uint16_t)((base + indice) % TRACE_MUESTRAS_MAX);
}

/**
 * @brief Deja la traza vacía.
 */
void TRACE_Init(void)
{
    traza_n   = 0U;
    traza_pos = 0U;
}

/**
 * @brief Vacía la traza y empieza a guardar de nuevo.
 */
void TRACE_Reiniciar(void)
{
    TRACE_Init();
}

/**
 * @brief Guarda una muestra en el anillo.
 */
void TRACE_Guarda(int16_t set, int16_t izq, int16_t der)
{
    TRACE_GuardaExtra(set, izq, der, 0);
}

/**
 * @brief Guarda una muestra en el anillo, con el cuarto canal.
 */
void TRACE_GuardaExtra(int16_t set, int16_t izq, int16_t der, int16_t extra)
{
    traza[TRACE_SET][traza_pos]   = set;
    traza[TRACE_IZQ][traza_pos]   = izq;
    traza[TRACE_DER][traza_pos]   = der;
    traza[TRACE_EXTRA][traza_pos] = extra;

    traza_pos = (uint16_t)((traza_pos + 1U) % TRACE_MUESTRAS_MAX);

    if (traza_n < TRACE_MUESTRAS_MAX) {
        traza_n++;
    }
}

/**
 * @brief Cuántas muestras hay guardadas.
 */
uint16_t TRACE_GetMuestras(void)
{
    return traza_n;
}

/**
 * @brief Lee una muestra del anillo.
 */
int16_t TRACE_Get(Trace_Canal_t canal, uint16_t indice)
{
    if (((uint8_t)canal >= (uint8_t)TRACE_CANAL_N) || (indice >= traza_n)) {
        return 0;
    }

    return traza[canal][trace_slot(indice)];
}

/**
 * @brief Vuelca la traza por telemetría, a velocidad real.
 */
void TRACE_Descargar(void)
{
    for (uint16_t i = 0U; i < traza_n; i++) {
        TELEM_SendInt("TR_SET", (int32_t)TRACE_Get(TRACE_SET, i));
        TELEM_SendInt("TR_IZQ", (int32_t)TRACE_Get(TRACE_IZQ, i));
        TELEM_SendInt("TR_DER", (int32_t)TRACE_Get(TRACE_DER, i));
        TELEM_SendInt("TR_EXTRA", (int32_t)TRACE_Get(TRACE_EXTRA, i));

        /* El retardo es el que fija el eje de tiempo: sin él, la traza se dibujaría comprimida y el
           periodo de la oscilación no se podría leer en segundos. */
        HAL_Delay(TRACE_PERIODO_MS);
    }

    TELEM_SendInt("TR_FIN", 1);
}
