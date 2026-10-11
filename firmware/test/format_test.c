/*
 * Banco del PC para app/format.c
 *
 * Comprueba FORMAT_Float() contra el `%.Nf` de printf en el PC. El refactor del 2026-10-10
 * cambio el formateo de **toda** la telemetria y de **todas** las pantallas de la OLED, asi
 * que conviene saber que el texto que sale es el mismo antes de flashear.
 *
 * Compilar y ejecutar (desde la raiz del repositorio):
 *
 *     gcc -I firmware -o /tmp/format_test firmware/test/format_test.c \
 *         firmware/app/format.c -lm && /tmp/format_test
 *
 * OJO: el `%.Nf` de printf solo se comporta igual porque aqui no se pasa por newlib-nano.
 * Este banco valida **la aritmetica**, no el enlazado del firmware: eso lo dice el mapa de
 * enlace (que `_dtoa_r` ya no aparece).
 */

#include "app/format.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Casos de prueba: normales, negativos, el cero, empates de redondeo y fronteras. */
static const float CASOS[] = {
    0.0f,     -0.0f,    7.394f,   -0.001f, 123.456f, -12.345f, 0.5f,    -0.5f,
    99.999f,  1000.0f,  0.3015f,  -7.775f, 1.05f,    -1.05f,   0.005f,  -0.005f,
    269.35f,  180.0f,   -180.0f,  0.0f,
};

/**
 * @brief Compara los dos textos como numeros, no como cadenas.
 *
 * La comparacion literal no vale: `printf` redondea **al par** y FORMAT_Float **hacia
 * arriba**, asi que en los empates exactos (`0.5`, `1.05`, `0.005`) discrepan. Eso es
 * correcto y no importa. Lo que hay que garantizar es que el numero coincide dentro de
 * **una unidad de la ultima cifra decimal**, que es el ruido maximo de comparar dos
 * redondeos legitimos del mismo valor (uno sube y el otro baja). Un error de verdad —un
 * signo cambiado, un decimal de mas, truncar en vez de redondear— se sale muy por encima.
 *
 * @param mio Texto de FORMAT_Float
 * @param ref Texto de printf
 * @param dec Cifras decimales de la prueba
 * @return 0 si coinciden dentro de la tolerancia; 1 si no; 2 si aparece un cero negativo
 */
static int difiere(const char *mio, const char *ref, unsigned dec)
{
    float va  = strtof(mio, NULL);
    float vb  = strtof(ref, NULL);
    float tol = 1.0f;

    for (unsigned i = 0U; i < dec; i++) {
        tol /= 10.0f;
    }

    /* El margen del 1 % cubre el redondeo de la propia resta en coma flotante: en float,
       `1.1f - 1.0f` vale 0,100000024 y no 0,1 exacto. */
    if (fabsf(va - vb) > (tol * 1.01f)) {
        return 1;
    }

    /* Regla propia, y la razon de que esta funcion exista: el cero NUNCA lleva signo,
       para que no aparezca un "-0.00" en la pantalla de la OLED. */
    if ((mio[0] == '-') && (va == 0.0f)) {
        return 2;
    }

    return 0;
}

/**
 * @brief Pasa el banco de pruebas de `FORMAT_Float()` contra `printf`.
 *
 * @return Numero de discrepancias reales (0 = todo bien)
 */
int main(void)
{
    int fallos = 0;
    int pruebas = 0;

    printf("FORMAT_Float  contra  %%.Nf  de printf\n\n");

    for (unsigned i = 0U; i < sizeof(CASOS) / sizeof(CASOS[0]); i++) {
        for (unsigned dec = 0U; dec <= FORMAT_DECIMALES_MAX; dec++) {
            char mio[24];
            char ref[24];
            int  mal;

            pruebas++;
            FORMAT_Float(mio, sizeof(mio), CASOS[i], (uint8_t)dec);
            snprintf(ref, sizeof(ref), "%.*f", (int)dec, (double)CASOS[i]);

            mal = difiere(mio, ref, dec);
            if (mal != 0) {
                const char *que = (mal == 2) ? "CERO NEGATIVO (no debe salir)"
                                             : "FUERA DE TOLERANCIA";
                printf("  MAL  %12.4f con %u decimales:  mio=\"%s\"  printf=\"%s\"  <- %s\n",
                       (double)CASOS[i], dec, mio, ref, que);
                fallos++;
            }
        }
    }

    printf("\n  %d pruebas, %d fallos\n", pruebas, fallos);
    if (fallos != 0) {
        printf("  (en los empates exactos, printf redondea al par y FORMAT_Float hacia\n"
               "   arriba: la tolerancia lo admite, asi que no deberia salir ninguno)\n");
    }

    return (fallos != 0) ? 1 : 0;
}

