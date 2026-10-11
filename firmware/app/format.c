#include "app/format.h"
#include <stdio.h>

/* ------------------------------------------------------------------ */
/**
 * @brief Escribe un numero en coma flotante como texto, sin usar el `%f` de `printf`.
 *
 * Hace la conversion con aritmetica entera: escala el valor por la potencia de diez que
 * toque, redondea, y separa parte entera y fraccion para escribirlas con `%lu` y `%0*lu`.
 * El texto que sale es el mismo que daria `%.Nf` para los valores de este robot.
 *
 * @param destino   Buffer de salida, terminado en '\0'
 * @param tam       Tamano del buffer, contando el terminador
 * @param valor     Numero a formatear
 * @param decimales Cifras decimales, de 0 a FORMAT_DECIMALES_MAX
 * @return Longitud del texto escrito (sin el terminador); 0 si no cabe
 *
 * @note El redondeo es al mas cercano y simetrico (1,5 -> 2 y -1,5 -> -2), y el cero
 *       negativo se normaliza a "0" para que no aparezca un "-0.00" en la pantalla.
 * @note El valor escalado se acota a +/-2e9 antes de pasarlo a entero: asi el cambio de
 *       signo no puede desbordar el `int32_t` y no hace falta aritmetica de 64 bits.
 */
uint8_t FORMAT_Float(char *destino, uint16_t tam, float valor, uint8_t decimales)
{
    uint32_t escala = 1U;
    float    escalado;
    int32_t  total;
    uint32_t magnitud;
    uint32_t parte_entera;
    uint32_t parte_frac;
    int      escrito;

    if ((destino == NULL) || (tam < 4U) || (decimales > FORMAT_DECIMALES_MAX)) {
        return 0U;
    }

    for (uint8_t i = 0U; i < decimales; i++) {
        escala *= 10U;
    }

    escalado = valor * (float)escala;
    if (escalado > 2.0e9f) {
        escalado = 2.0e9f;
    } else if (escalado < -2.0e9f) {
        escalado = -2.0e9f;
    }

    total    = (int32_t)((escalado >= 0.0f) ? (escalado + 0.5f) : (escalado - 0.5f));
    magnitud = (total < 0) ? (uint32_t)(-total) : (uint32_t)total;

    parte_entera = magnitud / escala;
    parte_frac   = magnitud % escala;

    if (decimales == 0U) {
        escrito = snprintf(destino, tam, "%s%lu",
                           (total < 0) ? "-" : "", (unsigned long)parte_entera);
    } else {
        escrito = snprintf(destino, tam, "%s%lu.%0*lu",
                           (total < 0) ? "-" : "",
                           (unsigned long)parte_entera,
                           (int)decimales, (unsigned long)parte_frac);
    }

    if ((escrito < 0) || (escrito >= (int)tam)) {
        destino[0] = '\0';
        return 0U;
    }

    return (uint8_t)escrito;
}
