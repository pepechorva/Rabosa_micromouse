#ifndef CORE_CORE_IO_H
#define CORE_CORE_IO_H

#include <stdint.h>

/*
 * CONTRATO entre la capa de APLICACION (`core/`) y la de DISPOSITIVO (`drivers/`).
 *
 * Esta cabecera es **la frontera**. Todo lo que la aplicacion necesita saber del robot
 * esta aqui, y nada mas: ni un registro del STM32, ni un pin, ni un perfil.
 *
 *   core/       la aplicacion: perfiles, movimiento, mapa, navegacion.
 *               ==> NO incluye nada de `hal/`, ni de `drivers/`, ni de `sensor/`.
 *   drivers/    los dispositivos: motores, encoders, sensores de pared, reloj.
 *               ==> implementa ESTAS funciones.
 *
 * La regla, entonces, no es «el core llama a los drivers» sino: **quien llama es el que
 * implementa el contrato**. La aplicacion pide «velocidad de la rueda izquierda» y no le
 * importa si detras hay un TIM2 en modo encoder o un sensor magnetico.
 *
 * @note **Se gana tres cosas**: la aplicacion se puede **compilar y probar en el PC** sin
 *       el STM32 (como ya se hace con `profile`), se puede **auditar** sin leer la HAL, y
 *       cambiar un driver no toca ni una linea de la logica.
 * @note Inspirado en la conclusion de la auditoria de `ukmars/mazerunner-core`: alli la
 *       capa de aplicacion tambien es portable y la de drivers no, pero **no habia
 *       ninguna interfaz escrita** que lo garantizara; esto es esa interfaz.
 *
 * ⚠️ **Aun no hay implementacion**: `drivers/` la tendra cuando `core/motion` empiece a
 *    usar el contrato. Las declaraciones de abajo son lo que el movimiento va a pedir,
 *    puestas por escrito **antes** de escribir el driver, para no descubrir la interfaz a
 *    base de parchear. Ver `drivers/README.md`.
 *
 * @note Ninguna de estas funciones puede bloquear ni tardar: se llaman desde el lazo de
 *       control. Lo que tarde (leer un ADC, mandar por UART) va en el driver, no aqui.
 */

/** Las dos ruedas del diferencial. */
typedef enum {
    CORE_RUEDA_IZQUIERDA = 0,
    CORE_RUEDA_DERECHA   = 1,
    CORE_RUEDA_N
} Core_Rueda_t;

/** Los cuatro canales de pared, en el orden fisico del robot. */
typedef enum {
    CORE_PARED_FRONTAL_DERECHA = 0,
    CORE_PARED_LATERAL_DERECHA = 1,
    CORE_PARED_LATERAL_IZQUIERDA = 2,
    CORE_PARED_FRONTAL_IZQUIERDA = 3,
    CORE_PARED_N
} Core_Pared_t;

/* ------------------------------------------------------------------ */
/*  Lo que el robot percibe                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Lectura cruda de un canal de pared, ya sin la luz ambiente.
 * @param pared Canal a leer
 * @return Cuentas del ADC, mayor cuanto mas cerca esta la pared
 */
uint16_t CORE_IO_LecturaPared(Core_Pared_t pared);

/**
 * @brief Dice si hay pared en un canal, aplicando el umbral que toque.
 * @param pared Canal a consultar
 * @return 1 si hay pared, 0 si no
 */
uint8_t CORE_IO_HayPared(Core_Pared_t pared);

/**
 * @brief Cuentas acumuladas del encoder de una rueda.
 * @param rueda Rueda a consultar
 * @return Cuentas, con signo (positivo = hacia adelante)
 */
int32_t CORE_IO_CuentasRueda(Core_Rueda_t rueda);

/**
 * @brief Velocidad de una rueda, medida con su encoder.
 * @param rueda Rueda a consultar
 * @return Velocidad en milimetros por segundo
 */
float CORE_IO_VelocidadRueda(Core_Rueda_t rueda);

/**
 * @brief Reloj monotono del robot.
 * @return Milisegundos desde el arranque
 */
uint32_t CORE_IO_Milisegundos(void);

/* ------------------------------------------------------------------ */
/*  Lo que el robot hace                                              */
/* ------------------------------------------------------------------ */

/**
 * @brief Pide una velocidad a una rueda.
 *
 * Es una **consigna**, no un duty: quien la recibe (el driver) es quien decide el PWM.
 *
 * @param rueda Rueda a mover
 * @param vel_mm_s Velocidad pedida en milimetros por segundo, con signo
 */
void CORE_IO_ConsignaRueda(Core_Rueda_t rueda, float vel_mm_s);

/**
 * @brief Frena las dos ruedas y deja de pedalear.
 */
void CORE_IO_Parar(void);

#endif /* CORE_CORE_IO_H */
