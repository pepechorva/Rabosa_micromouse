#include "app/oled.h"
#include "hal/i2c.h"
#include "hal/timers.h"

#include <string.h>

/* ==================================================================
 * Driver mínimo de SSD1306 128×32 por I2C1.
 *
 * OJO con las direcciones: el HAL de STM32 quiere la dirección de 8 bits,
 * es decir la de 7 bits desplazada a la izquierda (0x3C << 1 = 0x78). Se
 * prueban las dos típicas del SSD1306 (0x3C y 0x3D) al arrancar.
 * ================================================================== */

#define OLED_ADDR8_1    (0x3C << 1)
#define OLED_ADDR8_2    (0x3D << 1)

#define OLED_PAGINAS    (OLED_ALTO / 8)                    /* 32 px = 4 páginas */
#define OLED_BUF_SIZE   (OLED_ANCHO * OLED_PAGINAS)        /* 512 B */

/* Control bytes del SSD1306: lo que sigue son comandos (0x00) o datos (0x40) */
#define OLED_CTRL_CMD   0x00
#define OLED_CTRL_DATA  0x40

/* Tamaño máximo de datos por transmisión I2C (1 byte de control + n) */
#define OLED_CHUNK      32

/* ------------------------------------------------------------------
 * Orientación de la pantalla.
 *
 *   0 = como viene de fábrica   ·   1 = girada 180°
 *
 * Girarla son dos comandos del SSD1306, y hay que cambiar LOS DOS:
 *   - remapeo de segmentos: 0xA1 (por defecto) ↔ 0xA0 (invertido)
 *   - barrido de COM:       0xC8 (por defecto) ↔ 0xC0 (invertido)
 * Con los dos invertidos, la imagen queda girada 180° y las coordenadas del
 * framebuffer siguen siendo las mismas (0,0 = lo que el usuario ve arriba a la
 * izquierda), así que no hay que tocar nada del dibujo.
 * ------------------------------------------------------------------ */
#define OLED_ROTACION_180   1

#if OLED_ROTACION_180
  #define OLED_SEG_REMAP    0xA0U   /* segmentos invertidos */
  #define OLED_COM_SCAN     0xC0U   /* COM de arriba hacia abajo */
#else
  #define OLED_SEG_REMAP    0xA1U   /* segmentos normales */
  #define OLED_COM_SCAN     0xC8U   /* COM de abajo hacia arriba */
#endif

static uint8_t  oled_buf[OLED_BUF_SIZE];
static uint16_t oled_addr8 = 0;     /* 0 = pantalla no detectada */

/* ------------------------------------------------------------------
 * Fuente 5×7: un byte por columna, bit0 = fila de arriba (bit6 = abajo).
 * Solo los glifos que se usan; el orden debe coincidir con oled_chars[].
 * ------------------------------------------------------------------ */
static const char oled_chars[] = " 0123456789ABCDEIKLMNOPSTUW-:V.FGRX+/>_HJQYZ";

static const uint8_t oled_font[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00},   /* ' ' */
    {0x3E, 0x51, 0x49, 0x45, 0x3E},   /* '0' */
    {0x00, 0x42, 0x7F, 0x40, 0x00},   /* '1' */
    {0x42, 0x61, 0x51, 0x49, 0x46},   /* '2' */
    {0x21, 0x41, 0x45, 0x4B, 0x31},   /* '3' */
    {0x18, 0x14, 0x12, 0x7F, 0x10},   /* '4' */
    {0x27, 0x45, 0x45, 0x45, 0x39},   /* '5' */
    {0x3C, 0x4A, 0x49, 0x49, 0x30},   /* '6' */
    {0x01, 0x71, 0x09, 0x05, 0x03},   /* '7' */
    {0x36, 0x49, 0x49, 0x49, 0x36},   /* '8' */
    {0x06, 0x49, 0x49, 0x29, 0x1E},   /* '9' */
    {0x7E, 0x11, 0x11, 0x11, 0x7E},   /* 'A' */
    {0x7F, 0x49, 0x49, 0x49, 0x36},   /* 'B' */
    {0x3E, 0x41, 0x41, 0x41, 0x22},   /* 'C' */
    {0x7F, 0x41, 0x41, 0x22, 0x1C},   /* 'D' */
    {0x7F, 0x49, 0x49, 0x49, 0x41},   /* 'E' */
    {0x00, 0x41, 0x7F, 0x41, 0x00},   /* 'I' */
    {0x7F, 0x08, 0x14, 0x22, 0x41},   /* 'K' */
    {0x7F, 0x40, 0x40, 0x40, 0x40},   /* 'L' */
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},   /* 'M' */
    {0x7F, 0x04, 0x08, 0x10, 0x7F},   /* 'N' */
    {0x3E, 0x41, 0x41, 0x41, 0x3E},   /* 'O' */
    {0x7F, 0x09, 0x09, 0x09, 0x06},   /* 'P' */
    {0x46, 0x49, 0x49, 0x49, 0x31},   /* 'S' */
    {0x01, 0x01, 0x7F, 0x01, 0x01},   /* 'T' */
    {0x3F, 0x40, 0x40, 0x40, 0x3F},   /* 'U' */
    {0x7F, 0x20, 0x18, 0x20, 0x7F},   /* 'W' */
    {0x08, 0x08, 0x08, 0x08, 0x08},   /* '-' */
    {0x00, 0x36, 0x36, 0x00, 0x00},   /* ':' */
    {0x1F, 0x20, 0x40, 0x20, 0x1F},   /* 'V' */
    {0x00, 0x60, 0x60, 0x00, 0x00},   /* '.' */
    {0x7F, 0x09, 0x09, 0x09, 0x01},   /* 'F'  (INFO, FLOOD)   */
    {0x3E, 0x41, 0x49, 0x49, 0x7A},   /* 'G'  (DIAG)          */
    {0x7F, 0x09, 0x19, 0x29, 0x46},   /* 'R'  (RUN, IR, DER)  */
    {0x63, 0x14, 0x08, 0x14, 0x63},   /* 'X'                  */
    {0x08, 0x08, 0x3E, 0x08, 0x08},   /* '+'                  */
    {0x20, 0x10, 0x08, 0x04, 0x02},   /* '/'                  */
    {0x41, 0x22, 0x14, 0x08, 0x00},   /* '>'  (cursor de menú) */
    {0x40, 0x40, 0x40, 0x40, 0x40},   /* '_'  (va en >CMD_ERR, >PID_KP...) */
    /* Las cinco que se anadieron el 2026-10-11 para el menu nuevo: sin ellas no se pueden
       escribir DERECHA, IZQUIERDA ni AJUSTE. Van al final a proposito: el orden de esta
       tabla tiene que coincidir con oled_chars[], y reordenar la cadena entera es un cambio
       que el compilador NO comprueba glifo a glifo (el typedef solo cuenta). */
    {0x7F, 0x08, 0x08, 0x08, 0x7F},   /* 'H'  (DERECHA)   */
    {0x20, 0x40, 0x41, 0x3F, 0x01},   /* 'J'  (AJUSTE)    */
    {0x3E, 0x41, 0x51, 0x21, 0x5E},   /* 'Q'  (IZQUIERDA) */
    {0x07, 0x08, 0x70, 0x08, 0x07},   /* 'Y'              */
    {0x61, 0x51, 0x49, 0x45, 0x43}    /* 'Z'  (IZQUIERDA) */
};

/* Comprobación en tiempo de compilación: tiene que haber un glifo por cada
 * carácter de oled_chars[] (si no, se leería fuera de la tabla). El typedef
 * con tamaño negativo hace fallar la compilación. */
typedef char oled_font_chars_coinciden[
    ((sizeof(oled_font) / sizeof(oled_font[0])) == (sizeof(oled_chars) - 1U)) ? 1 : -1];

/* ==================================================================
 * Bajo nivel: comandos y datos por I2C
 * ================================================================== */

static void oled_cmd(uint8_t cmd)
{
    uint8_t buf[2];

    if (oled_addr8 == 0) return;

    buf[0] = OLED_CTRL_CMD;
    buf[1] = cmd;
    (void)I2C_WriteRaw(oled_addr8, buf, 2);
}

static void oled_data(const uint8_t *datos, uint16_t len)
{
    uint8_t  buf[OLED_CHUNK + 1];
    uint16_t enviados = 0;

    if (oled_addr8 == 0) return;

    while (enviados < len) {
        uint16_t n = (uint16_t)(len - enviados);
        if (n > OLED_CHUNK) n = OLED_CHUNK;

        buf[0] = OLED_CTRL_DATA;
        memcpy(&buf[1], &datos[enviados], n);
        (void)I2C_WriteRaw(oled_addr8, buf, (uint16_t)(n + 1));

        enviados = (uint16_t)(enviados + n);
    }
}

/* ==================================================================
 * API
 * ================================================================== */

void OLED_Init(void)
{
    static const uint8_t secuencia[] = {
        0xAE,               /* display off */
        0xD5, 0x80,         /* reloj del oscilador */
        0xA8, 0x1F,         /* multiplex: 32 filas (128×32) */
        0xD3, 0x00,         /* desplazamiento vertical */
        0x40,               /* línea de inicio = 0 */
        0x8D, 0x14,         /* charge pump ON (alimentación interna) */
        0x20, 0x00,         /* direccionamiento por páginas */
        OLED_SEG_REMAP,     /* remapeo de segmentos (según OLED_ROTACION_180) */
        OLED_COM_SCAN,      /* sentido de barrido de COM (según OLED_ROTACION_180) */
        0xDA, 0x02,         /* pines COM: configuración de 128×32 */
        0x81, 0x8F,         /* contraste */
        0xD9, 0xF1,         /* precharge */
        0xDB, 0x40,         /* VCOMH */
        0xA4,               /* mostrar el contenido de la RAM */
        0xA6,               /* no invertido */
        0xAF                /* display on */
    };

    /* ¿Está la pantalla? Si no responde, el driver se queda mudo. */
    if (oled_addr8 == 0) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, OLED_ADDR8_1, 3, 100) == HAL_OK) {
            oled_addr8 = OLED_ADDR8_1;
        } else if (HAL_I2C_IsDeviceReady(&hi2c1, OLED_ADDR8_2, 3, 100) == HAL_OK) {
            oled_addr8 = OLED_ADDR8_2;
        }
    }
    if (oled_addr8 == 0) return;

    for (uint32_t i = 0; i < sizeof(secuencia); i++) {
        oled_cmd(secuencia[i]);
    }

    OLED_Clear();
    OLED_Update();
}

uint8_t OLED_Detected(void)
{
    return (oled_addr8 != 0) ? 1U : 0U;
}

void OLED_Clear(void)
{
    memset(oled_buf, 0, sizeof(oled_buf));
}

/* ------------------------------------------------------------------ */
static void oled_pixel(uint8_t x, uint8_t y)
{
    if (x >= OLED_ANCHO || y >= OLED_ALTO) return;

    uint16_t idx = (uint16_t)((y / 8U) * OLED_ANCHO + x);
    oled_buf[idx] |= (uint8_t)(1U << (y % 8U));
}

void OLED_Update(void)
{
    uint16_t t0;

    if (oled_addr8 == 0) return;

    /* Cronometrado porque **bloquea**: son 528 bytes por I²C a `I2C1_CLOCK_HZ`, o sea ~47 ms a
       100 kHz. Dentro de un bucle de control eso es el lazo entero sin correr, así que conviene
       tenerlo medido y no estimado (orden `LAZO` de la consola). */
    t0 = TIM_Cuentas();

    for (uint8_t pagina = 0; pagina < OLED_PAGINAS; pagina++) {
        oled_cmd((uint8_t)(0xB0 | pagina));     /* página 0..3 */
        oled_cmd(0x00);                         /* columna baja = 0 */
        oled_cmd(0x10);                         /* columna alta = 0 */
        oled_data(&oled_buf[pagina * OLED_ANCHO], OLED_ANCHO);
    }

    TIM_Cronometrar(TIM_TRAMO_OLED, t0);
}

/* ------------------------------------------------------------------ */
static const uint8_t *oled_glyph(char c)
{
    for (uint8_t i = 0; i < (uint8_t)(sizeof(oled_chars) - 1U); i++) {
        if (oled_chars[i] == c) {
            return oled_font[i];
        }
    }
    return NULL;    /* sin glifo: se deja en blanco */
}

/* Ancho en píxeles de un texto: 5 de glifo + 1 de hueco, sin el último hueco */
static uint16_t oled_ancho_texto(const char *texto, uint8_t escala)
{
    uint16_t n = 0;

    while (texto[n] != '\0') n++;
    if (n == 0) return 0;

    return (uint16_t)((n * 6U - 1U) * escala);
}

void OLED_Print(uint8_t x, uint8_t y, uint8_t escala, const char *texto)
{
    uint16_t x0 = x;

    if (escala == 0) escala = 1;

    for (const char *p = texto; *p != '\0'; p++) {
        const uint8_t *g = oled_glyph(*p);

        if (x0 + 6U * escala > OLED_ANCHO) break;   /* no salirse por la derecha */

        if (g != NULL) {
            for (uint8_t col = 0; col < 5; col++) {
                for (uint8_t fila = 0; fila < 7; fila++) {
                    if (g[col] & (1U << fila)) {
                        /* Cada punto del glifo se convierte en escala×escala */
                        for (uint8_t dy = 0; dy < escala; dy++) {
                            for (uint8_t dx = 0; dx < escala; dx++) {
                                oled_pixel((uint8_t)(x0 + col * escala + dx),
                                           (uint8_t)(y + fila * escala + dy));
                            }
                        }
                    }
                }
            }
        }

        x0 += 6U * escala;
    }
}

void OLED_PrintCentrado(uint8_t y, uint8_t escala, const char *texto)
{
    uint16_t ancho = oled_ancho_texto(texto, escala);
    uint8_t  x = 0;

    if (ancho < OLED_ANCHO) {
        x = (uint8_t)((OLED_ANCHO - ancho) / 2U);
    }

    OLED_Print(x, y, escala, texto);
}
