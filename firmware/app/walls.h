#ifndef APP_WALLS_H
#define APP_WALLS_H

#include <stdint.h>

/**
 * @file walls.h
 * @brief Detección de paredes (capa APP): convierte las medidas de los receptores IR en «hay pared / no
 *        hay pared», que es lo que consumen el *wall follower* y el *flood fill*.
 *
 * El módulo responde a tres preguntas —frente, derecha, izquierda— con un **umbral por pared**, porque
 * cada receptor tiene su sensibilidad y su ángulo. La cuarta, la de atrás, no se pregunta: no hay
 * sensor detrás y el laberinto se recorre siempre hacia delante.
 *
 * **La medida que se usa no es la lectura del receptor, sino `encendido − apagado`.** La luz ambiente
 * cae en los dos términos y desaparece en la resta, así que el umbral **no depende de la iluminación
 * de la sala**. Es la misma idea que valida la prueba 16.
 *
 * @note **Los umbrales se ajustan sin cable**, con los botones, en la prueba de banco: el laberinto del
 *       sábado es ajeno y sus paredes pueden no reflejar como las de casa, y allí no se puede calibrar
 *       con el portátil enchufado.
 * @warning Los valores **no se guardan en Flash**: al reiniciar vuelven a los de arranque.
 * @see Procedimiento de ajuste en `Documentacion/pruebas-bringup.md`.
 */

/** @brief Las tres paredes que se pueden preguntar. */
typedef enum {
    WALL_FRENTE = 0,   /**< Delante: cualquiera de los dos receptores frontales */
    WALL_DERECHA,      /**< Lateral derecho (R1)                                */
    WALL_IZQUIERDA,    /**< Lateral izquierdo (R2)                              */
    WALL_N             /**< Número de paredes: no es una pared válida           */
} Wall_t;

/** @brief Umbrales de arranque, **uno por pared**, en cuentas de `encendido − apagado` [0..4095].
 *
 * ⚠️ **Uno por pared, y no uno común.** Cada receptor tiene su sensibilidad, su ángulo y su
 * **acoplamiento directo** con su propio emisor, así que el `DIF` de reposo de cada canal es distinto.
 * Con un umbral único bajo, las tres paredes leían «hay» siempre, y el *wall follower* habría dado
 * media vuelta en cada celda sin avanzar nunca.
 *
 * ✅ **Medido en el laberinto el 2026-10-02** (prueba 21, robot en el pasillo):
 *
 * | Canal | Con pared delante | Sin pared (fondo) | Umbral |
 * |:------|:-----------------:|:-----------------:|:------:|
 * | **Frente** (R0) | **149** | **0** | **75** |
 * | Derecha (R1) | — | 105 (medido con los lados al aire) | 2050 |
 * | Izquierda (R2) | — | −3 (medido con los lados al aire) | 900 |
 *
 * El frontal queda **a mitad de camino** de los dos valores, con un margen de 2× por cada lado, y la
 * histéresis de 60 sigue cabiendo (enchufa por encima de 105, apaga por debajo de 45).
 *
 * ⚠️ **Los laterales no ven paredes laterales**: apuntan hacia delante. Los números de la tabla son de
 * cuando **no había nada a los lados**, y el `−3` es ruido puro (la medida es una resta). Se dejan
 * altos para que digan «no hay» siempre, y da igual: el *wall follower* **no los usa**, decide solo con
 * el frontal. Si algún día se reorientan los receptores, esto hay que medirlo otra vez.
 *
 * El orden de los `#define` es el de `Wall_t`: frente, derecha, izquierda.
 */
#define WALLS_UMBRAL_FRENTE_DEF      75
#define WALLS_UMBRAL_DERECHA_DEF   2050
#define WALLS_UMBRAL_IZQUIERDA_DEF  900

/** @brief Margen entre el umbral de «hay» y el de «no hay», contra el parpadeo.
 *
 * Un umbral único hace que una medida que oscila alrededor del valor de corte encienda y apague la
 * pared en pasos consecutivos, y una FSM que decide con eso da tirones. Con la **histéresis** la pared
 * sigue el estado anterior hasta que la medida se aleja del umbral.
 */
#define WALLS_HISTERESIS       60

/**
 * @brief Deja los umbrales en su valor de arranque.
 */
void WALLS_Init(void);

/**
 * @brief Mide los receptores y decide si hay pared en cada lado.
 *
 * Se llama desde el lazo de quien la use (el *wall follower* la llama en cada paso). Cuesta tres pares
 * ON/OFF de ADC, del orden de 150 µs: se puede llamar a 50 Hz sin enterarse.
 */
void WALLS_Actualizar(void);

/**
 * @brief Dime si hay pared a ese lado.
 *
 * @param cual Pared a consultar
 * @return 1 si hay pared, 0 si no
 */
uint8_t WALLS_Hay(Wall_t cual);

/**
 * @brief Última medida de esa pared, en cuentas de `encendido − apagado`.
 *
 * Es lo que se enseña al ajustar el umbral: sin el número no hay forma de saber dónde ponerlo.
 *
 * @param cual Pared a consultar
 * @return Medida de la última actualización
 */
int16_t WALLS_GetMedida(Wall_t cual);

/**
 * @brief Cambia el umbral de una pared.
 *
 * @param cual Pared a ajustar
 * @param umbral Umbral nuevo, en cuentas
 * @return 1 si se aceptó
 */
uint8_t WALLS_SetUmbral(Wall_t cual, int16_t umbral);

/**
 * @brief Umbral en uso de una pared.
 *
 * @param cual Pared a consultar
 * @return Umbral en cuentas
 */
int16_t WALLS_GetUmbral(Wall_t cual);

#endif /* APP_WALLS_H */
