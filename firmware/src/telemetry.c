#include <stdio.h>
#include "telemetry.h"
#include "uart.h"

void Send_Teleplot_DutyValues(uint16_t dutyA8, uint16_t dutyA9, uint16_t dutyA10, uint16_t dutyA11)
{
    char buffer[96];
    int len = snprintf(buffer, sizeof(buffer),
                ">M8:%u\n"
                ">M9:%u\n"
                ">MA:%u\n"
                ">MB:%u\n",
                dutyA8, dutyA9, dutyA10, dutyA11);
    if (len > 0) {
        HAL_UART_Transmit(&huart3, (uint8_t *)buffer, (uint16_t)len, HAL_MAX_DELAY);
    }
}

void Send_Teleplot_MotorEncoder(int16_t enc1, int16_t speed1, int16_t enc2, int16_t speed2)
{
    char buffer[96];
    int len = snprintf(buffer, sizeof(buffer),
                ">1A:%d\n"
                ">1B:%d\n"
                ">2A:%d\n"
                ">2B:%d\n",
                enc1, speed1, enc2, speed2);
    if (len > 0) {
        HAL_UART_Transmit(&huart3, (uint8_t *)buffer, (uint16_t)len, HAL_MAX_DELAY);
    }
}

void Send_Teleplot_EncoderPulses(uint32_t p1a, uint32_t p1b, uint32_t p2a, uint32_t p2b)
{
    char buffer[96];
    int len = snprintf(buffer, sizeof(buffer),
                ">1A:%lu\n"
                ">1B:%lu\n"
                ">2A:%lu\n"
                ">2B:%lu\n",
                p1a, p1b, p2a, p2b);
    if (len > 0) {
        HAL_UART_Transmit(&huart3, (uint8_t *)buffer, (uint16_t)len, HAL_MAX_DELAY);
    }
}
