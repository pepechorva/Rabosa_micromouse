#ifndef FILTER_ODOMETRY_H
#define FILTER_ODOMETRY_H

/**
 * @file odometry.h
 * @brief Odometría diferencial (capa FILTER): de distancias de rueda a pose $(x, y, \theta)$.
 *
 * El módulo es **matemática pura**: no lee encoders, no mira el reloj y no sabe nada del laberinto.
 * Recibe los **incrementos de arco** de cada rueda (en metros) y mueve la pose. Quien la llama (la
 * capa APP) es quien lee `sensor/encoder.c` y le pasa las diferencias; eso permite compilarla en el
 * PC y validarla con `firmware/test/odometry_sim.c`, igual que se hizo con el PID.
 *
 * @note Se integran **arcos, no velocidades**: así no hace falta `dt` y el resultado no depende del
 *       *jitter* del bucle. Con las dos ruedas hay solución exacta (centro de rotación instantáneo),
 *       que es más precisa que integrar $v\cdot dt$.
 * @warning Los dos encoders deben contar **positivo al avanzar**. Si uno va invertido, empujando el
 *          robot en línea recta `>ODO_DL` y `>ODO_DR` salen con signos distintos y $\theta$ se dispara
 *          (prueba 19). Se arregla intercambiando los hilos A/B de ese encoder.
 */

#include <stdint.h>

/**
 * @brief Trocha efectiva por defecto: distancia entre los centros de contacto de las ruedas, en mm.
 *
 * @note **Medida el 2026-10-01** con la prueba 19, girando el robot 90° a mano sobre su centro: con
 *       los 83,6 mm (distancia entre ejes de motor) los dos sentidos daban 83° y −84°, un 7 % menos
 *       que los 90° reales ⇒ $L = 83{,}6 \times 83{,}5/90 = 77{,}6$ mm. Sale **menor que la
 *       geométrica**, que es lo normal en un robot que arrastra las ruedas al pivotar.
 * @note Es el único parámetro geométrico que entra en las cuentas: el radio de rueda ya va dentro de
 *       los µm por cuenta calibrados en la prueba 7.
 */
#define ODOM_TROCHA_MM_DEF      77.6f

/** @brief Pose estimada del robot y camino recorrido. */
typedef struct {
    float x;         /**< Posición X [m]                                   */
    float y;         /**< Posición Y [m]                                   */
    float theta;     /**< Orientación [rad], normalizada a $[-\pi, \pi]$    */
    float distance;  /**< Camino recorrido acumulado [m], siempre crece    */
} Odometry_t;

/**
 * @brief Deja la pose en un punto conocido y el camino a cero.
 *
 * @param odo Puntero a la odometría (se modifica in-situ)
 * @param x0_m Posición X inicial [m]
 * @param y0_m Posición Y inicial [m]
 * @param theta0_rad Orientación inicial [rad] (0 = mirando al frente del robot en el arranque)
 */
void ODOM_Init(Odometry_t *odo, float x0_m, float y0_m, float theta0_rad);

/**
 * @brief Pone la pose en el origen (0, 0, 0) y el camino a cero, sin mover el robot.
 *
 * @param odo Puntero a la odometría
 */
void ODOM_Reset(Odometry_t *odo);

/**
 * @brief Avanza la pose con los incrementos de arco de las dos ruedas.
 *
 * Aplica la cinemática directa de tracción diferencial y la integración exacta del arco:
 * $ds = (ds_d + ds_i)/2$, $d\theta = (ds_d - ds_i)/L$ y, si $d\theta \approx 0$, un tramo recto.
 *
 * @param odo Puntero a la odometría
 * @param ds_left_m Incremento de la rueda izquierda desde la llamada anterior [m]
 * @param ds_right_m Incremento de la rueda derecha desde la llamada anterior [m]
 *
 * @note Convenio: $\theta$ positivo es giro **a la izquierda** (antihorario visto desde arriba) y los
 *       dos incrementos son positivos al avanzar. Llamar con los dos incrementos a cero es inocuo.
 * @note Si la trocha configurada no es válida, la función no toca la pose (mejor una pose parada que
 *       una llena de NaN).
 */
void ODOM_Update(Odometry_t *odo, float ds_left_m, float ds_right_m);

/**
 * @brief Fija la trocha efectiva medida en el robot.
 *
 * @param trocha_mm Trocha efectiva [mm]; si es menor de 1 mm se ignora y se conserva la anterior
 */
void ODOM_SetTrocha(float trocha_mm);

/**
 * @brief Trocha efectiva que está usando la odometría [mm].
 *
 * @return Trocha en mm
 */
float ODOM_GetTrocha(void);

#endif /* FILTER_ODOMETRY_H */
