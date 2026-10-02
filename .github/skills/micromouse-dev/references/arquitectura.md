# Arquitectura y contrato de capas — Rabosa-MVR

> Doc canónica y más extensa: [`Documentacion/Layered Architecture.md`](../../../../Documentacion/Layered%20Architecture.md)
> Fichero de build: `firmware/platformio.ini` · `firmware/extra_script.py`

## 1. Contrato de capas

**Regla única:** cada capa solo llama a la inmediatamente inferior. Nunca al revés, nunca saltando una capa.

| Capa | Responsabilidad | Directorio | Puede llamar a | Nunca debe |
|---|---|---|---|---|
| **APP** | Misión, secuencias, presentación, telemetría | `firmware/app/` | CONTROL, (FILTER/SENSOR vía CONTROL) | tocar HAL/HW directamente |
| **CONTROL** | Consignas, perfiles, lazos PID | `firmware/control/` | FILTER, SENSOR | formatear texto, decidir misión |
| **FILTER** | Estimación de estado, filtrado, fusión | `firmware/filter/` | SENSOR | actuar sobre motores |
| **SENSOR** | Medida cruda y calibrada del mundo | `firmware/sensor/` | HAL | contener lógica de control |
| **HAL** | Periféricos del MCU | `firmware/hal/` | HW, HAL de STM32 | conocer el dominio "robot" |
| **HW** | Registros y pines | `firmware/hw/` | — | implementar protocolos |

**¿Dónde va mi cambio?** Si toca dos capas, son dos cambios. Si una capa superior necesita datos de dos capas, se pasa por la intermedia.

## 2. Mapa de ficheros

```
firmware/
├── platformio.ini          Configuración (mcu, f_cpu, flags, upload, monitor)
├── extra_script.py         Añade las .c de cada capa al build
├── include/
│   ├── peripherals.h       HAL STM32 + includes globales
│   └── pinout.h            Pines del robot (LED, máscara PWM...)
├── app/                    application.c/h, menu.c/h, fsm.h, telemetry.c/h, console.c/h, buttons.c/h, oled.c/h (✅ en I2C1)
├── control/                controller.c/h, pid.c/h, setpoint.c/h (✅ PID de velocidad, 28-sep)
├── filter/                 odometry.c/h (✅ pose por incrementos de arco, 28-sep); filter/adc_filter/iekf: por escribir
├── sensor/                 encoder.c/h, imu.c/h, battery.c/h, sensor.c/h, encoders.md
├── hal/                    adc, drv8833, gpio, i2c, pwm, spi, systemclock, timers, uart
├── hw/                     io.c/h, io_def.h
├── src/main.c              Punto de entrada: delega en APP_InitAll()/APP_Run()
└── test/                   (reservado)
```

## 3. Convenciones de include y build

- Directorios de include: `-I.` (raíz `firmware/`) e `-Iinclude`.
- **Las cabeceras se incluyen con la capa como prefijo**: `"hal/uart.h"`, `"sensor/imu.h"`, `"app/telemetry.h"`, `"peripherals.h"`, `"pinout.h"`.
- `framework = stm32cube` **no respeta** `build_src_filter` → las fuentes se añaden con `env.BuildSources()` en `extra_script.py`. Para una carpeta nueva, añádela a la lista `["app", "control", "filter", "sensor", "hal", "hw"]`.
- `board_build.f_cpu = 72000000L`; MCU `stm32f103c8t6`.
- `-Wl,-u,_printf_float` va **como una sola opción** para que PlatformIO no la tome por una librería.
- `printf` con `%f` está habilitado (newlib-nano no lo trae por defecto).

Comandos:

```bash
cd firmware
platformio run            # compilar
platformio run -t upload  # flashear por ST-Link
platformio device monitor -b 115200
```

> En el entorno del agente, el CLI de PlatformIO **no está en el PATH**: vive en `~/.platformio/penv/bin/pio`.
> El agente **sí compila** (y debe hacerlo para validar cada cambio): `~/.platformio/penv/bin/pio run -d firmware`
> desde la raíz del repo. Lo que no puede es **flashear** (no hay ST-Link conectado al agente).

### Estilo de código

- **Un comentario por función, al principio**, en formato **Doxygen** (`/** ... */`); también en las
  funciones internas (`static`).
- **Sin comentarios línea a línea**: los nombres explican el *qué*; el comentario, el ***porqué***
  (decisión de diseño, límite, unidades o dato de datasheet).
- **Una función, una tarea**: el PID calcula la señal de control; aplicarla al motor es de quien llama.
- Excepción: las tablas de datos admiten una anotación por fila (glifos de la OLED, mapas de pines).
- Todo el firmware cumple ya este estilo, **incluidas las 17 pruebas de `app/application.c`** (repaso
  de 28-sep): al tocar cualquier función, mantener el bloque Doxygen y no meter comentarios dentro.

### Terminología

Los tecnicismos van en **inglés** (*duty*, *setpoint*, *bring-up*, *wall follower*, *anti-windup*…) y
**no se traducen**: son los términos estándar y así cuadran con datasheets, librerías y foros.
La correspondencia con el español y las convenciones de escritura (código en inglés, comentarios y
documentación en español, etiquetas sin acentos) están en
[`Documentacion/glosario.md`](../../../../Documentacion/glosario.md).

## 4. Interfaces entre capas

### SENSOR → FILTER

```c
/* encoder.c: medida cruda + calibrada */
void     ENC_Init(void);
void     set_micrometers_per_count(float value);
void     update_encoder_readings(void);          /* llamar a 1 kHz */
float    get_encoder_left_speed(void);           /* m/s */
int32_t  get_encoder_average_micrometers(void);  /* µm */

/* imu.c: MPU6500 por SPI2 */
uint8_t  IMU_Init(void);
uint8_t  IMU_Read(float *ax, float *ay, float *az,
                  float *gx, float *gy, float *gz);
```

### FILTER → APP (la odometría no habla con CONTROL)

```c
/* odometry.h: matemática pura; recibe incrementos de arco, NO lee encoders */
typedef struct { float x, y, theta, distance; } Odometry_t;

void  ODOM_Init(Odometry_t *odo, float x0_m, float y0_m, float theta0_rad);
void  ODOM_Reset(Odometry_t *odo);
void  ODOM_Update(Odometry_t *odo, float ds_left_m, float ds_right_m);
void  ODOM_SetTrocha(float trocha_mm);   /* trocha efectiva, a medir */
float ODOM_GetTrocha(void);
```

> La APP es quien lee `get_encoder_left/right_micrometers()` y le pasa las diferencias, igual que
> hace con el PID. Así el módulo se compila en el PC (`firmware/test/odometry_sim.c`).
>
> ⚠️ **Excepción de capas declarada**: aquí la APP llama a FILTER sin pasar por CONTROL, igual que las
> pruebas de banco llaman a SENSOR/HAL. Es la misma excepción de bring-up documentada en
> `Documentacion/maquina-estados-menu.md` §1. Cuando existan los movimientos primitivos, quien
> consumirá la pose para el lazo lateral será CONTROL y la APP pedirá los movimientos ya resueltos.

### CONTROL → HAL (actuación)

```c
void DRV8833_Set_Speed(DRV8833_Motor_t *m, int32_t speed);  /* -100..100 */
void DRV8833_Brake(DRV8833_Motor_t *m);
void DRV8833_Coast(DRV8833_Motor_t *m);
```

### CONTROL → APP (API real, 28-sep)

```c
/* control/controller.h — el lazo NO lee encoders ni escribe PWM: entra la
 * velocidad medida (mm/s) y sale el duty (%). El cableado lo hace la APP. */
void  CTRL_Init(Controller_t *c, float kp, float ki, float kd, float duty_max);
void  CTRL_SetGanancias(Controller_t *c, float kp, float ki, float kd);
void  CTRL_SetRampa(Controller_t *c, float mm_s2);
void  CTRL_SetConsigna(Controller_t *c, float mm_s);         /* con rampa   */
void  CTRL_SetConsignaEscalon(Controller_t *c, float mm_s);  /* sintonía    */
void  CTRL_Parar(Controller_t *c);
float CTRL_Update(Controller_t *c, float medida_mm_s, float dt);  /* → duty % */
float CTRL_GetConsigna(const Controller_t *c);
float CTRL_GetMedida(const Controller_t *c);
float CTRL_GetDuty(const Controller_t *c);
```

> Unidades: **mm/s** y **%**. Ganancias en %/(mm/s)… Ver `Documentacion/control-pid.md`.

### APP → medio físico

```c
void TELEM_SendString(const char *label, const char *value);
void TELEM_SendFloat(const char *label, float value);
void TELEM_SendInt(const char *label, int32_t value);
void TELEM_SendIRValues(const uint16_t *ir, uint8_t count);
void TELEM_SendIMU(float ax, float ay, float az, float gx, float gy, float gz);
void TELEM_SendMotorEncoder(int16_t enc1, int16_t speed1, int16_t enc2, int16_t speed2);
```

⚠️ **Desde el 30-sep toda la telemetría es numérica** (`>ETIQUETA:numero`): estados y veredictos van
como **código entero** (`>APP_EST:1`, `>MENU_EST:2`, `>ADC_EST0:3`) con su tabla en la documentación,
y los mensajes de ayuda no se mandan. `TELEM_SendString()` queda como utilidad sin usar.

### Máquinas de estados (30-sep)

Patrón de `app/fsm.h` (Remo Dentato): cada estado es un bloque con etiqueta y cada transición un
`goto`. Un `fsm { }` por función, un solo `START`, nada de código entre estados.

```c
fsm {
    fsmstate(START) { /* despacho: guardas globales + salto al estado guardado */ }
    fsmstate(LISTA) { if (nada) fsmexit; fsmgoto(OTRO); }
}
```

⚠️ Estas FSMs **se reanudan de llamada en llamada**, así que `START` es un **despachador**
(`switch` al estado guardado). Cada transición publica su estado con una macro local:
`#define MENU_GOTO(s) do { menu_publicar_estado(MENU_EST_##s); fsmgoto(s); } while (0)`.

### APP ← medio físico (consola, 30-sep)

```c
void CONSOLE_Init(void);          /* arranca la recepción por IT de la UART */
void CONSOLE_Update(void);        /* en el bucle de 1 kHz: ejecuta las órdenes que lleguen */
```

Órdenes documentadas en `Documentacion/comandos-serie.md` (`HELP`, `BAT LIST`, `BAT <UMBRAL> <valor>`,
`PID KP|KI|KD <valor>`, `PID VEL <mm/s>`). Las de ajuste en caliente solo se atienden si la prueba en
curso llama a `CONSOLE_Update()` en su bucle.
La recepción la hace HAL (`UART_RxStart()`, `UART_RxGet()`); la consola solo monta líneas y despacha.
Los valores que se cambian en caliente necesitan **su setter con validación en la capa dueña**
(`BATTERY_SetUmbral()` en SENSOR), nunca validación en la consola.

⚠️ **La recepción NO usa `HAL_UART_Receive_IT()`**: en `stm32f1xx_hal_uart.c`, `UART_Receive_IT()` sale
**sin leer el registro de datos** si `RxState != HAL_UART_STATE_BUSY_RX` ⇒ con `RXNEIE` activo el flag
`RXNE` se queda puesto y la interrupción se repite **sin fin**: el bucle principal no avanza (menú
congelado, sin telemetría). La ISR propia (`UART_RxIRQHandler()`, llamada desde `USART3_IRQHandler`)
lee siempre `SR` y `DR` y solo se arma `UART_IT_RXNE` (el overrun se limpia al leer el dato).
**Si algún día alguien ve «la pantalla se queda como está y no responde ningún botón», que mire aquí.**

## 5. Estado real vs. documentado

⚠️ `Documentacion/Layered Architecture.md` es un **plano aspiracional** con restos de otra plataforma. No lo tomes como verdad para el hardware actual:

| Dice la doc | Realidad en el repo |
|---|---|
| `system_stm32l4xx.c` | STM32**F103**C8T6 (Blue Pill) |
| IMU MPU6050 por **I2C** | MPU6500/9250 por **SPI2** |
| `radio.c`, `monitor.c` | No existen (`hal/uart.c` cubre la USART3) |
| `encoder.c (MPU6050)` | Typo: `encoder.c` son encoders de rueda |
| Capas CONTROL/FILTER llenas | **CONTROL ✅** (PID + rampas, 28-sep) · **FILTER**: ✅ odometría (28-sep), faltan filtros y estimador de orientación |

Mantén esta tabla actualizada cuando se integren módulos.

## 6. Gotchas conocidos de la plataforma

- **STM32F1 HAL**: para MISO usa `GPIO_MODE_INPUT` (F1 no tiene modo "AF input").
- `gpio.c` precarga PB12–PB15 como AF; `MX_SPI2_Init()` reconfigura **PB12 como salida GPIO** para el CS.
- No declares el mismo pin en dos módulos sin anotarlo en `hardware.md` (causa habitual de fallos intermitentes).
