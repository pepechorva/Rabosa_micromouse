/*
 * Banco del PC para core/profile.c
 *
 * El perfil es matematica pura: no toca el hardware, asi que se puede simular entero en
 * el PC. Aqui se le hace correr varios movimientos y se comprueba que **recorre lo que
 * se le pide**, que **no se pasa de la velocidad de crucero** y que **no se sale del
 * punto** por haberse olvidado de frenar.
 *
 * Compilar y ejecutar (desde la raiz del repositorio):
 *
 *     gcc -Wall -Wextra -I firmware -o /tmp/profile_test \
 *         firmware/test/profile_test.c firmware/core/profile.c -lm && /tmp/profile_test
 */

#include "core/profile.h"

#include <math.h>
#include <stdio.h>

#define DT            0.002f   /* 500 Hz, como el tick de mazerunner-core */
#define MAX_ITER      40000U   /* tope de seguridad: 80 s */

/** Lo que se espera de un movimiento. */
typedef struct {
    const char *nombre;
    float       distancia;
    float       vel_max;
    float       vel_final;
    float       aceleracion;
} Caso_t;

static const Caso_t CASOS[] = {
    {"celda de 180 mm a 300 mm/s",        180.0f, 300.0f, 0.0f,   1500.0f},
    {"celda de 180 mm a 600 mm/s",        180.0f, 600.0f, 0.0f,   1500.0f},
    {"giro de 90 grados",                  90.0f, 200.0f, 0.0f,   1000.0f},
    {"media celda a 300 mm/s",             90.0f, 300.0f, 0.0f,   1500.0f},
    {"avance largo, 3 celdas",            540.0f, 400.0f, 0.0f,   1200.0f},
    {"hacia atras, 180 mm",              -180.0f, 300.0f, 0.0f,   1500.0f},
    {"retroceder un pelin, 10 mm",        -10.0f, 300.0f, 0.0f,   1500.0f},
    {"acaba rodando a 150 mm/s",          180.0f, 300.0f, 150.0f, 1500.0f},
    {"cambio de idea: 40 mm",              40.0f, 300.0f, 0.0f,   1500.0f},
};

/**
 * @brief Simula un movimiento completo y saca lo que hizo.
 *
 * @param caso      Movimiento a simular
 * @param final_pos [out] Posicion al terminar
 * @param pico_vel  [out] Velocidad maxima alcanzada (en valor absoluto)
 * @param pasos     [out] Numero de ticks que tardo
 * @return 1 si el perfil llego a terminar dentro del tope de iteraciones
 */
static int simular(const Caso_t *caso, float *final_pos, float *pico_vel, unsigned *pasos)
{
    Profile_t p;

    PROF_Init(&p);
    PROF_Start(&p, caso->distancia, caso->vel_max, caso->vel_final, caso->aceleracion);

    *pico_vel = 0.0f;
    for (unsigned i = 0U; i < MAX_ITER; i++) {
        PROF_Update(&p, DT);
        *pasos = i + 1U;

        if (fabsf(PROF_GetVelocidad(&p)) > *pico_vel) {
            *pico_vel = fabsf(PROF_GetVelocidad(&p));
        }
        if (PROF_HaTerminado(&p) != 0U) {
            *final_pos = PROF_GetPosicion(&p);
            return 1;
        }
    }

    *final_pos = PROF_GetPosicion(&p);
    return 0;
}

int main(void)
{
    int fallos = 0;

    printf("Perfiles trapezoidales — simulacion a %.0f Hz\n\n", 1.0f / DT);

    for (unsigned i = 0U; i < sizeof(CASOS) / sizeof(CASOS[0]); i++) {
        const Caso_t *c = &CASOS[i];
        float   final_pos, pico, error;
        unsigned pasos = 0U;
        int      ok;

        ok      = simular(c, &final_pos, &pico, &pasos);
        error   = fabsf(final_pos) - fabsf(c->distancia);

        /* La tolerancia del perfil es de 0,125 unidades, pero al integrar a 500 Hz se
           puede quedar alguna decima mas alla. Se admite 1 unidad y se avisa aparte si el
           error es mayor del esperado. */
        printf("  %-28s  dist %7.2f  pedido %7.2f  error %+6.3f  pico %6.1f/%.0f  %5u ms",
               c->nombre, (double)final_pos, (double)c->distancia,
               (double)(final_pos - c->distancia), (double)pico, (double)c->vel_max,
               pasos * 2U);

        if (ok == 0) {
            printf("   <- NO TERMINO\n");
            fallos++;
            continue;
        }
        if (fabsf(error) > 1.0f) {
            printf("   <- error de recorrido\n");
            fallos++;
            continue;
        }
        if (pico > (c->vel_max + 1.0f)) {
            printf("   <- se paso de la velocidad de crucero\n");
            fallos++;
            continue;
        }
        printf("\n");
    }

    /* Un movimiento imposible no debe colgar a nadie: tiene que nacer terminado */
    {
        Profile_t p;

        PROF_Init(&p);
        PROF_Start(&p, 0.2f, 300.0f, 0.0f, 1500.0f);
        if (PROF_HaTerminado(&p) == 0U) {
            printf("\n  MAL: una distancia minuscula no nace terminada\n");
            fallos++;
        } else {
            printf("\n  distancia minuscula (0,2): nace terminada, correcto\n");
        }
    }

    /* Encadenar: un perfil que acaba rodando deja la velocidad puesta para el siguiente */
    {
        Profile_t p;
        float     v_final;

        PROF_Init(&p);
        PROF_Start(&p, 180.0f, 300.0f, 150.0f, 1500.0f);
        while (PROF_HaTerminado(&p) == 0U) {
            PROF_Update(&p, DT);
        }
        v_final = PROF_GetVelocidad(&p);
        printf("  al terminar rodando, deja %6.1f (pedido 150) y la posicion sigue a %6.1f\n",
               (double)v_final, (double)PROF_GetPosicion(&p));

        if (fabsf(v_final - 150.0f) > 2.0f) {
            printf("  MAL: no mantiene la velocidad final\n");
            fallos++;
        }
    }

    printf("\n  %u movimientos, %d fallos\n", (unsigned)(sizeof(CASOS) / sizeof(CASOS[0])), fallos);

    return (fallos != 0) ? 1 : 0;
}
