/**
 * @file wall_follower.c
 * @brief Algoritmo de la mano (capa APP): decisión por celda y traza para correr sin cable.
 *
 * @see Por qué la mano y no un *flood fill*, y por qué se decide en el centro de la celda:
 *      `app/wall_follower.h`.
 */

#include "app/wall_follower.h"
#include "app/buttons.h"
#include "app/trace.h"
#include "app/walls.h"

/**
 * @brief Grados de cada giro [°].
 *
 * El signo positivo es a la izquierda, como θ en la odometría.
 */
#define WF_GIRO_GRADOS         90.0f

/** @brief Puntero a las primitivas de movimiento. Son de quien llama, no se copian. */
static Motion_t *wf_m;

/** @brief Mano por la que se está pegado. */
static WF_Mano_t wf_mano = WF_MANO_DERECHA;

/** @brief Decisión tomada en la última celda. */
static WF_Decision_t wf_decision = WF_DEC_AVANZAR;

/** @brief Celdas recorridas desde `WF_ReiniciarCeldas()`. */
static uint16_t wf_celdas = 0U;

/** @brief 1 si el último paso agotó su tiempo sin llegar (el robot está atascado). */
static uint8_t wf_atascado = 0U;

/**
 * @brief Guarda en la traza lo que vio y lo que decidió en esta celda.
 *
 * Se guarda **una muestra por celda**, no por milisegundo: lo que hay que poder reconstruir es la
 * decisión, y con 512 muestras el anillo da para más de 500 cruces. Es la única forma de depurar un
 * recorrido que se hace **sin cable**, donde no hay Teleplot ni consola.
 */
static void wf_trazar(void)
{
    TRACE_GuardaExtra(WALLS_GetMedida(WALL_FRENTE),
                      WALLS_GetMedida(WALL_IZQUIERDA),
                      WALLS_GetMedida(WALL_DERECHA),
                      (int16_t)wf_decision);
}

/**
 * @brief Decide qué hacer con las tres paredes que se ven desde el centro de la celda.
 *
 * Es la **regla de la mano**: se va siempre pegado a la pared de ese lado.
 *
 * 1. Si el lado preferido está **abierto**, se gira hacia él. Se mira **antes** que nada porque es lo
 *    que garantiza no dejar pasillos sin recorrer: si se mirara el frente primero, el robot seguiría
 *    de largo y volvería a pasar por el mismo cruce.
 * 2. Si no, si el frente está abierto, se sigue.
 * 3. Si no, se gira al otro lado.
 * 4. Y si tampoco, es un **callejón sin salida**: media vuelta.
 *
 * @return Decisión tomada
 * @note Las tres consultas se hacen de una vez al principio a propósito: `WALLS_Hay()` devuelve el
 *       estado de la última actualización, y leerlas por el camino invitaría a creer que cada llamada
 *       mide. No mide: mide `WALLS_Actualizar()`.
 */
static WF_Decision_t wf_decidir(void)
{
    const uint8_t frente    = WALLS_Hay(WALL_FRENTE);
    const uint8_t derecha   = WALLS_Hay(WALL_DERECHA);
    const uint8_t izquierda = WALLS_Hay(WALL_IZQUIERDA);
    const uint8_t hay_mano  = (wf_mano == WF_MANO_DERECHA) ? derecha : izquierda;
    const uint8_t hay_otra  = (wf_mano == WF_MANO_DERECHA) ? izquierda : derecha;
    const uint8_t gira_mano = (wf_mano == WF_MANO_DERECHA) ? WF_DEC_GIRAR_DER : WF_DEC_GIRAR_IZQ;
    const uint8_t gira_otra = (wf_mano == WF_MANO_DERECHA) ? WF_DEC_GIRAR_IZQ : WF_DEC_GIRAR_DER;

    if (hay_mano == 0U) {
        return (WF_Decision_t)gira_mano;
    }

    if (frente == 0U) {
        return WF_DEC_AVANZAR;
    }

    if (hay_otra == 0U) {
        return (WF_Decision_t)gira_otra;
    }

    return WF_DEC_MEDIA_VUELTA;
}

/**
 * @brief Gira lo que pida la decisión.
 *
 * @param decision Decisión tomada
 * @note La media vuelta va **hacia el lado de la mano**, para que el robot no tenga que cruzar su
 *       propio punto muerto y para que el giro sea siempre el mismo en el mismo sitio.
 */
static void wf_girar(WF_Decision_t decision)
{
    switch (decision) {
    case WF_DEC_GIRAR_IZQ:
        MOTION_Girar(wf_m, WF_GIRO_GRADOS);
        break;
    case WF_DEC_GIRAR_DER:
        MOTION_Girar(wf_m, -WF_GIRO_GRADOS);
        break;
    case WF_DEC_MEDIA_VUELTA:
        MOTION_Girar(wf_m, (wf_mano == WF_MANO_DERECHA) ? -2.0f * WF_GIRO_GRADOS
                                                        : 2.0f * WF_GIRO_GRADOS);
        break;    default:
        break;
    }
}

/**
 * @brief Deja el *wall follower* listo para recorrer.
 */
void WF_Init(Motion_t *m, WF_Mano_t mano)
{
    wf_m        = m;
    wf_mano     = mano;
    wf_decision = WF_DEC_AVANZAR;

    WF_ReiniciarCeldas();
    TRACE_Reiniciar();
}

/**
 * @brief Cambia la mano sin reiniciar el recorrido.
 */
void WF_SetMano(WF_Mano_t mano)
{
    if (mano < WF_MANO_N) {
        wf_mano = mano;
    }
}

/**
 * @brief Mano en uso.
 */
WF_Mano_t WF_GetMano(void)
{
    return wf_mano;
}

/**
 * @brief Última decisión tomada, para la pantalla.
 */
WF_Decision_t WF_GetDecision(void)
{
    return wf_decision;
}

/**
 * @brief Celdas recorridas desde el arranque.
 */
uint16_t WF_GetCeldas(void)
{
    return wf_celdas;
}

/**
 * @brief Pone el contador de celdas a cero.
 */
void WF_ReiniciarCeldas(void)
{
    wf_celdas    = 0U;
    wf_atascado  = 0U;
}

/**
 * @brief Dime si el último paso se quedó a medias.
 */
uint8_t WF_GetAtascado(void)
{
    return wf_atascado;
}

/**
 * @brief Da un paso: mira las paredes, decide, gira si toca y entra en la celda.
 */
uint8_t WF_Paso(void)
{
    if (wf_m == 0) {
        return 0U;
    }

    if ((BUTTONS_SalidaSolicitada() != 0U) || (wf_celdas >= WF_CELDAS_MAX)) {
        return 0U;
    }

    WALLS_Actualizar();
    wf_decision = wf_decidir();
    wf_trazar();

    wf_girar(wf_decision);

    /* Si el avance agota su tiempo sin llegar, el robot está **empujando contra algo**: se para aquí.
       Seguir daría un recorrido entero de mentira, y en un laberinto ajeno eso se paga con el robot
       arrastrándose por las paredes hasta que se acabe la batería. */
    if (MOTION_Avanzar(wf_m, WF_CELDA_MM) == 0U) {
        wf_atascado = 1U;
        return 0U;
    }

    wf_celdas++;

    return 1U;
}
