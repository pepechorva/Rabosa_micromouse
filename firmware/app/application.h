#ifndef APP_APPLICATION_H
#define APP_APPLICATION_H

#include "peripherals.h"

/*
 * Capa de aplicación: máquina de estados + menú.
 *
 * El banco de pruebas ya no se elige compilando: se elige en la pantalla OLED
 * con los 4 botones (ver app/menu.h) y se sale con MENU, que va por
 * interrupción y por eso responde siempre.
 *
 * Diseño completo: Documentacion/maquina-estados-menu.md
 */

/* Máquina de estados de la APP */
typedef enum {
    APP_STATE_BOOT = 0,  /* arranque: APP_InitAll() ya lo ha inicializado todo */
    APP_STATE_MENU,      /* navegando el menú (estado por defecto)            */
    APP_STATE_TEST,      /* ejecutando una prueba de banco desde el menú      */
    APP_STATE_STOP,      /* parada: motores frenados, se sale con OK          */
    APP_STATE_FAULT,     /* fallo (p. ej. batería crítica): frena y avisa     */
    APP_STATE_RUN        /* misión (F5: todavía no implementada)              */
} AppState_t;

/**
 * @brief Inicializa HAL + sensores + actuadores.
 *        Llamar una vez desde main().
 */
void APP_InitAll(void);

/**
 * @brief Un paso de la aplicación: botones, sensores y máquina de estados.
 *        Llamar cada 1 ms desde el bucle de APP_Run().
 */
void APP_Update(void);

/**
 * @brief Bucle principal: APP_Update() a 1 kHz. No retorna.
 */
void APP_Run(void);

/** @brief Frena los dos motores (se usa en cada cambio de estado). */
void APP_FrenoTotal(void);

/** @brief Estado actual de la máquina de estados. */
AppState_t APP_GetState(void);

/** @brief Fuerza un estado (útil para depurar y para los tests). */
void APP_SetState(AppState_t state);

/*
 * Bancos de pruebas (se lanzan desde el menú; cada una termina pulsando MENU).
 */
void App_Test_ADC(void);
void App_Test_MotoresEncoders(void);
void App_Test_Encoders(void);
void App_Test_IMU(void);
void App_Test_SPI2_Loopback(void);
void App_Test_PPR(void);
void App_Test_UmCuenta(void);
void App_Test_MinM1(void);
void App_Test_MinM2(void);
void App_Test_MinAmbos(void);
void App_Test_ComparaMotores(void);
void App_Test_Botones(void);
void App_Test_PWM_Rampa(void);
void App_Test_BotonesOLED(void);
void App_Test_I2C_Scan(void);
void App_Test_Bateria(void);
void App_Test_PID(void);
void App_Test_Odometria(void);
void App_Test_Movimiento(void);
void App_Test_WallFollower(void);
void App_Test_ESP8266(void);
void App_Test_Puente(void);
void App_Test_Paredes(void);

/*
 * Ajuste en caliente del lazo de velocidad (prueba 18), para la consola de órdenes.
 *
 * Las ganancias y la consigna viven en la APP porque es la APP la que empuja los ajustes a los lazos
 * (ver `control/controller.h`): la consola solo los deja escritos y es el lazo quien los aplica en su
 * siguiente paso. Así se puede sintonizar sin recompilar ni flashear, que es lo que hace viable el
 * barrido de Ziegler-Nichols.
 */
typedef enum {
    APP_PID_GANANCIA_KP = 0,
    APP_PID_GANANCIA_KI,
    APP_PID_GANANCIA_KD,
    APP_PID_GANANCIA_N    /* número de ganancias: no es una ganancia válida */
} App_PidGanancia_t;

/**
 * @brief Cambia una ganancia del lazo de velocidad (orden `PID` de la consola).
 * @param cual  Ganancia a cambiar
 * @param valor Valor nuevo
 * @return 1 si se aceptó, 0 si está fuera de rango
 */
uint8_t APP_PID_SetGanancia(App_PidGanancia_t cual, float valor);

/**
 * @brief Cambia la consigna de velocidad de la prueba 18 (orden `PID VEL`).
 * @param mm_s Consigna en mm/s
 * @return 1 si se aceptó, 0 si está fuera de rango
 */
uint8_t APP_PID_SetVelocidad(float mm_s);

/** @brief Ganancia en uso (la que aplicará la prueba 18). */
float APP_PID_GetGanancia(App_PidGanancia_t cual);

/** @brief Consigna de velocidad en uso [mm/s]. */
float APP_PID_GetVelocidad(void);

/*
 * Mando a distancia de las primitivas de movimiento (prueba 20), para la consola de órdenes.
 *
 * Existe porque las primitivas son **bloqueantes** y los botones son físicos: sin esto, cada tanteo
 * del banco exige tener a alguien delante pulsando, y un cuadrado de cuatro celdas y cuatro giros son
 * ocho pulsaciones que hay que repetir enteras cada vez que se toca una constante.
 *
 * El reparto es el de siempre: la consola deja la **petición** escrita en la APP y es el bucle de la
 * prueba quien la recoge y la ejecuta. La consola no mueve motores por su cuenta.
 */

/** @brief Número máximo de repeticiones que acepta una orden `MOV`. */
#define APP_MOV_REP_MAX    20U

typedef enum {
    APP_MOV_NINGUNO = 0,
    APP_MOV_AVANZAR,
    APP_MOV_RETROCEDER,
    APP_MOV_GIRAR_IZQ,
    APP_MOV_GIRAR_DER,
    APP_MOV_N    /* número de acciones: no es una acción válida */
} App_Movimiento_t;

/**
 * @brief Deja escrita una petición de movimiento para la prueba 20 (orden `MOV` de la consola).
 *
 * @param cual Acción pedida
 * @param repeticiones Cuántas veces hay que repetirla (1 = una)
 * @return 1 si se aceptó, 0 si la prueba 20 no está en marcha, ya había otra petición pendiente, o los
 *         argumentos no valen
 */
uint8_t APP_MOV_Pedir(App_Movimiento_t cual, uint8_t repeticiones);

/**
 * @brief Recoge la petición pendiente y la marca como atendida.
 * @param cual Acción pedida (solo es válido si se devuelve 1)
 * @param repeticiones Cuántas veces repetirla
 * @return 1 si había petición, 0 si no
 */
uint8_t APP_MOV_Recoger(App_Movimiento_t *cual, uint8_t *repeticiones);

/*
 * Bloque analógico (pruebas 13-17): sensores de pared IR.
 * Se ejecutan en orden: primero el chequeo de la soldadura y solo si sale bien se sigue.
 * Procedimientos y criterios: Documentacion/pruebas-bringup.md §13.
 */
void App_Test_ADC_Soldadura(void);
void App_Test_EmisoresIR(void);
void App_Test_IR_Receptores(void);
void App_Test_IR_Modulado(void);
void App_Test_CalibracionIR(void);

/* Un receptor a pantalla completa, con el barrido de emisores corriendo (submenús `VER Rn`). */
void App_Test_R0(void);
void App_Test_R1(void);
void App_Test_R2(void);
void App_Test_R3(void);

#endif /* APP_APPLICATION_H */
