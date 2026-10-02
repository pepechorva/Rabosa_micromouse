#ifndef APP_WALL_FOLLOWER_H
#define APP_WALL_FOLLOWER_H

#include <stdint.h>

#include "app/motion.h"

/**
 * @file wall_follower.h
 * @brief Algoritmo de la mano (capa APP): recorre el laberinto pegado a una pared.
 *
 * Junta las tres piezas que ya estaban sueltas —`app/walls.c` dice dónde hay pared, `app/motion.c`
 * avanza y gira— pero **no toca el hardware**: no lee sensores ni escribe duty.
 *
 * **Por qué la mano y no un *flood fill***: el laberinto del sábado es de la organización y han
 * quitado las islas, así que es un **árbol**: todos los pasillos se recorren sin dejar nada por
 * explorar, y la regla de la mano lo recorre entero sin conocer el mapa de antemano. Un *flood fill*
 * haría falta para resolverlo, y eso no es el objetivo: el objetivo es **moverse sin chocar**.
 *
 * **La decisión se toma en el centro de la celda**, no al ver el hueco. Los sensores van 12 cm por
 * delante del eje, así que a esa altura ya «ven» el cruce que viene; girar en cuanto se ve el hueco
 * dejaría al robot desalineado justo donde el pasillo nuevo no tiene pared que lo corrija. Se avanza
 * hasta el centro, se mira, se gira **ahí** y se entra. Cuesta medio segundo más por cruce y evita el
 * error que más caro sale en un laberinto ajeno.
 *
 * @warning **El robot se mueve solo**: dale sitio y ten el MENU a mano (sale en el paso siguiente).
 * @see El ajuste de los umbrales de pared, en la prueba de paredes (`Documentacion/pruebas-bringup.md`).
 */

/** @brief Celda del laberinto [mm]. La misma que las primitivas de movimiento. */
#define WF_CELDA_MM            180.0f

/** @brief Tope de celdas antes de pararse sola.
 *
 * En el laberinto del sábado, **16×16**, una mano recorre el árbol entero: hay que subirlo. Una vuelta
 * completa puede ser de varios cientos de celdas, a ~2,5 s por celda.
 */
#define WF_CELDAS_MAX          250U

/** @brief Por qué pared se pega el robot. */
typedef enum {
    WF_MANO_DERECHA = 0,   /**< Mano derecha: el pasillo de la derecha es el preferido */
    WF_MANO_IZQUIERDA,     /**< Mano izquierda                                      */
    WF_MANO_N              /**< Número de manos: no es una mano válida             */
} WF_Mano_t;

/** @brief Qué decidió el *wall follower* en una celda. Es lo que va al cuarto canal de la traza. */
typedef enum {
    WF_DEC_AVANZAR = 1,    /**< Seguir de frente      */
    WF_DEC_GIRAR_IZQ = 2,  /**< Girar a la izquierda  */
    WF_DEC_GIRAR_DER = 3,  /**< Girar a la derecha    */
    WF_DEC_MEDIA_VUELTA = 4 /**< Callejón sin salida  */
} WF_Decision_t;

/**
 * @brief Prepara el *wall follower*.
 *
 * @param m Primitivas de movimiento ya inicializadas; el *wall follower* no las posee, solo las usa
 * @param mano Por qué pared pegarse
 * @note Deja la traza grabando desde cero y el contador de celdas a 0.
 */
void WF_Init(Motion_t *m, WF_Mano_t mano);

/**
 * @brief Cambia la mano sin reiniciar el recorrido.
 *
 * @param mano Mano nueva
 */
void WF_SetMano(WF_Mano_t mano);

/**
 * @brief Mano en uso.
 *
 * @return Mano por la que se está pegando
 */
WF_Mano_t WF_GetMano(void);

/**
 * @brief Última decisión tomada, para la pantalla.
 *
 * @return Decisión de la última celda
 */
WF_Decision_t WF_GetDecision(void);

/**
 * @brief Celdas recorridas desde el arranque.
 *
 * @return Número de celdas
 */
uint16_t WF_GetCeldas(void);

/** @brief Pone el contador de celdas a cero. */
void WF_ReiniciarCeldas(void);

/**
 * @brief Dime si el último paso se quedó a medias.
 *
 * Un paso se queda a medias cuando `MOTION_Avanzar()` agota su tiempo (8 s) sin llegar a la celda: el
 * robot está **empujando contra algo** y no avanza. Es la única protección que hay contra un umbral de
 * pared mal puesto, y no es un adorno: sin ella, el robot sigue «recorriendo» un laberinto que no
 * existe, sin avanzar, hasta que se acaba la batería.
 *
 * @return 1 si el último paso no llegó a su celda
 */
uint8_t WF_GetAtascado(void);

/**
 * @brief Da **un** paso: mira las paredes, decide, gira si toca y entra en la celda.
 *
 * **Bloqueante**: tarda lo que tarden el giro y el avance, del orden de 2 a 3 s.
 *
 * @return 1 si dio el paso, 0 si hay que parar (MENU pulsado, o `WF_CELDAS_MAX` agotado)
 */
uint8_t WF_Paso(void);

#endif /* APP_WALL_FOLLOWER_H */
