/**
 * @file pid_sim.c
 * @brief Banco de pruebas en el PC de la capa CONTROL. NO se compila en el firmware.
 *
 * Valida la lógica del lazo de velocidad (`control/pid.c`, `setpoint.c` y `controller.c`) contra una
 * planta simulada de primer orden, antes de tener el robot montado. Comprueba las puertas de calidad
 * de la fase de control (sobreimpulso y error en régimen permanente) y el anti-windup.
 *
 * Compilar y ejecutar desde la raíz del repositorio:
 * @code
 *   gcc -I firmware -O2 -Wall -o /tmp/pid_sim firmware/test/pid_sim.c \
 *       firmware/control/pid.c firmware/control/setpoint.c firmware/control/controller.c -lm
 *   /tmp/pid_sim
 * @endcode
 *
 * @note `firmware/test/` no entra en el build de PlatformIO (solo se compilan las capas de
 *       `extra_script.py`), así que este fichero no interfiere con el firmware.
 * @warning La planta es ideal: sin rozamiento, sin deslizamiento y con tensión constante. Valida la
 *          LÓGICA del lazo, no la sintonía; esa se mide en el robot con la prueba 18.
 */

#include <stdio.h>
#include <math.h>
#include "control/controller.h"

#define PLANTA_MM_S_POR_DUTY   10.0f    /* mm/s que da cada 1 % de duty            */
#define PLANTA_TAU_S            0.15f   /* constante de tiempo del motor simulado  */
#define PASO_S                  0.01f   /* paso del lazo: 100 Hz                   */
#define TIEMPO_SIMULACION_S     3.0f

#define GANANCIA_KP             0.25f   /* las mismas de partida que la prueba 18  */
#define GANANCIA_KI             0.60f
#define GANANCIA_KD             0.00f

#define LIMITE_SOBREIMPULSO_PCT 20.0f   /* puertas de calidad de CONTROL           */
#define LIMITE_ERROR_FINAL_PCT   2.0f

#define PASOS_SATURACION_LARGA   500    /* 5 s pidiendo lo imposible               */
#define PASOS_PARA_LIBERAR       100    /* 1 s para ver si el integrador se suelta */
#define DUTY_LIBERADO_PCT        1.0f

/** @brief Resumen de una simulación, para poder comprobarlo y presentarlo aparte. */
typedef struct {
    float sobreimpulso_pct;
    float error_final_pct;
    float duty_maximo_pct;
    int   pasos_saturado;
} Resultado_t;

/**
 * @brief Avanza la planta simulada un paso de integración.
 * @param velocidad_actual_mm_s Velocidad antes del paso
 * @param duty_pct Duty aplicado, en %
 * @return Velocidad después del paso, en mm/s
 */
static float planta_avanzar(float velocidad_actual_mm_s, float duty_pct)
{
    float velocidad_final_mm_s = duty_pct * PLANTA_MM_S_POR_DUTY;

    return velocidad_actual_mm_s
         + (velocidad_final_mm_s - velocidad_actual_mm_s) * (PASO_S / PLANTA_TAU_S);
}

/**
 * @brief Simula el lazo cerrado y resume cómo se ha comportado.
 *
 * Una rampa enorme equivale a un escalón de consigna: así se prueba el peor caso sin duplicar código.
 *
 * @param objetivo_mm_s Consigna de velocidad
 * @param rampa_mm_s2 Pendiente de la rampa de consigna
 * @param resumen Estructura donde se deja el resultado
 */
static void simular_lazo(float objetivo_mm_s, float rampa_mm_s2, Resultado_t *resumen)
{
    Controller_t lazo;
    float velocidad_mm_s = 0.0f;
    float velocidad_maxima_mm_s = 0.0f;
    float duty_pct = 0.0f;
    int   pasos = (int)(TIEMPO_SIMULACION_S / PASO_S);

    resumen->duty_maximo_pct = 0.0f;
    resumen->pasos_saturado = 0;

    CTRL_Init(&lazo, GANANCIA_KP, GANANCIA_KI, GANANCIA_KD, 100.0f);
    CTRL_SetRampa(&lazo, rampa_mm_s2);
    CTRL_SetConsigna(&lazo, objetivo_mm_s);

    for (int i = 0; i < pasos; i++) {
        duty_pct = CTRL_Update(&lazo, velocidad_mm_s, PASO_S);

        if (fabsf(duty_pct) > resumen->duty_maximo_pct) {
            resumen->duty_maximo_pct = fabsf(duty_pct);
        }
        if (fabsf(duty_pct) >= 99.9f) {
            resumen->pasos_saturado++;
        }

        velocidad_mm_s = planta_avanzar(velocidad_mm_s, duty_pct);

        if (velocidad_mm_s > velocidad_maxima_mm_s) {
            velocidad_maxima_mm_s = velocidad_mm_s;
        }
    }

    resumen->sobreimpulso_pct = (velocidad_maxima_mm_s - objetivo_mm_s) / objetivo_mm_s * 100.0f;
    resumen->error_final_pct  = (objetivo_mm_s - velocidad_mm_s) / objetivo_mm_s * 100.0f;
}

/** @brief Escribe el resumen de una simulación en la salida estándar. */
static void imprimir_resultado(const char *titulo, const Resultado_t *resumen)
{
    printf("\n--- %s ---\n", titulo);
    printf("  sobreimpulso = %6.1f %%  (límite %.0f %%)\n",
           resumen->sobreimpulso_pct, LIMITE_SOBREIMPULSO_PCT);
    printf("  error final  = %6.1f %%  (límite %.0f %%)\n",
           resumen->error_final_pct, LIMITE_ERROR_FINAL_PCT);
    printf("  duty máximo  = %6.1f %%  · pasos saturado: %d\n",
           resumen->duty_maximo_pct, resumen->pasos_saturado);
}

/**
 * @brief Compara una simulación con las puertas de calidad.
 * @return Número de criterios incumplidos (0 = todo bien)
 */
static int contar_fallos(const Resultado_t *resumen)
{
    int fallos = 0;

    if (resumen->sobreimpulso_pct > LIMITE_SOBREIMPULSO_PCT) {
        fallos++;
    }
    if (fabsf(resumen->error_final_pct) > LIMITE_ERROR_FINAL_PCT) {
        fallos++;
    }

    return fallos;
}

/**
 * @brief Mide cuánto tarda el integrador en liberarse tras una saturación larga.
 *
 * Con la consigna fuera de alcance y la rueda bloqueada (medida 0) el duty se queda al tope. Al
 * pedir 0, un anti-windup correcto debe bajar el duty de inmediato; si el integrador se hubiera
 * quedado cargado, tardaría segundos.
 *
 * @return Pasos hasta que el duty baja de DUTY_LIBERADO_PCT, o -1 si no lo consigue
 */
static int medir_liberacion_integrador(void)
{
    Controller_t lazo;
    int pasos_liberacion = -1;

    CTRL_Init(&lazo, GANANCIA_KP, GANANCIA_KI, GANANCIA_KD, 100.0f);
    CTRL_SetRampa(&lazo, 100000.0f);
    CTRL_SetConsigna(&lazo, 3000.0f);

    for (int i = 0; i < PASOS_SATURACION_LARGA; i++) {
        (void)CTRL_Update(&lazo, 0.0f, PASO_S);
    }

    CTRL_SetConsigna(&lazo, 0.0f);

    for (int i = 0; i < PASOS_PARA_LIBERAR; i++) {
        if (CTRL_Update(&lazo, 0.0f, PASO_S) < DUTY_LIBERADO_PCT) {
            pasos_liberacion = i;
            break;
        }
    }

    return pasos_liberacion;
}

/** @brief Lanza las tres comprobaciones y resume el veredicto. */
int main(void)
{
    Resultado_t rampa;
    Resultado_t escalon;
    int fallos = 0;
    int pasos_liberacion;

    simular_lazo(300.0f, 600.0f, &rampa);
    imprimir_resultado("Rampa de consigna (0 a 300 mm/s)", &rampa);
    fallos += contar_fallos(&rampa);

    simular_lazo(300.0f, 100000.0f, &escalon);
    imprimir_resultado("Escalon puro de consigna", &escalon);
    fallos += contar_fallos(&escalon);

    pasos_liberacion = medir_liberacion_integrador();
    printf("\n--- Anti-windup tras 5 s saturado ---\n");

    if (pasos_liberacion < 0) {
        printf("  el integrador no se libera\n");
        fallos++;
    } else {
        printf("  el duty baja de %.0f %% en %d pasos (%.0f ms)\n",
               DUTY_LIBERADO_PCT, pasos_liberacion, (float)pasos_liberacion * PASO_S * 1000.0f);
    }

    printf("\n%s\n", (fallos == 0) ? "TODO OK" : "HAY FALLOS");

    return fallos;
}
