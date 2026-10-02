#ifndef APP_MOTION_H
#define APP_MOTION_H

#include <stdint.h>

#include "control/controller.h"
#include "filter/odometry.h"
#include "hal/drv8833.h"

/**
 * @file motion.h
 * @brief Primitivas de movimiento (capa APP): **avanzar una distancia** y **girar un ángulo**.
 *
 * Junta las tres piezas que ya estaban sueltas: la **velocidad** de cada rueda la cierra
 * `control/controller.c` (CONTROL), la **pose** la lleva `filter/odometry.c` (FILTER) y aquí se ata
 * todo —leer los encoders, mover la pose, pedir el duty al driver—, que es justo lo que solo la APP
 * puede hacer: es la única capa autorizada a saltar hacia abajo durante el bring-up.
 *
 * Las dos primitivas son la base de todo lo que viene: la celda del laberinto (`avanzar_celda()`), los
 * giros de 90°, el *wall follower* y el *flood fill*. Por eso lo que se pide de ellas no es velocidad,
 * sino **repetibilidad**: que 180 mm sean 180 mm y 90° sean 90°, hoy y con la batería a la mitad.
 *
 * @note Las dos son **bloqueantes** (no vuelven hasta terminar o agotar el tiempo), como el resto del
 *       banco de pruebas, y salen de inmediato si se pulsa MENU.
 * @note La distancia se mide **con la odometría**, no con el tiempo: por eso se apoya en los µm por
 *       cuenta y en la trocha ya calibrados (pruebas 7 y 19).
 * @see Procedimiento y criterios en `Documentacion/pruebas-bringup.md`.
 */

/** @brief Velocidad de crucero de las primitivas [mm/s]. */
#define MOTION_VEL_MM_S          250.0f

/** @brief Pendiente de la rampa de consigna [mm/s²], para no arrancar de golpe. */
#define MOTION_RAMPA_MM_S2       800.0f

/** @brief Tope de duty que puede pedir el PID durante los movimientos [%]. */
#define MOTION_DUTY_MAX          70.0f

/** @brief Tolerancia de la distancia recorrida [mm]. */
#define MOTION_TOL_MM            3.0f

/**
 * @brief Distancia que el robot recorre **después** de darse por llegado, y que hay que descontar [mm].
 *
 * Estuvo en **19,6 mm** mientras el asentamiento dejaba **rodar** al robot: el integrador del PID
 * seguía empujando mientras se descargaba y la odometría lo contaba. Con el asentamiento frenando de
 * verdad (`motion_asentar` resetea el PID y hace freno dinamico) el recorrido sobrante se queda en unos
 * pocos milímetros y **el descuento ya no hace falta**.
 *
 * @note Se deja el parámetro a **0** en vez de borrarlo: es donde poner la corrección si alguna vez
 *       vuelve a hacer falta, y deja constancia de que un día hizo falta.
 * @warning Descontar una frenada fija **no funciona**: medida en cuatro celdas seguidas salió de 2, 20
 *          y 44 mm, porque depende de cuánto le quedara al integrador. Si vuelve a aparecer el
 *          problema, la solución es quitar el rodaje, no ajustar este número.
 */
#define MOTION_FRENADA_MM        0.0f

/** @brief Tolerancia del ángulo girado [°]. */
#define MOTION_TOL_GRADOS        1.5f

/** @brief Velocidad mínima de giro [mm/s]: por debajo de esto el motor no vence la fricción estática. */
#define MOTION_GIRO_VEL_MIN_MM_S 40.0f

/** @brief Ganancia del lazo de ángulo [mm/s por grado de error]. */
#define MOTION_GIRO_KP           4.0f

/**
 * @brief Ganancia del corrector de **traslación** durante el giro [mm/s por mm desplazado].
 *
 * Un giro sobre el centro pide que los dos arcos sean **iguales y opuestos**, y en la práctica no lo
 * son: un motor vence el rozamiento antes que el otro. Medido el 2026-10-01 (prueba 20), cada giro de
 * 90° desplazaba el robot **~18 mm** — 72 mm en un cuadrado, sobre celdas de 180 mm.
 *
 * Con 4 el residuo medido era de **~15-19 mm** por giro de 90°, y subirlo a 8 lo baja aproximadamente
 * a la mitad: el corrector tarda 1/8 = 0,125 s, todavía muy por encima de las decenas de milisegundos
 * que tardan los lazos de velocidad que hay dentro.
 *
 * @note El desplazamiento que corrige **no es constante**: sale sobre todo del **transitorio** de
 *       arranque y frenada, donde una rueda responde antes que la otra. Por eso un corrector
 *       proporcional a lo *ya* desplazado reacciona tarde, y subir la ganancia es lo que más se nota.
 */
#define MOTION_GIRO_CENTRO_KP    8.0f

/** @brief Ganancia del corrector de rumbo al avanzar [mm/s por grado de desvío]. */
#define MOTION_RUMBO_KP          3.0f

/** @brief Tiempo máximo de un movimiento [ms]; al agotarse se para y se avisa. */
#define MOTION_TIMEOUT_MS        8000U

/**
 * @brief Estado de las primitivas: los dos lazos de velocidad y la pose que van construyendo.
 *
 * La odometría vive **aquí dentro** y no se reinicia entre movimientos: así el rumbo y el ángulo son
 * continuos dentro de una maniobra encadenada (`avanzar`, `girar`, `avanzar`…).
 */
typedef struct {
    Controller_t     izq;           /**< Lazo de la rueda izquierda     */
    Controller_t     der;           /**< Lazo de la rueda derecha       */
    Odometry_t       odo;           /**< Pose acumulada desde MOTION_Init() */
    DRV8833_Motor_t *motor_izq;     /**< Driver de la rueda izquierda   */
    DRV8833_Motor_t *motor_der;     /**< Driver de la rueda derecha     */
    int32_t          um_izq_previo; /**< µm de la rueda izquierda en la última lectura */
    int32_t          um_der_previo; /**< µm de la rueda derecha en la última lectura    */
} Motion_t;

/**
 * @brief Prepara las primitivas: arranca los dos lazos y pone la pose en el origen.
 *
 * @param m Estructura a inicializar
 * @param motor_izq Driver de la rueda izquierda (los motores son de la APP: `application.c`)
 * @param motor_der Driver de la rueda derecha
 * @param kp Ganancia proporcional de los lazos de velocidad
 * @param ki Ganancia integral
 * @param kd Ganancia derivativa
 * @note Conviene pasarle las ganancias que se hayan sintonizado en la prueba 18.
 * @note Los motores se pasan por parámetro en vez de usar los globales de `application.c`: así este
 *       módulo no depende de un estado que no controla.
 */
void MOTION_Init(Motion_t *m,
                 DRV8833_Motor_t *motor_izq,
                 DRV8833_Motor_t *motor_der,
                 float kp, float ki, float kd);

/**
 * @brief Avanza en línea recta la distancia pedida.
 *
 * Mantiene el rumbo con la odometría: si el robot se desvía, reparte la diferencia de velocidad entre
 * las dos ruedas. Es lo que evita que el robot se vaya de lado al cruzar un pasillo largo.
 *
 * @param m Estructura de las primitivas
 * @param mm Distancia a recorrer, positiva hacia delante
 * @return 1 si llegó dentro de tolerancia, 0 si agotó el tiempo o se pidió salir
 */
uint8_t MOTION_Avanzar(Motion_t *m, float mm);

/**
 * @brief Gira sobre el centro el ángulo pedido.
 *
 * @param m Estructura de las primitivas
 * @param grados Ángulo a girar; **positivo a la izquierda** (antihorario), como θ en la odometría
 * @return 1 si llegó dentro de tolerancia, 0 si agotó el tiempo o se pidió salir
 */
uint8_t MOTION_Girar(Motion_t *m, float grados);

/**
 * @brief Frena los dos motores y deja los lazos parados.
 *
 * @param m Estructura de las primitivas
 */
void MOTION_Parar(Motion_t *m);

/**
 * @brief Ángulo que llevas girado desde el último MOTION_Init() [°].
 *
 * @param m Estructura de las primitivas
 * @return Orientación estimada
 */
float MOTION_GetAngulo(const Motion_t *m);

#endif /* APP_MOTION_H */
