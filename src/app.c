#include "app.h"
#include "pwm.h"
#include "uart.h"
#include "telemetry.h"

void App_RunLoop(void)
{
    uint16_t PWMMotor1A = 0;
    uint16_t PWMMotor1B = 0;
    uint16_t PWMMotor2A = 0;
    uint16_t PWMMotor2B = 0;

    int8_t activeLED = 1;
    int8_t direction = 1;
    uint32_t moveCounter = 0;

    while (1)
    {
        PWMMotor1A = (uint16_t)(PWMMotor1A * 0.85);
        PWMMotor1B = (uint16_t)(PWMMotor1B * 0.85);
        PWMMotor2A = (uint16_t)(PWMMotor2A * 0.85);
        PWMMotor2B = (uint16_t)(PWMMotor2B * 0.85);

        moveCounter++;
        if (moveCounter >= 12)
        {
            moveCounter = 0;
            activeLED += direction;

            if (activeLED >= 4) {
                activeLED = 4;
                direction = -1;
            }
            else if (activeLED <= 1) {
                activeLED = 1;
                direction = 1;
            }
        }

        if (activeLED == 1) PWMMotor1A = 1000;
        if (activeLED == 2) PWMMotor1B = 1000;
        if (activeLED == 3) PWMMotor2A = 1000;
        if (activeLED == 4) PWMMotor2B = 1000;

        PWM_SetDuties(PWMMotor1A, PWMMotor1B, PWMMotor2A, PWMMotor2B);
        Send_Teleplot_DutyValues(PWMMotor1A, PWMMotor1B, PWMMotor2A, PWMMotor2B);

        HAL_Delay(10);
    }
}
