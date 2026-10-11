#ifndef APP_FORMAT_H
#define APP_FORMAT_H

#include <stdint.h>

/*
 * Formateo de numeros en texto SIN la coma flotante de printf.
 *
 * El `%f` de newlib arrastra la conversion `_dtoa_r` (2,9 KB) y, por la promocion de
 * argumentos a `double`, toda la aritmetica de doble precision (~6 KB mas). Medido el
 * 2026-10-10 en este firmware: **11,2 KB de Flash**, el 17 % del binario, para escribir
 * numeros de dos o tres decimales.
 *
 * Esta es la alternativa, para la telemetria y para las pantallas de la OLED.
 */

/** Maximo de cifras decimales que admite FORMAT_Float(). */
#define FORMAT_DECIMALES_MAX 4U

uint8_t FORMAT_Float(char *destino, uint16_t tam, float valor, uint8_t decimales);

#endif /* APP_FORMAT_H */
