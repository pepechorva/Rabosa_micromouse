/**
 * @file odometry_sim.c
 * @brief Banco de pruebas en el PC de la capa FILTER. NO se compila en el firmware.
 *
 * Valida la odometría (`filter/odometry.c`) con maniobras de resultado conocido, antes de empujar el
 * robot a mano en el banco (prueba 19). Comprueba que un tramo recto no gira, que un giro en el sitio
 * no traslada, que un cuadrado y un círculo **cierran** (vuelven al punto de partida) y que $\theta$
 * se queda siempre en $[-\pi, \pi]$.
 *
 * Compilar y ejecutar desde la raíz del repositorio:
 * @code
 *   gcc -I firmware -O2 -Wall -o /tmp/odometry_sim firmware/test/odometry_sim.c \
 *       firmware/filter/odometry.c -lm
 *   /tmp/odometry_sim
 * @endcode
 *
 * @note Las maniobras se construyen con incrementos de rueda puros, así que el cierre es exacto salvo
 *       el ruido de `float`: si aquí fallara, el problema está en la integración, no en el robot.
 */

#include <stdio.h>
#include <math.h>
#include "filter/odometry.h"

#define TOL_DIST_M          1.0e-3f     /* 1 mm: es ruido de float, no error de método */
#define TOL_ANGULO_DEG      0.1f

#define TROCHA_MM           ODOM_TROCHA_MM_DEF

#define PASOS_RECTO         200         /* 1 m en pasos de 5 mm                        */
#define LARGO_LADO_M        0.300f
#define PASOS_LADO          100
#define PASOS_GIRO_90       50
#define RADIO_CIRCULO_M     0.300f
#define PASOS_CIRCULO       72          /* 5° por paso = una vuelta                    */
#define VUELTAS_NORMALIZA   10.0f

/** @brief Error de cierre de una maniobra y datos de contexto. */
typedef struct {
    float error_pos_mm;     /**< Distancia entre la pose final y la esperada  */
    float error_theta_deg;  /**< Diferencia de orientación final              */
    float camino_m;         /**< Camino recorrido según la odometría          */
    float theta_final_deg;  /**< Orientación final, para presentarla          */
} Resultado_t;

/**
 * @brief Avanza en línea recta repartiendo la distancia entre las dos ruedas.
 *
 * @param odo Puntero a la odometría
 * @param distancia_m Distancia a recorrer [m]
 * @param pasos Número de incrementos con los que se recorre
 */
static void avanzar(Odometry_t *odo, float distancia_m, int pasos)
{
    float ds_m = distancia_m / (float)pasos;

    for (int i = 0; i < pasos; i++) {
        ODOM_Update(odo, ds_m, ds_m);
    }
}

/**
 * @brief Gira sobre sí mismo repartiendo el ángulo entre las dos ruedas.
 *
 * Un giro en el sitio son dos incrementos de rueda iguales y opuestos: el centro del robot no se
 * mueve, así que los dos deben ser la mitad del arco que describe cada rueda.
 *
 * @param odo Puntero a la odometría
 * @param angulo_deg Ángulo a girar [°], positivo a la izquierda
 * @param pasos Número de incrementos con los que se gira
 */
static void girar_en_sitio(Odometry_t *odo, float angulo_deg, int pasos)
{
    float trocha_m = TROCHA_MM / 1000.0f;
    float dtheta_rad = (angulo_deg * (float)M_PI / 180.0f) / (float)pasos;
    float ds_rueda_m = 0.5f * trocha_m * dtheta_rad;

    for (int i = 0; i < pasos; i++) {
        ODOM_Update(odo, -ds_rueda_m, ds_rueda_m);
    }
}

/**
 * @brief Recorre un arco de radio conocido describiéndolo con incrementos de rueda.
 *
 * Cada paso es un arco de curvatura constante, así que la composición de los pasos debe devolver el
 * robot al punto de partida cuando el total sea una vuelta completa.
 *
 * @param odo Puntero a la odometría
 * @param radio_m Radio del arco que describe el CENTRO del robot [m]
 * @param pasos Número de incrementos (el ángulo total sale del reparto de una vuelta)
 */
static void recorrer_circulo(Odometry_t *odo, float radio_m, int pasos)
{
    float trocha_m = TROCHA_MM / 1000.0f;
    float dtheta_rad = 2.0f * (float)M_PI / (float)pasos;
    float ds_centro_m = radio_m * dtheta_rad;

    for (int i = 0; i < pasos; i++) {
        ODOM_Update(odo,
                    ds_centro_m - 0.5f * trocha_m * dtheta_rad,
                    ds_centro_m + 0.5f * trocha_m * dtheta_rad);
    }
}

/**
 * @brief Compara la pose final con la esperada y deja el resultado en la estructura.
 *
 * @param odo Odometría ya movida por la maniobra
 * @param x_esp_m Posición X esperada [m]
 * @param y_esp_m Posición Y esperada [m]
 * @param theta_esp_deg Orientación esperada [°]
 * @param resumen Estructura donde se deja el resultado
 */
static void calcular_error(const Odometry_t *odo, float x_esp_m, float y_esp_m,
                           float theta_esp_deg, Resultado_t *resumen)
{
    float dx_mm = (odo->x - x_esp_m) * 1000.0f;
    float dy_mm = (odo->y - y_esp_m) * 1000.0f;
    float dtheta_deg = (odo->theta * 180.0f / (float)M_PI) - theta_esp_deg;

    resumen->error_pos_mm    = sqrtf(dx_mm * dx_mm + dy_mm * dy_mm);
    resumen->error_theta_deg = fabsf(dtheta_deg);
    resumen->camino_m        = odo->distance;
    resumen->theta_final_deg = odo->theta * 180.0f / (float)M_PI;
}

/**
 * @brief Escribe en la salida estándar el resultado de una maniobra.
 *
 * @param titulo Nombre de la maniobra
 * @param resumen Resultado a presentar
 * @param detalles Texto con lo esperado, para poder compararlo de un vistazo
 */
static void imprimir_resultado(const char *titulo, const Resultado_t *resumen, const char *detalles)
{
    printf("\n--- %s ---\n", titulo);
    printf("  esperado      : %s\n", detalles);
    printf("  error de cierre: %6.2f mm  ·  %6.2f °  (límite %.1f / %.1f)\n",
           resumen->error_pos_mm, resumen->error_theta_deg, TOL_DIST_M * 1000.0f, TOL_ANGULO_DEG);
    printf("  camino         : %6.3f m  ·  theta final %7.1f °\n",
           resumen->camino_m, resumen->theta_final_deg);
}

/**
 * @brief Cuenta los criterios incumplidos por una maniobra.
 *
 * @param resumen Resultado a comprobar
 * @return Número de fallos (0 = todo bien)
 */
static int contar_fallos(const Resultado_t *resumen)
{
    int fallos = 0;

    if (resumen->error_pos_mm > (TOL_DIST_M * 1000.0f)) {
        fallos++;
    }
    if (resumen->error_theta_deg > TOL_ANGULO_DEG) {
        fallos++;
    }

    return fallos;
}

/**
 * @brief Gira varias vueltas comprobando que $\theta$ nunca se sale de $[-\pi, \pi]$.
 *
 * @return Número de veces que la orientación se salió del rango
 */
static int contar_angulos_sin_normalizar(void)
{
    Odometry_t odo;
    int fuera_de_rango = 0;

    ODOM_Init(&odo, 0.0f, 0.0f, 0.0f);

    for (int i = 0; i < (int)(VUELTAS_NORMALIZA * 100.0f); i++) {
        girar_en_sitio(&odo, 360.0f / 100.0f, 1);

        if ((odo.theta > (float)M_PI) || (odo.theta < -(float)M_PI)) {
            fuera_de_rango++;
        }
    }

    return fuera_de_rango;
}

/** @brief Lanza las cuatro maniobras y resume el veredicto. */
int main(void)
{
    Odometry_t odo;
    Resultado_t recta;
    Resultado_t cuadrado;
    Resultado_t circulo;
    int fallos = 0;
    int fuera_de_rango;

    ODOM_SetTrocha(TROCHA_MM);

    printf("Odometría diferencial · trocha de %.1f mm\n", (double)ODOM_GetTrocha());

    ODOM_Init(&odo, 0.0f, 0.0f, 0.0f);
    avanzar(&odo, 1.0f, PASOS_RECTO);
    calcular_error(&odo, 1.0f, 0.0f, 0.0f, &recta);
    imprimir_resultado("Recto 1 m", &recta, "x = 1000 mm · y = 0 · theta = 0");
    fallos += contar_fallos(&recta);

    ODOM_Init(&odo, 0.0f, 0.0f, 0.0f);
    for (int lado = 0; lado < 4; lado++) {
        avanzar(&odo, LARGO_LADO_M, PASOS_LADO);
        girar_en_sitio(&odo, 90.0f, PASOS_GIRO_90);
    }
    calcular_error(&odo, 0.0f, 0.0f, 0.0f, &cuadrado);
    imprimir_resultado("Cuadrado de 4 lados + 4 giros de 90°", &cuadrado,
                       "vuelve al origen · 4 x 300 mm de camino");
    fallos += contar_fallos(&cuadrado);

    ODOM_Init(&odo, 0.0f, 0.0f, 0.0f);
    recorrer_circulo(&odo, RADIO_CIRCULO_M, PASOS_CIRCULO);
    calcular_error(&odo, 0.0f, 0.0f, 0.0f, &circulo);
    imprimir_resultado("Círculo de radio 300 mm (el centro)", &circulo,
                       "vuelve al origen · 1885 mm de camino");
    fallos += contar_fallos(&circulo);

    fuera_de_rango = contar_angulos_sin_normalizar();
    printf("\n--- Normalización de theta (10 vueltas) ---\n");

    if (fuera_de_rango > 0) {
        printf("  theta se salió de [-180°, 180°] en %d lecturas\n", fuera_de_rango);
        fallos++;
    } else {
        printf("  theta siempre dentro de [-180°, 180°]\n");
    }

    printf("\n%s\n", (fallos == 0) ? "TODO OK" : "HAY FALLOS");

    return fallos;
}
