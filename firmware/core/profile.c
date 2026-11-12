/*
 * Aviso de atribucion (licencia MIT).
 *
 * Este fichero es una adaptacion del `Profile` de `ukmars/mazerunner-core`
 * (https://github.com/ukmars/mazerunner-core), Copyright (c) Peter Harrison,
 * distribuido bajo licencia MIT. Se conserva este aviso en cumplimiento de
 * dicha licencia. El resto del repositorio se distribuye bajo GPL-3.0-or-later
 * (ver LICENSE en la raiz) y el diseno de hardware bajo CERN-OHL-S-2.0
 * (ver LICENSE-HARDWARE).
 */

#include "core/profile.h"

#include <math.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/**
 * @brief Distancia (o angulo) que hace falta para pasar de la velocidad actual a la final.
 *
 * Sale de las ecuaciones del movimiento uniformemente acelerado:
 * $d = \frac{v_{final}^2 - v_{actual}^2}{2a}$. Es la comprobacion que hace que el perfil
 * sepa **cuando tiene que empezar a frenar** en vez de pasarse del punto.
 *
 * @param perfil Perfil en curso
 * @return Distancia, siempre en positivo (se comparan las velocidades al cuadrado)
 */
static float prof_distancia_frenada(const Profile_t *perfil)
{
    return fabsf((perfil->velocidad * perfil->velocidad) -
                 (perfil->velocidad_final * perfil->velocidad_final)) *
           0.5f * perfil->inv_aceleracion;
}

/* ------------------------------------------------------------------ */
/**
 * @brief Deja el perfil parado y en reposo.
 *
 * @param perfil Perfil a inicializar
 */
void PROF_Init(Profile_t *perfil)
{
    if (perfil == NULL) {
        return;
    }

    perfil->estado            = PROF_IDLE;
    perfil->velocidad         = 0.0f;
    perfil->posicion          = 0.0f;
    perfil->velocidad_objetivo = 0.0f;
    perfil->velocidad_final   = 0.0f;
    perfil->posicion_final    = 0.0f;
    perfil->aceleracion       = 0.0f;
    perfil->inv_aceleracion   = 1.0f;
    perfil->signo             = 1;
}

/* ------------------------------------------------------------------ */
/**
 * @brief Arranca un perfil hacia una distancia, con velocidad de crucero y final.
 *
 * Una vez arrancado, el perfil **se termina solo**: no hay que volver a llamarlo salvo
 * que se quiera cambiar de idea, en cuyo caso una llamada nueva **sustituye** a la
 * anterior conservando la velocidad actual.
 *
 * @param perfil      Perfil a arrancar
 * @param distancia   Recorrido a hacer. **El signo indica el sentido** (hacia atras o
 *                    hacia la izquierda si es negativo)
 * @param vel_max     Velocidad de crucero, en unidades por segundo
 * @param vel_final   Velocidad al llegar, en unidades por segundo
 * @param aceleracion Ritmo de cambio de velocidad, en unidades por segundo al cuadrado
 *
 * @note **No toca `velocidad`**: si el perfil venia ya rodando, arranca desde esa
 *       velocidad. Es lo que permite **encadenar movimientos sin parar** (acabar un
 *       avance rodando y empezar un giro enlazado).
 * @note Si `vel_final` supera a `vel_max` se recorta: el perfil frena *desde* la de
 *       crucero, no puede terminar mas rapido que ella.
 * @note Una distancia por debajo de `PROF_DISTANCIA_MINIMA` nace **terminada**, para que
 *       el llamante no se quede esperando un movimiento que no va a ocurrir.
 */
void PROF_Start(Profile_t *perfil, float distancia, float vel_max,
                float vel_final, float aceleracion)
{
    if (perfil == NULL) {
        return;
    }

    perfil->signo = (distancia < 0.0f) ? -1 : 1;
    if (distancia < 0.0f) {
        distancia = -distancia;
    }

    if (distancia < PROF_DISTANCIA_MINIMA) {
        perfil->estado    = PROF_TERMINADO;
        perfil->velocidad = 0.0f;
        return;
    }

    if (vel_final > vel_max) {
        vel_final = vel_max;
    }

    perfil->posicion           = 0.0f;
    perfil->posicion_final     = distancia;
    perfil->velocidad_objetivo = (float)perfil->signo * fabsf(vel_max);
    perfil->velocidad_final    = (float)perfil->signo * fabsf(vel_final);
    perfil->aceleracion        = fabsf(aceleracion);
    perfil->inv_aceleracion    = (perfil->aceleracion >= 1.0f)
                                     ? (1.0f / perfil->aceleracion)
                                     : 1.0f;
    perfil->estado             = PROF_ACELERANDO;
}

/* ------------------------------------------------------------------ */
/**
 * @brief Avanza el perfil un intervalo de tiempo. Hay que llamarlo en **cada tick**.
 *
 * Hace tres cosas, en este orden: decide si toca frenar, acerca la velocidad a la
 * objetivo, e integra la posicion. Al final comprueba si ya se ha recorrido la distancia.
 *
 * @param perfil Perfil a avanzar
 * @param dt     Tiempo transcurrido desde la ultima llamada, en segundos
 *
 * @note El `dt` se pide **por parametro** en vez de usar una constante: el lazo de este
 *       robot no es de cadencia perfecta y dar por hecho un `dt` fijo ya costo una
 *       tarde entera (ver el historial del 2026-10-01). Quien llama es quien sabe el
 *       tiempo que ha pasado de verdad.
 */
void PROF_Update(Profile_t *perfil, float dt)
{
    float delta_v;
    float restante;

    if ((perfil == NULL) || (perfil->estado == PROF_IDLE)) {
        return;
    }

    delta_v  = perfil->aceleracion * dt;
    restante = fabsf(perfil->posicion_final) - fabsf(perfil->posicion);

    /* Cuando lo que queda es menos de lo que se tarda en frenar, hay que empezar a frenar
       YA o el robot se pasa del punto. Es el paso de trapecio a trapecio invertido. */
    if ((perfil->estado == PROF_ACELERANDO) &&
        (restante < prof_distancia_frenada(perfil))) {
        perfil->estado = PROF_FRENANDO;
        /* Al frenar hasta cero se apunta a una velocidad minima en vez de a cero: asi el
           robot cruza el punto final aunque el redondeo de la coma flotante se empenie en
           lo contrario. Es un apaño, pero es un apaño necesario. */
        perfil->velocidad_objetivo = (perfil->velocidad_final == 0.0f)
                                         ? ((float)perfil->signo * PROF_VELOCIDAD_MINIMA)
                                         : perfil->velocidad_final;
    }

    /* Acercar la velocidad a la objetivo, sin pasarse por arriba ni por abajo */
    if (perfil->velocidad < perfil->velocidad_objetivo) {
        perfil->velocidad += delta_v;
        if (perfil->velocidad > perfil->velocidad_objetivo) {
            perfil->velocidad = perfil->velocidad_objetivo;
        }
    }
    if (perfil->velocidad > perfil->velocidad_objetivo) {
        perfil->velocidad -= delta_v;
        if (perfil->velocidad < perfil->velocidad_objetivo) {
            perfil->velocidad = perfil->velocidad_objetivo;
        }
    }

    perfil->posicion += perfil->velocidad * dt;

    /* Y cuando queda menos que la holgura, se da por recorrido. La velocidad sigue
       siguiendo a la final, asi que un perfil terminado con velocidad final distinta de
       cero **sigue rodando**: es lo que permite enlazar un movimiento con el siguiente. */
    if ((perfil->estado != PROF_TERMINADO) && (restante < PROF_TOLERANCIA_POSICION)) {
        perfil->estado             = PROF_TERMINADO;
        perfil->velocidad_objetivo = perfil->velocidad_final;
    }
}

/* ------------------------------------------------------------------ */
/**
 * @brief Corta el perfil y deja la velocidad objetivo a cero.
 *
 * @param perfil Perfil a parar
 */
void PROF_Stop(Profile_t *perfil)
{
    if (perfil == NULL) {
        return;
    }

    perfil->velocidad_objetivo = 0.0f;
    PROF_Terminar(perfil);
}

/* ------------------------------------------------------------------ */
/**
 * @brief Da el perfil por terminado **ya**, sin recorrer lo que le quedaba.
 *
 * @param perfil Perfil a terminar
 */
void PROF_Terminar(Profile_t *perfil)
{
    if (perfil == NULL) {
        return;
    }

    perfil->velocidad = perfil->velocidad_objetivo;
    perfil->estado    = PROF_TERMINADO;
}

/* ------------------------------------------------------------------ */
/**
 * @brief Velocidad que pide el perfil ahora mismo.
 *
 * @param perfil Perfil a consultar
 * @return Velocidad, en las unidades con las que se arranco
 */
float PROF_GetVelocidad(const Profile_t *perfil)
{
    return (perfil == NULL) ? 0.0f : perfil->velocidad;
}

/* ------------------------------------------------------------------ */
/**
 * @brief Recorrido hecho desde que se arranco el perfil.
 *
 * @param perfil Perfil a consultar
 * @return Posicion, con el signo del sentido de la marcha
 */
float PROF_GetPosicion(const Profile_t *perfil)
{
    return (perfil == NULL) ? 0.0f : perfil->posicion;
}

/* ------------------------------------------------------------------ */
/**
 * @brief Dice si el perfil ya ha recorrido su distancia.
 *
 * @param perfil Perfil a consultar
 * @return 1 si ha terminado, 0 si sigue en curso
 */
uint8_t PROF_HaTerminado(const Profile_t *perfil)
{
    return ((perfil != NULL) && (perfil->estado == PROF_TERMINADO)) ? 1U : 0U;
}
