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
        HAL_UART_Transmit(&huart2, (uint8_t *)buffer, (uint16_t)len, HAL_MAX_DELAY);
    }
}
