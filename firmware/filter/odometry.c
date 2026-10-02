#include "filter/odometry.h"

#include <math.h>
#include <stddef.h>

/**
 * @brief π propia, para no depender de `M_PI`.
 *
 * `M_PI` no está garantizado en todos los toolchains (con `-std=c99` estricto, glibc no lo define).
 */
#define ODOM_PI                 3.14159265358979f

/**
 * @brief $d\theta$ por debajo del cual el arco se trata como un tramo recto.
 *
 * Con valores más pequeños, dividir por $d\theta$ para sacar el radio pierde precisión y el resultado
 * del arco recto y el del círculo son indistinguibles (el error es del orden de $d\theta^2$).
 */
#define ODOM_DTHETA_EPS         1.0e-4f

/** @brief Trocha efectiva en uso [mm]; se cambia con ODOM_SetTrocha(). */
static float odom_trocha_mm = ODOM_TROCHA_MM_DEF;

/**
 * @brief Normaliza un ángulo a $[-\pi, \pi]$.
 *
 * Se hace con restas/sumas de $2\pi$ en vez de `fmodf()` porque los incrementos son pequeños y así se
 * evita el coste de la función de biblioteca.
 *
 * @param theta_rad Ángulo en radianes
 * @return Ángulo equivalente dentro de $[-\pi, \pi]$
 */
static float odom_normalizar_angulo(float theta_rad)
{
    while (theta_rad > ODOM_PI) {
        theta_rad -= 2.0f * ODOM_PI;
    }
    while (theta_rad < -ODOM_PI) {
        theta_rad += 2.0f * ODOM_PI;
    }

    return theta_rad;
}

void ODOM_Init(Odometry_t *odo, float x0_m, float y0_m, float theta0_rad)
{
    if (odo == NULL) {
        return;
    }

    odo->x        = x0_m;
    odo->y        = y0_m;
    odo->theta    = odom_normalizar_angulo(theta0_rad);
    odo->distance = 0.0f;
}

void ODOM_Reset(Odometry_t *odo)
{
    ODOM_Init(odo, 0.0f, 0.0f, 0.0f);
}

void ODOM_Update(Odometry_t *odo, float ds_left_m, float ds_right_m)
{
    float trocha_m;
    float ds_centro_m;
    float dtheta_rad;
    float radio_m;

    if (odo == NULL) {
        return;
    }
    if (odom_trocha_mm < 1.0f) {
        return;
    }

    trocha_m    = odom_trocha_mm / 1000.0f;
    ds_centro_m = 0.5f * (ds_right_m + ds_left_m);
    dtheta_rad  = (ds_right_m - ds_left_m) / trocha_m;

    odo->distance += fabsf(ds_centro_m);

    if (fabsf(dtheta_rad) < ODOM_DTHETA_EPS) {
        odo->x += ds_centro_m * cosf(odo->theta);
        odo->y += ds_centro_m * sinf(odo->theta);
    } else {
        radio_m = ds_centro_m / dtheta_rad;
        odo->x += radio_m * (sinf(odo->theta + dtheta_rad) - sinf(odo->theta));
        odo->y -= radio_m * (cosf(odo->theta + dtheta_rad) - cosf(odo->theta));
    }

    odo->theta = odom_normalizar_angulo(odo->theta + dtheta_rad);
}

void ODOM_SetTrocha(float trocha_mm)
{
    if (trocha_mm < 1.0f) {
        return;
    }

    odom_trocha_mm = trocha_mm;
}

float ODOM_GetTrocha(void)
{
    return odom_trocha_mm;
}
