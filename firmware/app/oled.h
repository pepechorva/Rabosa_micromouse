#ifndef APP_OLED_H
#define APP_OLED_H

#include <stdint.h>

/*
 * Capa APP — pantalla OLED SSD1306 128×32 por I2C1 (PB6 = SCL, PB7 = SDA).
 *
 * Se dibuja en un framebuffer de RAM (512 B) y OLED_Update() lo manda entero:
 * montar la pantalla no toca el bus, y se refresca cuando interesa.
 *
 * Fuente de 5×7 px + 1 de separación (6×8 por carácter) con escalado entero
 * (escala 1 = 6×8 px · escala 2 = 12×16 px · …). En 128×32 caben:
 *   - 4 líneas de texto a escala 1 (8 px cada una)
 *   - 2 líneas a escala 2 (16 px cada una) ← lo que usa la prueba de botones
 *
 * ⚠️ Están definidos los glifos de la tabla de `oled.c`: el espacio, los dígitos 0-9 y las letras
 * **A–Z al completo** (H, J, Q, Y y Z se añadieron el 2026-10-11). Faltan las **minúsculas**, los
 * **acentos** y la **coma**, así que todo texto de pantalla va en mayúsculas y sin acentos. El `>`
 * es el cursor del menú. Cualquier otro carácter se dibuja en blanco: si hace falta otro glifo, se
 * añade a esa tabla (5 bytes por glifo, y hay un typedef que comprueba que las dos cuadran).
 */

#define OLED_ANCHO   128
#define OLED_ALTO    32

/**
 * @brief Inicializa la pantalla (I2C1 debe estar listo: MX_I2C1_Init()).
 *        Busca el módulo en 0x3C y, si no responde, en 0x3D. Si no aparece,
 *        deja el driver en modo mudo: todas las funciones hacen no-op y
 *        OLED_Detected() devuelve 0.
 */
void OLED_Init(void);

/** @return 1 si la pantalla respondió al inicializar, 0 si no. */
uint8_t OLED_Detected(void);

/** @brief Borra el framebuffer (no lo envía: hace falta OLED_Update()). */
void OLED_Clear(void);

/** @brief Envía el framebuffer completo a la pantalla. */
void OLED_Update(void);

/**
 * @brief Dibuja texto en el framebuffer.
 * @param x       Columna inicial (0 = izquierda)
 * @param y       Fila inicial (0 = arriba); admite valores no múltiplos de 8
 * @param escala  1, 2, 3… (0 se trata como 1)
 * @param texto   Cadena ASCII (los caracteres sin glifo salen en blanco)
 */
void OLED_Print(uint8_t x, uint8_t y, uint8_t escala, const char *texto);

/** @brief Igual que OLED_Print(), pero centrado horizontalmente. */
void OLED_PrintCentrado(uint8_t y, uint8_t escala, const char *texto);

#endif /* APP_OLED_H */
