/**
 * @file motion.c
 * @brief Primitivas de movimiento (capa APP): avanzar una distancia y girar un ángulo.
 *
 * El lazo es siempre el mismo, y es el único sitio que toca el hardware:
 *
 *   leer encoders -> mover la pose (FILTER) -> calcular el duty (CONTROL) -> escribirlo en el driver
 *
 * Lo que cambia entre avanzar y girar es **qué se pide** a cada rueda: la misma velocidad en las dos
 * (con el rumbo corregido) o la misma velocidad **opuesta** (para girar sobre el centro).
 *
 * Las dos primitivas van **despacio a propósito**. Para lo que se usan —cruzar una celda de 180 mm y
 * doblar una esquina— no hacen falta 600 mm/s, y la precisión de 180 mm y de 90° sale mucho más
 * barata en milisegundos de movimiento que en código de corrección.
 */

#include "app/motion.h"
#include "app/buttons.h"
#include "hal/drv8833.h"
#include "hal/systemclock.h"
#include "sensor/encoder.h"

#include <math.h>

/** @brief π, para no repetir la constante. */
#define MOTION_PI             3.14159265f

/** @brief Conversión radianes → grados. */
#define MOTION_RAD_A_GRADOS   (180.0f / MOTION_PI)

/** @brief Periodo del lazo de las primitivas [ms]: 100 Hz, el mismo que CONTROL. */
#define MOTION_PASO_MS        10U

/** @brief Tiempo que se espera a que el robot se detenga antes de dar un movimiento por terminado [ms]. */
#define MOTION_ASENTAR_MS     250U

/** @brief Ganancia de posición al avanzar [mm/s por mm que falta].

 * Con 1,5 el robot va a tope si le faltan más de 170 mm y entra en el tramo final frenando solo. */
#define MOTION_AVANCE_KP      1.5f

/** @brief Velocidad mínima al avanzar [mm/s]: por debajo, el motor no vence la fricción estática. */
#define MOTION_AVANCE_VEL_MIN_MM_S 50.0f

/**
 * @brief Un paso de las primitivas: mide, mueve la pose, calcula el duty y lo aplica.
 *
 * La odometría se alimenta con los incrementos de µm **de cada rueda** desde la lectura anterior, que
 * es lo que espera `ODOM_Update()` (arcos, no velocidades).
 *
 * @param m Estructura de las primitivas
 * @param consigna_izq Velocidad pedida a la rueda izquierda [mm/s]
 * @param consigna_der Velocidad pedida a la rueda derecha [mm/s]
 * @param dt Segundos desde el paso anterior
 */
static void motion_paso(Motion_t *m, float consigna_izq, float consigna_der, float dt)
{
    int32_t um_izq;
    int32_t um_der;

    update_encoder_readings();

    um_izq = get_encoder_left_micrometers();
    um_der = get_encoder_right_micrometers();

    ODOM_Update(&m->odo,
                (float)(um_izq - m->um_izq_previo) / 1000000.0f,
                (float)(um_der - m->um_der_previo) / 1000000.0f);

    m->um_izq_previo = um_izq;
    m->um_der_previo = um_der;

    CTRL_SetConsigna(&m->izq, consigna_izq);
    CTRL_SetConsigna(&m->der, consigna_der);

    DRV8833_Set_Speed(m->motor_izq,
                      (int16_t)CTRL_Update(&m->izq, get_encoder_left_speed() * 1000.0f, dt));
    DRV8833_Set_Speed(m->motor_der,
                      (int16_t)CTRL_Update(&m->der, get_encoder_right_speed() * 1000.0f, dt));
}

/**
 * @brief Deja de pedir velocidad y espera a que el robot se detenga del todo.
 *
 * Se llama al final de cada movimiento, y no es un adorno: si se frena en seco con el robot lanzado,
 * los últimos milímetros (o grados) se recorren **después** de haber mirado la medida, y el movimiento
 * siempre se pasa en el mismo sentido.
 *
 * @param m Estructura de las primitivas
 */
static void motion_asentar(Motion_t *m)
{
    uint32_t t_inicio = HAL_GetTick();
    uint32_t t_ultimo = t_inicio;

    /* Se **suelta el pedal de golpe**, y no es un detalle. El integrador del PID venía guardando el
       duty que hacía falta para avanzar (~13 %), y se descarga a `Ki · error` ≈ 25 %/s, así que
       seguía **empujando** unos 0,4 s despues de haberse dado el movimiento por terminado. Como la
       odometría sigue contando mientras el robot rueda, ese rato se sumaba al recorrido: medido el
       2026-10-01, **19,6 mm de más** en una celda de 180 y **~19° de más** en un giro de 90°.

       Reseteando el PID el empuje se apaga de golpe y, con la consigna a cero, es la **P** la que
       frena (el error es negativo).

       ⚠️ Y no bastaba con **descontar** esos milímetros del objetivo: la frenada real varió entre 2 y
       44 mm de una celda a otra, porque depende de cuánto le quedara al integrador. Lo que hay que
       quitar es el rodaje, no calibrarlo. */
    CTRL_Parar(&m->izq);
    CTRL_Parar(&m->der);

    while ((HAL_GetTick() - t_inicio) < MOTION_ASENTAR_MS) {
        uint32_t ahora = HAL_GetTick();
        float    dt    = (float)(ahora - t_ultimo) / 1000.0f;

        t_ultimo = ahora;

        motion_paso(m, 0.0f, 0.0f, dt);

        /* Además de la P, **freno dinámico**: el DRV8833 cortocircuita el motor y la rueda para en
           decenas de milisegundos. Sin esto el robot rueda libre y sigue sumando recorrido. */
        DRV8833_Brake(m->motor_izq);
        DRV8833_Brake(m->motor_der);

        HAL_Delay(MOTION_PASO_MS);
    }

    DRV8833_Brake(m->motor_izq);
    DRV8833_Brake(m->motor_der);
}

/**
 * @brief Lleva un ángulo al rango $(-\pi, \pi]$.
 *
 * Hace falta para que un giro que cruza el ±180° no pida dar la vuelta entera: sin esto, girar 10° a
 * la izquierda desde 175° pediría recorrer 350° en lugar de 10°.
 *
 * @param rad Ángulo en radianes
 * @return Ángulo equivalente dentro del rango
 */
static float motion_error_angulo(float rad)
{
    while (rad > MOTION_PI)  { rad -= 2.0f * MOTION_PI; }
    while (rad <= -MOTION_PI) { rad += 2.0f * MOTION_PI; }

    return rad;
}

/**
 * @brief Prepara las primitivas: arranca los dos lazos y pone la pose en el origen.
 */
void MOTION_Init(Motion_t *m,
                 DRV8833_Motor_t *motor_izq,
                 DRV8833_Motor_t *motor_der,
                 float kp, float ki, float kd)
{
    CTRL_Init(&m->izq, kp, ki, kd, MOTION_DUTY_MAX);
    CTRL_Init(&m->der, kp, ki, kd, MOTION_DUTY_MAX);
    CTRL_SetRampa(&m->izq, MOTION_RAMPA_MM_S2);
    CTRL_SetRampa(&m->der, MOTION_RAMPA_MM_S2);

    m->motor_izq = motor_izq;
    m->motor_der = motor_der;

    ODOM_Init(&m->odo, 0.0f, 0.0f, 0.0f);

    update_encoder_readings();
    m->um_izq_previo = get_encoder_left_micrometers();
    m->um_der_previo = get_encoder_right_micrometers();
}

/**
 * @brief Ángulo que llevas girado desde el último MOTION_Init() [°].
 */
float MOTION_GetAngulo(const Motion_t *m)
{
    return m->odo.theta * MOTION_RAD_A_GRADOS;
}

/**
 * @brief Frena los dos motores y deja los lazos parados.
 */
void MOTION_Parar(Motion_t *m)
{
    CTRL_Parar(&m->izq);
    CTRL_Parar(&m->der);

    DRV8833_Brake(m->motor_izq);
    DRV8833_Brake(m->motor_der);
}

/**
 * @brief Avanza en línea recta la distancia pedida.
 *
 * La velocidad se pide **proporcional a lo que falta**, así que el robot ya va frenando solo en el
 * tramo final y no hay que adivinar dónde cortar. El rumbo se corrige con la odometría: si el robot se
 * ha desviado, la corrección se reparte entre las dos ruedas, que es más suave que girar después.
 *
 * @param m Estructura de las primitivas
 * @param mm Distancia a recorrer: **positiva hacia delante, negativa hacia atrás**
 * @return 1 si llegó dentro de tolerancia, 0 si agotó el tiempo o se pidió salir
 * @see Las constantes `MOTION_AVANCE_*` y `MOTION_RUMBO_KP` son las que se ajustan en banco.
 */
uint8_t MOTION_Avanzar(Motion_t *m, float mm)
{
    int32_t  um_izq_0 = m->um_izq_previo;
    int32_t  um_der_0 = m->um_der_previo;
    float    theta_0  = m->odo.theta;
    uint32_t t_inicio = HAL_GetTick();
    uint32_t t_ultimo = t_inicio;
    uint8_t  llegado  = 0U;

    while ((HAL_GetTick() - t_inicio) < MOTION_TIMEOUT_MS) {
        uint32_t ahora;
        float    dt;
        float    avance_mm;
        float    falta_mm;
        float    error_rumbo;
        float    correccion;
        float    vel;

        if (BUTTONS_SalidaSolicitada() != 0U) {
            break;
        }

        avance_mm = (float)((m->um_izq_previo - um_izq_0) + (m->um_der_previo - um_der_0)) / 2000.0f;
        falta_mm  = mm - avance_mm;

        /* El criterio va en **valor absoluto**, y no es un detalle de estilo: con la comparación a
           secas, retroceder **no hacía nada**. `falta_mm` empieza en −180, y `−180 <= 3` se cumple en
           la primera vuelta, así que el movimiento se daba por llegado sin haber movido una rueda. */
        if (fabsf(falta_mm) <= (MOTION_TOL_MM + MOTION_FRENADA_MM)) {
            llegado = 1U;
            break;
        }

        ahora = HAL_GetTick();
        dt       = (float)(ahora - t_ultimo) / 1000.0f;
        t_ultimo = ahora;

        /* La velocidad sale del **signo** de lo que falta, así que la misma cuenta vale para los dos
           sentidos. Los topes hay que recortarlos por los dos lados: al retroceder, el tope inferior
           (`vel < MOTION_AVANCE_VEL_MIN_MM_S`) convertía cualquier velocidad negativa en **+50** y el
           cambio de signo posterior la dejaba en −50, o sea que el robot retrocedía siempre a la
           velocidad mínima, ignorando lo que le faltara. */
        vel = MOTION_AVANCE_KP * falta_mm;
        if (vel > MOTION_VEL_MM_S) {
            vel = MOTION_VEL_MM_S;
        } else if (vel < -MOTION_VEL_MM_S) {
            vel = -MOTION_VEL_MM_S;
        }

        if ((vel > 0.0f) && (vel < MOTION_AVANCE_VEL_MIN_MM_S)) {
            vel = MOTION_AVANCE_VEL_MIN_MM_S;
        } else if ((vel < 0.0f) && (vel > -MOTION_AVANCE_VEL_MIN_MM_S)) {
            vel = -MOTION_AVANCE_VEL_MIN_MM_S;
        }

        /* Rumbo: θ ha de quedarse donde estaba. La corrección va a las dos ruedas en sentidos opuestos,
           porque lo que se corrige es la diferencia, no la velocidad de crucero.

           ⚠️ **El SIGNO es lo que hace que esto sea realimentación NEGATIVA**, y equivocarlo no se nota
           en el código: se nota en el suelo. Girar a la izquierda (θ positivo) es que la rueda
           **derecha** avance más que la izquierda (`dθ = (ds_der − ds_izq) / L`). Si el robot se ha ido
           a la izquierda, `error_rumbo` sale **negativo** y hay que **frenar la derecha**: o sea,
           restarle la corrección a la derecha y sumársela a la izquierda.

           Con los signos al revés el lazo se realimenta solo y el robot **se va en curva**: medido el
           2026-10-01 en la primera pasada de la prueba 20, la celda terminó con **35,8° de desvío** y
           **233 mm** recorridos en vez de 180. Cualquier desvío inicial, por pequeño que sea, se
           multiplica por $e^{(2 K_p / L) t}$ en vez de apagarse.

           ⚠️ **La diferencia hay que normalizarla.** `theta_0` y `odo.theta` están las dos en
           $(-\pi, \pi]$, así que restarlas en crudo da como mucho $2\pi$ de salto: con el robot a 170°
           y un desvío pequeño, la resta puede salir cerca de 360° y la corrección pide **diez veces**
           la velocidad máxima (se va el robot de las manos). Con `motion_error_angulo()` el desvío se
           mide siempre por el camino corto. */
        error_rumbo = motion_error_angulo(theta_0 - m->odo.theta) * MOTION_RAD_A_GRADOS;
        correccion  = MOTION_RUMBO_KP * error_rumbo;

        motion_paso(m, vel - correccion, vel + correccion, dt);

        HAL_Delay(MOTION_PASO_MS);
    }

    motion_asentar(m);

    return llegado;
}

/**
 * @brief Gira sobre el centro el ángulo pedido.
 *
 * La velocidad se pide proporcional al error de ángulo, con un **mínimo** que vence la fricción
 * estática: sin ese mínimo el robot se queda a medio giro pidiendo un duty que no lo mueve, que es el
 * fallo clásico de un lazo de posición en un motor con reductora.
 *
 * @param m Estructura de las primitivas
 * @param grados Ángulo a girar; **positivo a la izquierda** (antihorario), como θ en la odometría
 * @return 1 si llegó dentro de tolerancia, 0 si agotó el tiempo o se pidió salir
 */
uint8_t MOTION_Girar(Motion_t *m, float grados)
{
    float    objetivo_rad = m->odo.theta + (grados / MOTION_RAD_A_GRADOS);
    int32_t  um_izq_0     = m->um_izq_previo;
    int32_t  um_der_0     = m->um_der_previo;
    uint32_t t_inicio     = HAL_GetTick();
    uint32_t t_ultimo     = t_inicio;
    uint8_t  llegado      = 0U;

    while ((HAL_GetTick() - t_inicio) < MOTION_TIMEOUT_MS) {
        uint32_t ahora;
        float    dt;
        float    error_grados;
        float    vel;
        float    desplazamiento_mm;
        float    centro_mm_s;

        if (BUTTONS_SalidaSolicitada() != 0U) {
            break;
        }

        error_grados = motion_error_angulo(objetivo_rad - m->odo.theta) * MOTION_RAD_A_GRADOS;

        if (fabsf(error_grados) <= MOTION_TOL_GRADOS) {
            llegado = 1U;
            break;
        }

        ahora = HAL_GetTick();
        dt       = (float)(ahora - t_ultimo) / 1000.0f;
        t_ultimo = ahora;

        vel = MOTION_GIRO_KP * fabsf(error_grados);
        if (vel > MOTION_VEL_MM_S) {
            vel = MOTION_VEL_MM_S;
        }
        if (vel < MOTION_GIRO_VEL_MIN_MM_S) {
            vel = MOTION_GIRO_VEL_MIN_MM_S;
        }
        if (error_grados < 0.0f) {
            vel = -vel;
        }

        /* **Girar sin desplazarse**: los dos arcos han de ser iguales y opuestos, o el robot se va
           mientras gira. No lo son —un motor vence el rozamiento antes que el otro— y medido el
           2026-10-01 un giro de 90° desplazaba **~18 mm** (72 mm en un cuadrado, sobre celdas de
           180 mm).

           El desplazamiento del centro es la **media** de los dos arcos, así que se corrige con un
           término **común**: lo mismo sumado a las dos ruedas mueve el centro y **no toca la
           diferencia**, o sea que no toca el ángulo. Los dos lazos quedan desacoplados.

           La corrección es proporcional a lo que **ya** se ha desplazado (control sobre la posición,
           no sobre la velocidad), así que no arrastra un error de régimen grande: con
           `MOTION_GIRO_CENTRO_KP` el residuo es de un par de milímetros. */
        desplazamiento_mm = (float)((m->um_izq_previo - um_izq_0) +
                                    (m->um_der_previo - um_der_0)) / 2000.0f;
        centro_mm_s       = -MOTION_GIRO_CENTRO_KP * desplazamiento_mm;

        /* Girar a la izquierda (θ positivo) es que la rueda derecha avance y la izquierda retroceda:
           dθ = (ds_der − ds_izq) / L. */
        motion_paso(m, -vel + centro_mm_s, vel + centro_mm_s, dt);

        HAL_Delay(MOTION_PASO_MS);
    }

    motion_asentar(m);

    return llegado;
}
