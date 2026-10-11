# Rabosa-MVR — Instrucciones del repositorio

Robot **micromouse** Rabosa-MVR. Firmware en **C puro** sobre **STM32F103C8T6** (Blue Pill),
**PlatformIO** con `framework = stm32cube`, **72 MHz**. Documentación y comentarios en **español**.

## Arquitectura: regla de capas

La frontera que **no se cruza nunca** es la de la **lógica de misión**: `core/` (junto con
`control/` y `filter/`) **no puede tocar `drivers/`, `sensor/`, `hal/` ni `hw/`**. Habla con el
mundo por el contrato `core/core_io.h`, y quien lo implementa es `drivers/`.

```
app/         interfaz y banco   ──► SÍ puede tocar hal/ y sensor/ (es su trabajo)
  │
core/        lógica de misión   ──► NO toca nada por debajo: solo core_io.h
  │            (core_io.h)
  ▼
drivers/     dispositivos        motores, encoders, paredes, reloj
  │
hal/         periféricos del MCU adc, i2c, pwm, spi, timers, uart…
  │
hw/          registros y pines
```

⚠️ **Dos cosas distintas dentro de lo que antes era APP**, y esta distinción es la que hay que
tener clara:

- **`app/` es interfaz y banco** (menú, telemetría, consola, OLED, botones y las pruebas de
  banco). **Puede tocar `hal/` y `sensor/` a propósito** — arrancar los periféricos, encender
  el LED, leer el reloj—, porque ese es justo su trabajo. Lo que no puede es tener lógica de
  misión.
- **`core/` es lógica de misión** (perfiles, movimiento, mapa, navegación). Ahí **cero
  símbolos de la HAL**, sin excepciones.

- **APP** `firmware/app/` — **interfaz y banco**: menú, telemetría, consola, OLED, botones y las
  pruebas de banco
- **CORE** `firmware/core/` — **lógica de misión portable**: perfiles, movimiento, mapa,
  navegación. **Cero símbolos de la HAL**, y tiene que compilar en el PC:
  `gcc -c -Wall -Wextra -I firmware firmware/core/*.c firmware/control/*.c firmware/filter/*.c`
- **CONTROL** `firmware/control/` — consignas y PID
- **FILTER** `firmware/filter/` — estimación, filtrado, odometría
- **DRIVERS** `firmware/drivers/` — dispositivos: motores, encoders, paredes, reloj
- **SENSOR** `firmware/sensor/` — medida cruda (imu.c, encoder.c)
- **HAL** `firmware/hal/` — periféricos del MCU (adc, i2c, pwm, spi, timers, uart…)
- **HW** `firmware/hw/` — registros y pines

Si un cambio toca dos capas, **divídelo en dos cambios**. **Nada de `HAL_*` en `core/`,
`control/` ni `filter/`**: medido el 2026-10-10, esas tres capas tienen **0 símbolos de la HAL** y
hay que mantenerlo así (ver `Documentacion/auditoria-capas.md`).

## Includes y build

- Directorios: `-I.` (raíz `firmware/`) e `-Iinclude`.
- Cabeceras con la capa como prefijo: `"hal/uart.h"`, `"sensor/imu.h"`, `"app/telemetry.h"`, `"peripherals.h"`, `"pinout.h"`.
- Las fuentes se añaden con `env.BuildSources()` en `firmware/extra_script.py` (no `build_src_filter`).
  Una carpeta nueva debe estar en la lista `["app", "control", "filter", "sensor", "hal", "hw"]`.
- `pio` **no está en el PATH** del terminal por defecto: vive en `~/.platformio/penv/bin/pio`.

## Telemetría

Nunca `printf`/UART directo fuera de la capa HAL, de `app/telemetry.c` (salida) y de `app/console.c`
(entrada).
Salida por **USART3** (PB10/PB11, 115200) en formato **Teleplot**: `>ETIQUETA:valor`.
Usa las funciones `TELEM_Send*()`. `%f` depende de `-Wl,-u,_printf_float` en `platformio.ini`.

⚠️ **Todo valor es NUMÉRICO** (decisión del usuario, 30-sep): nada de `>BOOT:init`, sino `>BOOT:1`.
Los estados y los veredictos van como **código entero** (`>APP_EST:2`, `>ADC_EST0:3`) y su tabla está
en la documentación del módulo; los mensajes de ayuda no se mandan por telemetría (viven en
`Documentacion/`). Para depurar se usa la **traza de las FSM** (`>APP_EST`, `>MENU_EST`).

La **entrada** (órdenes del usuario, p. ej. `BAT CRIT_PACK 6000`) llega por el mismo puerto: la recibe
por interrupción `hal/uart.c` y la interpreta `app/console.c` (ver `Documentacion/comandos-serie.md`).

## Flujo de trabajo obligatorio

1. **Documentar antes de codificar**: hardware nuevo → `Documentacion/hardware.md`;
   prueba nueva → `Documentacion/pruebas-bringup.md`; módulo nuevo → la tabla de capas.
2. **Bring-up aislado**: cada periférico se valida con su `App_Test_*` en `app/application.c`
   (se elige en `APP_Run()`) antes de entrar en el lazo de control.
3. **Observabilidad**: toda prueba imprime telemetría o cambia un LED.
4. **Actualizar estado** en `Documentacion/estado-del-proyecto.md`.

Actualiza la documentación **directamente, sin pedir confirmación**, y resume al final qué cambió.

## Gotchas verificados

- **IMU por SPI2**: `SDA/SDI` (entrada) → **MOSI PB15**; `ADO/SDO` (salida) → **MISO PB14**.
  Si se cruzan, `WHO_AM_I` = `0xFF`. Config validada: modo 0, prescaler /64.
- `gpio.c` precarga PB12–PB15 como AF; `MX_SPI2_Init()` reconfigura **PB12 como GPIO de salida (CS)**.
- STM32F1 HAL: MISO se configura como `GPIO_MODE_INPUT` (F1 no tiene modo "AF input").
- Encoders en TIM2 (PA0/PA1) y TIM3 (PA6/PA7), modo TI12 (×4). Contador de 16 bits:
  usar `max_likelihood_counter_diff()` para dirección y *overflow*.
- Los `App_Test_*` se ejecutan en un `while(1)`: el código que va después en `APP_Run()` no corre.

## Terminología

Se usan **tecnicismos en inglés** (*duty*, *setpoint*, *bring-up*, *wall follower*, *anti-windup*,
*framebuffer*…): son los términos estándar del sector y **no se traducen**. La correspondencia con el
español y cómo se usa cada uno en este robot están en **`Documentacion/glosario.md`**.

- El **código** (ficheros, funciones, variables) va en **inglés** con prefijo de módulo
  (`CTRL_Update`, `TELEM_SendFloat`, `App_Test_PID`).
- Los **comentarios, la documentación y las respuestas** van en **español**.
- Las etiquetas de la **telemetría** van en MAYÚSCULAS y **sin acentos** (`>PID_SET`, `>VBAT`).
- Las etiquetas de la **OLED** van sin acentos y **solo con los glifos de la fuente** (`app/oled.h`).

## Estilo de código

- **Las máquinas de estados se escriben con el patrón de `app/fsm.h`**: un `fsm { }` por función,
  `fsmstate()`, `fsmgoto()` y `fsmexit`, con `START` como despachador (estas FSMs se reanudan de
  llamada en llamada). Cada transición publica su estado con una macro local (`APP_GOTO`, `MENU_GOTO`).
- **Un comentario por función, al principio**, y en formato **Doxygen** (`/** ... */`) con `@brief`,
  `@param`, `@return` y `@note`/`@warning` cuando aporten, para que funcionen las herramientas de
  documentación. Las funciones internas (`static`) se documentan igual.
- **Nada de comentarios línea a línea**: el nombre de la función y de las variables explica el *qué*;
  el comentario explica el ***porqué*** (razón compleja, decisión de diseño, limitación, unidades, o
  un valor que viene de un datasheet). Si un comentario describe lo que ya dice el código, se borra.
- **Una función, una tarea.** Ejemplo: el PID solo calcula la señal de control; aplicarla al motor es
  cosa de quien lo llama. Si una función hace dos cosas, se parte en dos con nombres descriptivos.
- Única excepción a lo anterior: las **tablas de datos** pueden llevar una anotación por fila (por
  ejemplo, los glifos de `app/oled.c`).

## Documentación

- Índice: `Documentacion/README.md`
- Skill del agente con el flujo completo: `.github/skills/micromouse-dev/`

## Referencias externas

El proyecto se inspira en **UKMARSBOT** (`micromouseonline/ukmarsbot`), y evoluciona hacia
**ZoroBot** (OPRobots) y **Green Giant 5.19V** (Green Ye).
