#ifndef HW_IO_DEF_H
#define HW_IO_DEF_H

/*
 * Definiciones de pines y registros a nivel de hardware.
 *
 * Este archivo centraliza las direcciones de registros y bits
 * para acceso directo, complementando pinout.h.
 */

/* ------------------------------------------------------------------
 * Mapa de pines acelerado (índices para arrays)
 * ------------------------------------------------------------------ */

#define IO_IDX_LED         0
#define IO_IDX_MOTOR1_IN1  1
#define IO_IDX_MOTOR1_IN2  2
#define IO_IDX_MOTOR2_IN1  3
#define IO_IDX_MOTOR2_IN2  4
#define IO_IDX_BTN_MENU    5
#define IO_IDX_ENC1_A      6
#define IO_IDX_ENC1_B      7
#define IO_IDX_ENC2_A      8
#define IO_IDX_ENC2_B      9

/* ------------------------------------------------------------------
 * Máscaras rápidas predefinidas
 * ------------------------------------------------------------------ */

#define IO_MASK_LED        (GPIO_PIN_13)                 /* PC13 */
#define IO_MASK_MOTOR_IN1  (GPIO_PIN_8 | GPIO_PIN_10)   /* PA8, PA10 */
#define IO_MASK_MOTOR_IN2  (GPIO_PIN_9 | GPIO_PIN_11)   /* PA9, PA11 */

#endif /* HW_IO_DEF_H */
