#ifndef CORE_PROFILE_H
#define CORE_PROFILE_H

#include <stdint.h>

/*
 * Perfiles de velocidad trapezoidales — capa de APLICACION (core).
 *
 * Un perfil es la velocidad en funcion del tiempo para recorrer una distancia: acelera
 * hasta la velocidad de crucero, la mantiene y frena hasta la velocidad final. Con el
 * perfil se acaba el «avanza 180 mm a ciegas»: la consigna se recalcula en **cada tick**
 * y, si el robot se queda corto o se pasa, el error lo dice enseguida.
 *
 * @note Es **unit-agnostico**: no sabe si las unidades son mm, grados o lo que sea. Solo
 *       pide coherencia entre distancia, velocidad y aceleracion. Eso es lo que permite
 *       usar el **mismo modulo** para el avance (mm, mm/s, mm/s²) y para el giro
 *       (grados, grados/s, grados/s²), en vez de tener dos lazos distintos.
 *
 * @note **No depende de nada**: ni del STM32, ni de los drivers, ni del reloj. Se puede
 *       compilar y probar en el PC (ver `firmware/test/profile_test.c`). Es la primera
 *       pieza de la capa de aplicacion portable.
 *
 * Inspirado en el `Profile` de `ukmars/mazerunner-core` (MIT, Peter Harrison).
 */

/** Velocidad minima para cruzar el punto final, en unidades por segundo. */
#define PROF_VELOCIDAD_MINIMA 5.0f

/** Holgura con la que se da por recorrida la distancia: por debajo, el perfil termina. */
#define PROF_TOLERANCIA_POSICION 0.125f

/** Distancia por debajo de la cual no vale la pena moverse y el perfil nace terminado. */
#define PROF_DISTANCIA_MINIMA 1.0f

/** Estados del perfil. */
typedef enum {
    PROF_IDLE = 0,      /**< Parado, sin perfil en curso */
    PROF_ACELERANDO,    /**< Acercandose a la velocidad de crucero */
    PROF_FRENANDO,      /**< Acercandose a la velocidad final */
    PROF_TERMINADO      /**< Distancia recorrida (la velocidad sigue la final) */
} Profile_Estado_t;

/**
 * @brief Un perfil trapezoidal.
 *
 * La estructura es publica porque el llamante la declara (no hay memoria dinamica en el
 * robot), pero **nadie debe tocar sus campos a mano**: se usan las funciones `PROF_*`.
 *
 * @note `estado`, `velocidad` y `posicion` son `volatile` y de 32 bits, asi que se leen y
 *       se escriben de una sola vez en un Cortex-M: `PROF_Update()` puede correr en una
 *       interrupcion y los `PROF_Get*()` desde el lazo sin seccion critica.
 */
typedef struct {
    volatile uint8_t estado;
    volatile float   velocidad;
    volatile float   posicion;
    float            velocidad_objetivo;
    float            velocidad_final;
    float            posicion_final;
    float            aceleracion;
    float            inv_aceleracion;
    int8_t           signo;
} Profile_t;

void    PROF_Init(Profile_t *perfil);
void    PROF_Start(Profile_t *perfil, float distancia, float vel_max,
                   float vel_final, float aceleracion);
void    PROF_Update(Profile_t *perfil, float dt);
void    PROF_Stop(Profile_t *perfil);
void    PROF_Terminar(Profile_t *perfil);
float   PROF_GetVelocidad(const Profile_t *perfil);
float   PROF_GetPosicion(const Profile_t *perfil);
uint8_t PROF_HaTerminado(const Profile_t *perfil);

#endif /* CORE_PROFILE_H */
