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
 *
 * @note Desde el 2026-10-02 el robot decide **girando y midiendo el frente**, así que la primera
 *       columna (`WALLS_GetMedida(WALL_FRENTE)`) es la medida **en la dirección que acabó eligiendo**,
 *       no la del frente por el que venía. Las de los laterales se guardan también, pero **están
 *       medido que no ven pared**: sirven de adorno, no de dato.
 */
static void wf_trazar(void)
{
    TRACE_GuardaExtra(WALLS_GetMedida(WALL_FRENTE),
                      WALLS_GetMedida(WALL_IZQUIERDA),
                      WALLS_GetMedida(WALL_DERECHA),
                      (int16_t)wf_decision);
}

/**
 * @brief Mide el frente con la orientación actual.
 *
 * @return 1 si hay pared delante, 0 si el paso está libre
 */
static uint8_t wf_hay_pared(void)
{
    WALLS_Actualizar();

    return WALLS_Hay(WALL_FRENTE);
}

/**
 * @brief Gira 90° hacia el lado de la mano (o hacia el contrario, para deshacer).
 *
 * @param hacia_mano 1 para girar hacia el lado de la mano, 0 para el lado contrario
 * @return 0 si se pidió salir (MENU) antes de girar
 */
static uint8_t wf_giro_90(uint8_t hacia_mano)
{
    float grados = (((hacia_mano != 0U) == (wf_mano == WF_MANO_DERECHA)) ? -WF_GIRO_GRADOS
                                                                        : WF_GIRO_GRADOS);

    if (BUTTONS_SalidaSolicitada() != 0U) {
        return 0U;
    }

    return MOTION_Girar(wf_m, grados);
}

/**
 * @brief Busca una dirección con el paso libre y **deja al robot mirando hacia ella**.
 *
 * ⚠️ **Aquí está el cambio del 2026-10-02.** Antes se miraban las tres paredes de una vez
 * (`WALLS_Hay(WALL_FRENTE/IZQUIERDA/DERECHA)`) y se aplicaba la regla clásica de la mano. Eso exige
 * que los **receptores laterales** vean las paredes de los lados, y está medido que **no las ven**:
 * con una pared a 10 cm de su lado y sin ella leen lo mismo (`R2`: 546 contra 544), porque lo que
 * miden es acoplamiento con su propio emisor. Consecuencia: la regla clásica decidía con dos datos
 * falsos.
 *
 * Solo hay un sensor de frente utilizable, así que la única forma de mirar a un lado es **girar y
 * volver a medir** — no existe manera de «ver» la derecha sin ponerse de cara a ella.
 *
 * El orden de preferencia es **frente → mano → atrás**, y **el frente se mira SIN girar**: si está
 * libre, se avanza y no se gasta ni un giro.
 *
 * @warning **Esto es una decisión de banco, no de gusto.** Medido el 2026-10-02 en el pasillo de casa
 *          (casillas de 180 mm): el robot **no cabe girando sobre su eje** — chasis de 106×84 mm barre un
 *          círculo de ~68 mm de radio, más lo que sobresalen los sensores, y el pasillo deja 90 mm por
 *          lado. Gira rozando, y entonces **la odometría cuenta vueltas de rueda que el robot no da**:
 *          tras un par *gira-derecha + deshace*, el rumbo quedó a **44,5°** (y a 73,7° en el segundo
 *          intento) y el robot acabó empujando contra la pared con `>WF_ATASCO:1`.
 *
 *          ⇒ **Cuantos menos giros, mejor.** Con el frente primero, un pasillo recto se recorre con
 *          **cero giros**; con la mano primero costaba **dos por casilla** y el rumbo se iba.
 *
 * @warning **El precio**: en un cruce en el que el frente esté libre, el robot **pasa de largo** y no
 *          entra en el ramal lateral (en el pasillo de casa, la rama de la casilla 3 no se visita).
 *          Es el precio de no gastar giros, y se acepta a cambio de que el robot **vaya recto de
 *          verdad**.
 *
 * @warning **Y no se mide el otro lado** (decisión del usuario, 2026-10-02). Comprobar la izquierda
 *          costaba **tres giros más** en cada celda sin salida —girar, deshacer, girar al otro lado,
 *          medir y volver—, y en el laberinto **cada giro roza las paredes**: el chasis barre un
 *          círculo de ~147 mm en una celda de 180, así que gira sin margen y la odometría cuenta
 *          vueltas de rueda que el robot no da. Con el orden **frente → mano → atrás**, una celda sin
 *          salida se resuelve con **dos giros** en vez de cinco. El precio es que un cruce en «T»
 *          hacia la izquierda se pasa de largo: se cambia explorar más por **atascarse menos**, que
 *          es lo que hace falta para moverse sin chocar.
 * @note «Atrás» no se mide: es por donde se acaba de venir, así que está libre por construcción.
 * @return 1 si eligió dirección (en `wf_decision`), 0 si se pidió salir
 */
static uint8_t wf_buscar_direccion(void)
{
    const WF_Decision_t gira_mano = (wf_mano == WF_MANO_DERECHA) ? WF_DEC_GIRAR_DER
                                                                 : WF_DEC_GIRAR_IZQ;

    if (wf_hay_pared() == 0U) {      /* el frente, sin gastar un solo giro */
        wf_decision = WF_DEC_AVANZAR;
        return 1U;
    }

    if (wf_giro_90(1U) == 0U) {      /* mira al lado de la mano */
        return 0U;
    }

    if (wf_hay_pared() == 0U) {
        wf_decision = gira_mano;
        return 1U;
    }

    /* Otros 90° **en el mismo sentido** dejan al robot mirando atrás, por donde vino: eso está libre
       por construcción y no hay nada que medir. */
    if (wf_giro_90(1U) == 0U) {
        return 0U;
    }

    wf_decision = WF_DEC_MEDIA_VUELTA;

    return 1U;
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

    if (wf_buscar_direccion() == 0U) {
        return 0U;
    }

    wf_trazar();

    /* Si el avance agota su tiempo sin llegar, el robot está **empujando contra algo**: se para aquí.
       Seguir daría un recorrido entero de mentira, y en un laberinto ajeno eso se paga con el robot
       arrastrándose por las paredes hasta que se acabe la batería.
       ⚠️ `MOTION_Avanzar()` devuelve 0 **también cuando se pulsa MENU**, que es la forma normal de
       parar el robot ⇒ hay que mirar **quién** cortó el paso antes de llamarlo atasco, o la pantalla
       diría `PARADO: ATASCO` (y `>WF_ATASCO:1`) cada vez que el usuario lo para a mano. */
    if (MOTION_Avanzar(wf_m, WF_CELDA_MM) == 0U) {
        wf_atascado = (BUTTONS_SalidaSolicitada() != 0U) ? 0U : 1U;
        return 0U;
    }

    wf_celdas++;

    return 1U;
}
