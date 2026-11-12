#include "app/telemetry.h"
#include "app/format.h"
#include "hal/uart.h"
#include <stdio.h>

/* ------------------------------------------------------------------ */
void TELEM_SendDutyValues(uint16_t d1a, uint16_t d1b, uint16_t d2a, uint16_t d2b)
{
    char buf[96];
    int len = snprintf(buf, sizeof(buf),
                ">M8:%u\n>M9:%u\n>MA:%u\n>MB:%u\n",
                d1a, d1b, d2a, d2b);
    if (len > 0) UART_SendData((uint8_t *)buf, (uint16_t)len);
}

/* ------------------------------------------------------------------ */
void TELEM_SendMotorEncoder(int16_t enc1, int16_t speed1, int16_t enc2, int16_t speed2)
{
    char buf[96];
    int len = snprintf(buf, sizeof(buf),
                ">1A:%d\n>1B:%d\n>2A:%d\n>2B:%d\n",
                enc1, speed1, enc2, speed2);
    if (len > 0) UART_SendData((uint8_t *)buf, (uint16_t)len);
}

/* ------------------------------------------------------------------ */
void TELEM_SendEncoderPulses(uint32_t p1a, uint32_t p1b, uint32_t p2a, uint32_t p2b)
{
    char buf[96];
    int len = snprintf(buf, sizeof(buf),
                ">1A:%lu\n>1B:%lu\n>2A:%lu\n>2B:%lu\n",
                p1a, p1b, p2a, p2b);
    if (len > 0) UART_SendData((uint8_t *)buf, (uint16_t)len);
}

/* ------------------------------------------------------------------ */
void TELEM_SendString(const char *label, const char *value)
{
    char buf[64];
    int len = snprintf(buf, sizeof(buf), ">%s:%s\n", label, value);
    if (len > 0) UART_SendData((uint8_t *)buf, (uint16_t)len);
}

/* ------------------------------------------------------------------ */
/**
 * @brief Manda un valor en coma flotante con 3 decimales, en formato Teleplot.
 *
 * No usa `%f` a proposito: la conversion de newlib arrastra `_dtoa_r` y toda la
 * aritmetica de doble precision (11,2 KB de Flash medidos, ver `app/format.h`). El texto
 * que sale es **el mismo**, asi que Teleplot lo sigue leyendo como un numero.
 *
 * @param label Etiqueta de la traza, en mayusculas y sin acentos
 * @param value Valor a publicar
 */
void TELEM_SendFloat(const char *label, float value)
{
    char numero[24];
    char buf[64];
    int  len;

    if (FORMAT_Float(numero, sizeof(numero), value, 3U) == 0U) {
        return;
    }

    len = snprintf(buf, sizeof(buf), ">%s:%s\n", label, numero);
    if (len > 0) UART_SendData((uint8_t *)buf, (uint16_t)len);
}

/* ------------------------------------------------------------------ */
void TELEM_SendInt(const char *label, int32_t value)
{
    char buf[64];
    int len = snprintf(buf, sizeof(buf), ">%s:%ld\n", label, (long)value);
    if (len > 0) UART_SendData((uint8_t *)buf, (uint16_t)len);
}

/* ------------------------------------------------------------------ */
void TELEM_SendIRValues(const uint16_t *ir, uint8_t count)
{
    char buf[96];
    int pos = 0;
    for (uint8_t i = 0; i < count && i < 5; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        ">IR%u:%u\n", i, ir[i]);
    }
    if (pos > 0) UART_SendData((uint8_t *)buf, (uint16_t)pos);
}

/* ------------------------------------------------------------------ */
void TELEM_SendButton(uint8_t numero)
{
    char buf[32];
    int len = snprintf(buf, sizeof(buf), ">BTN:%u\n", numero);
    if (len > 0) UART_SendData((uint8_t *)buf, (uint16_t)len);
}

/* ------------------------------------------------------------------ */
/**
 * @brief Manda las seis magnitudes de la IMU, cada una con su precision.
 *
 * Aceleraciones con 2 decimales y velocidades angulares con 1, como antes, pero sin `%f`
 * (ver `app/format.h`).
 */
void TELEM_SendIMU(float ax, float ay, float az, float gx, float gy, float gz)
{
    char s_ax[16];
    char s_ay[16];
    char s_az[16];
    char s_gx[16];
    char s_gy[16];
    char s_gz[16];
    char buf[160];
    int  len;

    FORMAT_Float(s_ax, sizeof(s_ax), ax, 2U);
    FORMAT_Float(s_ay, sizeof(s_ay), ay, 2U);
    FORMAT_Float(s_az, sizeof(s_az), az, 2U);
    FORMAT_Float(s_gx, sizeof(s_gx), gx, 1U);
    FORMAT_Float(s_gy, sizeof(s_gy), gy, 1U);
    FORMAT_Float(s_gz, sizeof(s_gz), gz, 1U);

    len = snprintf(buf, sizeof(buf),
                   ">AX:%s\n>AY:%s\n>AZ:%s\n>GX:%s\n>GY:%s\n>GZ:%s\n",
                   s_ax, s_ay, s_az, s_gx, s_gy, s_gz);
    if (len > 0) UART_SendData((uint8_t *)buf, (uint16_t)len);
}
