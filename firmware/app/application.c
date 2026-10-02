#include "app/application.h"
#include "app/buttons.h"
#include "app/console.h"
#include "app/fsm.h"
#include "app/menu.h"
#include "app/motion.h"
#include "app/oled.h"
#include "app/telemetry.h"
#include "app/trace.h"
#include "app/wall_follower.h"
#include "app/walls.h"
#include "hal/esp8266.h"
#include "control/controller.h"
#include "filter/odometry.h"
#include "hal/adc.h"
#include "hal/gpio.h"
#include "hal/i2c.h"
#include "hal/pwm.h"
#include "hal/drv8833.h"
#include "hal/reset.h"
#include "hal/systemclock.h"
#include "hal/timers.h"
#include "hal/uart.h"
#include "hal/spi.h"
#include "sensor/encoder.h"
#include "sensor/imu.h"
#include "sensor/battery.h"

#include "peripherals.h"
#include "pinout.h"

#include <stdio.h>
#include <math.h>

/**
 * @file application.c
 * @brief Capa APP: máquina de estados, menú y banco de pruebas.
 *
 * Las capas HW, HAL, SENSOR y CONTROL están validadas y FILTER está por escribir
 * (`Documentacion/estado-del-proyecto.md`). El banco de pruebas se lanza desde el menú de la OLED, sin
 * recompilar; las pruebas se catalogan en `Documentacion/pruebas-bringup.md`.
 */

static AppState_t app_state = APP_STATE_BOOT;
static DRV8833_Motor_t motor1;
static DRV8833_Motor_t motor2;

/* Lo que ha elegido el usuario en el menú. Vive fuera de `app_fsm_paso()` porque el estado TEST
   necesita saber qué rutina lanzar, y MENU_Update() solo la devuelve una vez. */
static const Menu_Entrada_t *app_elegida;

/* El FAULT por batería se puede abandonar con OK. Esta marca evita que la FSM vuelva a entrar de
   inmediato (la batería sigue igual de baja): no se reintenta hasta que la medida se recupere. */
static uint8_t fault_reconocido;

static void app_aviso_bateria(uint8_t mostrar_salida);
static void app_fsm_paso(void);
static void app_publicar_estado(AppState_t estado);
static void oled_print_line(uint8_t fila, const char *texto);

/**
 * @brief Inicializa reloj, pines, periféricos, sensores y actuadores.
 *
 * @note El orden importa: MX_GPIO_Init() deja PA4-PA7 como analógicos y es ENC_Init() quien devuelve
 *       PA6/PA7 a entrada; BUTTONS_Init() necesita los pines ya configurados, e IMU_Init() va casi al
 *       final porque si el bus no responde se queda esperando.
 */
void APP_InitAll(void)
{
    /* Lo primero de todo, porque las banderas de reinicio se borran al leerlas y cualquier otra
       inicialización puede dar por buenas unas banderas que ya no están. */
    RESET_LeerCausa();

    SystemClock_Config();
    LEDINBOARD_GPIO_CLK_ENABLE();
    MX_GPIO_Init();

    BUTTONS_Init();

    MX_TIM1_Init();
    MX_ADC1_Init();
    MX_USART3_UART_Init(115200);
    CONSOLE_Init();
    TELEM_SendInt("RESET", (int)RESET_GetCausa());
    TELEM_SendInt("BOOT", 1);
    MX_I2C1_Init();

    OLED_Init();
    TELEM_SendInt("OLED", OLED_Detected() ? 1 : 0);

    PWM_StartAll();

    /* ⚠️ El motor 1 va montado al revés que el 2 (comprobado el 30-sep): con el mismo signo de duty, M1
       gira en sentido contrario a M2. Se cruzan sus dos canales del DRV8833 para que un duty positivo
       mueva las dos ruedas hacia adelante. El freno y el coast no se ven afectados, porque usan los dos
       canales a la vez. */
    motor1.htim      = &htim1;
    motor1.channel1  = TIM_CHANNEL_1;
    motor1.channel2  = TIM_CHANNEL_2;
    motor2.htim      = &htim1;
    motor2.channel1  = TIM_CHANNEL_3;
    motor2.channel2  = TIM_CHANNEL_4;

    /* ⚠️ El **motor 1 va montado en espejo** respecto al 2 (es lo normal en un diferencial: uno gira
       al revés que el otro para el mismo voltaje), así que solo ese necesita invertir el sentido. Con
       los dos invertidos o ninguno, uno de los dos lazos de velocidad se realimenta al revés.

       Medido el 2026-10-01 en la prueba 18 con el integrador a cero: con `>DUTY_IZQ:+17,6` el encoder
       izquierdo daba `>VEL_IZQ:+667` (bien) y con `>DUTY_DER:+60` el derecho daba `>VEL_DER:-2500`
       (invertido).

       Si algún día se recablea un motor, esto es lo primero que hay que revisar. */
    motor1.invertido = 1U;
    motor2.invertido = 0U;
    DRV8833_Init(&motor1);
    DRV8833_Init(&motor2);

    ENC_Init();
    ENC_ResetPulses();
    ENC_ResetCount1();
    ENC_ResetCount2();

    BATTERY_Init();

    /* Los emisores de pared hay que dejarlos apagados y los umbrales en su valor de arranque antes de
       que ninguna prueba (ni el wall follower) pregunte por una pared. */
    WALLS_Init();
    TRACE_Init();

    TELEM_SendInt("IMU", 0);
    IMU_Init();
    TELEM_SendInt("IMU", 1);

    /* Señal de vida que no depende de la pantalla ni del PC: 3 destellos al terminar el arranque.
       Si el robot se queda mudo y el LED no parpadea, el cuelgue es DURANTE el init. */
    for (uint8_t i = 0U; i < 3U; i++) {
        HAL_GPIO_WritePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN, GPIO_PIN_RESET);
        HAL_Delay(100U);
        HAL_GPIO_WritePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN, GPIO_PIN_SET);
        HAL_Delay(200U);
    }

    app_state = APP_STATE_BOOT;
}

/**
 * @brief Menú: tablas de opciones.
 *
 * El banco no se elige compilando, se elige en la pantalla con los 4 botones; se sale con MENU, que va
 * por interrupción (ver `hal/gpio.c`) y por eso responde incluso dentro del `while (1)` de una prueba.
 *
 * El árbol tiene tres niveles: raíz → bloque → prueba. Los bloques existen porque 17 pruebas en una
 * lista con 3 filas visibles se navegan fatal.
 *
 * @note Añadir una prueba es una fila más en el bloque que le toque, con su `App_Test_*`.
 * @note Añadir un submenu es una fila más en una tabla y una tabla más en `submenus[]`.
 * @see Documentacion/maquina-estados-menu.md
 */

/**
 * @brief Pruebas de banco **retiradas del menú** el 2026-10-02 para recortar Flash.
 *
 * La Flash estaba al **88,3 %** (57 856 B de 65 536) y el código de banco ocupaba **~23 KB**, un
 * tercio del mapa, cuando el sábado solo hace falta *moverse sin chocar*. Se retiran las pruebas
 * **ya validadas y sin uso previsto**; se quedan las pendientes (16, 17, 21 y 22), el **PUENTE
 * WIFI** —que desde hoy es la única ventana al robot—, **MOVIMIENTO** (falta re-medir los giros) y
 * el **PID**.
 *
 * ⚠️ **Lo que libera la Flash es quitar la fila, no comentar la función.** Se compila con
 * `-ffunction-sections` y el enlazador lleva `--gc-sections`, así que en cuanto nadie referencia
 * una función **se queda fuera del binario**. Por eso los cuerpos se quedan tal cual —compilados,
 * con su Doxygen y a un `git revert` de distancia— y aquí solo se comenta la fila: envolverlos en
 * `#if 0` no ahorraría **ni un byte** más y sí dejaría sin usar los `static` que solo ellas llaman.
 *
 * En cada tabla, **la primera fila conservada no es un descarte**: en ANALÓGICO se queda `ADC` (la
 * prueba 1 está pendiente de revalidar tras el paso a 4 canales) y en DIAG se queda `BUS I2C` (es
 * el diagnóstico que hace falta si la OLED no responde). Con eso ninguna tabla queda vacía y **no
 * hay que renumerar `submenus[]`**, cuyos índices son un contrato.
 *
 * @note Para **recuperar una prueba**: descomentar su fila. Nada más.
 * @see Documentacion/pruebas-bringup.md (catálogo de pruebas)
 */
static const Menu_Entrada_t menu_test_analogico[] = {
    { "ADC",           MENU_TIPO_LANZAR, App_Test_ADC,           0U },
    /* BATERIA VUELVE (2026-10-02, por la tarde): en el laberinto **no habrá PC**, así que la orden
       `BAT LIST` no existe y sin esta prueba no habría **ninguna** forma de leer la batería hasta que
       apareciera el FAULT. Cuesta ~1,1 KB y sobran 16. */
    { "BATERIA",       MENU_TIPO_LANZAR, App_Test_Bateria,       0U },
    /* Retirada: ADC SOLDADURA (13) */
    /* { "ADC SOLDADURA", MENU_TIPO_LANZAR, App_Test_ADC_Soldadura, 0U }, */
};

static const Menu_Entrada_t menu_test_ir[] = {
    { "PAREDES",       MENU_TIPO_LANZAR, App_Test_Paredes,       0U },
    /* EMISORES IR VUELVE (2026-10-02, por la tarde): es la herramienta para **alinear el apuntado**
       de los focos con un folio blanco y la camara del movil. Con `5` se encienden **los cuatro a la
       vez**, y el que apunta a otra provincia se ve de un vistazo; con el parpadeo a 4 Hz de la 16
       habia que mirarlos de uno en uno y adivinar. */
    { "EMISORES IR",   MENU_TIPO_LANZAR, App_Test_EmisoresIR,    0U },
    /* IR RECEPTORES y VER R0..R3 VUELVEN TAMBIEN (2026-10-02): son **la matriz 4x4** -- encienden
       cada emisor y ensenan lo que recibe CADA receptor --, y son la unica forma de comprobar el
       emparejamiento con el montaje delante de los ojos. El de hardware.md se dedujo con una pared
       de frente a 10 cm, y con una pared delante **los cuatro emisores iluminan a los cuatro
       receptores**: la pareja «ganadora» pudo ser simplemente la mas cercana, no la buena. */
    { "IR RECEPTORES", MENU_TIPO_LANZAR, App_Test_IR_Receptores, 0U },
    { "VER R0",        MENU_TIPO_LANZAR, App_Test_R0,            0U },
    { "VER R1",        MENU_TIPO_LANZAR, App_Test_R1,            0U },
    { "VER R2",        MENU_TIPO_LANZAR, App_Test_R2,            0U },
    { "VER R3",        MENU_TIPO_LANZAR, App_Test_R3,            0U },
    { "IR MODULADO",   MENU_TIPO_LANZAR, App_Test_IR_Modulado,   0U },
    { "CURVA IR",      MENU_TIPO_LANZAR, App_Test_CalibracionIR, 0U },
};

static const Menu_Entrada_t menu_test_motores[] = {
    { "MOVIMIENTO",    MENU_TIPO_LANZAR, App_Test_Movimiento,           0U },
    /* Retiradas: ENCODERS (5) · PPR y UM MOTORES (7) · MOTORES ENCOD (5) · MIN M1/M2/AMBOS (2, 3)
       · COMPARA M · RAMPA PWM (9) · ODOMETRIA (19). Todas medidas, y sus números (µm/cuenta,
       trocha) viven ya en los ajustes: repetirlas no cambiaría nada */
    /* { "ENCODERS",      MENU_TIPO_LANZAR, App_Test_Encoders,            0U }, */
    /* { "PPR A MANO",    MENU_TIPO_LANZAR, App_Test_PPR,                0U }, */
    /* { "UM MOTORES",    MENU_TIPO_LANZAR, App_Test_UmCuenta,           0U }, */
    /* { "MOTORES ENCOD", MENU_TIPO_LANZAR, App_Test_MotoresEncoders,     0U }, */
    /* { "MIN M1",        MENU_TIPO_LANZAR, App_Test_MinM1,               0U }, */
    /* { "MIN M2",        MENU_TIPO_LANZAR, App_Test_MinM2,               0U }, */
    /* { "MIN AMBOS",     MENU_TIPO_LANZAR, App_Test_MinAmbos,            0U }, */
    /* { "COMPARA M",     MENU_TIPO_LANZAR, App_Test_ComparaMotores,      0U }, */
    /* { "RAMPA PWM",     MENU_TIPO_LANZAR, App_Test_PWM_Rampa,           0U }, */
    { "PID VELOCIDAD", MENU_TIPO_LANZAR, App_Test_PID,                 0U },
    /* { "ODOMETRIA",     MENU_TIPO_LANZAR, App_Test_Odometria,           0U }, */
};

static const Menu_Entrada_t menu_test_varios[] = {
    { "WIFI AL PC",    MENU_TIPO_LANZAR, App_Test_Puente,        0U },
    { "ESP8266",      MENU_TIPO_LANZAR, App_Test_ESP8266,       0U },
    /* Retiradas: IMU (4) · BOTONES OLED · I2C SCAN (11, sigue en DIAG) · SPI LOOPBACK (6,
       validada el 10-sep; el bucle PB15→PB14 no se va a mover) */
    /* { "IMU",           MENU_TIPO_LANZAR, App_Test_IMU,           0U }, */
    /* { "BOTONES OLED",  MENU_TIPO_LANZAR, App_Test_BotonesOLED,   0U }, */
    /* { "I2C SCAN",      MENU_TIPO_LANZAR, App_Test_I2C_Scan,      0U }, */
    /* { "SPI LOOPBACK",  MENU_TIPO_LANZAR, App_Test_SPI2_Loopback, 0U }, */
};

static const Menu_Entrada_t menu_test[] = {
    { "ANALOGICO", MENU_TIPO_SUBMENU, NULL, 1U },
    { "IR",        MENU_TIPO_SUBMENU, NULL, 2U },
    { "MOTORES",   MENU_TIPO_SUBMENU, NULL, 3U },
    { "VARIOS",    MENU_TIPO_SUBMENU, NULL, 4U },
};

/**
 * @brief Navegación del laberinto.
 *
 * `WALL FOLLOWER` ya es real; `FLOOD FILL` sigue siendo un marcador hasta que haya mapa en RAM.
 */
static const Menu_Entrada_t menu_explore[] = {
    { "WALL FOLLOWER", MENU_TIPO_LANZAR,  App_Test_WallFollower, 0U },
    { "FLOOD FILL",    MENU_TIPO_NINGUNA, NULL,                  0U },
};

/**
 * @brief Resolver el laberinto: la vuelta rápida, con la velocidad de crucero ya conocida.
 */
static const Menu_Entrada_t menu_resolve[] = {
    { "RESOLVER", MENU_TIPO_NINGUNA, NULL, 0U },
};

/**
 * @brief Ajustes del robot (fase M6: las pantallas todavía no están hechas).
 *
 * Los valores viven en `app/settings.c` y cada capa tiene su setter con validación
 * (`BATTERY_SetUmbral()`, `CTRL_SetGanancias()`, …): la APP empuja, nadie lee ajustes por su cuenta.
 */
static const Menu_Entrada_t menu_settings[] = {
    { "BATERIA",    MENU_TIPO_NINGUNA, NULL, 0U },
    { "VELOCIDAD",  MENU_TIPO_NINGUNA, NULL, 0U },
    { "PID",        MENU_TIPO_NINGUNA, NULL, 0U },
    { "LABERINTO",  MENU_TIPO_NINGUNA, NULL, 0U },
    { "TELEMETRIA", MENU_TIPO_NINGUNA, NULL, 0U },
    { "GUARDAR",    MENU_TIPO_NINGUNA, NULL, 0U },
    { "RESET",      MENU_TIPO_NINGUNA, NULL, 0U },
};

/**
 * @brief Diagnóstico de buses y de la alimentación.
 */
static const Menu_Entrada_t menu_diag[] = {
    { "BUS I2C",      MENU_TIPO_LANZAR, App_Test_I2C_Scan,      0U },
    /* Retiradas: SPI LOOPBACK (6) · BATERIA (12), que se lee por la orden `BAT LIST` */
    /* { "SPI LOOPBACK", MENU_TIPO_LANZAR, App_Test_SPI2_Loopback, 0U }, */
    /* { "BATERIA",      MENU_TIPO_LANZAR, App_Test_Bateria,       0U }, */
};

static const Menu_Tabla_t menu_test_tabla = {
    "TEST", menu_test, (uint8_t)(sizeof(menu_test) / sizeof(menu_test[0]))
};
static const Menu_Tabla_t menu_test_analogico_tabla = {
    "TEST ANALOGICO", menu_test_analogico,
    (uint8_t)(sizeof(menu_test_analogico) / sizeof(menu_test_analogico[0]))
};
static const Menu_Tabla_t menu_test_ir_tabla = {
    "TEST IR", menu_test_ir, (uint8_t)(sizeof(menu_test_ir) / sizeof(menu_test_ir[0]))
};
static const Menu_Tabla_t menu_test_motores_tabla = {
    "TEST MOTORES", menu_test_motores,
    (uint8_t)(sizeof(menu_test_motores) / sizeof(menu_test_motores[0]))
};
static const Menu_Tabla_t menu_test_varios_tabla = {
    "TEST VARIOS", menu_test_varios,
    (uint8_t)(sizeof(menu_test_varios) / sizeof(menu_test_varios[0]))
};
static const Menu_Tabla_t menu_explore_tabla = {
    "EXPLORE", menu_explore, (uint8_t)(sizeof(menu_explore) / sizeof(menu_explore[0]))
};
static const Menu_Tabla_t menu_resolve_tabla = {
    "RESOLVE", menu_resolve, (uint8_t)(sizeof(menu_resolve) / sizeof(menu_resolve[0]))
};
static const Menu_Tabla_t menu_settings_tabla = {
    "SETTINGS", menu_settings, (uint8_t)(sizeof(menu_settings) / sizeof(menu_settings[0]))
};
static const Menu_Tabla_t menu_diag_tabla = {
    "DIAG", menu_diag, (uint8_t)(sizeof(menu_diag) / sizeof(menu_diag[0]))
};

/**
 * @brief Submenús, indexados por el campo `submenu` de cada entrada.
 *
 * ⚠️ El orden de esta lista es el contrato: los índices que usan las filas de arriba apuntan aquí.
 */
static const Menu_Tabla_t *const submenus[] = {
    &menu_test_tabla,           /* 0 · TEST               */
    &menu_test_analogico_tabla, /* 1 · TEST → ANALOGICO   */
    &menu_test_ir_tabla,        /* 2 · TEST → IR          */
    &menu_test_motores_tabla,   /* 3 · TEST → MOTORES     */
    &menu_test_varios_tabla,    /* 4 · TEST → VARIOS      */
    &menu_explore_tabla,        /* 5 · EXPLORE            */
    &menu_resolve_tabla,        /* 6 · RESOLVE            */
    &menu_settings_tabla,       /* 7 · SETTINGS           */
    &menu_diag_tabla,           /* 8 · DIAG               */
};
#define APP_N_SUBMENUS   ((uint8_t)(sizeof(submenus) / sizeof(submenus[0])))

/**
 * @brief Raíz: entrar en un bloque, ver los ajustes o parar ya.
 */
static const Menu_Entrada_t menu_raiz_entradas[] = {
    { "TEST",     MENU_TIPO_SUBMENU, NULL, 0U },
    { "EXPLORE",  MENU_TIPO_SUBMENU, NULL, 5U },
    { "RESOLVE",  MENU_TIPO_SUBMENU, NULL, 6U },
    { "SETTINGS", MENU_TIPO_SUBMENU, NULL, 7U },
    { "DIAG",     MENU_TIPO_SUBMENU, NULL, 8U },
    { "PARAR",    MENU_TIPO_PARAR,   NULL, 0U },
};
static const Menu_Tabla_t menu_raiz = {
    "RABOSA-MVR", menu_raiz_entradas,
    (uint8_t)(sizeof(menu_raiz_entradas) / sizeof(menu_raiz_entradas[0]))
};

/**
 * @brief Frena los dos motores en seco (short brake del DRV8833).
 */
void APP_FrenoTotal(void)
{
    DRV8833_Brake(&motor1);
    DRV8833_Brake(&motor2);
}

/**
 * @brief Un paso de la aplicación: alimenta lo que caduca por tiempo y avanza el estado.
 *
 * Botones y batería se atienden a 1 kHz: el antirrebote mide tiempo y la medida de batería acumula
 * medias. Los comandos que llegan por telemetría los ejecuta CONSOLE_Update().
 * Llamar desde el bucle de APP_Run().
 */
void APP_Update(void)
{
    BUTTONS_Update();
    BATTERY_Update();
    CONSOLE_Update();

    app_fsm_paso();
}

/**
 * @brief Anota el estado de la aplicación y lo publica por telemetría.
 *
 * @param estado Estado nuevo
 */
static void app_publicar_estado(AppState_t estado)
{
    app_state = estado;
    TELEM_SendInt("APP_EST", (int32_t)estado);
}

/** @brief Transición con traza: publica el estado destino y salta a él. */
#define APP_GOTO(s)   do { app_publicar_estado(APP_STATE_##s); fsmgoto(s); } while (0)

/**
 * @brief Pinta en la OLED el nombre de la prueba que se va a lanzar.
 *
 * Casi ninguna prueba de banco escribe en la pantalla (la mayoría solo publica telemetría), así que
 * sin este cartel parece que OK no ha hecho nada. El propio cartel dice cómo salir.
 *
 * @param etiqueta Nombre de la prueba, tal como aparece en el menú
 */
static void app_mostrar_prueba(const char *etiqueta)
{
    OLED_Clear();
    oled_print_line(0U, "PRUEBA");
    oled_print_line(1U, etiqueta);
    oled_print_line(2U, "MENU: SALIR");
    OLED_Update();
}

/**
 * @brief Un paso de la máquina de estados de la aplicación.
 *
 * El despacho de `START` hace dos cosas: aplica las **guardas globales** (batería crítica, que puede
 * interrumpir la misión) y salta al estado en que se quedó la máquina, porque este bucle se reanuda
 * de llamada en llamada.
 *
 * @note El estado `TEST` llama a la rutina de banco, que es **bloqueante** (su bucle sale con MENU):
 *       mientras dura, esta máquina no avanza. Es el compromiso aceptado hasta que las pruebas se
 *       migren a FSM.
 * @note La batería crítica **solo** corta la misión (`RUN`): en el banco se trabaja con fuente de
 *       laboratorio y una batería baja no puede tapar la pantalla, parpadear el LED ni cortar una
 *       prueba. Para verla están la prueba 12 y la telemetría (`>VBAT_ESTADO`).
 */
static void app_fsm_paso(void)
{
    fsm {
        fsmstate(START) {
            if (BATTERY_GetEstado() != BATTERY_CRITICA) {
                fault_reconocido = 0U;
            }

            if ((app_state == APP_STATE_RUN) && (fault_reconocido == 0U) &&
                (BATTERY_GetEstado() == BATTERY_CRITICA)) {
                APP_FrenoTotal();
                APP_GOTO(FAULT);
            }

            /* Despacho al estado en que se quedó la máquina */
            switch (app_state) {
            case APP_STATE_MENU:  fsmgoto(MENU);
            case APP_STATE_TEST:  fsmgoto(TEST);
            case APP_STATE_STOP:  fsmgoto(STOP);
            case APP_STATE_FAULT: fsmgoto(FAULT);
            case APP_STATE_RUN:   fsmgoto(RUN);
            case APP_STATE_BOOT:
            default:              fsmgoto(BOOT);
            }
        }

        fsmstate(BOOT) {
            MENU_Init(&menu_raiz, submenus, APP_N_SUBMENUS);
            APP_GOTO(MENU);
        }

        fsmstate(MENU) {
            app_elegida = MENU_Update();

            if (app_elegida == NULL) {
                fsmexit;   /* nada pulsado: se sigue en el menú */
            }

            if (app_elegida->tipo == MENU_TIPO_PARAR) {
                APP_FrenoTotal();
                APP_GOTO(STOP);
            }

            if ((app_elegida->tipo == MENU_TIPO_LANZAR) && (app_elegida->ejecutar != NULL)) {
                TELEM_SendInt("APP_TEST", 1);
                TELEM_SendInt("APP_TEST_N", (int32_t)MENU_IndiceActual());
                app_mostrar_prueba(app_elegida->etiqueta);
                APP_GOTO(TEST);
            }

            fsmexit;   /* fila informativa: el menú ya ha hecho lo suyo */
        }

        fsmstate(TEST) {
            if (app_elegida == NULL) {
                APP_GOTO(MENU);   /* no debería pasar: evita un HardFault por puntero nulo */
            }

            app_elegida->ejecutar();   /* bloqueante: la rutina sale con MENU */

            APP_FrenoTotal();
            TELEM_SendInt("APP_TEST", 0);
            MENU_Dibujar();
            APP_GOTO(MENU);
        }

        fsmstate(STOP) {
            APP_FrenoTotal();

            if (BUTTONS_GetEvent() != BTN_NUM_OK) {
                fsmexit;
            }

            MENU_Dibujar();
            APP_GOTO(MENU);
        }

        fsmstate(FAULT) {
            app_aviso_bateria(1U);

            /* OK abandona el FAULT aunque la batería siga crítica: el robot queda frenado y el aviso
               sigue saliendo por telemetría, pero el banco se puede seguir usando (con otra química,
               con fuente de laboratorio o con los umbrales sin ajustar la crítica es normal y no
               puede dejar el robot encerrado). No se vuelve a entrar hasta que la medida se recupere
               y vuelva a caer. */
            if (BUTTONS_GetEvent() != BTN_NUM_OK) {
                fsmexit;
            }

            fault_reconocido = 1U;
            HAL_GPIO_WritePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN, GPIO_PIN_SET);
            MENU_Dibujar();
            APP_GOTO(MENU);
        }

        fsmstate(RUN) {
            /* F5: misión (wall follower / flood fill). De momento no hay nada: vuelve al menú */
            APP_GOTO(MENU);
        }
    }
}

/**
 * @brief Bucle principal: un paso de la aplicación cada milisegundo.
 */
void APP_Run(void)
{
    app_state = APP_STATE_BOOT;

    for (;;) {
        APP_Update();
        HAL_Delay(1);
    }
}

/**
 * @brief Publica las cuentas de los 4 receptores de pared (prueba 1).
 *
 * @note Es la prueba de humo del ADC: solo los receptores (PA2-PA5). La batería (PB0/PB1) tiene su
 *       propia prueba (12) y el chequeo completo de la soldadura, la 13.
 */
void App_Test_ADC(void)
{
    while (!BUTTONS_SalidaSolicitada()) {
        uint16_t ir[4];

        ir[0] = ADC_ReadPin(ADC_PIN_A2);
        ir[1] = ADC_ReadPin(ADC_PIN_A3);
        ir[2] = ADC_ReadPin(ADC_PIN_A4);
        ir[3] = ADC_ReadPin(ADC_PIN_A5);

        TELEM_SendIRValues(ir, 4);

        HAL_Delay(10);
    }
}

/**
 * @brief Mueve las dos ruedas al 25 % y espera a contar 50 pulsos de encoder (prueba 2).
 *
 * @note Es la primera prueba que junta PWM, driver y encoders en bucle cerrado: si avanza 50 pulsos y
 *       frena, las tres cosas funcionan.
 */
void App_Test_MotoresEncoders(void)
{
    ENC_ResetCount1();
    ENC_ResetCount2();

    while (!BUTTONS_SalidaSolicitada()) {
        int32_t base = ENC_GetCount1();

        while (ENC_GetCount1() - base < 50) {
            DRV8833_Set_Speed(&motor1, 25);
            DRV8833_Set_Speed(&motor2, 25);

            TELEM_SendMotorEncoder(
                (int16_t)ENC_GetCount1(), 25,
                (int16_t)ENC_GetCount2(), 25);

            HAL_Delay(1);
        }

        DRV8833_Brake(&motor1);
        DRV8833_Brake(&motor2);
        HAL_Delay(3000);
    }
}

/*
 * Prueba 3: RETIRADA (2026-09-28).
 *
 * Era "ADC → PWM directo" con potenciómetro, un banco de la primera fase: el robot no lleva
 * potenciómetros. Quedan la prueba 1 (leer el ADC) y la 9 (mover los motores a lazo abierto).
 */

/**
 * @brief Lee los encoders y publica los pulsos, reiniciándolos tras 5 s sin movimiento (prueba 4).
 *
 * @note El reinicio por inactividad deja el contador a cero para poder girar la rueda a mano sin que
 *       se acumule el total de la sesión.
 */
void App_Test_Encoders(void)
{
    int32_t  ultimo_enc1 = 0;
    int32_t  ultimo_enc2 = 0;
    uint32_t t_actividad = HAL_GetTick();

    ENC_ResetPulses();

    while (!BUTTONS_SalidaSolicitada()) {
        int32_t enc1;
        int32_t enc2;

        ENC_UpdatePulses();

        enc1 = ENC_GetCount1();
        enc2 = ENC_GetCount2();

        if ((enc1 != ultimo_enc1) || (enc2 != ultimo_enc2)) {
            t_actividad = HAL_GetTick();
            ultimo_enc1 = enc1;
            ultimo_enc2 = enc2;
        }

        if ((HAL_GetTick() - t_actividad) >= 5000U) {
            ENC_ResetCount1();
            ENC_ResetCount2();
            ENC_ResetPulses();
            ultimo_enc1 = 0;
            ultimo_enc2 = 0;
            t_actividad = HAL_GetTick();
        }

        TELEM_SendEncoderPulses(
            ENC_GetPulse1A(), ENC_GetPulse1B(),
            ENC_GetPulse2A(), ENC_GetPulse2B());

        TELEM_SendMotorEncoder(
            (int16_t)enc1, (int16_t)enc1,
            (int16_t)enc2, (int16_t)enc2);

        HAL_Delay(10);
    }
}

/**
 * @brief Comprueba el puente PB15-PB14 con GPIO (fase 1 de la prueba 6).
 *
 * PB15 se pone como salida y PB14 como entrada con pull-down: si el puente está puesto, PB14 sigue a
 * PB15. Sirve para separar un problema del MCU de uno del módulo SPI.
 */
static void spi2_probar_puente_gpio(void)
{
    GPIO_InitTypeDef gi = {0};
    char buf[64];
    int len;

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gi.Pin   = GPIO_PIN_15;
    gi.Mode  = GPIO_MODE_OUTPUT_PP;
    gi.Pull  = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gi);

    gi.Pin  = GPIO_PIN_14;
    gi.Mode = GPIO_MODE_INPUT;
    gi.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOB, &gi);

    TELEM_SendInt("SPI2_FASE", 1);

    for (int i = 0; i < 5; i++) {
        GPIO_PinState r_hi;
        GPIO_PinState r_lo;
        GPIO_PinState s_hi;
        GPIO_PinState s_lo;

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
        HAL_Delay(100);
        s_hi = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15);
        r_hi = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14);

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
        HAL_Delay(100);
        s_lo = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15);
        r_lo = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14);

        len = snprintf(buf, sizeof(buf),
                       ">GPIO B15out=1 B15rd:%d B14:%d | B15out=0 B15rd:%d B14:%d\n",
                       (s_hi == GPIO_PIN_SET) ? 1 : 0,
                       (r_hi == GPIO_PIN_SET) ? 1 : 0,
                       (s_lo == GPIO_PIN_SET) ? 1 : 0,
                       (r_lo == GPIO_PIN_SET) ? 1 : 0);
        if (len > 0) {
            UART_SendData((uint8_t *)buf, (uint16_t)len);
        }
    }
}

/**
 * @brief Envía un byte por SPI2 y lo recibe por MISO (fase 2 de la prueba 6).
 *
 * Con el puente PB15-PB14 puesto, RX debe ser el mismo byte que se envió (0xA5). Si la fase 1 daba
 * bien y esta no, el problema está en la configuración del periférico o del CS.
 */
void App_Test_SPI2_Loopback(void)
{
    char    buf[64];
    int     len;
    uint8_t tx = 0xA5;
    uint8_t rx = 0x00;

    spi2_probar_puente_gpio();

    MX_SPI2_Init();
    TELEM_SendInt("SPI2_FASE", 2);

    while (!BUTTONS_SalidaSolicitada()) {
        SPI2_CS_Low();
        SPI2_Transfer(&tx, &rx, 1);
        SPI2_CS_High();

        len = snprintf(buf, sizeof(buf), ">TX:%02X RX:%02X\n", tx, rx);
        if (len > 0) {
            UART_SendData((uint8_t *)buf, (uint16_t)len);
        }

        HAL_Delay(500);
    }
}

/**
 * @brief Lee acelerómetro y giroscopio y los publica por telemetría (prueba 5).
 *
 * Si la IMU no responde, reintenta el init y espera informando del WHO_AM_I leído; esa espera también
 * termina con el botón MENU, para no quedarse encerrado si el módulo no está.
 *
 * @note WHO_AM_I esperado: 0x70 (MPU-6500) o 0x71 (MPU-9250).
 * @warning Si sale 0xFF, revisar el cableado SPI: SDA/SDI → MOSI (PB15) y ADO/SDO → MISO (PB14).
 */
void App_Test_IMU(void)
{
    IMU_Data_t imu;

    TELEM_SendInt("IMU_TEST", 1);

    if (!IMU_Detected()) {
        IMU_Init();
    }

    while (!IMU_Detected() && !BUTTONS_SalidaSolicitada()) {
        TELEM_SendInt("IMU_WHO", (int32_t)IMU_WhoAmI());
        TELEM_SendInt("IMU_DET", 0);
        HAL_Delay(500);
    }

    TELEM_SendInt("IMU_DET", 1);

    while (!BUTTONS_SalidaSolicitada()) {
        if (IMU_Read(&imu)) {
            TELEM_SendIMU(imu.ax, imu.ay, imu.az,
                          imu.gx, imu.gy, imu.gz);
        }

        HAL_Delay(10);
    }
}

/**
 * @brief Parámetros de la calibración de encoders (prueba 7).
 *
 * Verifican por medición lo que publica el fabricante: 7 PPR por canal y reductora 1:10.
 */
#define CAL_N_VUELTAS            10.0f
#define CAL_REDUCCION            10.0f
#define CAL_PPR_ESPERADO         7.0f

/** @brief Diámetro de la rueda **montada**, en mm (informativo).
 *
 * **Ø24 mm con la goma puesta**, medido con el calibre; la llanta sola es de Ø20 y la goma va de Ø20
 * interior a Ø24 exterior. Es el Ø nominal, y el µm/cuenta medido con las ruedas nuevas (**287,0**) sale
 * un **6,6 %** por encima de los 269,3 que le corresponden ⇒ el diámetro real en reposo debe ser mayor,
 * probablemente porque el **calibre comprime la goma** al medirla.
 *
 * @note Manda la medida empujando el robot (287,0), que es la que está en `ENCODER_UM_POR_CUENTA_DEF`.
 * @note Solo se publica por telemetría (`>CAL_DIAM_MM`); no entra en ningún cálculo.
 */
#define CAL_DIAMETRO_RUEDA_MM    24.0f
#define CAL_DUTY_MOTOR           35
#define CAL_PULSOS_OBJETIVO      2800
#define CAL_TIMEOUT_MS           15000U

/** @brief Tensión del pack por debajo de la cual la prueba frena y avisa.
 *
 * Si el riel se cae (un motor forzado pide mucha corriente), la placa se reinicia y se pierde el
 * diagnóstico: mejor frenar a tiempo y decirlo en la pantalla. */
#define CAL_VBAT_MIN              6.0f

/** @brief Margen entre pulsar OK (o salir del menú) y que los motores arranquen, en ms. */
#define CAL_ARRANQUE_MS          2000U

/** @brief Rampa de arranque: de `CAL_RAMPA_DUTY_INI` % a `CAL_DUTY_MOTOR` % en `CAL_RAMPA_PASOS` pasos.
 *
 * Los valores salen de medir el **duty mínimo de arranque** con `MIN M1`/`MIN M2`/`MIN AMBOS` (30-sep):
 * los dos motores arrancan entre el **25 % y el 29 %**, así que la rampa empieza por debajo (20 %) y se
 * trabaja con margen (35 %). Por debajo del mínimo el motor solo zumba y no se mueve. */
#define CAL_RAMPA_DUTY_INI       20
#define CAL_RAMPA_PASOS          10U
#define CAL_RAMPA_PASO_MS        50U

/** @brief Modo de la prueba 7, elegido en el menú. */
typedef enum {
    ENC_MODO_PPR = 1,   /**< A mano: girar la rueda `CAL_N_VUELTAS` vueltas y contar los pulsos   */
    ENC_MODO_UM         /**< Con motores: avanzar `CAL_PULSOS_OBJETIVO` y medir con la regla      */
} Enc_Modo_t;

/**
 * @brief Distancia medida con la cinta métrica, en mm, para el cálculo de los µm por cuenta.
 *
 * Es la distancia que separa las **dos marcas del suelo** entre las que se empuja el robot (modo 1) o lo
 * que avanza hasta parar (modo 2). Con 0 no se calculan los µm por cuenta y la pantalla vuelve a mostrar
 * el PPR.
 *
 * @note Puesta a **1000 mm** (1 m) para la medida empujando a mano: con cinta métrica en el suelo el
 *       error es mucho menor que contando vueltas de rueda, y una distancia larga lo reparte todavía más.
 */
#define CAL_DISTANCIA_MEDIDA_MM  1000.0f

/**
 * @brief Pone los contadores a cero de forma robusta y deja las referencias.
 *
 * Se hacen DOS lecturas seguidas tras el reset: la segunda da diferencia cero, así el total acumulado
 * queda limpio aunque update_encoder_readings() se hubiera llamado antes.
 *
 * @param base_l Referencia de la rueda izquierda
 * @param base_r Referencia de la rueda derecha
 */
static void cal_encoders_referencias(int32_t *base_l, int32_t *base_r)
{
    ENC_ResetCount1();
    ENC_ResetCount2();
    update_encoder_readings();
    update_encoder_readings();

    *base_l = get_encoder_left_total_count();
    *base_r = get_encoder_right_total_count();
}

/**
 * @brief Pinta los pulsos de las **dos** ruedas mientras se gira a mano (prueba 7, modo 1).
 *
 * @param pulsos_l Pulsos de la rueda izquierda desde el reset
 * @param pulsos_r Pulsos de la rueda derecha desde el reset
 * @param ppr PPR que sale de los pulsos de la izquierda con la reducción asumida
 * @note Las dos ruedas salen en pantalla porque el **sentido de conteo** de cada encoder se comprueba
 *       girando su rueda a mano hacia adelante: si el número **baja**, ese encoder cuenta al revés.
 * @note Cuando ya está apuntada `CAL_DISTANCIA_MEDIDA_MM`, la línea 2 muestra el **µm por cuenta** en
 *       lugar del PPR: con la distancia medida ese es el dato que manda y el PPR ya ha hecho su trabajo.
 */
static void app_pantalla_enc_ppr(int32_t pulsos_l, int32_t pulsos_r, float ppr)
{
    char linea[32];

    OLED_Clear();
    oled_print_line(0U, "PPR A MANO");

    snprintf(linea, sizeof(linea), "P1 %d P2 %d", (int)pulsos_l, (int)pulsos_r);
    oled_print_line(1U, linea);

    if (CAL_DISTANCIA_MEDIDA_MM > 0.0f) {
        float pulsos_medios = (float)(pulsos_l + pulsos_r) / 2.0f;
        float um_cuenta     = (pulsos_medios > 0.0f)
                                  ? (CAL_DISTANCIA_MEDIDA_MM * 1000.0f) / pulsos_medios
                                  : 0.0f;

        snprintf(linea, sizeof(linea), "UM/C %.1f", (double)um_cuenta);
    } else {
        snprintf(linea, sizeof(linea), "PPR %.2f", (double)ppr);
    }

    oled_print_line(2U, linea);

    oled_print_line(3U, "MENU: SALIR");

    OLED_Update();
}

/**
 * @brief Anuncia por telemetría los parámetros con los que se va a calibrar.
 *
 * @param modo Modo que se va a ejecutar (`ENC_MODO_PPR` o `ENC_MODO_UM`)
 */
static void cal_encoders_anunciar_parametros(uint8_t modo)
{
    TELEM_SendInt("CAL_MODO", (int32_t)modo);
    TELEM_SendFloat("CAL_NVUELTAS", CAL_N_VUELTAS);
    TELEM_SendFloat("CAL_REDUCCION", CAL_REDUCCION);
    TELEM_SendFloat("CAL_PPR_ESP", CAL_PPR_ESPERADO);
    TELEM_SendFloat("CAL_DIAM_MM", CAL_DIAMETRO_RUEDA_MM);
    TELEM_SendFloat("CAL_UM_ESP", ENCODER_UM_POR_CUENTA);
}

/**
 * @brief Modo 1 de la prueba 7: obtiene el PPR girando la rueda a mano.
 *
 * Se gira la rueda CAL_N_VUELTAS vueltas en el mismo sentido y se lee el total de pulsos; el PPR
 * implícito sale de `PPR = pulsos / (4 × vueltas × reducción)`. Si no sale entero, la reducción
 * asumida no es la correcta.
 *
 * @note **También saca los µm por cuenta sin arrancar los motores**, y es el camino recomendado mientras
 *       el robot no vaya recto o la placa se reinicie al forzar un motor: se marca una rueda, se empuja
 *       el robot a mano `CAL_N_VUELTAS` vueltas exactas, se mide con la regla lo que ha avanzado y ese
 *       valor se apunta en `CAL_DISTANCIA_MEDIDA_MM`. Empujando **no hay deslizamiento** de rueda (que
 *       sí aparece al arrancar) y el resultado es la circunferencia **real** sobre la mesa, con el
 *       aplastamiento del neumático incluido.
 *
 * @param base_l Referencia de la rueda izquierda
 * @param base_r Referencia de la rueda derecha
 */
static void cal_encoders_por_mano(int32_t base_l, int32_t base_r)
{
    uint32_t t_impresion = HAL_GetTick();


    while (!BUTTONS_SalidaSolicitada()) {
        update_encoder_readings();

        if ((HAL_GetTick() - t_impresion) >= 100U) {
            int32_t pulsos_l;
            int32_t pulsos_r;
            float   ppr;

            t_impresion = HAL_GetTick();

            pulsos_l = get_encoder_left_total_count() - base_l;
            pulsos_r = get_encoder_right_total_count() - base_r;
            ppr      = (float)pulsos_l / (4.0f * CAL_N_VUELTAS * CAL_REDUCCION);

            TELEM_SendInt("CAL_P1", pulsos_l);
            TELEM_SendInt("CAL_P2", pulsos_r);
            TELEM_SendFloat("CAL_PPR1", ppr);

            /* Con la distancia ya apuntada, esta pasada sirve para los µm por cuenta. Empujando el robot
               recto las dos ruedas giran casi lo mismo, así que vale la media. */
            if (CAL_DISTANCIA_MEDIDA_MM > 0.0f) {
                float pulsos_medios = (float)(pulsos_l + pulsos_r) / 2.0f;

                if (pulsos_medios > 0.0f) {
                    TELEM_SendFloat("CAL_UM_CUENTA",
                                    (CAL_DISTANCIA_MEDIDA_MM * 1000.0f) / pulsos_medios);
                }
            }

            app_pantalla_enc_ppr(pulsos_l, pulsos_r, ppr);
        }

        HAL_Delay(1);
    }
}

/**
 * @brief Pinta la cuenta atrás previa al arranque de los motores (prueba 7 y prueba de arranque).
 *
 * @param titulo Nombre de la prueba, que va en la línea de arriba
 * @param segundos Segundos que faltan para que arranquen
 * @note Existe porque el pico de arranque con el robot sujeto hace saltar la protección de la fuente y
 *       la placa se reinicia: estos segundos son para soltarlo.
 * @note La última línea lleva la **causa del último reinicio** (`RST n`, ver `hal/reset.h`). Va aquí,
 *       y no en una pantalla propia, porque es justo el momento en el que el usuario está mirando: si la
 *       placa se ha reiniciado sola, al volver a entrar en la prueba se ve de dónde vino.
 */
static void app_pantalla_espera(const char *titulo, uint8_t segundos)
{
    char linea[32];

    OLED_Clear();
    oled_print_line(0U, titulo);
    oled_print_line(1U, "SUELTA EL ROBOT");

    snprintf(linea, sizeof(linea), "ARRANCA EN %u", (unsigned)segundos);
    oled_print_line(2U, linea);

    if (RESET_GetCausa() != RESET_CAUSA_DESCONOCIDA) {
        snprintf(linea, sizeof(linea), "RST %d  MENU:ABORTA", (int)RESET_GetCausa());
        oled_print_line(3U, linea);
    } else {
        oled_print_line(3U, "MENU: ABORTA");
    }

    OLED_Update();
}

/**
 * @brief Pinta el resultado de la pasada con motores (prueba 7, modo 2).
 *
 * @param pulsos_l Pulsos contados en la rueda izquierda
 * @param pulsos_r Pulsos contados en la rueda derecha
 * @param aviso 1 si se agotó el tiempo antes de llegar al objetivo · 3 si la tensión del pack se cayó
 * @note Se pinta **al terminar**, no durante el avance: un `OLED_Update()` son ~50 ms de bus I2C, y
 *       con el lazo parado ese rato el frenado por encoder se pasaría de largo (a 30 % de duty son
 *       unas 200 cuentas).
 */
static void app_pantalla_enc_um(int32_t pulsos_l, int32_t pulsos_r, uint8_t aviso)
{
    const char *motivo = "MIDE CON REGLA";
    char        linea[32];

    OLED_Clear();
    oled_print_line(0U, "UM POR CUENTA");

    snprintf(linea, sizeof(linea), "P1 %d P2 %d", (int)pulsos_l, (int)pulsos_r);
    oled_print_line(1U, linea);

    if (aviso == 1U) {
        motivo = "TIEMPO AGOTADO";
    } else if (aviso == 3U) {
        motivo = "VBAT SE CAE";
    }

    oled_print_line(2U, motivo);

    oled_print_line(3U, "MENU: SALIR");

    OLED_Update();
}

/**
 * @brief Modo 2 de la prueba 7: avanza a lazo abierto y deja los µm por cuenta.
 *
 * Cada rueda se frena por separado al alcanzar CAL_PULSOS_OBJETIVO, así las dos recorren lo mismo y el
 * robot tiende a ir recto aunque los motores no sean idénticos. La distancia se mide con la regla y se
 * apunta en CAL_DISTANCIA_MEDIDA_MM para que la siguiente pasada calcule los µm por cuenta.
 *
 * @note Los pulsos de la frenada se cuentan también, para que la distancia medida y el total de pulsos
 *       se correspondan.
 * @note Antes de moverse hay una **cuenta atrás** (`CAL_ARRANQUE_MS`, para poder soltar el robot) y una
 *       **rampa de duty** (`CAL_RAMPA_*`) que sube los dos motores a la vez, para que arranquen rectos.
 * @param base_l Referencia de la rueda izquierda
 * @param base_r Referencia de la rueda derecha
 */
static void cal_encoders_con_motores(int32_t base_l, int32_t base_r)
{
    uint32_t t0;
    uint32_t t_vbat = HAL_GetTick();
    uint32_t t_pulsos = HAL_GetTick();
    uint8_t  run_l = 1U;
    uint8_t  run_r = 1U;
    uint8_t  aviso = 0U;
    uint8_t  segundos = (uint8_t)(CAL_ARRANQUE_MS / 1000U);
    int32_t  pulsos_l;
    int32_t  pulsos_r;
    float    pulsos_medios;

    /* Cuenta atrás antes de arrancar: si los motores empiezan a girar con el robot sujeto, el pico de
       arranque hace saltar la protección de la fuente y la placa se reinicia. */
    while ((segundos > 0U) && !BUTTONS_SalidaSolicitada()) {
        app_pantalla_espera("UM POR CUENTA", segundos);
        HAL_Delay(1000U);
        segundos--;
    }

    if (BUTTONS_SalidaSolicitada()) {
        return;   /* se ha salido durante la espera: los motores no llegan a arrancar */
    }

    /* Rampa de arranque: los dos motores suben **a la vez** de `CAL_RAMPA_DUTY_INI` a `CAL_DUTY_MOTOR`,
       para que el robot arranque recto. Los pulsos que se cuentan aquí valen para la medida, porque la
       referencia es la del principio. */
    for (uint8_t i = 0U; i < CAL_RAMPA_PASOS; i++) {
        uint8_t duty = (uint8_t)(CAL_RAMPA_DUTY_INI +
                                 (i * (CAL_DUTY_MOTOR - CAL_RAMPA_DUTY_INI)) /
                                     (CAL_RAMPA_PASOS - 1U));

        DRV8833_Set_Speed(&motor1, (int16_t)duty);
        DRV8833_Set_Speed(&motor2, (int16_t)duty);
        update_encoder_readings();
        HAL_Delay(CAL_RAMPA_PASO_MS);
    }

    t0 = HAL_GetTick();

    while ((run_l || run_r) && !BUTTONS_SalidaSolicitada()) {
        update_encoder_readings();
        BATTERY_Update();

        if ((HAL_GetTick() - t_vbat) >= 100U) {
            t_vbat = HAL_GetTick();
            TELEM_SendFloat("VBAT", BATTERY_GetPackV());
        }

        /* Si el riel se cae (el motor se fuerza al chocar o al sujetarlo), la placa se reinicia y no
           queda ni el diagnóstico: mejor frenar aquí y decirlo en la pantalla. */
        if ((BATTERY_PackPresente() != 0U) && (BATTERY_GetPackV() < CAL_VBAT_MIN)) {
            aviso = 3U;
            TELEM_SendInt("CAL_AVISO", 3);
            break;
        }

        pulsos_l = get_encoder_left_total_count() - base_l;
        pulsos_r = get_encoder_right_total_count() - base_r;

        /* Los pulsos de las dos ruedas durante el avance: en Teleplot se ve cuál frena antes y cuál
           sigue, que es lo que hace falta para entender una pasada rara. */
        if ((HAL_GetTick() - t_pulsos) >= 100U) {
            t_pulsos = HAL_GetTick();
            TELEM_SendInt("CAL_P1", pulsos_l);
            TELEM_SendInt("CAL_P2", pulsos_r);
        }

        if (run_l) {
            if (pulsos_l >= CAL_PULSOS_OBJETIVO) {
                DRV8833_Brake(&motor1);
                run_l = 0U;
            } else {
                DRV8833_Set_Speed(&motor1, CAL_DUTY_MOTOR);
            }
        }

        if (run_r) {
            if (pulsos_r >= CAL_PULSOS_OBJETIVO) {
                DRV8833_Brake(&motor2);
                run_r = 0U;
            } else {
                DRV8833_Set_Speed(&motor2, CAL_DUTY_MOTOR);
            }
        }

        if ((HAL_GetTick() - t0) > CAL_TIMEOUT_MS) {
            aviso = 1U;
            TELEM_SendInt("CAL_AVISO", 1);
            break;
        }

        HAL_Delay(1);
    }

    DRV8833_Brake(&motor1);
    DRV8833_Brake(&motor2);

    t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < 500U) {
        update_encoder_readings();
        HAL_Delay(1);
    }

    pulsos_l = get_encoder_left_total_count() - base_l;
    pulsos_r = get_encoder_right_total_count() - base_r;
    pulsos_medios = (float)(pulsos_l + pulsos_r) / 2.0f;

    TELEM_SendInt("CAL_P1", pulsos_l);
    TELEM_SendInt("CAL_P2", pulsos_r);
    TELEM_SendFloat("CAL_PPMED", pulsos_medios);

    if ((CAL_DISTANCIA_MEDIDA_MM > 0.0f) && (pulsos_medios > 0.0f)) {
        TELEM_SendFloat("CAL_UM_CUENTA", (CAL_DISTANCIA_MEDIDA_MM * 1000.0f) / pulsos_medios);
    } else {
        TELEM_SendInt("CAL_AVISO", 2);
    }

    app_pantalla_enc_um(pulsos_l, pulsos_r, aviso);

    while (!BUTTONS_SalidaSolicitada()) {
        HAL_Delay(1000);
    }
}

/**
 * @brief Calibra los encoders por medición: PPR (a mano) o µm por cuenta (con motores), prueba 7.
 *
 * @param modo `ENC_MODO_PPR` o `ENC_MODO_UM`
 * @note Procedimiento paso a paso en `Documentacion/pruebas-bringup.md` §7 y teoría en
 *       `Documentacion/calibracion-encoders.md`.
 * @see ENCODER_UM_POR_CUENTA es el valor del fabricante que esta prueba verifica.
 */
static void app_test_encoders(uint8_t modo)
{
    int32_t base_l;
    int32_t base_r;

    cal_encoders_referencias(&base_l, &base_r);
    cal_encoders_anunciar_parametros(modo);

    if (modo == ENC_MODO_PPR) {
        cal_encoders_por_mano(base_l, base_r);
    } else {
        cal_encoders_con_motores(base_l, base_r);
    }
}

/** @brief Prueba 7 modo 1: PPR girando la rueda `CAL_N_VUELTAS` vueltas a mano. */
void App_Test_PPR(void)
{
    app_test_encoders(ENC_MODO_PPR);
}

/**
 * @brief Prueba 7 modo 2: µm por cuenta avanzando con los motores.
 *
 * @warning **El robot avanza**: ruedas en el aire o con espacio de sobra. Da `CAL_ARRANQUE_MS` de
 *          margen para soltarlo (si arranca sujeto, el pico de corriente reinicia la placa) y luego va
 *          a `CAL_DUTY_MOTOR` hasta `CAL_PULSOS_OBJETIVO` (2800 cuentas ≈ 66 cm) o hasta agotar
 *          `CAL_TIMEOUT_MS`.
 */
void App_Test_UmCuenta(void)
{
    app_test_encoders(ENC_MODO_UM);
}

/**
 * @brief Parámetros de la prueba de arranque: el duty mínimo con el que se mueve cada motor.
 *
 * Sirve para elegir el duty inicial de la rampa de `UM MOTORES` (`CAL_RAMPA_DUTY_INI`): por debajo del
 * mínimo el motor solo zumba (el PWM no vence la fricción estática) y no se mueve nada.
 */
#define ARR_DUTY_INI        5U     /* % con el que empieza a probar                          */
#define ARR_DUTY_PASO       2U     /* % que sube en cada paso                                */
#define ARR_PASO_MS       400U     /* ms que se mantiene cada duty antes de mirar el encoder  */
#define ARR_CUENTAS_MIN     3U     /* cuentas que hay que ver para darlo por arrancado        */

/** @brief Motores que mueve la prueba de arranque (se combinan: `ARR_M1 | ARR_M2`). */
#define ARR_M1              1U
#define ARR_M2              2U
#define ARR_AMBOS          (ARR_M1 | ARR_M2)

/** @brief Cuánto duran los motores girando en la prueba de comparación, en ms. */
#define COMP_MS          2000U

/**
 * @brief Pinta el resultado de la comparación de motores.
 *
 * @param p1 Cuentas de la rueda izquierda
 * @param p2 Cuentas de la rueda derecha
 * @param desv Diferencia entre las dos, en % de la izquierda
 */
static void app_pantalla_compara(int32_t p1, int32_t p2, int32_t desv)
{
    char linea[32];

    OLED_Clear();
    oled_print_line(0U, "COMPARA M1 M2");

    snprintf(linea, sizeof(linea), "P1 %d P2 %d", (int)p1, (int)p2);
    oled_print_line(1U, linea);

    snprintf(linea, sizeof(linea), "DESV %d %%", (int)desv);
    oled_print_line(2U, linea);

    oled_print_line(3U, "MENU: SALIR");

    OLED_Update();
}

/**
 * @brief Da el mismo duty a los dos motores y compara lo que avanza cada rueda.
 *
 * Es la prueba que dice si un motor tira menos que el otro: con el mismo duty, la rueda que hace menos
 * cuentas es la que va más lenta, y esa diferencia es exactamente la que hace que el robot se vaya de
 * lado. Los dos reciben el mismo signo, así que sirve además para ver si giran en el mismo sentido.
 *
 * @note Telemetría: `>COMP_P1`, `>COMP_P2` (cuentas de cada rueda) y `>COMP_DESV` (diferencia en %).
 * @warning **El robot avanza**: ruedas en el aire o con espacio de sobra.
 * @see `Documentacion/pruebas-bringup.md` §7.4
 */
void App_Test_ComparaMotores(void)
{
    int32_t base_l;
    int32_t base_r;
    int32_t p1;
    int32_t p2;
    int32_t desv = 0;
    uint8_t segundos = (uint8_t)(CAL_ARRANQUE_MS / 1000U);

    cal_encoders_referencias(&base_l, &base_r);

    while ((segundos > 0U) && !BUTTONS_SalidaSolicitada()) {
        app_pantalla_espera("COMPARA M1 M2", segundos);
        HAL_Delay(1000U);
        segundos--;
    }

    if (BUTTONS_SalidaSolicitada()) {
        return;
    }

    for (uint8_t i = 0U; i < CAL_RAMPA_PASOS; i++) {
        uint8_t duty = (uint8_t)(CAL_RAMPA_DUTY_INI +
                                 (i * (CAL_DUTY_MOTOR - CAL_RAMPA_DUTY_INI)) /
                                     (CAL_RAMPA_PASOS - 1U));

        DRV8833_Set_Speed(&motor1, (int16_t)duty);
        DRV8833_Set_Speed(&motor2, (int16_t)duty);
        HAL_Delay(CAL_RAMPA_PASO_MS);
    }

    HAL_Delay(COMP_MS);
    update_encoder_readings();

    p1 = get_encoder_left_total_count() - base_l;
    p2 = get_encoder_right_total_count() - base_r;

    DRV8833_Brake(&motor1);
    DRV8833_Brake(&motor2);

    if (p1 != 0) {
        desv = ((p2 - p1) * 100) / p1;
    }

    TELEM_SendInt("COMP_P1", p1);
    TELEM_SendInt("COMP_P2", p2);
    TELEM_SendInt("COMP_DESV", desv);

    app_pantalla_compara(p1, p2, desv);

    while (!BUTTONS_SalidaSolicitada()) {
        HAL_Delay(1000);
    }
}

/**
 * @brief Pinta la prueba de arranque: el duty que se está probando y lo ya averiguado.
 *
 * @param motores Qué motores se mueven (`ARR_M1`, `ARR_M2` o `ARR_AMBOS`)
 * @param duty Duty (%) que se está probando
 * @param min_m1 Duty mínimo del motor 1 (0 = todavía no se ha movido)
 * @param min_m2 Duty mínimo del motor 2 (0 = todavía no se ha movido)
 */
static void app_pantalla_arranque(uint8_t motores, uint8_t duty, uint8_t min_m1, uint8_t min_m2)
{
    char linea[32];

    OLED_Clear();

    if (motores == ARR_AMBOS) {
        oled_print_line(0U, "DUTY MIN M1+M2");
    } else if (motores == ARR_M1) {
        oled_print_line(0U, "DUTY MIN M1");
    } else {
        oled_print_line(0U, "DUTY MIN M2");
    }

    snprintf(linea, sizeof(linea), "PROBANDO %u%%", (unsigned)duty);
    oled_print_line(1U, linea);

    snprintf(linea, sizeof(linea), "M1 %u  M2 %u", (unsigned)min_m1, (unsigned)min_m2);
    oled_print_line(2U, linea);

    oled_print_line(3U, "MENU: SALIR");

    OLED_Update();
}

/**
 * @brief Busca el duty mínimo con el que arrancan los motores, subiéndolo poco a poco.
 *
 * Sube el duty de `ARR_DUTY_INI` en pasos de `ARR_DUTY_PASO` y, en cada paso, mira si el encoder de ese
 * motor ha contado: cuando cuenta, ese es el duty que ha vencido la fricción estática. Por debajo de
 * ese valor el motor solo zumba.
 *
 * @param motores Qué motores se mueven: `ARR_M1`, `ARR_M2` o `ARR_AMBOS` (los dos con el mismo duty)
 * @note Telemetría: `>ARR_DUTY` en cada paso y `>ARR_M1`/`>ARR_M2` cuando se averigua cada mínimo.
 * @warning **Los motores se mueven**: ruedas en el aire o con espacio de sobra. Antes hay la cuenta
 *          atrás para soltar el robot.
 * @see Documentacion/pruebas-bringup.md §7.3
 */
static void app_test_arranque(uint8_t motores)
{
    int32_t base_l;
    int32_t base_r;
    uint8_t duty = ARR_DUTY_INI;
    uint8_t min_m1 = 0U;
    uint8_t min_m2 = 0U;
    uint8_t arrancados = 0U;
    uint8_t segundos = (uint8_t)(CAL_ARRANQUE_MS / 1000U);

    cal_encoders_referencias(&base_l, &base_r);

    while ((segundos > 0U) && !BUTTONS_SalidaSolicitada()) {
        app_pantalla_espera("MIN DUTY MOTOR", segundos);
        HAL_Delay(1000U);
        segundos--;
    }

    while ((duty <= 100U) && (arrancados != motores) && !BUTTONS_SalidaSolicitada()) {
        int32_t l0;
        int32_t r0;
        int32_t l1;
        int32_t r1;
        int32_t avance;

        if ((motores & ARR_M1) != 0U) {
            DRV8833_Set_Speed(&motor1, (int16_t)duty);
        }
        if ((motores & ARR_M2) != 0U) {
            DRV8833_Set_Speed(&motor2, (int16_t)duty);
        }

        l0 = get_encoder_left_total_count() - base_l;
        r0 = get_encoder_right_total_count() - base_r;

        TELEM_SendInt("ARR_DUTY", (int32_t)duty);

        HAL_Delay(ARR_PASO_MS);
        update_encoder_readings();

        l1 = get_encoder_left_total_count() - base_l;
        r1 = get_encoder_right_total_count() - base_r;

        if (((motores & ARR_M1) != 0U) && ((arrancados & ARR_M1) == 0U)) {
            avance = l1 - l0;
            if (avance < 0) {
                avance = -avance;
            }
            if (avance >= (int32_t)ARR_CUENTAS_MIN) {
                arrancados |= ARR_M1;
                min_m1 = duty;
                TELEM_SendInt("ARR_M1", (int32_t)min_m1);
            }
        }

        if (((motores & ARR_M2) != 0U) && ((arrancados & ARR_M2) == 0U)) {
            avance = r1 - r0;
            if (avance < 0) {
                avance = -avance;
            }
            if (avance >= (int32_t)ARR_CUENTAS_MIN) {
                arrancados |= ARR_M2;
                min_m2 = duty;
                TELEM_SendInt("ARR_M2", (int32_t)min_m2);
            }
        }

        app_pantalla_arranque(motores, duty, min_m1, min_m2);

        duty = (uint8_t)(duty + ARR_DUTY_PASO);
    }

    DRV8833_Brake(&motor1);
    DRV8833_Brake(&motor2);

    app_pantalla_arranque(motores, duty, min_m1, min_m2);

    while (!BUTTONS_SalidaSolicitada()) {
        HAL_Delay(1000);
    }
}

/** @brief Duty mínimo del motor 1, subiendo el PWM hasta que su encoder cuenta. */
void App_Test_MinM1(void)
{
    app_test_arranque(ARR_M1);
}

/** @brief Duty mínimo del motor 2, subiendo el PWM hasta que su encoder cuenta. */
void App_Test_MinM2(void)
{
    app_test_arranque(ARR_M2);
}

/**
 * @brief Duty mínimo con los dos motores a la vez, que es como los pide la rampa de `UM MOTORES`.
 *
 * El mismo duty para los dos: se anota por separado en qué valor arranca cada uno.
 */
void App_Test_MinAmbos(void)
{
    app_test_arranque(ARR_AMBOS);
}

/**
 * @brief Publica por telemetría las pulsaciones y el estado mantenido de los 4 botones (prueba 8).
 *
 * @note Sustituida por la prueba 10 (botones + pantalla) para el uso normal; se conserva porque es la
 *       que comprueba BUTTONS_GetMask() y el estado mantenido de cada botón.
 * @note La pulsación de MENU sale de la prueba (va por interrupción), así que aquí no se ve como
 *       evento: en el banco, MENU siempre significa "volver al menú".
 */
void App_Test_Botones(void)
{
    uint32_t t_impresion = HAL_GetTick();


    while (!BUTTONS_SalidaSolicitada()) {
        uint8_t btn;

        BUTTONS_Update();

        btn = BUTTONS_GetEvent();
        if (btn != BTN_NUM_NINGUNO) {
            TELEM_SendButton(btn);
        }

        if ((HAL_GetTick() - t_impresion) >= 100U) {
            t_impresion = HAL_GetTick();

            TELEM_SendInt("BTN_MASK", (int32_t)BUTTONS_GetMask());
            TELEM_SendInt("BTN_MENU", (int32_t)BUTTONS_IsPressed(BTN_MENU));
            TELEM_SendInt("BTN_UP",   (int32_t)BUTTONS_IsPressed(BTN_UP));
            TELEM_SendInt("BTN_DOWN", (int32_t)BUTTONS_IsPressed(BTN_DOWN));
            TELEM_SendInt("BTN_OK",   (int32_t)BUTTONS_IsPressed(BTN_OK));
        }

        HAL_Delay(1);
    }
}

/**
 * @brief Parámetros de la prueba 9 (rampas de PWM medidas en ticks de encoder).
 *
 * La unidad de la rampa son TICKS del encoder (500 ticks ≈ 1 s), así que 1500 ticks ≈ 3 s, y el ritmo
 * lo marca la rueda, no el reloj. La pausa va en milisegundos a propósito: frenado no hay ticks.
 */
#define PWMRAMPA_TICKS             1500U
#define PWMRAMPA_PASOS             100U
#define PWMRAMPA_PAUSA_MS          2000U
#define PWMRAMPA_TICKS_TIMEOUT_MS  300U
#define PWMRAMPA_FASE_TIMEOUT_MS   20000U

/**
 * @brief Motor(es) que mueve la prueba 9.
 *
 * 1 = izquierdo (PA8/PA9) · 2 = derecho (PA10/PA11) · 0 = los dos. Con 1 o 2 la otra rueda se deja
 * frenada; con 0 el robot se mueve de verdad, así que hay que dejarlo con las ruedas en el aire.
 */
#define PWM_RAMPA_MOTOR    0

static void pwm_rampa_enviar_telemetria(int32_t fase, int16_t duty, int16_t base,
                                        uint32_t t_anterior, int16_t contador_anterior);

/**
 * @brief Lee el contador crudo del encoder de la rueda (o ruedas) que se está moviendo.
 *
 * Devuelve int16_t a propósito: los contadores de 16 bits giran en los dos sentidos y, con signo, la
 * resta contra la referencia sale bien al cruzar el cero o el desbordamiento. Solo es válido si entre
 * dos lecturas no se pasa de ±32767 ticks, y aquí se lee cada pocos milisegundos.
 *
 * @return Ticks del encoder izquierdo (1), del derecho (2) o la media de los dos (0)
 */
static int16_t pwm_ticks_contador(void)
{
#if PWM_RAMPA_MOTOR == 1
    return (int16_t)ENC_GetCount1();
#elif PWM_RAMPA_MOTOR == 2
    return (int16_t)ENC_GetCount2();
#else
    int16_t izq = (int16_t)ENC_GetCount1();
    int16_t der = (int16_t)ENC_GetCount2();
    return (int16_t)(((int32_t)izq + (int32_t)der) / 2);
#endif
}

/**
 * @brief Ticks recorridos desde una referencia, en valor absoluto.
 *
 * Se toma el valor absoluto para que la fase 2 (giro al revés) también avance.
 *
 * @param base Referencia tomada al empezar la fase
 * @return Ticks recorridos, siempre positivos
 */
static int32_t pwm_ticks_avance(int16_t base)
{
    int16_t delta = (int16_t)(pwm_ticks_contador() - base);

    return (delta < 0) ? -(int32_t)delta : (int32_t)delta;
}

/**
 * @brief Rampa lineal de duty con el ritmo marcado por el encoder.
 *
 * Cada paso aplica su duty y espera a que la rueda acumule los ticks que le tocan
 * (PWMRAMPA_TICKS / PWMRAMPA_PASOS). Si un paso se queda sin ticks más de PWMRAMPA_TICKS_TIMEOUT_MS se
 * pasa al siguiente duty igualmente, para que la prueba no se cuelgue si el motor no arranca en frío;
 * si la fase entera pasa de PWMRAMPA_FASE_TIMEOUT_MS, se aborta.
 *
 * @param motores Motores que se mueven en esta fase
 * @param n_motores Cuántos son
 * @param desde Duty inicial, en %
 * @param hasta Duty final, en %
 * @param fase Número de fase, para la telemetría
 * @return 1 si la fase terminó, 0 si se abortó por timeout
 */
static uint8_t pwm_rampa(DRV8833_Motor_t **motores, uint8_t n_motores,
                         int16_t desde, int16_t hasta, int32_t fase)
{
    int32_t  pasos = (hasta > desde) ? (hasta - desde) : (desde - hasta);
    int16_t  base;
    uint32_t t_fase;
    uint32_t t_anterior;
    int16_t  contador_anterior;

    if (pasos < 1) {
        pasos = 1;
    }

    base   = pwm_ticks_contador();
    t_fase = HAL_GetTick();
    t_anterior = t_fase;
    contador_anterior = base;

    for (int32_t i = 0; i <= pasos; i++) {
        int16_t duty = (int16_t)(desde + ((int32_t)(hasta - desde) * i) / pasos);
        int32_t ticks_objetivo = ((int32_t)i * (int32_t)PWMRAMPA_TICKS) / pasos;
        uint32_t t_paso;

        for (uint8_t m = 0; m < n_motores; m++) {
            DRV8833_Set_Speed(motores[m], duty);
        }

        t_paso = HAL_GetTick();
        while (pwm_ticks_avance(base) < ticks_objetivo) {
            if ((HAL_GetTick() - t_paso) > PWMRAMPA_TICKS_TIMEOUT_MS) {
                TELEM_SendInt("PWM_AVISO", 1);
                break;
            }
            if ((HAL_GetTick() - t_fase) > PWMRAMPA_FASE_TIMEOUT_MS) {
                TELEM_SendInt("PWM_AVISO", 2);
                return 0;
            }
        }

        pwm_rampa_enviar_telemetria(fase, duty, base, t_anterior, contador_anterior);

        t_anterior = HAL_GetTick();
        contador_anterior = pwm_ticks_contador();
    }

    return 1;
}

/**
 * @brief Envía la telemetría de un paso de la rampa, incluida la velocidad instantánea.
 *
 * @param fase Fase en curso
 * @param duty Duty aplicado, en %
 * @param base Referencia de la fase
 * @param t_anterior Marca de tiempo del paso anterior
 * @param contador_anterior Contador del encoder en el paso anterior
 */
static void pwm_rampa_enviar_telemetria(int32_t fase, int16_t duty, int16_t base,
                                        uint32_t t_anterior, int16_t contador_anterior)
{
    uint32_t t_ahora = HAL_GetTick();
    int16_t  contador_ahora = pwm_ticks_contador();
    int32_t  dt = (int32_t)(t_ahora - t_anterior);

    TELEM_SendInt("PWM_FASE", fase);
    TELEM_SendInt("PWM_DUTY", duty);
    TELEM_SendInt("PWM_TICKS", pwm_ticks_avance(base));
    TELEM_SendInt("ENC_IZQ", (int16_t)ENC_GetCount1());
    TELEM_SendInt("ENC_DER", (int16_t)ENC_GetCount2());

    if (dt > 0) {
        int16_t ticks_paso = (int16_t)(contador_ahora - contador_anterior);

        TELEM_SendInt("PWM_TPS", ((int32_t)ticks_paso * 1000) / dt);
    }
}

/**
 * @brief Frena los motores de la prueba 9, lo anuncia y espera la pausa entre fases.
 *
 * @param motores Motores a frenar
 * @param n_motores Cuántos son
 */
static void pwm_rampa_frenar_y_pausar(DRV8833_Motor_t **motores, uint8_t n_motores)
{
    for (uint8_t m = 0; m < n_motores; m++) {
        DRV8833_Brake(motores[m]);
    }

    TELEM_SendInt("PWM_DUTY", 0);
    TELEM_SendInt("PWM_EVT", 1);
    HAL_GPIO_WritePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN, GPIO_PIN_SET);
    HAL_Delay(PWMRAMPA_PAUSA_MS);
}

/**
 * @brief Mueve los motores en rampas de duty medidas en ticks de encoder (prueba 9).
 *
 * Secuencia en bucle: rampa 0 → +100 % en 1500 ticks, frenada en seco, pausa, rampa −100 % → 0 % en
 * 1500 ticks, frenada y pausa. El LED onboard queda encendido durante las rampas.
 *
 * @note El salto de 0 % a −100 % al empezar la fase 2 es intencionado (es la secuencia pedida) y
 *       supone un pico de corriente; para arrancar suave, llamar a pwm_rampa() con desde = 0.
 * @warning Con PWM_RAMPA_MOTOR = 0 se mueven las dos ruedas: el robot avanza y retrocede de verdad.
 * @note Se sale con MENU, entre fase y fase (cada fase dura unos 3 s).
 */
void App_Test_PWM_Rampa(void)
{
    DRV8833_Motor_t *activos[2];
    uint8_t n_activos = 0;

#if PWM_RAMPA_MOTOR == 1
    activos[n_activos++] = &motor1;
    DRV8833_Brake(&motor2);
    TELEM_SendInt("PWM_MOTOR", 1);
#elif PWM_RAMPA_MOTOR == 2
    activos[n_activos++] = &motor2;
    DRV8833_Brake(&motor1);
    TELEM_SendInt("PWM_MOTOR", 2);
#else
    activos[n_activos++] = &motor1;
    activos[n_activos++] = &motor2;
    TELEM_SendInt("PWM_MOTOR", 0);
#endif

    TELEM_SendInt("PWM_MOTOR", PWM_RAMPA_MOTOR);
    TELEM_SendInt("PWM_TICKS_RAMPA", (int32_t)PWMRAMPA_TICKS);

    while (!BUTTONS_SalidaSolicitada()) {
        HAL_GPIO_WritePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN, GPIO_PIN_RESET);
        pwm_rampa(activos, n_activos, 0, 100, 1);
        pwm_rampa_frenar_y_pausar(activos, n_activos);

        HAL_GPIO_WritePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN, GPIO_PIN_RESET);
        pwm_rampa(activos, n_activos, -100, 0, 2);
        pwm_rampa_frenar_y_pausar(activos, n_activos);
    }
}

/**
 * @brief Publica el estado del bus I2C1: niveles en reposo y dispositivos que responden.
 *
 * Lo usan la prueba 10 (al arrancar) y la 11 (en bucle).
 */
static void i2c_info(void)
{
    uint8_t dirs[8];
    uint8_t niveles = I2C_BusLevels();
    uint8_t n       = I2C_Scan(dirs, sizeof(dirs));

    TELEM_SendInt("I2C_SCL", (niveles & 0x01) ? 1 : 0);
    TELEM_SendInt("I2C_SDA", (niveles & 0x02) ? 1 : 0);
    TELEM_SendInt("I2C_N", (int32_t)n);

    for (uint8_t i = 0; i < n; i++) {
        TELEM_SendInt("I2C_FOUND", (int32_t)dirs[i]);
    }
}

/**
 * @brief Muestra el botón pulsado (nombre y pin) en la OLED y publica el estado del bus (prueba 10).
 *
 * Arranca con "PULSA UN BOTON" y, al pulsar, dibuja el nombre y el pin del botón a escala ×2, dos
 * líneas de 16 px que llenan los 32 px de la pantalla. Publica además el estado de I2C1 al empezar, para
 * no tener que cambiar de prueba si la pantalla no aparece.
 *
 * @note Si la pantalla no responde, APP_InitAll() ya avisa con `>OLED:no detectada`: revisar SDA/SCL,
 *       la alimentación y las pull-ups del bus.
 */
void App_Test_BotonesOLED(void)
{
    i2c_info();

    OLED_Clear();
    OLED_PrintCentrado(12, 1, "PULSA UN BOTON");
    OLED_Update();

    while (!BUTTONS_SalidaSolicitada()) {
        uint8_t btn;

        BUTTONS_Update();

        btn = BUTTONS_GetEvent();
        if (btn != BTN_NUM_NINGUNO) {
            const char *pin = BUTTONS_GetPinName(btn);

            TELEM_SendButton(btn);

            OLED_Clear();
            OLED_PrintCentrado(0, 2, BUTTONS_GetName(btn));
            OLED_PrintCentrado(16, 2, pin);
            OLED_Update();
        }

        HAL_Delay(1);
    }
}

/**
 * @brief Escanea el bus I2C1 y publica niveles en reposo y dispositivos que responden (prueba 11).
 *
 * Repite cada 2 s para poder ir tocando el cableado y ver el efecto. Los niveles se leen con los pines
 * en flotante: en un bus sano SCL y SDA están altos porque algo los sube.
 *
 * @note Interpretación de los niveles: si los dos leen 0 no hay nadie que suba el bus (faltan
 *       pull-ups); si solo uno lee 0, esa línea está a GND (cortocircuito o cable mal).
 * @note El SSD1306 típico responde en 0x3C = 60 (o 0x3D = 61).
 * @warning En el STM32F1 el pull-up interno no se puede activar en AF_OD (el campo `Pull` solo se aplica
 *          en modo entrada, comprobado en stm32f1xx_hal_gpio.c), así que el I2C necesita pull-ups
 *          externos: 2× 4.7 kΩ de PB6 y PB7 a 3.3 V. Si son débiles, bajar I2C1_CLOCK_HZ a 50000
 *          en `hal/i2c.h`.
 */
void App_Test_I2C_Scan(void)
{

    while (!BUTTONS_SalidaSolicitada()) {
        i2c_info();

        HAL_Delay(2000);
    }
}

/**
 * @brief Avisa de batería baja parpadeando el LED onboard y, en el FAULT, también por pantalla.
 *
 * Parpadeo lento (1 Hz) con BATTERY_AVISO y rápido (4 Hz) con BATTERY_CRITICA.
 *
 * @param mostrar_salida **1** en el FAULT: además del LED, pinta qué está bajo (BATERIA o CELDA), su
 *        tensión y cómo salir. **0** en la prueba 12: **solo el LED**, porque allí la pantalla la lleva
 *        `app_pantalla_bateria()`, que enseña las tres tensiones y se pinta siempre.
 *
 * @note Se llama desde el bucle principal a ~1 kHz, así que solo redibuja cuando hace falta.
 * @note Con BATTERY_OK solo apaga el LED al cambiar de estado: el LED onboard es activo a bajo, y
 *       dejarlo encendido pisaría los avisos de los demás estados.
 */
static void app_aviso_bateria(uint8_t mostrar_salida)
{
    static Battery_Estado_t estado_prev = BATTERY_AVISO;
    static uint8_t  primera_vez = 1;
    static uint32_t t_led = 0;
    static uint32_t t_pantalla = 0;
    static uint8_t  led_encendido = 0;

    Battery_Estado_t estado = BATTERY_GetEstado();
    uint8_t cambio = (uint8_t)(primera_vez || (estado != estado_prev));

    primera_vez = 0;
    estado_prev = estado;

    if (estado == BATTERY_OK) {
        if (cambio) {
            HAL_GPIO_WritePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN, GPIO_PIN_SET);
            led_encendido = 0;
        }
        return;
    }

    uint32_t semiperiodo = (estado == BATTERY_CRITICA) ? 125U : 500U;

    if (cambio || ((HAL_GetTick() - t_led) >= semiperiodo) || (t_led == 0U)) {
        t_led = HAL_GetTick();
        led_encendido = (uint8_t)(!led_encendido);
        HAL_GPIO_WritePin(LEDINBOARD_GPIO_PORT, LEDINBOARD_PIN,
                          led_encendido ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }

    if ((mostrar_salida != 0U) && (cambio || ((HAL_GetTick() - t_pantalla) >= 1000U))) {
        char        texto[24];
        const char *rotulo = BATTERY_AvisoCelda() ? "CELDA" : "BATERIA";

        t_pantalla = HAL_GetTick();

        OLED_Clear();

        /* El FAULT lleva tres líneas a escala 1: con las dos a escala 2 no cabe decir cómo salir */
        snprintf(texto, sizeof(texto), "%s %.2fV", rotulo, (double)BATTERY_GetAvisoV());
        oled_print_line(0U, texto);
        oled_print_line(1U, "MOTORES PARADOS");
        oled_print_line(2U, "OK: VOLVER AL MENU");

        OLED_Update();
    }
}

/**
 * @brief Pinta la medida de batería en la OLED: pack y las dos celdas (pantalla de la prueba 12).
 *
 * Las tres tensiones salen juntas porque son lo que hace falta para juzgar el pack: el **total**, la
 * **celda 1** medida directa y la **celda 2**, que **sale por resta** (`v_pack - v_celda1`).
 *
 * @note Se pinta **siempre**, haya aviso o no. Antes solo se pintaba con el pack sano, así que con un
 *       aviso de celda la pantalla se quedaba en la tensión de esa celda y **desaparecía el total**
 *       (visto el 30-sep) — y el aviso es justo cuando más falta hace ver las tres.
 * @note Sin esto la prueba **parece colgada**: el LED solo avisa cuando la batería está baja, así que
 *       con el pack sano el cartel de la prueba se queda fijo aunque esté midiendo y publicando.
 * @see `app_aviso_bateria()`, que en la prueba 12 solo lleva el LED.
 */
static void app_pantalla_bateria(void)
{
    char        texto[24];
    const char *estado = "OK";

    if (BATTERY_GetEstado() == BATTERY_AVISO) {
        estado = "AVISO";
    } else if (BATTERY_GetEstado() == BATTERY_CRITICA) {
        estado = "CRITICA";
    }

    OLED_Clear();

    snprintf(texto, sizeof(texto), "BATERIA %s", estado);
    oled_print_line(0U, texto);

    if (BATTERY_PackPresente() != 0U) {
        snprintf(texto, sizeof(texto), "TOTAL %5.2fV", (double)BATTERY_GetPackV());
    } else {
        snprintf(texto, sizeof(texto), "TOTAL SIN MEDIDA");
    }
    oled_print_line(1U, texto);

    if (BATTERY_Celda1Presente() != 0U) {
        snprintf(texto, sizeof(texto), "CEL1  %5.2fV", (double)BATTERY_GetCelda1V());
        oled_print_line(2U, texto);

        snprintf(texto, sizeof(texto), "CEL2  %5.2fV", (double)BATTERY_GetCelda2V());
        oled_print_line(3U, texto);
    } else {
        oled_print_line(2U, "CEL1  SIN DIVISOR");

        snprintf(texto, sizeof(texto), "K     %.4f", (double)BATTERY_GetKDivisor());
        oled_print_line(3U, texto);
    }

    OLED_Update();
}

/**
 * @brief Pantalla común de las pruebas de banco cuyo resultado va por telemetría.
 *
 * @param titulo Nombre de la prueba (una línea, sin acentos)
 * @param estado Qué está haciendo la prueba ahora mismo (una línea, sin acentos); `""` para no ponerla
 * @note Se repinta como mucho cada 500 ms, así que se puede llamar en cada vuelta del bucle sin frenar
 *       la medida. El contador `LATIDO` es la señal de vida: sin él, un cartel fijo parece una prueba
 *       colgada aunque el robot esté midiendo tan tranquilo.
 */
static void app_pantalla_telemetria(const char *titulo, const char *estado)
{
    static uint32_t t_ultima = 0U;
    static uint32_t latidos  = 0U;
    uint32_t        ahora    = HAL_GetTick();
    char            linea[24];

    if ((t_ultima != 0U) && ((ahora - t_ultima) < 500U)) {
        return;
    }
    t_ultima = ahora;
    latidos++;

    OLED_Clear();
    oled_print_line(0U, titulo);

    if (estado[0] != '\0') {
        oled_print_line(1U, estado);
    }

    snprintf(linea, sizeof(linea), "VER TELEPLOT %u", (unsigned)latidos);
    oled_print_line(2U, linea);
    oled_print_line(3U, "MENU: SALIR");

    OLED_Update();
}

/**
 * @brief Publica la tensión del pack y de las dos celdas, y avisa si están bajas (prueba 12).
 *
 * @note El ADC mide tras el divisor de 22 kΩ / 10 kΩ (k = 0.3125, o sea 8.4 V → 2.625 V en el pin),
 *       sin condensador porque no hay sitio. PB0 mide el pack completo y PB1 la celda 1 (la pegada a
 *       GND); la celda 2 **sale por resta** (`pack - celda 1`). Cada canal lleva **su** $k$, porque los
 *       divisores **no son iguales** (ver `sensor/battery.h`). Si PB1 lee ≈0 V (divisor sin montar) se publica `>CELDA1_USADA:0` y el
 *       estado sale solo del pack; si es PB0 el que no mide (placa alimentada por USB o ST-Link) se
 *       publica `>PACK_PRESENTE:0` y no se clasifica nada. En el banco el robot se alimenta con una
 *       fuente y esos canales quedan al aire: un 0 ahí no es una avería y no debe dejar el robot en
 *       FAULT, porque el FAULT no se sale hasta que la batería deja de estar crítica. Las celdas son
 *       **monitorización** (`>VCELL1`, `>VCELL2`, `>VBAT_DESEQ`, `>CELDA_AVISO`): solo el pack puede
 *       llevar el estado a CRÍTICA. Los umbrales se cambian en caliente desde la consola
 *       (`BAT AVISO_PACK 7.4`, ver [comandos-serie.md](../Documentacion/comandos-serie.md)), y por eso
 *       esta prueba llama a CONSOLE_Update(): se puede ajustar el umbral mientras se ve la tensión.
 * @note La pantalla la pinta `app_pantalla_bateria()` **siempre** (estado, total, celda 1 y celda 2),
 *       haya aviso o no: `app_aviso_bateria()` solo lleva aquí el LED. Antes la pantalla normal se
 *       saltaba cuando había aviso, y con un aviso de celda desaparecía el total de la batería.
 * @warning Calibrar BAT_K_DIVISOR (`sensor/battery.c`) midiendo la tensión real con el polímetro,
 *          viendo cuánto marca `>VBAT` y corrigiendo la constante: así se compensan de golpe la
 *          tolerancia de las resistencias y la de VREF+ = VDDA.
 * @see La medida la hace la capa SENSOR (`sensor/battery.c`); aquí solo se presenta.
 */
void App_Test_Bateria(void)
{
    uint32_t t_impresion = HAL_GetTick();

    BATTERY_Init();

    while (!BUTTONS_SalidaSolicitada()) {
        BATTERY_Update();
        CONSOLE_Update();
        app_aviso_bateria(0U);

        TELEM_SendInt("VBAT_ESTADO", (int32_t)BATTERY_GetEstado());
        TELEM_SendInt("PACK_PRESENTE", (int32_t)BATTERY_PackPresente());
        TELEM_SendInt("CELDA1_USADA", (int32_t)BATTERY_Celda1Presente());
        TELEM_SendInt("CELDA_AVISO", (int32_t)BATTERY_AvisoCelda());

        if ((HAL_GetTick() - t_impresion) >= 200U) {
            t_impresion = HAL_GetTick();

            TELEM_SendFloat("VBAT", BATTERY_GetPackV());
            TELEM_SendFloat("VCELL1", BATTERY_GetCelda1V());
            TELEM_SendFloat("VCELL2", BATTERY_GetCelda2V());
            TELEM_SendFloat("VBAT_DESEQ", BATTERY_GetDesequilibrioV());

            /* El factor del divisor en uso: dice de un vistazo si la medida cuadra con el hardware
               montado, y de paso si el firmware que corre es el que se acaba de compilar. */
            TELEM_SendFloat("BAT_K", BATTERY_GetKDivisor());

            /* La medida también en pantalla y **siempre**: con el pack sano no hay aviso que pintar, y
               con aviso interesa seguir viendo las tres tensiones. */
            app_pantalla_bateria();
        }

        HAL_Delay(1);
    }
}

/** @brief Canales del ADC de los receptores de pared, en orden `WALL A1`..`A4` (PA2..PA5). */
static const ADC_Pin_t ir_adc_canal[4] = { ADC_PIN_A2, ADC_PIN_A3, ADC_PIN_A4, ADC_PIN_A5 };

/** @brief Pines de los emisores de pared, en orden `WALL L1`..`L4`.
 *
 * ⚠️ El orden `L1`..`L4` **no es geométrico** (posiciones medidas el 30-sep, prueba 14): L1 y L4 son
 *    los frontales (izquierdo y derecho) y L2/L3 los laterales (derecho e izquierdo). La tabla está
 *    en `hardware.md` §4.4, y es la que hay que mirar al emparejar emisor↔receptor (prueba 15). */
static const uint16_t ir_emisor_pin[4] = { IR_E1_PIN, IR_E2_PIN, IR_E3_PIN, IR_E4_PIN };

/**
 * @brief Emisor que ve cada receptor, medido en la prueba 15 (`IRn`) que usan las pruebas 16 y 17.
 *
 * Está en la **numeración de la telemetría** (`IR0`..`IR3`, la misma de `>IR0`..`>IR3`):
 * `ir_pareja_emisor[canal]` es el emisor que hace crecer la lectura de ese receptor.
 *
 * Medido con la **matriz 4×4** de la prueba 15 (02-oct, robot a 10 cm de una pared y los LEDs
 * apantallados con tubo negro): **R0←IR3, R1←IR1, R2←IR2 y R3←IR0**.
 *
 * @warning **Los laterales no sirven para esto**: con una pared a 10 cm de su lado y sin ella leen lo
 *          mismo (546 contra 544 en `R2`), así que su `DIF` es acoplamiento con su emisor y no pared.
 *          La pareja está puesta igualmente porque es la del 30-sep y **no es lo que limita** —el
 *          cruzado da el mismo resultado—, pero **`IR MODULADO` y `CURVA IR` no dicen nada útil de
 *          `R1` ni de `R2`**. Ver `app/walls.c` para la historia completa.
 */
static const uint8_t ir_pareja_emisor[4] = { 3U, 1U, 2U, 0U };

/** @brief Número de receptores y de emisores de pared. */
#define IR_N_CANALES        4U

/** @brief Valor de `>IR_LED` cuando lucen los cuatro emisores a la vez. */
#define IR_LED_TODOS        5U

/** @brief Muestras que se promedian en cada lectura suelta (≈0.4 ms de ADC). */
#define IR_MUESTRAS_ADC     16U

/** @brief Pares ON/OFF que se promedian en la prueba 16. */
#define IR_PARES_16         8U

/** @brief Tiempo que se queda encendido cada emisor en la prueba 15, en ms. */
#define IR_PASO_MS        2000U

/** @brief Tiempo que se queda encendido cada paso de la prueba 14, en ms. */
#define IR_LED_MS           1000U

/** @brief Distancias (cm) de la curva de calibración, en el orden en que se piden. */
static const uint8_t ir_cal_distancias[] = { 3U, 5U, 8U, 10U, 15U };

/** @brief Puntos de la curva de calibración y muestras que se congelan en cada punto. */
#define IR_CAL_PUNTOS       ((uint8_t)(sizeof(ir_cal_distancias) / sizeof(ir_cal_distancias[0])))
#define IR_CAL_MUESTRAS     64U

/**
 * @brief Veredicto del chequeo de soldadura de un canal (prueba 13).
 */
typedef enum {
    ADC_EST_OK = 0,       /**< El canal responde y está en rango                        */
    ADC_EST_MUDO,         /**< Clavado a 0: pin sin conectar, soldadura fría o pista rota */
    ADC_EST_SATURADO,     /**< Pegado a 4095: ambiente, RL alta o C/E invertidos         */
    ADC_EST_FLOJO,        /**< Responde, pero con muy poca señal                         */
    ADC_EST_SIN_DIVISOR,  /**< Canal de batería sin divisor montado                      */
    ADC_EST_MAL_DIVISOR,  /**< Canal de batería con el divisor mal (R2 alta o R1 puenteada) */
    ADC_EST_CRUZADOS,     /**< Los dos canales de batería leen lo mismo                  */
    ADC_EST_NO_USADA      /**< Canal de la celda 1 sin divisor: en el banco no se usa     */
} Adc_Estado_t;

/** @brief Medida de un receptor con el emisor apagado y encendido. */
typedef struct {
    uint16_t apagado;   /**< Lectura media con el emisor apagado (solo luz ambiente) */
    uint16_t encendido; /**< Lectura media con el emisor encendido                   */
    int16_t  dif;       /**< Media de encendido − apagado: la señal que buscamos     */
    int16_t  dif_min;   /**< La menor de las diferencias medidas (dispersión)        */
    int16_t  dif_max;   /**< La mayor de las diferencias medidas                     */
} Ir_ParMedidas_t;

/**
 * @brief Lectura promediada de un canal del ADC.
 *
 * @param pin Canal a leer
 * @param muestras Cuántas conversiones se promedian (como mínimo 1)
 * @return Cuentas del ADC (0..4095)
 */
static uint16_t adc_leer_media(ADC_Pin_t pin, uint8_t muestras)
{
    uint32_t suma = 0U;

    if (muestras == 0U) {
        muestras = 1U;
    }

    for (uint8_t i = 0U; i < muestras; i++) {
        suma += ADC_ReadPin(pin);
    }

    return (uint16_t)(suma / muestras);
}

/**
 * @brief Enciende un emisor de pared y apaga el resto.
 *
 * @param emisor 1..4 para uno concreto, `IR_LED_TODOS` para los cuatro y 0 para apagarlos
 */
static void ir_emisor_seleccionar(uint8_t emisor)
{
    for (uint8_t i = 0U; i < IR_N_CANALES; i++) {
        uint8_t encender = (uint8_t)((emisor == IR_LED_TODOS) || (emisor == (uint8_t)(i + 1U)));

        HAL_GPIO_WritePin(IR_GPIO_PORT, ir_emisor_pin[i],
                          encender ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

/**
 * @brief Mide un receptor con su emisor apagado y encendido, promediando varios pares.
 *
 * La diferencia ON − OFF es lo que cancela la luz ambiente: cae en los dos términos y se va al restar.
 * Se devuelven también la mayor y la menor de las diferencias, que es lo que dice si la medida es
 * estable.
 *
 * @param canal Receptor a leer (0..IR_N_CANALES-1)
 * @param emisor Emisor que se conmuta (0..IR_N_CANALES-1: la numeración de `>IR0`..`>IR3`)
 * @param pares Cuántos pares ON/OFF se promedian (como mínimo 1)
 * @return Medidas del par, con la diferencia ya calculada
 * @note `ir_emisor_seleccionar()` numera los emisores 1..4, de ahí el `+1` de la llamada.
 */
static Ir_ParMedidas_t ir_medir_par(uint8_t canal, uint8_t emisor, uint8_t pares)
{
    Ir_ParMedidas_t m = { 0U, 0U, 0, 0, 0 };
    uint32_t suma_apagado = 0U;
    uint32_t suma_encendido = 0U;

    if (pares == 0U) {
        pares = 1U;
    }

    for (uint8_t i = 0U; i < pares; i++) {
        uint16_t apagado;
        uint16_t encendido;
        int16_t  dif;

        ir_emisor_seleccionar(0U);
        apagado = ADC_ReadPin(ir_adc_canal[canal]);

        ir_emisor_seleccionar((uint8_t)(emisor + 1U));
        encendido = ADC_ReadPin(ir_adc_canal[canal]);

        suma_apagado += apagado;
        suma_encendido += encendido;

        dif = (int16_t)((int32_t)encendido - (int32_t)apagado);
        if ((i == 0U) || (dif < m.dif_min)) {
            m.dif_min = dif;
        }
        if ((i == 0U) || (dif > m.dif_max)) {
            m.dif_max = dif;
        }
    }

    ir_emisor_seleccionar(0U);

    m.apagado   = (uint16_t)(suma_apagado / pares);
    m.encendido = (uint16_t)(suma_encendido / pares);
    m.dif       = (int16_t)((int32_t)m.encendido - (int32_t)m.apagado);

    return m;
}

/**
 * @brief Veredicto de un receptor a partir de sus dos lecturas (emisores apagados y encendidos).
 *
 * @param apagado Lectura con los emisores apagados
 * @param encendido Lectura con los emisores encendidos
 * @return Veredicto del canal
 */
static Adc_Estado_t adc_estado_receptor(uint16_t apagado, uint16_t encendido)
{
    uint16_t mayor = (encendido > apagado) ? encendido : apagado;

    if (mayor < 8U) {
        return ADC_EST_MUDO;
    }
    if (mayor >= 4090U) {
        return ADC_EST_SATURADO;
    }
    if (mayor < 200U) {
        return ADC_EST_FLOJO;
    }

    return ADC_EST_OK;
}

/**
 * @brief Veredicto de un canal de batería a partir de sus cuentas de ADC.
 *
 * @param cuentas Cuentas leídas
 * @return Veredicto del canal
 */
static Adc_Estado_t adc_estado_bateria(uint16_t cuentas)
{
    if (cuentas < 50U) {
        return ADC_EST_SIN_DIVISOR;
    }
    if (cuentas > 3200U) {
        return ADC_EST_MAL_DIVISOR;
    }

    return ADC_EST_OK;
}

/**
 * @brief Veredicto del canal de la celda 1, que en el banco puede estar sin divisor.
 *
 * @param cuentas Cuentas leídas en PB1
 * @return Veredicto del canal, o `ADC_EST_NO_USADA` si el divisor no está montado
 */
static Adc_Estado_t adc_estado_celda1(uint16_t cuentas)
{
    if (!BATTERY_Celda1Presente()) {
        return ADC_EST_NO_USADA;
    }

    return adc_estado_bateria(cuentas);
}

/** @brief Nombre corto de cada veredicto de canal, en el orden del enum `Adc_Estado_t`.
 *
 * ⚠️ Solo glifos de la fuente (`app/oled.h`): `FLOJO` y `CRUZ` no se pueden escribir porque la fuente
 *    no tiene `J` ni `Z`, así que van como `DEBIL` e `IGUAL`. Todo nombre mide 5 caracteres o menos
 *    para que quepan los dos canales en una línea de 21. */
static const char *const adc_estado_nombre[] = {
    "OK", "MUDO", "SAT", "DEBIL", "S/DIV", "M/DIV", "IGUAL", "N/USA"
};

/* El nombre de cada veredicto tiene que cuadrar con su índice: si no, no compila. */
typedef char adc_nombres_cuadran[
    ((sizeof(adc_estado_nombre) / sizeof(adc_estado_nombre[0])) == (ADC_EST_NO_USADA + 1)) ? 1 : -1];

/**
 * @brief Pinta el veredicto de la prueba 13, canal a canal.
 *
 * @param est_ir Veredicto de los 4 receptores de pared, en el orden `R0`..`R3`
 * @param est_b0 Veredicto del canal del pack
 * @param est_b1 Veredicto del canal de la celda 1
 * @note El veredicto se traduce a una palabra corta para poder leerlo sin Teleplot; la tabla de los
 *       códigos está en [pruebas-bringup.md §13.1](../Documentacion/pruebas-bringup.md).
 * @see `adc_estado_receptor()`, `adc_estado_bateria()` y `adc_estado_celda1()`, que son los que juzgan.
 */
static void app_pantalla_soldadura(const Adc_Estado_t *est_ir, Adc_Estado_t est_b0,
                                   Adc_Estado_t est_b1)
{
    char linea[24];

    OLED_Clear();
    oled_print_line(0U, "ADC SOLDADURA");

    snprintf(linea, sizeof(linea), "R0 %s R1 %s", adc_estado_nombre[est_ir[0]],
             adc_estado_nombre[est_ir[1]]);
    oled_print_line(1U, linea);

    snprintf(linea, sizeof(linea), "R2 %s R3 %s", adc_estado_nombre[est_ir[2]],
             adc_estado_nombre[est_ir[3]]);
    oled_print_line(2U, linea);

    snprintf(linea, sizeof(linea), "B0 %s B1 %s", adc_estado_nombre[est_b0],
             adc_estado_nombre[est_b1]);
    oled_print_line(3U, linea);

    /* Las cuatro filas de la pantalla ya están ocupadas: no cabe el aviso de salida (y en la fila 4
       no se dibujaría nada, que es la trampa que tuvo la prueba 16). */
    OLED_Update();
}

/**
 * @brief Comprueba la soldadura de los 6 canales del ADC y publica un veredicto (prueba 13).
 *
 * Los 4 receptores se miden con los emisores apagados y con los cuatro encendidos: la lectura dice si
 * el canal está muerto, saturado o flojo, y la diferencia entre las dos confirma que ve luz. Los dos
 * canales de batería se juzgan aparte, porque el pack nunca puede leer lo mismo que la celda 1. El de
 * la celda 1 sale `NO USADA` si su divisor no está montado (en el banco esa celda no se usa).
 *
 * @note Telemetría a 2 Hz: `>ADC_R0`..`R3` (cuentas de cada receptor) y `>ADC_B0`/`>ADC_B1`
 *       (canales de batería), `>ADC_Rn_DIF` (encendido − apagado) y `>ADC_Rn_EST` con el veredicto
 *       (`OK`, `MUDO`, `SATURADO`, `FLOJO`, `SIN DIVISOR`, `MAL DIVISOR`, `CANALES CRUZADOS`,
 *       `NO USADA`).
 * @note Primera prueba del bloque analógico: si aquí sale algo raro, se repasa la soldadura antes de
 *       seguir con las demás.
 * @note La pantalla la pinta `app_pantalla_soldadura()`: un veredicto por canal en palabra corta, para
 *       no depender de tener el Teleplot delante mientras se repasa la soldadura.
 * @see Documentacion/pruebas-bringup.md §13.1
 */
void App_Test_ADC_Soldadura(void)
{

    while (!BUTTONS_SalidaSolicitada()) {
        uint16_t     fondo[IR_N_CANALES];
        uint16_t     con_luz[IR_N_CANALES];
        uint16_t     bat[2];
        Adc_Estado_t bat_estado[2];
        Adc_Estado_t ir_estado[IR_N_CANALES];

        ir_emisor_seleccionar(0U);
        for (uint8_t i = 0U; i < IR_N_CANALES; i++) {
            fondo[i] = adc_leer_media(ir_adc_canal[i], IR_MUESTRAS_ADC);
        }

        ir_emisor_seleccionar(IR_LED_TODOS);
        for (uint8_t i = 0U; i < IR_N_CANALES; i++) {
            con_luz[i] = adc_leer_media(ir_adc_canal[i], IR_MUESTRAS_ADC);
        }
        ir_emisor_seleccionar(0U);

        bat[0] = adc_leer_media(ADC_PIN_B0, IR_MUESTRAS_ADC);
        bat[1] = adc_leer_media(ADC_PIN_B1, IR_MUESTRAS_ADC);

        for (uint8_t i = 0U; i < IR_N_CANALES; i++) {
            char etiqueta[14];

            snprintf(etiqueta, sizeof(etiqueta), "ADC_R%u", i);
            TELEM_SendInt(etiqueta, (int32_t)con_luz[i]);

            snprintf(etiqueta, sizeof(etiqueta), "ADC_R%u_DIF", i);
            TELEM_SendInt(etiqueta, (int32_t)con_luz[i] - (int32_t)fondo[i]);

            ir_estado[i] = adc_estado_receptor(fondo[i], con_luz[i]);

            snprintf(etiqueta, sizeof(etiqueta), "ADC_R%u_EST", i);
            TELEM_SendInt(etiqueta, (int32_t)ir_estado[i]);
        }

        bat_estado[0] = adc_estado_bateria(bat[0]);
        bat_estado[1] = adc_estado_celda1(bat[1]);
        if ((bat_estado[0] == ADC_EST_OK) && (bat_estado[1] == ADC_EST_OK) &&
            (bat[0] < (uint16_t)(bat[1] + 40U))) {
            bat_estado[0] = ADC_EST_CRUZADOS;
            bat_estado[1] = ADC_EST_CRUZADOS;
        }

        TELEM_SendInt("ADC_B0", (int32_t)bat[0]);
        TELEM_SendInt("ADC_B1", (int32_t)bat[1]);
        TELEM_SendInt("ADC_EST_B0", (int32_t)bat_estado[0]);
        TELEM_SendInt("ADC_EST_B1", (int32_t)bat_estado[1]);

        app_pantalla_soldadura(ir_estado, bat_estado[0], bat_estado[1]);

        HAL_Delay(500U);
    }

    ir_emisor_seleccionar(0U);
}

/**
 * @brief Enciende los emisores IR —de uno en uno o los cuatro— y **los deja fijos** (prueba 14).
 *
 * Es la herramienta para **alinear el apuntado** de los focos: con los cuatro encendidos y un folio
 * blanco delante, el emisor que apunta a otra provincia se ve de un vistazo con la cámara del móvil.
 * Y al quedarse **fijos**, el punto no se escapa mientras se gira el LED.
 *
 * También confirma que el **JTAG está desactivado**: PB3 y PB4 son JTDO/NJTRST y, si alguien vuelve a
 * habilitarlo, L1 y L2 dejan de encender. El LED de 860 nm no se ve a simple vista: hay que mirarlo a
 * través de la cámara de un móvil.
 *
 * @note Botones: **UP/DOWN** eligen (apagados · 1..4 · los cuatro) · **MENU** sale y los apaga.
 * @note Telemetría: `>IR_LED:0` (apagados), `1`..`4` (un emisor) y `5` (los cuatro juntos).
 * @warning Al encender los cuatro juntos el riel pide ≈300 mA: en uso normal solo luce uno.
 * @warning **No parpadean a propósito, y es una decisión, no un olvido.** El pico de corriente lo fija
 *          la resistencia en serie (22 Ω ⇒ ≈75 mA), así que **pulsarlos no daría más luz**: la media
 *          baja con el ciclo de trabajo y la cámara vería **menos**. En continua se ve el máximo y sin
 *          parpadeo. Para más brillo de verdad hay que **bajar esa resistencia**, y el BC337 no pasa
 *          de ≈250-300 mA desde un pin del MCU.
 * @see Documentacion/pruebas-bringup.md §13.2
 */
void App_Test_EmisoresIR(void)
{
    static const uint8_t opciones[] = { 0U, 1U, 2U, 3U, 4U, IR_LED_TODOS };
    const uint8_t        n_opciones = (uint8_t)(sizeof(opciones) / sizeof(opciones[0]));
    uint8_t              elegido    = 1U;   /* Arranca con el primero encendido: ya se ve algo. */

    while (!BUTTONS_SalidaSolicitada()) {
        char    estado[16];
        uint8_t evento;

        ir_emisor_seleccionar(opciones[elegido]);

        BUTTONS_Update();
        evento = BUTTONS_GetEvent();

        if (evento == BTN_NUM_UP) {
            elegido = (uint8_t)((elegido + 1U) % n_opciones);
            TELEM_SendInt("IR_LED", (int32_t)opciones[elegido]);
        } else if (evento == BTN_NUM_DOWN) {
            elegido = (uint8_t)((elegido + (n_opciones - 1U)) % n_opciones);
            TELEM_SendInt("IR_LED", (int32_t)opciones[elegido]);
        }

        if (opciones[elegido] == IR_LED_TODOS) {
            snprintf(estado, sizeof(estado), "TODOS");
        } else if (opciones[elegido] == 0U) {
            snprintf(estado, sizeof(estado), "APAGADOS");
        } else {
            snprintf(estado, sizeof(estado), "LED %u", (unsigned)opciones[elegido]);
        }

        app_pantalla_telemetria("EMISORES IR", estado);

        /* 50 ms: el botón se siente inmediato, y la pantalla lleva su propio ritmo (500 ms). */
        HAL_Delay(50U);
    }

    ir_emisor_seleccionar(0U);
}

/**
 * @brief Pinta la matriz de la prueba 15: qué emisor luce y las 4 lecturas de los receptores.
 *
 * @param emisor Emisor encendido (0..3)
 * @param lecturas Cuentas del ADC de cada receptor, en orden `R0`..`R3`
 * @note Sin contador de latidos: aquí lo que importa es el número, que además se mueve al acercar la
 *       pared o al mover el robot. La primera línea es la misma información que la telemetría, para
 *       no tener que cruzar dos ventanas al emparejar emisor con receptor.
 */
static void app_pantalla_receptores(uint8_t emisor, const uint16_t *lecturas)
{
    char linea[24];

    OLED_Clear();

    snprintf(linea, sizeof(linea), "IR %u %u %u %u", (emisor == 0U) ? 1U : 0U,
             (emisor == 1U) ? 1U : 0U, (emisor == 2U) ? 1U : 0U, (emisor == 3U) ? 1U : 0U);
    oled_print_line(0U, linea);

    snprintf(linea, sizeof(linea), "R0 %u  R1 %u", (unsigned)lecturas[0], (unsigned)lecturas[1]);
    oled_print_line(1U, linea);

    snprintf(linea, sizeof(linea), "R2 %u  R3 %u", (unsigned)lecturas[2], (unsigned)lecturas[3]);
    oled_print_line(2U, linea);

    oled_print_line(3U, "MENU: SALIR");

    OLED_Update();
}

/**
 * @brief Recorre la matriz emisor×receptor para emparejarlos (prueba 15).
 *
 * Con un emisor encendido se leen los 4 receptores y se publica la **cuenta cruda del ADC** de cada
 * uno: el receptor cuyo valor sube con un emisor concreto es el que lo ve. Así se levanta la matriz
 * 4×4, se elige la pareja buena de cada canal (para `ir_pareja_emisor[]`) y se anota la posición
 * física de cada uno en hardware.md §4.4.
 *
 * @note Telemetría continua y **solo** estas 8 etiquetas: `>IR0`..`>IR3` dicen qué emisor está
 *       encendido (1 = encendido) y `>R0`..`>R3` son las cuentas del ADC de los 4 receptores. El
 *       emisor cambia cada `IR_PASO_MS`, así que en Teleplot se ve qué receptor sigue a qué emisor.
 *       Al ser continua y sin pausas, el barrido se puede dejar corriendo mientras se mueve la pared.
 * @warning Es también la prueba que decide si la $R_L$ de 3 kΩ vale: si la cuenta se pega a 4095, el
 *          problema es la luz ambiente (apantallar) antes que la resistencia.
 * @see Documentacion/pruebas-bringup.md §13.3
 */
void App_Test_IR_Receptores(void)
{

    while (!BUTTONS_SalidaSolicitada()) {
        for (uint8_t emisor = 0U; emisor < IR_N_CANALES; emisor++) {
            uint16_t lecturas[IR_N_CANALES];
            char     etiqueta[8];

            ir_emisor_seleccionar((uint8_t)(emisor + 1U));

            for (uint8_t i = 0U; i < IR_N_CANALES; i++) {
                snprintf(etiqueta, sizeof(etiqueta), "IR%u", i);
                TELEM_SendInt(etiqueta, (i == emisor) ? 1 : 0);
            }

            for (uint8_t canal = 0U; canal < IR_N_CANALES; canal++) {
                lecturas[canal] = adc_leer_media(ir_adc_canal[canal], IR_MUESTRAS_ADC);

                snprintf(etiqueta, sizeof(etiqueta), "R%u", canal);
                TELEM_SendInt(etiqueta, (int32_t)lecturas[canal]);
            }

            app_pantalla_receptores(emisor, lecturas);

            HAL_Delay(IR_PASO_MS);
        }
    }

    ir_emisor_seleccionar(0U);
}

/**
 * @brief Pinta un receptor a pantalla completa, con el emisor que está luciendo (submenús `VER Rn`).
 *
 * @param emisor Emisor encendido (0..3)
 * @param canal Receptor que se está mirando (0..3)
 * @param lectura Cuentas del ADC de ese receptor
 * @note Formato pedido el 30-sep: el barrido de emisores arriba, el valor del receptor debajo y cómo
 *       salir. Sin latidos ni nada más, porque de lo que se trata es de ver un número que se mueve.
 */
static void app_pantalla_un_receptor(uint8_t emisor, uint8_t canal, uint16_t lectura)
{
    char linea[24];

    OLED_Clear();

    snprintf(linea, sizeof(linea), "IR %u %u %u %u", (emisor == 0U) ? 1U : 0U,
             (emisor == 1U) ? 1U : 0U, (emisor == 2U) ? 1U : 0U, (emisor == 3U) ? 1U : 0U);
    oled_print_line(0U, linea);

    snprintf(linea, sizeof(linea), "R%u %u", (unsigned)canal, (unsigned)lectura);
    oled_print_line(1U, linea);

    oled_print_line(2U, "MENU: SALIR");

    OLED_Update();
}

/**
 * @brief Mira un solo receptor mientras los emisores van rotando (submenús `VER Rn`).
 *
 * Es la prueba 15 vista por un canal: el barrido de emisores es el mismo, pero la pantalla se queda
 * con la lectura de un solo receptor, que es mucho más fácil de seguir cuando se mueve la pared.
 *
 * @param canal Receptor que se mira (0..3)
 * @note Telemetría: `>IR0`..`>IR3` (qué emisor está encendido) y `>R<canal>` con la cuenta del ADC
 *       de ese receptor, y nada más.
 * @see `App_Test_IR_Receptores()`, que saca los cuatro a la vez.
 */
static void app_test_receptor(uint8_t canal)
{
    while (!BUTTONS_SalidaSolicitada()) {
        for (uint8_t emisor = 0U; emisor < IR_N_CANALES; emisor++) {
            uint16_t lectura;
            char     etiqueta[8];

            ir_emisor_seleccionar((uint8_t)(emisor + 1U));

            for (uint8_t i = 0U; i < IR_N_CANALES; i++) {
                snprintf(etiqueta, sizeof(etiqueta), "IR%u", i);
                TELEM_SendInt(etiqueta, (i == emisor) ? 1 : 0);
            }

            lectura = adc_leer_media(ir_adc_canal[canal], IR_MUESTRAS_ADC);

            snprintf(etiqueta, sizeof(etiqueta), "R%u", canal);
            TELEM_SendInt(etiqueta, (int32_t)lectura);

            app_pantalla_un_receptor(emisor, canal, lectura);

            HAL_Delay(IR_PASO_MS);
        }
    }

    ir_emisor_seleccionar(0U);
}

/** @brief Submenú `VER R0`: la lectura del receptor 0 con el barrido de emisores. */
void App_Test_R0(void)
{
    app_test_receptor(0U);
}

/** @brief Submenú `VER R1`: la lectura del receptor 1 con el barrido de emisores. */
void App_Test_R1(void)
{
    app_test_receptor(1U);
}

/** @brief Submenú `VER R2`: la lectura del receptor 2 con el barrido de emisores. */
void App_Test_R2(void)
{
    app_test_receptor(2U);
}

/** @brief Submenú `VER R3`: la lectura del receptor 3 con el barrido de emisores. */
void App_Test_R3(void)
{
    app_test_receptor(3U);
}

/**
 * @brief Pinta el `DIF` de los cuatro canales, de dos en dos (prueba 16).
 *
 * @param dif Diferencia encendido − apagado de cada receptor, en cuentas
 * @note **Dos canales por fila, y no uno.** En 128×32 solo hay 4 filas de texto: con una fila por
 *       canal el `R3` caía en la fila 4 y no se dibujaba, así que en pantalla solo salían R0, R1 y
 *       R2. Con dos por fila caben los cuatro y sobra la fila del aviso de salida.
 * @note El `VAR` no se pinta: para seguir el `DIF` estorba, y como indicador se sigue publicando por
 *       telemetría (`>Rn_VAR`).
 * @note Sin contador de latidos: aquí lo que decide son los números —`DIF` alto y `VAR` bajo— y se
 *       mueven solos al encender y apagar la luz, que es lo que se está probando.
 */
static void app_pantalla_modulado(const int16_t *dif)
{
    char linea[32];

    OLED_Clear();
    oled_print_line(0U, "IR MODULADO");

    /* Los valores son de 16 bits: `%5hd` los alinea a la derecha para que no bailen de columna al
       pasar de 4 a 3 dígitos, o al cambiar de signo (con `%d` el compilador supone un `int`). */
    snprintf(linea, sizeof(linea), "R0%5hd  R1%5hd", dif[0], dif[1]);
    oled_print_line(1U, linea);

    snprintf(linea, sizeof(linea), "R2%5hd  R3%5hd", dif[2], dif[3]);
    oled_print_line(2U, linea);

    oled_print_line(3U, "MENU: SALIR");

    OLED_Update();
}

/**
 * @brief Comprueba que la resta ON−OFF cancela la luz ambiente (prueba 16).
 *
 * Hace 8 pares por canal y publica la media de la diferencia más su dispersión: si al encender y
 * apagar la luz de la sala el `DIF` aguanta (variación < 10 %), la cancelación funciona y el
 * condensador del receptor no hace falta.
 *
 * @note Telemetría: `>R0_OFF`, `>R0_ON`, `>R0_DIF` y `>R0_VAR` (dispersión de la diferencia, en %
 *       de su valor medio). Se repite a 4 Hz.
 * @note La pantalla (`app_pantalla_modulado()`) enseña el `DIF` de los 4 canales, de dos en dos. El
 *       `VAR` solo va por telemetría.
 * @warning **Luz ambiente**: con sol directo sobre la pared el fondo sube y el `DIF` se queda corto.
 *          Se hace con luz controlada (sala encendida y apagada, pared en sombra), que es justo lo
 *          que la prueba quiere demostrar que da igual.
 * @note Usa la pareja emisor↔receptor de `ir_pareja_emisor[]`, que la mide la prueba 15.
 * @see Documentacion/pruebas-bringup.md §13.4
 */
void App_Test_IR_Modulado(void)
{

    while (!BUTTONS_SalidaSolicitada()) {
        int16_t dif[IR_N_CANALES];

        for (uint8_t canal = 0U; canal < IR_N_CANALES; canal++) {
            Ir_ParMedidas_t m = ir_medir_par(canal, ir_pareja_emisor[canal], IR_PARES_16);
            char    etiqueta[14];
            int32_t desviacion = 0;

            if (m.dif != 0) {
                int32_t dif_abs = (m.dif < 0) ? -(int32_t)m.dif : (int32_t)m.dif;

                desviacion = ((int32_t)m.dif_max - (int32_t)m.dif_min) * 100 / dif_abs;
            }

            snprintf(etiqueta, sizeof(etiqueta), "R%u_OFF", canal);
            TELEM_SendInt(etiqueta, (int32_t)m.apagado);

            snprintf(etiqueta, sizeof(etiqueta), "R%u_ON", canal);
            TELEM_SendInt(etiqueta, (int32_t)m.encendido);

            snprintf(etiqueta, sizeof(etiqueta), "R%u_DIF", canal);
            TELEM_SendInt(etiqueta, (int32_t)m.dif);

            snprintf(etiqueta, sizeof(etiqueta), "R%u_VAR", canal);
            TELEM_SendInt(etiqueta, desviacion);

            dif[canal] = m.dif;
        }

        app_pantalla_modulado(dif);

        HAL_Delay(250U);
    }

    ir_emisor_seleccionar(0U);
}

/**
 * @brief Escribe una línea de texto a escala 1 en una fila de la OLED.
 *
 * @param fila Fila donde empieza (0 arriba, 8 px por fila)
 * @param texto Texto a escribir; se corta al llegar al ancho de la pantalla
 * @warning La pantalla es 128×32: **solo hay 4 filas (0..3)**. De la 4 en adelante `OLED_Print()`
 *          sale sin dibujar y el texto desaparece en silencio, sin error ni aviso.
 */
static void oled_print_line(uint8_t fila, const char *texto)
{
    OLED_Print(0U, (uint8_t)(fila * 8U), 1U, texto);
}

/**
 * @brief Dibuja en la OLED el estado de la calibración de la curva IR (prueba 17).
 *
 * @param canal Receptor que se está calibrando
 * @param hechos Cuántos puntos lleva congelados ese canal
 */
static void ir_cal_dibujar(uint8_t canal, uint8_t hechos)
{
    char linea[24];

    OLED_Clear();
    oled_print_line(0, "CALIBRACION IR");

    snprintf(linea, sizeof(linea), "CANAL %u  %u/%u", canal, hechos, IR_CAL_PUNTOS);
    oled_print_line(1, linea);

    if (hechos < IR_CAL_PUNTOS) {
        snprintf(linea, sizeof(linea), "PON A %u CM", ir_cal_distancias[hechos]);
        oled_print_line(2, linea);
        oled_print_line(3, "OK MEDIR - MENU SALIR");
    } else {
        oled_print_line(2, "CANAL COMPLETO");
        oled_print_line(3, "UP/DOWN OTRO CANAL");
    }

    OLED_Update();
}

/**
 * @brief Enciende el emisor que ilumina un canal y apaga los otros tres.
 *
 * @param canal Receptor que se está calibrando (0..IR_N_CANALES-1)
 * @note La prueba 17 **no encendía ningún emisor**: medía el fondo de la sala y guardaba eso como
 *       «curva de distancia», que no depende de la distancia. Ver el registro de
 *       `estado-del-proyecto.md` (2026-10-02).
 */
static void ir_cal_emisor(uint8_t canal)
{
    ir_emisor_seleccionar((uint8_t)(ir_pareja_emisor[canal] + 1U));
}

/**
 * @brief Congela un punto de la curva y lo publica (prueba 17).
 *
 * @param tabla Tabla de calibración (se escribe la celda)
 * @param canal Receptor que se está calibrando
 * @param punto Índice del punto, que es también el de `ir_cal_distancias`
 */
static void ir_cal_guardar_punto(uint16_t tabla[][IR_CAL_PUNTOS], uint8_t canal, uint8_t punto)
{
    uint16_t valor;
    char     etiqueta[14];

    /* El emisor del canal tiene que estar encendido **al congelar el punto**: sin él lo que se guarda
       es la luz de la sala, y una curva de fondo no depende de la distancia. */
    ir_cal_emisor(canal);
    valor = adc_leer_media(ir_adc_canal[canal], IR_CAL_MUESTRAS);

    tabla[canal][punto] = valor;

    TELEM_SendInt("IR_CAL_CANAL", (int32_t)canal);
    TELEM_SendInt("IR_CAL_CM", (int32_t)ir_cal_distancias[punto]);

    snprintf(etiqueta, sizeof(etiqueta), "R%u_CAL", canal);
    TELEM_SendInt(etiqueta, (int32_t)valor);
}

/**
 * @brief Estima la distancia de una lectura interpolando en la curva de un canal.
 *
 * La curva es decreciente (más distancia, menos luz reflejada), así que se busca el tramo que encierra
 * la lectura y se interpola linealmente. Fuera de la tabla se devuelve el extremo más cercano.
 *
 * @param puntos Cuentas de los IR_CAL_PUNTOS puntos, en el orden de ir_cal_distancias
 * @param lectura Cuentas medidas ahora
 * @return Distancia estimada, en cm
 */
static float ir_cal_estimar_cm(const uint16_t *puntos, uint16_t lectura)
{
    if (lectura >= puntos[0]) {
        return (float)ir_cal_distancias[0];
    }

    for (uint8_t i = 0U; i < (uint8_t)(IR_CAL_PUNTOS - 1U); i++) {
        uint16_t alto = puntos[i];
        uint16_t bajo = puntos[i + 1U];

        if ((alto > bajo) && (lectura <= alto) && (lectura >= bajo)) {
            float fraccion = (float)(alto - lectura) / (float)(alto - bajo);

            return (float)ir_cal_distancias[i] +
                   (fraccion * (float)(ir_cal_distancias[i + 1U] - ir_cal_distancias[i]));
        }
    }

    return (float)ir_cal_distancias[IR_CAL_PUNTOS - 1U];
}

/**
 * @brief Publica la tabla de calibración completa y la distancia estimada en vivo (prueba 17).
 *
 * @param tabla Tabla ya completa (4 canales × IR_CAL_PUNTOS puntos)
 */
static void ir_cal_estimar_vivo(const uint16_t tabla[][IR_CAL_PUNTOS])
{
    float cm[IR_N_CANALES];
    char  linea[24];

    for (uint8_t punto = 0U; punto < IR_CAL_PUNTOS; punto++) {
        TELEM_SendInt("IR_CAL_CM", (int32_t)ir_cal_distancias[punto]);

        for (uint8_t canal = 0U; canal < IR_N_CANALES; canal++) {
            char etiqueta[14];

            snprintf(etiqueta, sizeof(etiqueta), "R%u_CAL", canal);
            TELEM_SendInt(etiqueta, (int32_t)tabla[canal][punto]);
        }
    }

    for (uint8_t canal = 0U; canal < IR_N_CANALES; canal++) {
        char etiqueta[14];

        /* Un emisor cada vez (solo uno encendido): hay que conmutar por canal antes de leerlo. */
        ir_cal_emisor(canal);
        cm[canal] = ir_cal_estimar_cm(tabla[canal], adc_leer_media(ir_adc_canal[canal],
                                                                  IR_MUESTRAS_ADC));

        snprintf(etiqueta, sizeof(etiqueta), "R%u_CM", canal);
        TELEM_SendFloat(etiqueta, cm[canal]);
    }

    OLED_Clear();
    oled_print_line(0, "DISTANCIA EN CM");
    snprintf(linea, sizeof(linea), "A1 %4.1f  A2 %4.1f", (double)cm[0], (double)cm[1]);
    oled_print_line(1, linea);
    snprintf(linea, sizeof(linea), "A3 %4.1f  A4 %4.1f", (double)cm[2], (double)cm[3]);
    oled_print_line(2, linea);
    oled_print_line(3, "MENU SALIR");
    OLED_Update();
}

/**
 * @brief Curva distancia↔ADC guiada con los botones (prueba 17).
 *
 * La pantalla pide el canal y la distancia, y con OK se congela la media de 64 muestras de ese punto.
 * Al completar los 5 puntos de los 4 canales publica la tabla y pasa a estimar en vivo la distancia de
 * cada receptor interpolando entre los puntos.
 *
 * @note Botones: `UP`/`DOWN` cambian de canal, `OK` congela el punto y `MENU` sale.
 * @note Telemetría: `>IR_CAL_CANAL`, `>IR_CAL_CM` y `>Rn_CAL` al congelar cada punto; al terminar, la
 *       tabla entera y `>Rn_CM` con la distancia estimada de cada receptor.
 * @see Documentacion/pruebas-bringup.md §13.5
 */
void App_Test_CalibracionIR(void)
{
    uint16_t tabla[IR_N_CANALES][IR_CAL_PUNTOS];
    uint8_t  hechos[IR_N_CANALES] = { 0U, 0U, 0U, 0U };
    uint8_t  canal = 0U;
    uint8_t  n_hechos = 0U;

    for (uint8_t c = 0U; c < IR_N_CANALES; c++) {
        for (uint8_t p = 0U; p < IR_CAL_PUNTOS; p++) {
            tabla[c][p] = 0U;
        }
    }

    ir_cal_emisor(canal);
    ir_cal_dibujar(canal, hechos[canal]);

    while (!BUTTONS_SalidaSolicitada()) {
        BUTTONS_Update();

        if (n_hechos < (uint8_t)(IR_N_CANALES * IR_CAL_PUNTOS)) {
            uint8_t evento = BUTTONS_GetEvent();

            if (evento == BTN_NUM_UP) {
                canal = (uint8_t)((canal + 1U) % IR_N_CANALES);
                ir_cal_emisor(canal);
                ir_cal_dibujar(canal, hechos[canal]);
            } else if (evento == BTN_NUM_DOWN) {
                canal = (uint8_t)((canal + IR_N_CANALES - 1U) % IR_N_CANALES);
                ir_cal_emisor(canal);
                ir_cal_dibujar(canal, hechos[canal]);
            } else if ((evento == BTN_NUM_OK) && (hechos[canal] < IR_CAL_PUNTOS)) {
                ir_cal_guardar_punto(tabla, canal, hechos[canal]);
                hechos[canal]++;
                n_hechos++;

                if (hechos[canal] >= IR_CAL_PUNTOS) {
                    canal = (uint8_t)((canal + 1U) % IR_N_CANALES);
                }

                ir_cal_emisor(canal);
                ir_cal_dibujar(canal, hechos[canal]);
            }
        } else {
            ir_cal_estimar_vivo(tabla);
            HAL_Delay(200U);
        }

        HAL_Delay(1);
    }

    ir_emisor_seleccionar(0U);
}

/**
 * @brief Parámetros de la prueba 18 (PID de velocidad por rueda, capa CONTROL).
 *
 * PID_TEST_MODO = 1 lanza un escalón (para sintonizar a mano) y = 2 una rampa (para ver el
 * seguimiento en condiciones normales).
 *
 * @note El **modo** y el tope de duty siguen siendo `#define` (se eligen antes de flashear), pero las
 *       **ganancias y la consigna se ajustan en caliente** desde la consola (`PID KP 0.35`). Sin eso,
 *       cada tanteo de Ziegler-Nichols obliga a recompilar y flashear, y el reinicio borra la
 *       observación que se estaba haciendo.
 *
 * @warning **Modo puesto a 1 (escalón alternante) el 2026-10-01 para la sintonía por respuesta al
 *          escalón.** Cuando el lazo esté ajustado hay que volver a **2** (rampa): el escalón es un
 *          ensayo, no una forma de mover el robot.
 *
 * @note **Por qué Ziegler-Nichols (buscar la oscilación) no se usa aquí**: la velocidad de una rueda es
 *       un sistema de **primer orden**, y un lazo proporcional sobre un sistema de primer orden **no
 *       oscila nunca**, suba lo que suba `Kp` (comprobado: el modo 3 llegó a `Kp = 2,0` sin oscilar). El
 *       método de la ganancia crítica necesita fase, y aquí la única que hay son los ~10 ms de la media
 *       móvil del encoder. Se sintoniza **por la respuesta al escalón**: subir `Kp` hasta que responda
 *       rápido y sin pasarse, y añadir `Ki` para quitar el error de régimen.
 */
#define PID_TEST_MODO          1

/** @brief Modo 3: escalón que se queda puesto y **barrido automático de `Kp`** buscando `Ku`.
 *
 * El modo 1 alterna la consigna cada 2 s, y eso **corta la oscilación** justo cuando hay que ver si es
 * sostenida. Para Ziegler-Nichols hace falta un escalón **único**: se pide la velocidad una vez, se
 * queda puesta, y lo que se mira es cómo responde el lazo.
 */
#define PID_AUTO_KP_INI        0.05f   /* `Kp` con el que empieza el barrido          */
#define PID_AUTO_KP_PASO       0.05f   /* cuánto sube en cada tanteo                  */
#define PID_AUTO_KP_MAX        2.00f   /* hasta dónde se atreve                       */
#define PID_AUTO_PASO_MS       3000U   /* lo que se queda en cada valor antes de medir */

/**
 * @brief Tiempo de cada tramo que **no se mide**, para dejar pasar el arranque [ms].
 *
 * La consigna se pide de golpe y la velocidad salta de 0 a su valor: ese pico **no es oscilación**,
 * pero contarlo da una amplitud enorme y el barrido se declara terminado en el primer tramo con un
 * `Ku` falso. Se descarta el transitorio y se mide solo la parte final del tramo.
 */
#define PID_AUTO_ASENTAR_MS    1500U

/** @brief Ventanas **seguidas** con amplitud alta que hacen falta para dar el `Ku` por bueno. */
#define PID_AUTO_CONFIRMACIONES 2U

/** @brief Cruces por la consigna que hacen falta en la ventana para creerse que oscila. */
#define PID_AUTO_CRUCES_MIN    3U

/** @brief Amplitud (pico a pico) que cuenta como oscilación, en fracción de la consigna. */
#define PID_AUTO_AMP_FRAC      0.40f

/**
 * @brief Ganancias de arranque del lazo de velocidad, medidas el 2026-10-01 sobre el banco.
 *
 * **La planta** (duty → velocidad de rueda, con las ruedas en el aire) es de primer orden y rápida:
 * `K ≈ 15 mm/s` por cada 1 % de duty, con `tau ≈ 45 ms`. Medido con un escalón del 30 %: la rueda pasa
 * de 0 a 301 mm/s en 50 ms.
 *
 * **Lo que limita `Kp` no es la planta, sino el retardo del lazo** (~25 ms: la media móvil de 20
 * muestras del encoder más los 10 ms del lazo). Con `T/tau ≈ 0,6` el margen de fase se agota en
 * `K*Kp ≈ 3`, o sea `Kp ≈ 0,2`: **comprobado que con `Kp = 0,3` el lazo satura y oscila** (±60 % de
 * duty, la velocidad de −725 a +1112 mm/s, y las dos ruedas desincronizadas).
 *
 * **Corolario: con `Kp` tan bajo no se puede quitar el error de régimen a base de `P`** (con `Kp = 0,10`
 * la velocidad se quedaba en 180 mm/s pidiendo 300, un 40 % de error). Lo quita el integrador: con el
 * cero del PI una década por debajo del cruce (`Ki/Kp = 5 rad/s` frente a `wc ≈ 25 rad/s`) se consigue
 * **error de régimen del 0,5 %** (298,5 mm/s de mediana) y un asentamiento de 0,6 s.
 *
 * @note `Kd = 0` **a propósito**: la planta es de primer orden (no hay inercia dominante que amortiguar)
 *       y el derivativo solo amplificaría el ruido de cuantificación del encoder, que es de ~7 mm/s.
 * @note El pico de ~490 mm/s que se ve en el modo escalón es **el escalón**, no el lazo: `P + I` piden
 *       40 % de duty de golpe. En uso real la consigna lleva la rampa de `SP_PENDIENTE_DEF_MM_S2`
 *       (600 mm/s²), que es la que usan las primitivas de `app/motion.c`.
 */
#define PID_TEST_VEL_MM_S_DEF  300.0f
#define PID_TEST_RAMPA_MM_S2   600.0f
#define PID_TEST_KP_DEF        0.10f
#define PID_TEST_KI_DEF        0.50f
#define PID_TEST_KD_DEF        0.00f
#define PID_TEST_DUTY_MAX      60.0f

/**
 * @brief Periodo del escalón de sintonía [ms].
 *
 * Con 2 s el escalón cambiaba antes de que la respuesta se hubiera asentado, y en la gráfica no se
 * distinguía la subida del régimen. Con 4 s hay tiempo de ver las dos cosas.
 */
#define PID_TEST_PERIODO_MS    4000U
#define PID_TEST_LAZO_MS       10U

/** @brief Rango que acepta la orden `PID`: por encima de esto el lazo solo oscila. */
#define PID_TEST_KP_MAX        10.0f
#define PID_TEST_KI_MAX        50.0f
#define PID_TEST_KD_MAX        5.0f
#define PID_TEST_VEL_MAX_MM_S  1000.0f

/** @brief Periodo de repintado de la pantalla de la prueba 18, en ms. */
#define PID_TEST_PANTALLA_MS   200U

/**
 * @brief Parámetro de la prueba 18 que cambian los botones.
 *
 * El ajuste por botones existe porque para sintonizar el robot tiene que estar **en el suelo**, y ahí
 * el cable estorba. Además, **un puerto serie solo lo abre un programa a la vez**: si el que lee la
 * telemetría no deja escribir, las órdenes no se pueden mandar. Con los botones se sintoniza mirando
 * solo la pantalla.
 */
typedef enum {
    PID_TEST_AJUSTE_KP = 0,
    PID_TEST_AJUSTE_KI,
    PID_TEST_AJUSTE_KD,
    PID_TEST_AJUSTE_VEL,
    PID_TEST_AJUSTE_N
} PidTestAjuste_t;

/** @brief Cuánto cambia cada pulsación de UP/DOWN en cada parámetro. */
static const float pid_test_ajuste_paso[PID_TEST_AJUSTE_N] = { 0.05f, 0.05f, 0.05f, 25.0f };

/**
 * @brief Estado del barrido automático de `Kp` (modo 3).
 */
typedef enum {
    PID_AUTO_BUSCANDO = 0,   /**< Subiendo `Kp` y midiendo la oscilación            */
    PID_AUTO_ENCONTRADO,     /**< Se ha encontrado `Ku`: el barrido se para         */
    PID_AUTO_AGOTADO         /**< Se llegó a `PID_AUTO_KP_MAX` sin verla oscilar    */
} PidAutoEstado_t;

/**
 * @brief Margen (pico a pico) de la velocidad medido en una ventana.
 *
 * Es el número que decide si el lazo oscila: mientras la amplitud se queda pequeña, el lazo está
 * siguiendo la consigna; cuando la amplitud **no se apaga**, está oscilando y ese `Kp` es `Ku`.
 */
typedef struct {
    float   minimo;   /**< Menor velocidad medida en la ventana [mm/s] */
    float   maximo;   /**< Mayor velocidad medida en la ventana [mm/s] */
    uint8_t hay;      /**< 1 si ya hay alguna medida en la ventana     */
} PidTestAmplitud_t;

/**
 * @brief Reinicia la ventana de amplitud.
 *
 * @param amp Ventana a reiniciar
 */
static void pid_test_amp_reiniciar(PidTestAmplitud_t *amp)
{
    amp->minimo = 0.0f;
    amp->maximo = 0.0f;
    amp->hay    = 0U;
}

/**
 * @brief Anota una medida en la ventana de amplitud.
 *
 * @param amp Ventana
 * @param medida Velocidad medida [mm/s]
 */
static void pid_test_amp_paso(PidTestAmplitud_t *amp, float medida)
{
    if (amp->hay == 0U) {
        amp->minimo = medida;
        amp->maximo = medida;
        amp->hay    = 1U;
        return;
    }

    if (medida < amp->minimo) { amp->minimo = medida; }
    if (medida > amp->maximo) { amp->maximo = medida; }
}

/**
 * @brief Margen pico a pico de la ventana [mm/s].
 *
 * @param amp Ventana
 * @return Diferencia entre la mayor y la menor medida; 0 si la ventana está vacía
 */
static float pid_test_amp_rango(const PidTestAmplitud_t *amp)
{
    return (amp->hay != 0U) ? (amp->maximo - amp->minimo) : 0.0f;
}

/** @brief Margen alrededor de la consigna para contar un cruce [mm/s]. */
#define PID_TEST_OSC_MARGEN_MM_S   20.0f

/**
 * @brief Detector de la oscilación del lazo, para medir `Tu` sin cronómetro.
 *
 * Cuenta los cruces de la velocidad por la consigna **de abajo hacia arriba** y mide con el reloj el
tiempo entre el primero y el último. Es la forma de sacar `Tu` (Ziegler-Nichols) **con el robot en el
suelo y sin Teleplot**: el número sale en la pantalla.
 *
 * El margen evita contar el ruido del encoder como si fuera oscilación: la velocidad tiene que
 * alejarse `PID_TEST_OSC_MARGEN_MM_S` de la consigna en los dos sentidos para que el cruce cuente.
 */
typedef struct {
    uint8_t  encima;     /**< 1 si la medida está por encima de la consigna + margen   */
    uint8_t  cuenta;     /**< 1 si ya hay un primer cruce del que contar                */
    uint16_t cruces;     /**< Cruces hacia arriba contados desde ese primero            */
    uint32_t t_primero;  /**< Instante del primer cruce [ms]                            */
    uint32_t t_ultimo;   /**< Instante del último cruce [ms]                            */
} PidTestOsc_t;

/**
 * @brief Anota un cruce de la velocidad por la consigna, si lo hay.
 *
 * @param osc Detector
 * @param consigna Consigna que se está pidiendo [mm/s]
 * @param medida Velocidad medida [mm/s]
 * @param ahora Instante actual [ms]
 * @note Durante la fase baja del escalón (`consigna` a cero) el detector se reinicia: ahí no hay
 *       oscilación que medir y dejar los cruces viejos falsearía el periodo.
 */
static void pid_test_osc_paso(PidTestOsc_t *osc, float consigna, float medida, uint32_t ahora)
{
    if (consigna <= 0.0f) {
        osc->encima  = 0U;
        osc->cuenta  = 0U;
        osc->cruces  = 0U;
        return;
    }

    if (medida > (consigna + PID_TEST_OSC_MARGEN_MM_S)) {
        if (osc->encima == 0U) {
            osc->encima = 1U;

            if (osc->cuenta == 0U) {
                osc->cuenta    = 1U;
                osc->cruces    = 0U;
                osc->t_primero = ahora;
            } else {
                osc->cruces++;
                osc->t_ultimo = ahora;
            }
        }
    } else if (medida < (consigna - PID_TEST_OSC_MARGEN_MM_S)) {
        osc->encima = 0U;
    }
}

/**
 * @brief Periodo de la oscilación medido hasta ahora [s]; 0 si todavía no hay cruces suficientes.
 *
 * @param osc Detector
 * @return Periodo medio entre cruces, o 0 si no se puede calcular
 */
static float pid_test_osc_tu(const PidTestOsc_t *osc)
{
    if (osc->cruces == 0U) {
        return 0.0f;
    }

    return (float)(osc->t_ultimo - osc->t_primero) / ((float)osc->cruces * 1000.0f);
}

/* Ganancia y consigna en uso (ver la nota de aplicación arriba). */
static float   pid_test_kp        = PID_TEST_KP_DEF;
static float   pid_test_ki        = PID_TEST_KI_DEF;
static float   pid_test_kd        = PID_TEST_KD_DEF;
static float   pid_test_vel_mm_s  = PID_TEST_VEL_MM_S_DEF;
static uint8_t pid_test_pendiente = 0U;   /* 1 = la consola ha cambiado algo: reaplicar al lazo */

/**
 * @brief Cambia una ganancia del lazo de velocidad (orden `PID` de la consola).
 *
 * @param cual Ganancia a cambiar
 * @param valor Valor nuevo
 * @return 1 si se aceptó, 0 si está fuera de rango
 * @note La comprobación `!(valor >= 0.0f)` descarta también los negativos y el `NaN` de un texto
 *       mal formado, sin necesidad de comparar contra sí mismo.
 */
uint8_t APP_PID_SetGanancia(App_PidGanancia_t cual, float valor)
{
    if (!(valor >= 0.0f)) {
        return 0U;
    }

    switch (cual) {
    case APP_PID_GANANCIA_KP:
        if (valor > PID_TEST_KP_MAX) { return 0U; }
        pid_test_kp = valor;
        break;
    case APP_PID_GANANCIA_KI:
        if (valor > PID_TEST_KI_MAX) { return 0U; }
        pid_test_ki = valor;
        break;
    case APP_PID_GANANCIA_KD:
        if (valor > PID_TEST_KD_MAX) { return 0U; }
        pid_test_kd = valor;
        break;
    default:
        return 0U;
    }

    pid_test_pendiente = 1U;

    return 1U;
}

/**
 * @brief Cambia la consigna de velocidad de la prueba 18 (orden `PID VEL`).
 *
 * @param mm_s Consigna en mm/s
 * @return 1 si se aceptó, 0 si está fuera de rango
 */
uint8_t APP_PID_SetVelocidad(float mm_s)
{
    if (!(mm_s >= 0.0f) || (mm_s > PID_TEST_VEL_MAX_MM_S)) {
        return 0U;
    }

    pid_test_vel_mm_s  = mm_s;
    pid_test_pendiente = 1U;

    return 1U;
}

/**
 * @brief Ganancia en uso (la que aplicará la prueba 18).
 */
float APP_PID_GetGanancia(App_PidGanancia_t cual)
{
    switch (cual) {
    case APP_PID_GANANCIA_KP: return pid_test_kp;
    case APP_PID_GANANCIA_KI: return pid_test_ki;
    case APP_PID_GANANCIA_KD: return pid_test_kd;
    default:                  return 0.0f;
    }
}

/** @brief Consigna de velocidad en uso [mm/s]. */
float APP_PID_GetVelocidad(void) { return pid_test_vel_mm_s; }

/**
 * @brief Publica por telemetría la configuración del lazo.
 *
 * Se manda al iniciar la prueba y **cada vez que la consola cambia algo**, para que el registro de
 * Teleplot diga con qué ganancias se hizo cada tramo de la sintonía.
 */
static void pid_test_publicar_config(void)
{
    TELEM_SendInt("PID_MODO", PID_TEST_MODO);
    TELEM_SendFloat("PID_VEL_OBJ", pid_test_vel_mm_s);
    TELEM_SendFloat("PID_KP", pid_test_kp);
    TELEM_SendFloat("PID_KI", pid_test_ki);
    TELEM_SendFloat("PID_KD", pid_test_kd);
}

/**
 * @brief Prepara los dos lazos de la prueba y anuncia su configuración.
 *
 * @param izq Lazo de la rueda izquierda
 * @param der Lazo de la rueda derecha
 */
static void pid_test_iniciar(Controller_t *izq, Controller_t *der)
{
    CTRL_Init(izq, pid_test_kp, pid_test_ki, pid_test_kd, PID_TEST_DUTY_MAX);
    CTRL_Init(der, pid_test_kp, pid_test_ki, pid_test_kd, PID_TEST_DUTY_MAX);
    CTRL_SetRampa(izq, PID_TEST_RAMPA_MM_S2);
    CTRL_SetRampa(der, PID_TEST_RAMPA_MM_S2);

    if (PID_TEST_MODO == 1) {
        CTRL_SetConsignaEscalon(izq, 0.0f);
        CTRL_SetConsignaEscalon(der, 0.0f);
    } else if (PID_TEST_MODO == 3) {
        /* El barrido automático necesita la consigna alta desde el principio y **sin cambios**: lo que
           se mira es cómo responde el lazo a un escalón que se queda puesto. */
        CTRL_SetConsignaEscalon(izq, pid_test_vel_mm_s);
        CTRL_SetConsignaEscalon(der, pid_test_vel_mm_s);
    } else {
        CTRL_SetConsigna(izq, pid_test_vel_mm_s);
        CTRL_SetConsigna(der, pid_test_vel_mm_s);
    }

    pid_test_pendiente = 0U;   /* la configuración ya se ha aplicado en el CTRL_Init */
    pid_test_publicar_config();
}

/**
 * @brief Reaplica al lazo lo que la consola haya cambiado (`PID KP …`).
 *
 * Se aplica aquí, en el propio lazo, y no desde la consola, para no saltarse el reparto de capas: la
 * consola deja el ajuste escrito en la APP y es el lazo quien lo empuja a CONTROL.
 *
 * @param izq Lazo de la rueda izquierda
 * @param der Lazo de la rueda derecha
 */
static void pid_test_aplicar_cambios(Controller_t *izq, Controller_t *der)
{
    if (pid_test_pendiente == 0U) {
        return;
    }

    pid_test_pendiente = 0U;

    CTRL_SetGanancias(izq, pid_test_kp, pid_test_ki, pid_test_kd);
    CTRL_SetGanancias(der, pid_test_kp, pid_test_ki, pid_test_kd);

    /* En el modo escalón la consigna la lleva el escalón: no se toca desde aquí. */
    if (PID_TEST_MODO != 1) {
        CTRL_SetConsigna(izq, pid_test_vel_mm_s);
        CTRL_SetConsigna(der, pid_test_vel_mm_s);
    }

    pid_test_publicar_config();
}

/**
 * @brief Aplica el escalón de consigna del modo de sintonía.
 *
 * @param izq Lazo de la rueda izquierda
 * @param der Lazo de la rueda derecha
 * @param consigna_alta 1 para pedir la velocidad de la prueba, 0 para pedir parada
 */
static void pid_test_aplicar_escalon(Controller_t *izq, Controller_t *der, uint8_t consigna_alta)
{
    float consigna_mm_s = consigna_alta ? pid_test_vel_mm_s : 0.0f;

    CTRL_SetConsignaEscalon(izq, consigna_mm_s);
    CTRL_SetConsignaEscalon(der, consigna_mm_s);
}

/**
 * @brief Ejecuta un paso del lazo en las dos ruedas y aplica el duty resultante.
 *
 * @param izq Lazo de la rueda izquierda
 * @param der Lazo de la rueda derecha
 * @param dt Segundos desde el paso anterior
 */
static void pid_test_paso_lazo(Controller_t *izq, Controller_t *der, float dt)
{
    float velocidad_izq_mm_s = get_encoder_left_speed() * 1000.0f;
    float velocidad_der_mm_s = get_encoder_right_speed() * 1000.0f;

    DRV8833_Set_Speed(&motor1, (int16_t)CTRL_Update(izq, velocidad_izq_mm_s, dt));
    DRV8833_Set_Speed(&motor2, (int16_t)CTRL_Update(der, velocidad_der_mm_s, dt));
}

/**
 * @brief Envía la consigna, las velocidades y los duties por telemetría.
 *
 * @param izq Lazo de la rueda izquierda
 * @param der Lazo de la rueda derecha
 */
static void pid_test_enviar_telemetria(const Controller_t *izq, const Controller_t *der)
{
    TELEM_SendFloat("PID_SET", CTRL_GetConsigna(izq));
    TELEM_SendFloat("VEL_IZQ", CTRL_GetMedida(izq));
    TELEM_SendFloat("VEL_DER", CTRL_GetMedida(der));
    TELEM_SendFloat("DUTY_IZQ", CTRL_GetDuty(izq));
    TELEM_SendFloat("DUTY_DER", CTRL_GetDuty(der));
}

/**
 * @brief Aplica a la configuración del lazo el cambio pedido con los botones.
 *
 * @param cual Parámetro a mover
 * @param pasos Cuántos escalones, positivo hacia arriba
 * @note La validación la hace la APP (`APP_PID_Set*`): aquí no se decide qué valor es bueno, igual que
 *       hace la consola.
 */
static void pid_test_ajustar(PidTestAjuste_t cual, int8_t pasos)
{
    float delta = pid_test_ajuste_paso[cual] * (float)pasos;

    switch (cual) {
    case PID_TEST_AJUSTE_KP:
        APP_PID_SetGanancia(APP_PID_GANANCIA_KP,
                            APP_PID_GetGanancia(APP_PID_GANANCIA_KP) + delta);
        break;
    case PID_TEST_AJUSTE_KI:
        APP_PID_SetGanancia(APP_PID_GANANCIA_KI,
                            APP_PID_GetGanancia(APP_PID_GANANCIA_KI) + delta);
        break;
    case PID_TEST_AJUSTE_KD:
        APP_PID_SetGanancia(APP_PID_GANANCIA_KD,
                            APP_PID_GetGanancia(APP_PID_GANANCIA_KD) + delta);
        break;
    case PID_TEST_AJUSTE_VEL:
        APP_PID_SetVelocidad(APP_PID_GetVelocidad() + delta);
        break;
    default:
        break;
    }
}

/**
 * @brief Pinta el estado del lazo de velocidad (prueba 18).
 *
 * Sin esto la prueba parece colgada: el cartel de `app_mostrar_prueba()` se queda fijo aunque el lazo
 * esté trabajando. Las dos velocidades van juntas y el duty también, porque lo que se mira de reojo
 * es **la diferencia entre ruedas**, que es lo que decide si el robot ira recto.
 *
 * @param izq Lazo de la rueda izquierda
 * @param der Lazo de la rueda derecha
 * @param ajuste Parámetro que mueven los botones, marcado con `>`
 * @note Se enseñan **las tres ganancias a la vez**, no solo la seleccionada. Es por las órdenes de
 *       consola: al mandar `PID KI 0` el valor cambia, pero si la pantalla solo enseña el parámetro
 *       marcado **no hay forma de darse cuenta** y parece que la orden no ha entrado.
 */
static void app_pantalla_pid(const Controller_t *izq, const Controller_t *der, PidTestAjuste_t ajuste)
{
    char texto[24];
    char otras[24];

    switch (ajuste) {
    case PID_TEST_AJUSTE_KI:
        snprintf(texto, sizeof(texto), "PID >KI %.2f", (double)pid_test_ki);
        snprintf(otras, sizeof(otras), "KP %.2f KD %.2f", (double)pid_test_kp, (double)pid_test_kd);
        break;
    case PID_TEST_AJUSTE_KD:
        snprintf(texto, sizeof(texto), "PID >KD %.2f", (double)pid_test_kd);
        snprintf(otras, sizeof(otras), "KP %.2f KI %.2f", (double)pid_test_kp, (double)pid_test_ki);
        break;
    case PID_TEST_AJUSTE_VEL:
        snprintf(texto, sizeof(texto), "PID >VEL %d", (int)APP_PID_GetVelocidad());
        snprintf(otras, sizeof(otras), "KP %.2f KI %.2f", (double)pid_test_kp, (double)pid_test_ki);
        break;
    case PID_TEST_AJUSTE_KP:
    default:
        snprintf(texto, sizeof(texto), "PID >KP %.2f", (double)pid_test_kp);
        snprintf(otras, sizeof(otras), "KI %.2f KD %.2f", (double)pid_test_ki, (double)pid_test_kd);
        break;
    }

    OLED_Clear();

    oled_print_line(0U, texto);
    oled_print_line(1U, otras);

    snprintf(texto, sizeof(texto), "IZQ %d DER %d",
             (int)CTRL_GetMedida(izq), (int)CTRL_GetMedida(der));
    oled_print_line(2U, texto);

    snprintf(texto, sizeof(texto), "SET %d D %d %d",
             (int)CTRL_GetConsigna(izq), (int)CTRL_GetDuty(izq), (int)CTRL_GetDuty(der));
    oled_print_line(3U, texto);

    OLED_Update();
}

/**
 * @brief Pinta el estado del barrido automático de `Kp` (modo 3).
 *
 * @param izq Lazo de la rueda izquierda
 * @param der Lazo de la rueda derecha
 * @param estado En qué punto va la búsqueda de `Ku`
 * @param pausa 1 si el barrido está en pausa (**OK**)
 * @param ku Ganancia crítica encontrada
 * @param rango Amplitud pico a pico de la ventana actual [mm/s]
 * @param tu Periodo de la oscilación medido [s]
 */
static void app_pantalla_pid_auto(const Controller_t *izq, const Controller_t *der,
                                  PidAutoEstado_t estado, uint8_t pausa,
                                  float ku, float rango, float tu)
{
    char texto[24];

    OLED_Clear();

    if (pausa != 0U) {
        snprintf(texto, sizeof(texto), "PAUSA KP %.2f", (double)pid_test_kp);
    } else if (estado == PID_AUTO_ENCONTRADO) {
        snprintf(texto, sizeof(texto), "KU %.2f TU %.2f", (double)ku, (double)tu);
    } else if (estado == PID_AUTO_AGOTADO) {
        snprintf(texto, sizeof(texto), "SIN OSCILAR");
    } else {
        snprintf(texto, sizeof(texto), "AUTO KP %.2f",
                 (double)APP_PID_GetGanancia(APP_PID_GANANCIA_KP));
    }
    oled_print_line(0U, texto);

    snprintf(texto, sizeof(texto), "IZQ %d DER %d",
             (int)CTRL_GetMedida(izq), (int)CTRL_GetMedida(der));
    oled_print_line(1U, texto);

    snprintf(texto, sizeof(texto), "AMP %d  SET %d", (int)rango, (int)CTRL_GetConsigna(izq));
    oled_print_line(2U, texto);

    oled_print_line(3U, (pausa != 0U) ? "OK:SIGUE MENU:SALIR" : "OK:PAUSA MENU:SALIR");

    OLED_Update();
}

/**
 * @brief Lazo de velocidad por rueda con consigna conmutable (prueba 18).
 *
 * Cierra el lazo de velocidad de cada rueda midiendo con su encoder: consigna (con rampa) → PID →
 * duty. Es la puerta de salida de la capa CONTROL.
 *
 * @warning El robot se mueve: ruedas en el aire o espacio de sobra.
 * @note El modo 1 de PID_TEST_MODO sirve para sintonizar con Ziegler-Nichols (escalón que provoca la
 *       oscilación sostenida); el 2, con rampa, es el de trabajo.
 * @note **Se puede sintonizar sin recompilar y sin cable**, de dos maneras que conviven:
 *       los **botones** de la propia prueba (**OK** cambia de parámetro, **UP/DOWN** lo mueven) y la
 *       orden `PID KP|KI|KD|VEL <valor>` de la consola
 *       ([comandos-serie.md](../Documentacion/comandos-serie.md)). El cambio se aplica en el paso
 *       siguiente del lazo. `PID LIST` publica la configuración, que también sale sola al arrancar y
 *       en cada cambio (`>PID_KP`…).
 * @note Los botones están porque para que la sintonía valga el robot tiene que estar **en el suelo**,
 *       no sobre la mesa, y ahí el cable estorba. Además, **un puerto serie solo lo abre un programa a
 *       la vez**: si el que lee la telemetría no deja escribir, las órdenes no se pueden mandar.
 * @note Se sale con el botón MENU, como el resto del banco; al volver se frena.
 * @see `Documentacion/control-pid.md` y `Documentacion/pruebas-bringup.md` §14
 */
void App_Test_PID(void)
{
    Controller_t      lazo_izq;
    Controller_t      lazo_der;
    PidTestAjuste_t   ajuste        = PID_TEST_AJUSTE_KP;
    PidTestOsc_t      osc           = { 0U, 0U, 0U, 0U, 0U };
    PidTestAmplitud_t amp           = { 0.0f, 0.0f, 0U };
    PidAutoEstado_t   auto_estado   = PID_AUTO_BUSCANDO;
    float             auto_ku       = 0.0f;
    uint8_t           auto_alta     = 0U;
    uint8_t           auto_pausa    = 0U;
    uint32_t          t_ultimo_paso = HAL_GetTick();
    uint32_t          t_ultimo_log  = HAL_GetTick();
    uint32_t          t_escalon     = HAL_GetTick();
    uint32_t          t_auto        = HAL_GetTick();
    uint32_t          t_pantalla    = HAL_GetTick();
    uint8_t           consigna_alta = 0U;

    TRACE_Reiniciar();

    if (PID_TEST_MODO == 3) {
        /* Ziegler-Nichols se hace **solo con la P**. El integrador viene puesto a 0,60 de fábrica y con
           él el lazo oscila por su cuenta desde el primer `Kp`: la amplitud sale alta siempre y el
           barrido se declara terminado en el primer escalón con un `Ku` falso. */
        APP_PID_SetGanancia(APP_PID_GANANCIA_KI, 0.0f);
        APP_PID_SetGanancia(APP_PID_GANANCIA_KD, 0.0f);
        APP_PID_SetGanancia(APP_PID_GANANCIA_KP, PID_AUTO_KP_INI);
    }

    pid_test_iniciar(&lazo_izq, &lazo_der);

    if (PID_TEST_MODO == 3) {
        app_pantalla_pid_auto(&lazo_izq, &lazo_der, auto_estado, auto_pausa, auto_ku, 0.0f, 0.0f);
    } else {
        app_pantalla_pid(&lazo_izq, &lazo_der, ajuste);
    }

    while (!BUTTONS_SalidaSolicitada()) {
        uint32_t ahora = HAL_GetTick();
        uint8_t  evento;

        update_encoder_readings();

        /* La consola hay que atenderla AQUÍ: esta prueba es bloqueante y, sin esta llamada, las
           órdenes `PID KP …` no se leerían hasta salir al menú, que es justo cuando ya no sirven. */
        CONSOLE_Update();
        pid_test_aplicar_cambios(&lazo_izq, &lazo_der);

        BUTTONS_Update();
        evento = BUTTONS_GetEvent();

        if ((PID_TEST_MODO == 3) && (evento == BTN_NUM_OK)) {
            /* La pausa para los motores: con el barrido subiendo `Kp` y los motores temblando no hay
               quien lea la pantalla. */
            auto_pausa = (uint8_t)(!auto_pausa);

            if (auto_pausa != 0U) {
                CTRL_Parar(&lazo_izq);
                CTRL_Parar(&lazo_der);
                APP_FrenoTotal();
            } else {
                CTRL_SetConsignaEscalon(&lazo_izq, pid_test_vel_mm_s);
                CTRL_SetConsignaEscalon(&lazo_der, pid_test_vel_mm_s);
                t_auto = HAL_GetTick();   /* tramo completo tras reanudar */
            }

            app_pantalla_pid_auto(&lazo_izq, &lazo_der, auto_estado, auto_pausa, auto_ku,
                                  pid_test_amp_rango(&amp), pid_test_osc_tu(&osc));
        } else if ((PID_TEST_MODO != 3) && (evento == BTN_NUM_OK)) {
            ajuste = (PidTestAjuste_t)(((uint8_t)ajuste + 1U) % (uint8_t)PID_TEST_AJUSTE_N);
            app_pantalla_pid(&lazo_izq, &lazo_der, ajuste);
        } else if ((PID_TEST_MODO != 3) && (evento == BTN_NUM_UP)) {
            pid_test_ajustar(ajuste, 1);
            app_pantalla_pid(&lazo_izq, &lazo_der, ajuste);
        } else if ((PID_TEST_MODO != 3) && (evento == BTN_NUM_DOWN)) {
            pid_test_ajustar(ajuste, -1);
            app_pantalla_pid(&lazo_izq, &lazo_der, ajuste);
        }

        if (PID_TEST_MODO == 1) {
            if ((ahora - t_escalon) >= PID_TEST_PERIODO_MS) {
                t_escalon = ahora;
                consigna_alta = (uint8_t)(!consigna_alta);
                pid_test_aplicar_escalon(&lazo_izq, &lazo_der, consigna_alta);
            }
        } else if (PID_TEST_MODO == 3) {
            /* Barrido automático: se deja `Kp` quieto `PID_AUTO_PASO_MS`, se mide la amplitud de lo que
               ha oscilado en la parte final del tramo (sin el arranque) y, si se ha apagado, se sube un
               escalón más. */
            if ((auto_pausa == 0U) && (auto_estado == PID_AUTO_BUSCANDO) &&
                ((ahora - t_auto) >= PID_AUTO_PASO_MS)) {
                float rango = pid_test_amp_rango(&amp);

                t_auto = ahora;
                pid_test_amp_reiniciar(&amp);

                /* El contador de cruces también se reinicia en cada tramo: así lo que se evalúa es la
                   oscilación de **este** `Kp`, no la de los anteriores. */
                osc.cruces = 0U;
                osc.cuenta = 0U;
                osc.encima = 0U;

                /* No basta con que haya amplitud: tiene que haber **cruces**, que es lo que distingue
                   una oscilación de un pico suelto. */
                if ((rango >= (PID_AUTO_AMP_FRAC * pid_test_vel_mm_s)) &&
                    (osc.cruces >= PID_AUTO_CRUCES_MIN)) {
                    /* Amplitud alta, pero no se canta todavía: un pico suelto no es oscilación sostenida.
                       Solo se sube `Kp` cuando una ventana sale **limpia**. */
                    auto_alta++;

                    if (auto_alta >= PID_AUTO_CONFIRMACIONES) {
                        auto_estado = PID_AUTO_ENCONTRADO;
                        auto_ku     = pid_test_kp;

                        TELEM_SendFloat("PID_KU", auto_ku);
                        TELEM_SendFloat("PID_TU", pid_test_osc_tu(&osc));
                        TELEM_SendInt("PID_AUTO", (int32_t)auto_estado);

                        /* Se deja puesta la `Kp` que Ziegler-Nichols pide para el lazo afinado, para no
                           tener que teclear nada antes de empezar a ajustar. */
                        APP_PID_SetGanancia(APP_PID_GANANCIA_KP, 0.6f * auto_ku);
                    }
                } else {
                    auto_alta = 0U;

                    if (pid_test_kp < PID_AUTO_KP_MAX) {
                        APP_PID_SetGanancia(APP_PID_GANANCIA_KP, pid_test_kp + PID_AUTO_KP_PASO);
                    } else {
                        auto_estado = PID_AUTO_AGOTADO;
                        TELEM_SendInt("PID_AUTO", (int32_t)auto_estado);
                    }
                }

                app_pantalla_pid_auto(&lazo_izq, &lazo_der, auto_estado, auto_pausa, auto_ku, rango,
                                      pid_test_osc_tu(&osc));
            }
        }

        if ((auto_pausa != 0U) && (PID_TEST_MODO == 3)) {
            HAL_Delay(PID_TEST_LAZO_MS);
            continue;   /* en pausa no se mueve nada: solo se atienden los botones */
        }

        if ((ahora - t_ultimo_paso) >= PID_TEST_LAZO_MS) {
            float consigna;
            float vel_izq;
            float vel_der;

            pid_test_paso_lazo(&lazo_izq, &lazo_der, (float)(ahora - t_ultimo_paso) / 1000.0f);
            t_ultimo_paso = ahora;

            /* Un paso del lazo es una muestra de la traza, y también la ocasión de mirar si la
               velocidad ha cruzado la consigna (para medir Tu) y cuánto oscila. */
            consigna = CTRL_GetConsigna(&lazo_izq);
            vel_izq  = CTRL_GetMedida(&lazo_izq);
            vel_der  = CTRL_GetMedida(&lazo_der);

            TRACE_Guarda((int16_t)consigna, (int16_t)vel_izq, (int16_t)vel_der);
            pid_test_osc_paso(&osc, consigna, vel_izq, ahora);

            /* La amplitud no se mide desde el primer instante del tramo: el arranque tiene un pico que
               no es oscilación. */
            if ((PID_TEST_MODO != 3) || ((ahora - t_auto) >= PID_AUTO_ASENTAR_MS)) {
                pid_test_amp_paso(&amp, vel_izq);
            }
        }

        if ((ahora - t_ultimo_log) >= 50U) {
            t_ultimo_log = ahora;
            pid_test_enviar_telemetria(&lazo_izq, &lazo_der);
        }

        if ((ahora - t_pantalla) >= PID_TEST_PANTALLA_MS) {
            t_pantalla = ahora;

            if (PID_TEST_MODO == 3) {
                app_pantalla_pid_auto(&lazo_izq, &lazo_der, auto_estado, auto_pausa, auto_ku,
                                      pid_test_amp_rango(&amp), pid_test_osc_tu(&osc));
            } else {
                app_pantalla_pid(&lazo_izq, &lazo_der, ajuste);
            }
        }

        HAL_Delay(1);
    }

    APP_FrenoTotal();
}

/**
 * @brief Conversión radianes → grados para la telemetría de la prueba 19.
 *
 * La pose se guarda en SI (rad) y se publica en grados porque un giro de 90° se lee de un vistazo
 * mientras se empuja el robot.
 */
#define ODOM_TEST_RAD_A_GRADOS    57.29577951f

/** @brief Periodo de repintado de la pantalla de la prueba 19, en ms (la OLED tarda ~50 ms). */
#define ODOM_TEST_PANTALLA_MS      200U

/**
 * @brief Publica la pose estimada y el estado de los encoders (prueba 19).
 *
 * X e Y van en mm y θ en grados por comodidad al leer Teleplot; los µm acumulados de cada rueda son
 * el diagnóstico: si al empujar recto los dos se separan mucho, hay un encoder invertido o perdiendo
 * pulsos.
 *
 * @param odo Pose a publicar
 * @param um_izq Micrómetros acumulados de la rueda izquierda
 * @param um_der Micrómetros acumulados de la rueda derecha
 */
static void odom_test_enviar_telemetria(const Odometry_t *odo, int32_t um_izq, int32_t um_der)
{
    TELEM_SendFloat("ODO_X", odo->x * 1000.0f);
    TELEM_SendFloat("ODO_Y", odo->y * 1000.0f);
    TELEM_SendFloat("ODO_TH", odo->theta * ODOM_TEST_RAD_A_GRADOS);
    TELEM_SendFloat("ODO_D", odo->distance * 1000.0f);
    TELEM_SendInt("ODO_UM_L", um_izq);
    TELEM_SendInt("ODO_UM_R", um_der);
}

/**
 * @brief Distancia del robot al origen de la prueba [mm].
 *
 * Es la cifra que se mira al final de una maniobra cerrada (el cuadrado de §15). Con la X y la Y en mm
 * sale **un solo número**, que es además el único que se puede escribir en la pantalla: la fuente de
 * `app/oled.c` no tiene glifo de `Y`.
 *
 * @param odo Pose de la que sacar el cierre
 * @return Distancia al origen en mm
 */
static float odom_test_cierre_mm(const Odometry_t *odo)
{
    return sqrtf((odo->x * odo->x) + (odo->y * odo->y)) * 1000.0f;
}

/**
 * @brief Pinta la pose estimada mientras se empuja el robot a mano (prueba 19).
 *
 * La prueba no mueve los motores, así que la pantalla es lo único que dice que está viva: sin ella se
 * queda fijo el cartel de `app_mostrar_prueba()` y parece que OK no ha hecho nada.
 *
 * Se enseñan las tres cifras que se miran al terminar una maniobra, **sin necesidad de Teleplot** (con
 * el robot en el suelo el cable estorba): el camino recorrido `D` —que comparado con la maniobra dice
 * el error de distancia—, el ángulo `ANG` y el cierre `CIE` respecto al origen.
 *
 * @param odo Pose a pintar
 * @param congelado 1 si la pantalla está retenida (UP): se pinta el rótulo pero no se refresca
 * @note La `Y` no se puede escribir con la fuente de `app/oled.c` (no tiene glifo), igual que la `H`
 *       de «TH»: de ahí que el ángulo vaya como `ANG` y que la posición se dé como cierre.
 */
static void app_pantalla_odometria(const Odometry_t *odo, uint8_t congelado)
{
    char texto[24];

    OLED_Clear();
    oled_print_line(0U, (congelado != 0U) ? "CONGELADO" : "ODOMETRIA");

    snprintf(texto, sizeof(texto), "D %d MM", (int)(odo->distance * 1000.0f));
    oled_print_line(1U, texto);

    snprintf(texto, sizeof(texto), "ANG %d X %d",
             (int)(odo->theta * ODOM_TEST_RAD_A_GRADOS), (int)(odo->x * 1000.0f));
    oled_print_line(2U, texto);

    snprintf(texto, sizeof(texto), "CIE %d MM", (int)odom_test_cierre_mm(odo));
    oled_print_line(3U, texto);

    OLED_Update();
}

/**
 * @brief Mueve la pose por odometría mientras se empuja el robot a mano (prueba 19).
 *
 * Es la prueba de banco de la capa FILTER: no hay control, el robot se mueve a mano y lo único que se
 * comprueba es que la pose estimada se corresponde con lo recorrido de verdad en el suelo. El botón OK
 * borra el origen sin salir de la prueba, para poder encadenar maniobras.
 *
 * @note La odometría consume distancias, no velocidades: se le pasan los incrementos de µm de cada
 *       rueda entre lecturas (ver `filter/odometry.h`).
 * @note La pantalla (`app_pantalla_odometria()`) se repinta a 5 Hz con el camino (`D`), el ángulo
 *       (`ANG`) y el **cierre** respecto al origen (`CIE`), que son las tres cifras que se miran al
 *       terminar una maniobra; el detalle de cada rueda (`>ODO_UM_L`, `>ODO_UM_R`) va solo por
 *       telemetría.
 * @note **UP congela la pantalla** (`>ODO_HOLD:1`) para poder apuntar la lectura sin que se mueva si
 *       se toca el robot; **OK** pone el origen a cero. La odometría sigue integrándose con la
 *       pantalla congelada: solo se deja de repintar.
 * @warning Los dos encoders deben contar positivo al avanzar. Empujando en línea recta, `>ODO_UM_L` y
 *          `>ODO_UM_R` deben parecerse y `>ODO_TH` quedarse cerca de 0; si θ se dispara, un encoder
 *          está invertido (intercambiar sus hilos A/B).
 * @see Procedimiento, cotas y calibración de la trocha en `Documentacion/pruebas-bringup.md` §15.
 */
void App_Test_Odometria(void)
{
    Odometry_t odo;
    int32_t    um_izq_previo;
    int32_t    um_der_previo;
    uint32_t   t_telemetria;
    uint32_t   t_pantalla;
    uint8_t    congelado = 0U;

    ODOM_Init(&odo, 0.0f, 0.0f, 0.0f);
    update_encoder_readings();
    um_izq_previo = get_encoder_left_micrometers();
    um_der_previo = get_encoder_right_micrometers();
    t_telemetria  = HAL_GetTick();
    t_pantalla    = t_telemetria;

    TELEM_SendFloat("ODO_TROCHA_MM", ODOM_GetTrocha());
    app_pantalla_odometria(&odo, congelado);

    while (!BUTTONS_SalidaSolicitada()) {
        int32_t  um_izq;
        int32_t  um_der;
        uint8_t  evento;
        uint32_t ahora;

        update_encoder_readings();
        um_izq = get_encoder_left_micrometers();
        um_der = get_encoder_right_micrometers();

        ODOM_Update(&odo,
                    (float)(um_izq - um_izq_previo) / 1000000.0f,
                    (float)(um_der - um_der_previo) / 1000000.0f);

        BUTTONS_Update();
        evento = BUTTONS_GetEvent();

        if (evento == BTN_NUM_OK) {
            ODOM_Reset(&odo);
            TELEM_SendInt("ODO_EVT", 1);
            app_pantalla_odometria(&odo, congelado);   /* el cero se ve en el acto */
        } else if (evento == BTN_NUM_UP) {
            congelado = (uint8_t)(!congelado);
            TELEM_SendInt("ODO_HOLD", (int32_t)congelado);
            app_pantalla_odometria(&odo, congelado);
        }

        um_izq_previo = um_izq;
        um_der_previo = um_der;

        ahora = HAL_GetTick();
        if ((ahora - t_telemetria) >= 50U) {
            t_telemetria = ahora;
            odom_test_enviar_telemetria(&odo, um_izq, um_der);
        }

        if ((congelado == 0U) && ((ahora - t_pantalla) >= ODOM_TEST_PANTALLA_MS)) {
            t_pantalla = ahora;
            app_pantalla_odometria(&odo, congelado);
        }

        HAL_Delay(1);
    }
}

/**
 * @brief Parámetros de la prueba de primitivas de movimiento.
 *
 * Los valores por defecto son los del laberinto: una celda de 180 mm y un giro de 90°. Son los que hay
 * que dejar clavados, porque de su repetibilidad depende todo lo que venga después.
 */
#define MOV_TEST_MM            180.0f
#define MOV_TEST_GRADOS        90.0f

/** @brief Periodo de repintado de la pantalla de la prueba de movimiento, en ms. */
#define MOV_TEST_PANTALLA_MS   200U

/**
 * @brief Lleva una **diferencia** de ángulos al rango $(-180, 180]$ [°].
 *
 * Hace falta porque la odometría normaliza $\\theta$ en ese rango: girando siempre a la izquierda el
 * ángulo va 90 → 180 → **−90** → 0, así que al compararlo con el teórico acumulado (90, 180, 270, 360)
 * la resta sale **−360** a partir del tercer giro aunque el robot esté perfecto.
 *
 * @note El giro en sí **no** se ve afectado: `MOTION_Girar()` ya normaliza su objetivo con
 *       `motion_error_angulo()`. El desfase era solo del número que se enseña.
 * @param grados Diferencia de ángulos [°]
 * @return La misma diferencia dentro de $(-180, 180]$
 */
static float mov_test_error_angulo(float grados)
{
    while (grados > 180.0f) {
        grados -= 360.0f;
    }
    while (grados <= -180.0f) {
        grados += 360.0f;
    }

    return grados;
}

/**
 * @brief Publica por telemetría el estado de las primitivas.
 *
 * Los errores van **acumulados** desde que se lanzó la prueba: es lo que dice si un movimiento suelto
 * acierta (poco interesante) o si cuatro movimientos encadenados siguen acertando (lo que importa).
 *
 * @param m Estructura de las primitivas
 * @param teorico_mm Distancia que debería llevar recorrida [mm]
 * @param teorico_ang Ángulo que debería llevar girado [°]
 * @param ok 1 si el último movimiento llegó dentro de tolerancia
 */
static void mov_test_publicar(const Motion_t *m, float teorico_mm, float teorico_ang, uint8_t ok)
{
    TELEM_SendFloat("MOV_D", m->odo.distance * 1000.0f);
    TELEM_SendFloat("MOV_ANG", MOTION_GetAngulo(m));
    /* La posición es lo único que dice si el robot **ha vuelto** al sitio: `MOV_D` es el camino
       recorrido y solo sube, así que en una ida y vuelta vale el doble y no informa de nada. */
    TELEM_SendFloat("MOV_X", m->odo.x * 1000.0f);
    TELEM_SendFloat("MOV_Y", m->odo.y * 1000.0f);
    TELEM_SendFloat("MOV_E_D", (m->odo.distance * 1000.0f) - teorico_mm);
    TELEM_SendFloat("MOV_E_ANG", mov_test_error_angulo(MOTION_GetAngulo(m) - teorico_ang));
    /* El ángulo del **giroscopio**, para comparar: no depende de las ruedas, así que si se separa de
       `MOV_ANG` la que miente es la odometría. `MOV_GYROK` dice si ese número sirve de algo. */
    TELEM_SendFloat("MOV_GYR", MOTION_GetGyroAngulo(m));
    TELEM_SendInt("MOV_GYROK", (int32_t)MOTION_GyroDisponible(m));
    TELEM_SendFloat("MOV_SESGO", MOTION_GetGyroSesgo(m));
    /* La referencia del giro que está en uso, para que el registro diga **con cuál** se hizo cada
       maniobra (orden `GYRO` de la consola). */
    TELEM_SendInt("MOV_GMODO", (int32_t)MOTION_GetModoGiro());
    TELEM_SendInt("MOV_OK", (int32_t)ok);
}

/**
 * @brief Pinta el resultado de las primitivas: lo recorrido y el error acumulado.
 *
 * @param m Estructura de las primitivas
 * @param teorico_mm Distancia que debería llevar recorrida [mm]
 * @param teorico_ang Ángulo que debería llevar girado [°]
 */
static void app_pantalla_movimiento(const Motion_t *m, float teorico_mm, float teorico_ang)
{
    char texto[24];

    OLED_Clear();
    oled_print_line(0U, "MOVIMIENTO");

    snprintf(texto, sizeof(texto), "D %d E %d",
             (int)(m->odo.distance * 1000.0f), (int)((m->odo.distance * 1000.0f) - teorico_mm));
    oled_print_line(1U, texto);

    snprintf(texto, sizeof(texto), "ANG %d E %d",
             (int)MOTION_GetAngulo(m),
             (int)mov_test_error_angulo(MOTION_GetAngulo(m) - teorico_ang));
    oled_print_line(2U, texto);

    oled_print_line(3U, "OK:180 UP:90 DN:90");

    OLED_Update();
}

/* ------------------------------------------------------------------ */
/** @brief Petición de movimiento pendiente de atender (ver `AG_MOV_Pedir`). */
static App_Movimiento_t app_mov_peticion     = APP_MOV_NINGUNO;

/** @brief Cuántas veces hay que repetir la petición pendiente. */
static uint8_t          app_mov_repeticiones = 1U;

/** @brief 1 mientras la prueba 20 está en marcha y recoge peticiones. */
static uint8_t          app_mov_activo       = 0U;

/**
 * @brief Deja escrita una petición de movimiento para la prueba 20 (orden `MOV` de la consola).
 *
 * @param cual Acción pedida
 * @param repeticiones Cuántas veces hay que repetirla (1 = una)
 * @return 1 si se aceptó, 0 si la prueba 20 no está en marcha, ya había otra petición pendiente, o los
 *         argumentos no valen
 * @note Se **rechaza** en vez de encolar cuando ya hay una petición sin atender. Con una cola de una
 *       posición, encolar en silencio daría la impresión de que las órdenes se pierden al azar.
 */
uint8_t APP_MOV_Pedir(App_Movimiento_t cual, uint8_t repeticiones)
{
    if ((app_mov_activo == 0U) || (app_mov_peticion != APP_MOV_NINGUNO)) {
        return 0U;
    }

    if ((cual == APP_MOV_NINGUNO) || (cual >= APP_MOV_N) ||
        (repeticiones == 0U) || (repeticiones > APP_MOV_REP_MAX)) {
        return 0U;
    }

    app_mov_peticion     = cual;
    app_mov_repeticiones = repeticiones;

    return 1U;
}

/**
 * @brief Recoge la petición pendiente y la marca como atendida.
 *
 * @param cual Acción pedida (solo es válido si se devuelve 1)
 * @param repeticiones Cuántas veces repetirla
 * @return 1 si había petición, 0 si no
 */
uint8_t APP_MOV_Recoger(App_Movimiento_t *cual, uint8_t *repeticiones)
{
    if ((cual == 0) || (repeticiones == 0) || (app_mov_peticion == APP_MOV_NINGUNO)) {
        return 0U;
    }

    *cual         = app_mov_peticion;
    *repeticiones = app_mov_repeticiones;

    app_mov_peticion = APP_MOV_NINGUNO;

    return 1U;
}

/**
 * @brief Ejecuta una de las tres acciones del banco y publica el resultado.
 *
 * Es el **único** sitio que lanza un movimiento: lo llaman tanto los botones como las órdenes `MOV`,
 * así que las dos vías se comportan exactamente igual (misma acumulación del teórico, misma telemetría
 * y misma pantalla). Con dos caminos separados, lo que se mide por consola no sería lo mismo que lo
 * que se mide con el dedo.
 *
 * @param m Estructura de las primitivas
 * @param cual Acción a ejecutar
 * @param teorico_mm Distancia teórica acumulada [mm]; se actualiza
 * @param teorico_ang Ángulo teórico acumulado [°]; se actualiza
 */
static void mov_test_ejecutar(Motion_t *m, App_Movimiento_t cual,
                              float *teorico_mm, float *teorico_ang)
{
    uint8_t ok;

    switch (cual) {
    case APP_MOV_AVANZAR:
        *teorico_mm += MOV_TEST_MM;
        ok = MOTION_Avanzar(m, MOV_TEST_MM);
        break;
    case APP_MOV_RETROCEDER:
        *teorico_mm -= MOV_TEST_MM;
        ok = MOTION_Avanzar(m, -MOV_TEST_MM);
        break;
    case APP_MOV_GIRAR_IZQ:
        *teorico_ang += MOV_TEST_GRADOS;
        ok = MOTION_Girar(m, MOV_TEST_GRADOS);
        break;
    case APP_MOV_GIRAR_DER:
        *teorico_ang -= MOV_TEST_GRADOS;
        ok = MOTION_Girar(m, -MOV_TEST_GRADOS);
        break;
    default:
        return;
    }

    mov_test_publicar(m, *teorico_mm, *teorico_ang, ok);
    app_pantalla_movimiento(m, *teorico_mm, *teorico_ang);
}

/**
 * @brief Prueba de las primitivas de movimiento: avanzar una celda y girar 90°.
 *
 * Es el banco de `app/motion.c`. Cada pulsación lanza **un** movimiento y deja el resultado en la
 * pantalla, así que encadenando pulsaciones se prueba lo que de verdad importa: que los errores **no se
 * acumulen**. Cuatro avances seguidos deben sumar 720 mm, no 720 ± 40.
 *
 * @note Botones: **OK** avanza `MOV_TEST_MM` · **UP** gira `MOV_TEST_GRADOS` a la izquierda ·
 *       **DOWN** gira lo mismo a la derecha · **MENU** sale.
 * @note Las ganancias del lazo de velocidad no se piden aquí: son las de la prueba 18
 *       (`APP_PID_GetGanancia()`), que se sintoniza en caliente por consola. Se sintoniza una vez y
 *       vale para todo.
 * @note Lo mismo que hacen los botones se puede pedir por consola con `MOV OK` / `MOV UP` / `MOV DOWN`
 *       (y un contador: `MOV OK 4`). Existe para poder encadenar un cuadrado entero sin tener a nadie
 *       pulsando, y para que las pruebas las pueda guionizar el PC. Las dos vías llaman al mismo
 *       `mov_test_ejecutar()`.
 * @warning **El robot se mueve**: dale sitio. En el suelo, no sobre la mesa.
 * @see Los criterios de la celda y del giro están en `Documentacion/pruebas-bringup.md`.
 */
void App_Test_Movimiento(void)
{
    Motion_t m;
    float    teorico_mm  = 0.0f;
    float    teorico_ang = 0.0f;

    MOTION_Init(&m, &motor1, &motor2,
                APP_PID_GetGanancia(APP_PID_GANANCIA_KP),
                APP_PID_GetGanancia(APP_PID_GANANCIA_KI),
                APP_PID_GetGanancia(APP_PID_GANANCIA_KD));

    /* A partir de aquí la consola puede mandar movimientos, y hasta aquí no: entrar en la prueba no
       debe ejecutar una orden que se quedara escrita antes. */
    app_mov_activo   = 1U;
    app_mov_peticion = APP_MOV_NINGUNO;

    app_pantalla_movimiento(&m, teorico_mm, teorico_ang);

    while (!BUTTONS_SalidaSolicitada()) {
        App_Movimiento_t pedido;
        uint8_t          repeticiones = 1U;
        uint8_t          evento;

        /* La consola hay que atenderla AQUÍ: esta prueba es bloqueante y, sin esta llamada, las
           órdenes `MOV ...` no se leerían hasta salir al menú, que es justo cuando ya no sirven. */
        CONSOLE_Update();

        BUTTONS_Update();
        evento = BUTTONS_GetEvent();

        if (evento == BTN_NUM_OK) {
            mov_test_ejecutar(&m, APP_MOV_AVANZAR, &teorico_mm, &teorico_ang);
        } else if (evento == BTN_NUM_UP) {
            mov_test_ejecutar(&m, APP_MOV_GIRAR_IZQ, &teorico_mm, &teorico_ang);
        } else if (evento == BTN_NUM_DOWN) {
            mov_test_ejecutar(&m, APP_MOV_GIRAR_DER, &teorico_mm, &teorico_ang);
        } else if (APP_MOV_Recoger(&pedido, &repeticiones) != 0U) {
            /* Las repeticiones son movimientos **independientes**, no uno más largo: es lo que mide si
               el error se acumula, que es justo lo que se viene a ver aquí. */
            for (uint8_t i = 0U; i < repeticiones; i++) {
                if (BUTTONS_SalidaSolicitada() != 0U) {
                    break;
                }

                mov_test_ejecutar(&m, pedido, &teorico_mm, &teorico_ang);
            }
        } else {
            HAL_Delay(1);
        }
    }

    app_mov_activo = 0U;

    MOTION_Parar(&m);
}

/* ------------------------------------------------------------------ */
/**
 * @brief Publica por telemetría el estado del *wall follower*.
 *
 * Se publica **una vez por celda**, que es lo que dura un paso. Con el cable puesto es la vista en
 * vivo; sin cable, lo que vale es la traza, que guarda lo mismo pero dentro del robot.
 *
 * @param m Primitivas de movimiento, de donde sale la pose
 * @param mano Mano en uso
 */
static void wf_test_publicar(const Motion_t *m, WF_Mano_t mano)
{
    TELEM_SendInt("WF_CELDAS", (int32_t)WF_GetCeldas());
    TELEM_SendInt("WF_MANO", (int32_t)mano);
    TELEM_SendInt("WF_DEC", (int32_t)WF_GetDecision());
    TELEM_SendInt("WF_FRENTE", (int32_t)WALLS_Hay(WALL_FRENTE));
    TELEM_SendInt("WF_IZQ", (int32_t)WALLS_Hay(WALL_IZQUIERDA));
    TELEM_SendInt("WF_DER", (int32_t)WALLS_Hay(WALL_DERECHA));
    TELEM_SendFloat("WF_X", m->odo.x * 1000.0f);
    TELEM_SendFloat("WF_Y", m->odo.y * 1000.0f);
    TELEM_SendFloat("WF_ANG", MOTION_GetAngulo(m));
    /* El ángulo del giroscopio al lado del de la odometría, y no es un adorno: es lo que dice si el
       robot se gira de verdad o es la odometría la que lo cree. `WF_GYROK` avisa si el dato vale. */
    TELEM_SendFloat("WF_GYR", MOTION_GetGyroAngulo(m));
    TELEM_SendInt("WF_GYROK", (int32_t)MOTION_GyroDisponible(m));
    /* El **sesgo** en uso: un valor de decenas de °/s con el robot quieto delata que la calibración se
       contaminó y que el lazo va a integrar el error todo el rato. */
    TELEM_SendFloat("WF_SESGO", MOTION_GetGyroSesgo(m));
}

/**
 * @brief Pinta el estado del *wall follower*.
 *
 * Sin acentos y solo con los glifos de `app/oled.c` (no hay `H`, `J`, `Q`, `Y` ni `Z`): de ahí `IZQ`
 * en vez de `IZQUIERDA` y `VUELTA` en vez de media vuelta.
 *
 * @param m Primitivas de movimiento
 * @param mano Mano en uso
 * @param marcha 1 si está recorriendo
 */
static void app_pantalla_wall_follower(const Motion_t *m, WF_Mano_t mano, uint8_t marcha)
{
    static const char *const nombres_mano[WF_MANO_N] = { "DER", "IZQ" };
    static const char *const nombres_decision[]      = { "-", "AVANZA", "IZQ", "DER", "VUELTA" };
    const char *pie;
    char texto[24];

    /* La última línea dice **por qué** se ha parado, y no es un adorno: en el laberinto el robot va
       **sin cable**, así que la telemetría (`>WF_ATASCO`) no existe para quien está mirando. Sin esto,
       un atasco, el tope de celdas y una parada a mano se ven exactamente igual: el robot quieto. */
    if (marcha != 0U) {
        pie = "OK:PARA MENU:SALIR";
    } else if (WF_GetAtascado() != 0U) {
        pie = "PARADO: ATASCO";
    } else if (WF_GetCeldas() >= WF_CELDAS_MAX) {
        pie = "PARADO: FIN 250";
    } else {
        pie = "OK:ANDA UP:IZ DN:DE";
    }

    OLED_Clear();
    oled_print_line(0U, (marcha != 0U) ? "WALL FOLLOWER >" : "WALL FOLLOWER");

    snprintf(texto, sizeof(texto), "MANO %s C %d", nombres_mano[mano], (int)WF_GetCeldas());
    oled_print_line(1U, texto);

    snprintf(texto, sizeof(texto), "F%d I%d D%d  %s",
             (int)WALLS_Hay(WALL_FRENTE), (int)WALLS_Hay(WALL_IZQUIERDA),
             (int)WALLS_Hay(WALL_DERECHA), nombres_decision[(uint8_t)WF_GetDecision()]);
    oled_print_line(2U, texto);

    oled_print_line(3U, pie);

    OLED_Update();
}

/**
 * @brief Prueba del *wall follower*: recorre el laberinto pegado a una pared.
 *
 * Es el banco de `app/wall_follower.c`. **No se puede probar sobre la mesa**: necesita un laberinto de
 * verdad, o al menos un pasillo con cruces. Los umbrales de pared se ajustan antes, en la prueba de
 * paredes.
 *
 * @note Botones: **OK** arranca o para · **UP** mano izquierda · **DOWN** mano derecha · **MENU** sale.
 * @note Cada arranque pone el contador de celdas a cero y **reinicia la traza**, para que lo que se
 *       descargue después sea el recorrido que se acaba de hacer y no una mezcla con el anterior.
 * @note Se para sola por tres motivos, y cada uno se distingue: MENU, tope de `WF_CELDAS_MAX` celdas, o
 *       **atasco** (`>WF_ATASCO:1`) cuando un avance agota sus 8 s sin llegar a la celda.
 * @warning **El robot se mueve solo**: dale sitio y ten el MENU a mano.
 * @see Procedimiento y criterios en `Documentacion/pruebas-bringup.md`.
 */
void App_Test_WallFollower(void)
{
    Motion_t  m;
    WF_Mano_t mano   = WF_MANO_DERECHA;
    uint8_t   marcha = 0U;

    MOTION_Init(&m, &motor1, &motor2,
                APP_PID_GetGanancia(APP_PID_GANANCIA_KP),
                APP_PID_GetGanancia(APP_PID_GANANCIA_KI),
                APP_PID_GetGanancia(APP_PID_GANANCIA_KD));

    WF_Init(&m, mano);

    app_pantalla_wall_follower(&m, mano, marcha);

    while (!BUTTONS_SalidaSolicitada()) {
        uint8_t evento;

        BUTTONS_Update();
        evento = BUTTONS_GetEvent();

        if (evento == BTN_NUM_OK) {
            marcha = (uint8_t)(!marcha);

            if (marcha != 0U) {
                WF_ReiniciarCeldas();
                TRACE_Reiniciar();
            } else {
                MOTION_Parar(&m);
            }

            app_pantalla_wall_follower(&m, mano, marcha);
        } else if (evento == BTN_NUM_UP) {
            mano = WF_MANO_IZQUIERDA;
            WF_SetMano(mano);
            app_pantalla_wall_follower(&m, mano, marcha);
        } else if (evento == BTN_NUM_DOWN) {
            mano = WF_MANO_DERECHA;
            WF_SetMano(mano);
            app_pantalla_wall_follower(&m, mano, marcha);
        }

        if (marcha == 0U) {
            HAL_Delay(1);
            continue;
        }

        if (WF_Paso() == 0U) {
            marcha = 0U;
            MOTION_Parar(&m);

            /* El motivo que importa es el atasco: MENU o el tope de celdas ya se ven en la pantalla,
               pero un avance que agota sus 8 s sin llegar a la celda solo se sabe por aquí. */
            TELEM_SendInt("WF_ATASCO", (int32_t)WF_GetAtascado());
            TELEM_SendInt("WF_FIN", 1);
            wf_test_publicar(&m, mano);
            app_pantalla_wall_follower(&m, mano, marcha);
            continue;
        }

        wf_test_publicar(&m, mano);
        app_pantalla_wall_follower(&m, mano, marcha);
    }

    MOTION_Parar(&m);
}

/* ------------------------------------------------------------------
 * Prueba del módulo ESP8266 por AT (capa HAL).
 *
 * ⚠️ **Esta prueba NO publica telemetría**, y no es un descuido: el ESP comparte la USART3 con la
 * telemetría y con la consola, así que cualquier `>ETIQUETA:valor` que salga lo recibe el módulo como
 * si fuera un comando, y las respuestas del módulo se las come la consola. Mientras dure la prueba, la
 * **pantalla es la única salida** — que es justo lo que hace falta, porque con el ESP puesto ya no hay
 * cable al PC.
 * ------------------------------------------------------------------ */

/** @brief Tiempo máximo de espera de cada comando [ms]. El barrido de redes tarda varios segundos. */
#define ESP_TEST_TIMEOUT_CORTO  3000U
#define ESP_TEST_TIMEOUT_LAP    25000U

/** @brief En qué punto de la secuencia está la prueba. */
typedef enum {
    ESP_TEST_ESPERA = 0,   /**< Esperando a que se pulse OK        */
    ESP_TEST_AT,           /**< Mandando `AT`                        */
    ESP_TEST_MODO,         /**< Mandando `AT+CWMODE=1`               */
    ESP_TEST_LAP,          /**< Mandando `AT+CWLAP`                  */
    ESP_TEST_CONECTADO,    /**< El módulo contesta                    */
    ESP_TEST_SIN_ESP,      /**< El módulo no contesta                 */
    ESP_TEST_ESTADO_N
} EspTestEstado_t;

static EspTestEstado_t esp_test_estado = ESP_TEST_ESPERA;

/** @brief Redes que ha visto el barrido; es lo **único** que se guarda de `AT+CWLAP`. */
static uint8_t esp_test_redes = 0U;

/**
 * @brief Cuenta las redes del barrido.
 *
 * **Se cuentan, no se guardan.** Lo que se viene a saber aquí es una cosa de sí o no —«¿el módulo
 * contesta?»— y guardar los nombres costaba 800 B de RAM para nada: la fuente de `oled.c` no tiene
 * minúsculas, así que un SSID real no se puede escribir en la pantalla de todas formas. Para
 * configurar la red, el nombre y la contraseña se teclean a mano.
 *
 * @param linea Línea recibida del módulo
 */
static void esp_test_linea(const char *linea)
{
    if ((linea != 0) && (strncmp(linea, "+CWLAP:(", 8) == 0)) {
        esp_test_redes++;
    }
}

/**
 * @brief Deja un texto en lo que la OLED sabe escribir.
 *
 * La fuente de `app/oled.c` solo tiene `" 0123456789ABCDEIKLMNOPSTUW-:V.FGRX+/>"`: **ni minúsculas ni
 * H, J, Q, Y, Z**. Se usa para enseñar **lo que haya contestado el módulo** cuando algo falla, y ahí
 * puede venir cualquier cosa: se pasa a mayúsculas y lo que no exista se sustituye por un punto. Un
 * punto y no un blanco, porque un hueco en medio de un texto no se distingue de un espacio y así se ve
 * que **había** algo.
 *
 * @param origen Texto de entrada
 * @param destino Buffer de salida
 * @param max Capacidad del buffer, terminador incluido
 */
static void esp_test_a_pantalla(const char *origen, char *destino, uint8_t max)
{
    static const char *const permitidos = " 0123456789ABCDEIKLMNOPSTUW-:V.FGRX+/>";
    uint8_t n = 0U;

    if ((origen == 0) || (destino == 0) || (max == 0U)) {
        return;
    }

    while ((*origen != '\0') && (n < (max - 1U))) {
        char c = *origen;

        if ((c >= 'a') && (c <= 'z')) {
            c = (char)(c - 'a' + 'A');
        }

        if (strchr(permitidos, c) == 0) {
            c = '.';
        }

        destino[n] = c;
        n++;
        origen++;
    }

    destino[n] = '\0';
}

/**
 * @brief Pinta el estado de la prueba del ESP.
 */
static void app_pantalla_esp(void)
{
    static const char *const nombres_estado[] = {
        "PULSA OK", "MANDO AT", "MODO ESTACION", "BUSCANDO REDES"
    };
    char texto[24];
    char limpio[24];

    OLED_Clear();

    if ((esp_test_estado == ESP_TEST_CONECTADO) || (esp_test_estado == ESP_TEST_SIN_ESP)) {
        /* El veredicto, a doble tamaño: es lo único que se viene a leer aquí. */
        OLED_PrintCentrado(0U, 2U,
                           (esp_test_estado == ESP_TEST_CONECTADO) ? "CONECTADO" : "SIN ESP");
    } else {
        oled_print_line(0U, "ESP8266");
        oled_print_line(1U, nombres_estado[esp_test_estado]);
    }

    if (esp_test_estado == ESP_TEST_CONECTADO) {
        snprintf(texto, sizeof(texto), "REDES %d", (int)esp_test_redes);
        oled_print_line(2U, texto);
    } else if (esp_test_estado == ESP_TEST_SIN_ESP) {
        if (strlen(ESP_GetRespuesta()) >= (ESP_RESPUESTA_MAX - 1U)) {
            /* El buffer se ha llenado **y no había ningún `OK` dentro**: el módulo está hablando sin
               parar. Enseñar ese texto no sirve de nada —es basura— así que se dice lo que significa:
               casi siempre es un **bucle de arranque por alimentación**, o TX y RX sin cruzar. */
            oled_print_line(2U, "RUIDO");
            oled_print_line(3U, "REVISA 3V3 GND");
        } else {
            /* Lo que haya llegado, aunque sea basura: con el módulo puesto no hay cable al PC por el
               que preguntar. */
            esp_test_a_pantalla(ESP_GetRespuesta(), limpio, sizeof(limpio));
            snprintf(texto, sizeof(texto), "%s", (limpio[0] != '\0') ? limpio : "NADA");
            oled_print_line(2U, texto);
            oled_print_line(3U, "OK:OTRA VEZ");
        }
    }

    if (esp_test_estado == ESP_TEST_CONECTADO) {
        oled_print_line(3U, "OK:OTRA VEZ");
    }

    OLED_Update();
}

/**
 * @brief Da un paso de la secuencia de la prueba del ESP.
 *
 * Un comando por llamada, para que entre uno y otro se pueda repintar la pantalla: si no, el robot se
 * pasa 25 s buscando redes con la pantalla congelada y no se sabe si está trabajando o colgado.
 */
static void esp_test_paso(void)
{
    switch (esp_test_estado) {
    case ESP_TEST_AT:
        /* El veredicto se decide **aquí**, y no al final: si el módulo contesta un `AT`, el enlace
           está bien. Lo que venga después solo añade información. `ESP_Preparar()` además le quita el
           **eco**, que es lo que impide que la telemetría rebote y el robot se realimente solo. */
        esp_test_estado = (ESP_Preparar() == ESP_OK) ? ESP_TEST_MODO : ESP_TEST_SIN_ESP;
        break;

    case ESP_TEST_MODO:
        /* `AT+CWMODE=1` pone el módulo en **modo estación**, que es lo que hace falta para poder
           barrer redes: en modo punto de acceso el `AT+CWLAP` no devuelve nada. Si esto falla, el
           enlace ya está demostrado con el `AT`, así que se pasa a CONECTADO igual. */
        esp_test_estado = (ESP_Comando("AT+CWMODE=1", ESP_TEST_TIMEOUT_CORTO) == ESP_OK) ? ESP_TEST_LAP
                                                                                        : ESP_TEST_CONECTADO;
        break;

    case ESP_TEST_LAP:
        esp_test_redes = 0U;
        (void)ESP_Comando("AT+CWLAP", ESP_TEST_TIMEOUT_LAP);

        /* El número de redes es **información de regalo**: dice que la radio funciona, pero el
           veredicto no depende de él. */
        esp_test_estado = ESP_TEST_CONECTADO;
        break;

    default:
        return;
    }

    app_pantalla_esp();
}

/**
 * @brief Prueba del módulo ESP8266 por comandos AT.
 *
 * Manda la secuencia mínima para saber si el módulo está bien cableado y vivo: `AT` (que conteste),
 * `AT+CWMODE=1` (ponerlo en modo estación) y `AT+CWLAP` (barrer las redes y enseñarlas). **El
 * resultado va a la pantalla**, porque el módulo ocupa la USART3 y mientras está puesto no hay cable
 * al PC: no se puede publicar telemetría.
 *
 * @note Botones: **OK** lanza la secuencia (o la repite) · **MENU** sale.
 * @note El veredicto es **`CONECTADO` o `SIN ESP`**, y sale cuando el módulo contesta al `AT`: es la
 *       pregunta que se viene a responder. Lo de después —ponerlo en modo estación y contar las
 *       redes— solo confirma que el firmware AT está entero.
 * @note Si sale `SIN ESP`, en la fila 2 va **lo que haya llegado**, aunque sea basura. Si pone `NADA`,
 *       no ha llegado **ni un byte**: casi siempre es **TX y RX sin cruzar**, o alimentación del
 *       módulo.
 * @warning Mientras dure la prueba, la telemetría está **apagada** a propósito: si se encendiera, el
 *          ESP recibiría los `>ETIQUETA:valor` como comandos y la consola leería sus respuestas.
 * @see Procedimiento y diagnóstico en `Documentacion/pruebas-bringup.md`.
 */
void App_Test_ESP8266(void)
{
    ESP_Init();
    ESP_SetLineaCallback(esp_test_linea);

    esp_test_estado = ESP_TEST_ESPERA;
    esp_test_redes  = 0U;

    app_pantalla_esp();

    while (!BUTTONS_SalidaSolicitada()) {
        uint8_t evento;

        BUTTONS_Update();
        evento = BUTTONS_GetEvent();

        if (evento == BTN_NUM_OK) {
            esp_test_estado = ESP_TEST_AT;
            esp_test_redes  = 0U;
            app_pantalla_esp();
        }

        esp_test_paso();

        HAL_Delay(1);
    }

    /* Al salir se tira lo que haya quedado en el anillo de recepción: si no, la consola intentaría
       ejecutar las respuestas del módulo como si fueran órdenes suyas. */
    ESP_Init();
}

/* ------------------------------------------------------------------
 * Prueba del puente WiFi con el PC.
 *
 * A diferencia de la anterior, aquí **sí** se publica telemetría: el módulo ya no está en modo de
 * comandos, está en **modo transparente**, y hace de cable. Es justo lo que se viene a comprobar.
 * ------------------------------------------------------------------ */

/** @brief Periodo del latido que se manda por el puente [ms]. */
#define PUENTE_LATIDO_MS    1000U

/** @brief En qué punto está el puente. */
typedef enum {
    PUENTE_ESPERA = 0,   /**< Esperando a que se pulse OK     */
    PUENTE_LEVANTANDO,   /**< Haciendo la secuencia           */
    PUENTE_ARRIBA,       /**< Puente levantado               */
    PUENTE_FALLO,        /**< Algún paso falló              */
    PUENTE_ESTADO_N
} PuenteEstado_t;

/** @brief Nombres de los estados, para la pantalla. */
static const char *const puente_nombres[PUENTE_ESTADO_N] = {
    "PULSA OK", "LEVANTANDO", "PUENTE", "FALLO"
};

/** @brief Nombres de los pasos de la secuencia, para saber en cuál se quedó. */
static const char *const puente_pasos[ESP_PASO_N] = {
    "-", "AT", "ATE0", "CWMODE", "WIFI", "CIPMUX", "CIPMODE", "SOCKET", "CIPSEND"
};

static PuenteEstado_t puente_estado = PUENTE_ESPERA;

/**
 * @brief Pinta el estado del puente.
 */
static void app_pantalla_puente(void)
{
    char texto[24];
    char limpio[24];

    OLED_Clear();

    if (puente_estado == PUENTE_ARRIBA) {
        OLED_PrintCentrado(0U, 2U, "PUENTE");
        oled_print_line(2U, "SIN CABLE");
        oled_print_line(3U, "MENU:SALIR");
    } else if (puente_estado == PUENTE_FALLO) {
        /* **En qué paso falló**, que es lo que de verdad hace falta saber: cada paso apunta a un sitio
           distinto —la contraseña de la red, la IP del PC, el firmware AT—. Y debajo, lo que contestó
           el módulo, que es el otro dato que se necesita. */
        snprintf(texto, sizeof(texto), "FALLO EN %s", puente_pasos[(uint8_t)ESP_GetPaso()]);
        oled_print_line(0U, texto);

        esp_test_a_pantalla(ESP_GetRespuesta(), limpio, sizeof(limpio));
        snprintf(texto, sizeof(texto), "%s", (limpio[0] != '\0') ? limpio : "NADA");
        oled_print_line(1U, texto);

        oled_print_line(3U, "OK:OTRA VEZ");
    } else {
        oled_print_line(0U, "WIFI AL PC");
        oled_print_line(1U, puente_nombres[puente_estado]);
        oled_print_line(3U, "OK:LEVANTAR");
    }

    OLED_Update();
}

/**
 * @brief Prueba del puente WiFi: el módulo pasa a ser el cable.
 *
 * Levanta el **modo transparente** del ESP8266: a partir de ahí la USART3 *es* el socket con el PC, así
 * que la telemetría y la consola funcionan **sin cambiar nada** en el resto del firmware. El robot sigue
 * accesible por WiFi aunque se salga de esta prueba.
 *
 * @note Botones: **OK** levanta el puente · **MENU** sale **sin cerrarlo**, a propósito: se sale al menú
 *       y el robot queda accesible por red para el resto de las pruebas. Se cerraría con
 *       `ESP_PuenteParar()` si hiciera falta.
 * @note Aquí **sí** hay telemetría, y un latido `>PUENTE_T` cada segundo: es lo que dice desde el PC
 *       que el puente está vivo y en qué segundo, sin tener que mirar la pantalla.
 * @note Se atiende la consola en el bucle: las órdenes llegan **por el puente**, y es la forma de
 *       comprobar los dos sentidos de una vez.
 * @see Las credenciales están en `include/wifi_secrets.h`, que **no se sube al repositorio**.
 */
void App_Test_Puente(void)
{
    uint32_t t_latido = HAL_GetTick();

    ESP_Init();
    ESP_SetLineaCallback(0);   /* el puente no parsea líneas: a partir de aquí es un cable */

    puente_estado = PUENTE_ESPERA;

    app_pantalla_puente();

    while (!BUTTONS_SalidaSolicitada()) {
        uint8_t evento;

        BUTTONS_Update();
        evento = BUTTONS_GetEvent();

        if ((evento == BTN_NUM_OK) && (puente_estado != PUENTE_LEVANTANDO)) {
            puente_estado = PUENTE_LEVANTANDO;
            app_pantalla_puente();   /* se pinta **antes** de empezar: la secuencia tarda segundos */

            puente_estado = (ESP_PuenteIniciar() == ESP_OK) ? PUENTE_ARRIBA : PUENTE_FALLO;
            t_latido      = HAL_GetTick();

            app_pantalla_puente();
        }

        CONSOLE_Update();

        if ((puente_estado == PUENTE_ARRIBA) && ((HAL_GetTick() - t_latido) >= PUENTE_LATIDO_MS)) {
            t_latido = HAL_GetTick();
            TELEM_SendInt("PUENTE_T", (int32_t)(HAL_GetTick() / 1000U));
        }

        HAL_Delay(1);
    }
}

/**
 * @brief Nombres de las paredes, en el orden de `Wall_t`, para la pantalla y la telemetría.
 *
 * Sin acentos y solo con los glifos de `app/oled.c` (no hay `H`, `J`, `Q`, `Y` ni `Z`).
 */
static const char *const walls_nombre[WALL_N] = { "FRENTE", "DERECHA", "IZQUIERDA" };

/**
 * @brief Diferencia de umbral que aplica cada pulsación de UP/DOWN en la prueba de paredes.
 *
 * Con el umbral de fábrica (200) este paso valía. Con los umbrales **medidos** (del orden de 1500) un
 * paso de 20 obligaba a **70 pulsaciones** para recorrer el margen útil, así que sube a 100.
 */
#define WALLS_PASO_UMBRAL   100

/** @brief Periodo de repintado de la pantalla de paredes, en ms. */
#define WALLS_PANTALLA_MS   250U

/**
 * @brief Paso del bucle de la prueba de paredes, en ms.
 *
 * **No es el ritmo de la pantalla** (ese es `WALLS_PANTALLA_MS`), es el de **mirar los botones**. Tiene
 * que ser bastante menor que el antirrebote (`BUTTONS_DEBOUNCE_MS`, 30 ms) o una pulsación corta cae
 * entre dos muestreos y el firmware no llega a verla: el botón «no hace nada».
 */
#define WALLS_PANTALLA_PASO_MS  10U

/**
 * @brief Pinta las tres paredes con su medida, su umbral y su veredicto.
 *
 * La pared **seleccionada** va marcada con `>`: es la que cambian UP y DOWN.
 *
 * @param seleccion Pared que están ajustando los botones
 */
static void app_pantalla_paredes(Wall_t seleccion)
{
    char texto[24];

    OLED_Clear();

    snprintf(texto, sizeof(texto), "PAREDES U %d", (int)WALLS_GetUmbral(seleccion));
    oled_print_line(0U, texto);

    for (uint8_t i = 0U; i < (uint8_t)WALL_N; i++) {
        snprintf(texto, sizeof(texto), "%c%s %d %d",
                 ((Wall_t)i == seleccion) ? '>' : ' ',
                 walls_nombre[i],
                 (int)WALLS_GetMedida((Wall_t)i),
                 (int)WALLS_Hay((Wall_t)i));
        oled_print_line((uint8_t)(1U + i), texto);
    }

    OLED_Update();
}

/**
 * @brief Publica por telemetría las medidas, los umbrales y los veredictos de las tres paredes.
 */
static void walls_test_publicar(void)
{
    for (uint8_t i = 0U; i < (uint8_t)WALL_N; i++) {
        char etiqueta[20];

        snprintf(etiqueta, sizeof(etiqueta), "WALL_%u", (unsigned)i);
        TELEM_SendInt(etiqueta, (int32_t)WALLS_GetMedida((Wall_t)i));

        snprintf(etiqueta, sizeof(etiqueta), "WALL_%u_UMB", (unsigned)i);
        TELEM_SendInt(etiqueta, (int32_t)WALLS_GetUmbral((Wall_t)i));

        snprintf(etiqueta, sizeof(etiqueta), "WALL_%u_HAY", (unsigned)i);
        TELEM_SendInt(etiqueta, (int32_t)WALLS_Hay((Wall_t)i));
    }
}

/**
 * @brief Prueba de la detección de paredes: ajusta los umbrales **sin cable**.
 *
 * Es la prueba que hay que hacer **en el laberinto**, no en casa: las paredes de un laberinto ajeno
 * pueden no reflejar como las de casa, y el umbral bueno de un sitio no vale en el otro. Por eso el
 * ajuste se hace con los botones y mirando la pantalla, sin portátil y sin Teleplot.
 *
 * @note Botones: **OK** cambia de pared · **UP** sube el umbral · **DOWN** lo baja · **MENU** sale.
 * @note La medida (`WALL_n`) es `encendido − apagado`, así que **no depende de la luz de la sala**.
 * @note Los umbrales **no se guardan en Flash**: al reiniciar vuelven a los de arranque, que son
 *       **uno por pared** (`WALLS_UMBRAL_*_DEF` en `app/walls.h`).
 * @warning El robot debe estar **quieto y en su sitio**: se está midiendo lo que ve desde ahí.
 * @see `Documentacion/pruebas-bringup.md`.
 */
void App_Test_Paredes(void)
{
    Wall_t   seleccion  = WALL_FRENTE;
    uint32_t t_pantalla = 0U;

    WALLS_Actualizar();
    app_pantalla_paredes(seleccion);

    while (!BUTTONS_SalidaSolicitada()) {
        uint8_t evento;

        WALLS_Actualizar();

        /* Los botones se miran **a 100 Hz**, no cada 250 ms. Con el antirrebote a 30 ms y un bucle
           lento, una pulsacion corta caia **entre dos muestreos y no existia**: el firmware no llegaba
           a ver el nivel pulsado, y esta es justo la prueba que se maneja a base de pulsaciones.
           El ritmo lento es cosa de la PANTALLA -- el `OLED_Update()` se lleva ~50 ms por I2C --, asi
           que se repinta cada WALLS_PANTALLA_MS y se sigue dando vueltas a 10 ms. */
        BUTTONS_Update();
        evento = BUTTONS_GetEvent();

        if (evento == BTN_NUM_OK) {
            seleccion = (Wall_t)(((uint8_t)seleccion + 1U) % (uint8_t)WALL_N);
        } else if (evento == BTN_NUM_UP) {
            WALLS_SetUmbral(seleccion, (int16_t)(WALLS_GetUmbral(seleccion) + WALLS_PASO_UMBRAL));
        } else if (evento == BTN_NUM_DOWN) {
            WALLS_SetUmbral(seleccion, (int16_t)(WALLS_GetUmbral(seleccion) - WALLS_PASO_UMBRAL));
        }

        if (evento != BTN_NUM_NINGUNO) {
            walls_test_publicar();

            /* Al tocar un boton se repinta en el acto: un ajuste que no se ve, no se ha hecho. */
            app_pantalla_paredes(seleccion);
            t_pantalla = HAL_GetTick();
        } else if ((HAL_GetTick() - t_pantalla) >= WALLS_PANTALLA_MS) {
            app_pantalla_paredes(seleccion);
            t_pantalla = HAL_GetTick();
        }

        HAL_Delay(WALLS_PANTALLA_PASO_MS);
    }
}

/**
 * @brief Consulta y fija el estado de la máquina de estados de la aplicación.
 */
AppState_t APP_GetState(void) { return app_state; }
void APP_SetState(AppState_t state) { app_state = state; }
