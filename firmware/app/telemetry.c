#include "app/telemetry.h"
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
void TELEM_SendFloat(const char *label, float value)
{
    char buf[64];
    int len = snprintf(buf, sizeof(buf), ">%s:%.3f\n", label, (double)value);
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
void TELEM_SendIMU(float ax, float ay, float az, float gx, float gy, float gz)
{
    char buf[128];
    int len = snprintf(buf, sizeof(buf),
                ">AX:%.2f\n>AY:%.2f\n>AZ:%.2f\n>GX:%.1f\n>GY:%.1f\n>GZ:%.1f\n",
                (double)ax, (double)ay, (double)az,
                (double)gx, (double)gy, (double)gz);
    if (len > 0) UART_SendData((uint8_t *)buf, (uint16_t)len);
}
