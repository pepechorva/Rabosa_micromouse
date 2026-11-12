# 🛠️ Entorno de desarrollo — Rabosa-MVR

Cómo se compila, se carga y se depura el firmware, y qué convenciones de código se siguen.

---

## 1. Cadena de herramientas

| Herramienta | Uso | Notas |
|:------------|:----|:------|
| **VS Code + extensión PlatformIO** | Desarrollo y compilación | Flujo recomendado |
| **PlatformIO CLI** | Compilación desde terminal | No está en el `PATH`; vive en `~/.platformio/penv/bin/pio` |
| **ST-Link v2** | Flasheo por SWD | Dispositivo en `/dev/stlinkv2_2` |
| **Monitor serie / Teleplot** | Telemetría | 115200 baudios en `/dev/ttyUSB0` |
| **STM32CubeMX** | Opcional, para regenerar HAL | Cuidado: regenerar puede pisar código propio |

---

## 2. Comandos

```bash
cd firmware

platformio run                # compilar
platformio run -t upload      # compilar y flashear por ST-Link
platformio device monitor -b 115200
platformio run -t clean       # limpiar artefactos
```

---

## 3. Configuración de compilación

Fichero: `firmware/platformio.ini`

| Clave | Valor | Por qué |
|:------|:------|:--------|
| `board` | `bluepill_f103c8` | Placa base |
| `board_build.mcu` | `stm32f103c8t6` | MCU real |
| `board_build.f_cpu` | `72000000L` | Reloj a 72 MHz |
| `framework` | `stm32cube` | HAL de STM32 |
| `build_flags` | `-D STM32F1 -I. -Iinclude` | Include de todas las capas |
| `-Wl,-u,_printf_float` | — | Habilita `%f` en `printf` |
| ~~`-flto`~~ | **NO USAR** | ⚠️ Libera 5,4 KB pero **rompe las interrupciones** (ver §3.1) |
| `extra_scripts` | `pre:extra_script.py` | Añade las capas al build |
| `upload_protocol` | `stlink` | Programador |
| `monitor_speed` | `115200` | Baudios de la telemetría |

### 3.1 Optimización y tamaño

⚠️ **En `platformio.ini` no se declara ningún `-O`**: PlatformIO ya aplica **`-Os`** por su cuenta,
con `-ffunction-sections` y `-fdata-sections`, más el `--gc-sections` del enlazador. **Eso es lo
correcto en este chip y no hay que «mejorarlo».**

#### Cuánta Flash hay de verdad: **64 KB**, medido (2026-10-10)

⚠️ **El *environment* se llama `[env:bluepill_f103c8_128k]`, pero ni el `board` ni el chip son de
128 KB.** El `board` es `bluepill_f103c8` (64 KB) y el chip **también es de 64 KB**, comprobado
leyendo la **firma del chip**:

```
$ openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
    -c "init" -c "flash probe 0" -c "mdh 0x1FFFF7E0 2" -c "shutdown"
Info : device id = 0x20036410
Info : flash size = 64 KiB
0x1ffff7e0: 0040 ffff      <- 0x0040 = 64
```

- El registro `0x1FFFF7E0` contiene el **tamano de flash en KB** (`0x1FFFF7E8` es el ID unico de 96 bits).
- ⚠️ **El `chipid` NO sirve para esto**: vale `0x410`, que es «F1 *medium density*» y cubre de 32 a
  128 KB. Hay que leer `F_SIZE`.
- **Consecuencia practica**: el porcentaje que informa PlatformIO y **el presupuesto de este
  documento son los reales**. Con 64 KB y ~52,9 KB usados quedan **~12,6 KB libres** ⇒ **no hay
  holgura**, y el «cuando haga falta espacio se recortan las pruebas» del historial **no es una
  precaucion teorica**.
- ⇒ Si algun dia se pasa a un chip de 128 KB, ademas de la placa hay que cambiar el **`board`**
  (`bluepill_f103c8_128k`), porque **el linker script sale de ahi**: el de 64 KB es
  `STM32F103C8TX_FLASH.ld` (`MEMORY FLASH = 64K`) y el de 128 KB, `STM32F103CBTX_FLASH.ld`. Tener el
  chip de 128 KB con el `board` de 64 **desperdicia la mitad de la memoria sin avisar**.
- 📌 **Pendiente**: renombrar el *environment* a `bluepill_f103c8` para que no mienta (implica
  **recompilar limpio**, porque cambia el directorio de `build`).

Medido el 2026-10-02 sobre el firmware real (65 536 B de Flash):

| Configuración | Flash | Veredicto |
|:--------------|------:|:----------|
| **`-Os` (actual)** | **48 644 B (74,2 %)** | ✅ lo que hay |
| ~~`-Os` sin `_printf_float`~~ | ~~34 676 B (52,9 %)~~ | ✅ **HECHO el 2026-10-10**: ver abajo |
| `-Os` + `-flto` | 43 272 B (66,0 %) | ❌ **destruye las interrupciones** |
| **`-O3`** | **59 272 B (90,4 %)** | ❌ engorda un **37 %** |

> **`-O3` es una trampa en un microcontrolador sin caché.** Agresiva el *inlining*, desenrolla bucles
y vectoriza, así que **el código crece** —y ese código de más se paga en saltos de línea de Flash,
que en un M3 sin caché cuestan tiempo de ejecución—. Además aquí **no compra nada**: los bucles
calientes (el PID a 1 kHz) son diminutos. Medido, no supuesto.

### ⚠️ `-flto` está prohibido en este proyecto

Baja la Flash **5 372 B** y compila **sin un solo aviso**, así que parece gratis. **No lo es: roba
el subsistema de interrupciones entero, en silencio.**

Los manejadores que define `src/main.c` (`SysTick_Handler`, `USART3_IRQHandler`,
`EXTI15_10_IRQHandler`, `HardFault_Handler`…) son **overrides de funciones `__weak` de la HAL**, y
esas `__weak` vienen **dentro del `.a` del framework** (`framework-stm32cubef1`). El LTO resuelve el
conflicto **al revés** y se queda con la versión vacía, así que **el vector table acaba apuntando
todos los manejadores a `Default_Handler`**:

| Símbolo | Sin `-flto` | Con `-flto` |
|:--------|:------------|:------------|
| `SysTick_Handler` | `T 08007aa0` | `W 08008818` ← el stub |
| `EXTI15_10_IRQHandler` | `T 08007aa4` | `W 08008818` |
| `USART3_IRQHandler` | `T 08007aa8` | `W 08008818` |
| `HardFault_Handler` | `T 08007aac` | `W 08008818` |

> **El síntoma en el robot** es que **se queda mudo en el arranque**: sin `SysTick_Handler` no hay
> `HAL_IncTick()` y **`HAL_Delay()` se cuelga para siempre** —ni siquiera termina los tres destellos
del LED—; sin `EXTI15_10_IRQHandler` no hay botón MENU y sin `USART3_IRQHandler` no hay consola.

> ✅ **Cómo se comprueba sin flashear, y hay que hacerlo SIEMPRE antes de dar por bueno un cambio de
> enlazado:**
> ```
> arm-none-eabi-nm firmware.elf | grep -E "_IRQHandler|_Handler"
> ```
> Los que el proyecto define tienen que salir **`T`** y **cada uno en su dirección**. Si salen **`W`**
> y **todos compartiendo la misma dirección**, están resolviéndose al stub y no existen.
>
> (Si algún día se quiere recuperar ese 5,4 KB: la vía sería forzar los símbolos con
> `-Wl,-u,EXTI15_10_IRQHandler` y compañía, que es el mismo truco de `_printf_float`. **No se ha
> probado**, y habría que repasar **todos** los overrides débiles, no solo los tres que se ven aquí.)

#### ✅ Hecho el 2026-10-10: fuera el `%f`, y con el **toda** la doble precision

El refactor ya esta hecho, y salio **mejor de lo previsto** porque el `%f` no venia solo: los
argumentos de `printf` se **promocionan a `double`**, asi que enlazarlo obligaba a traer tambien
toda la aritmetica de doble precision.

| Bloque | Antes | Despues |
|:--|--:|--:|
| `_dtoa_r` y el formateo | 5 076 B | **2 156 B** |
| Coma flotante de **doble** precision (26 simbolos) | 6 108 B | **0 B** |
| Trigonometria (odometria) | 3 344 B | 3 344 B |
| Floats de biblioteca | 4 224 B | 4 054 B |
| **Flash total** | **53 196 B (81,2 %)** | **43 716 B (66,7 %)** |
| **Libre** | **12 340 B** | **21 820 B** |

Medido con `arm-none-eabi-nm --print-size` y el flag `-Wl,-u,_printf_float` retirado de
`platformio.ini`.

**Como se hizo**: modulo nuevo `app/format.c` con `FORMAT_Float()`, que escala el valor, redondea y
separa parte entera y fraccion para escribirlas con `%lu` y `%0*lu` — es decir, **con formateo de
enteros, que ya estaba enlazado**. Luego se sustituyeron los **23 usos**: 2 en `app/telemetry.c` y
21 en las pantallas de `app/application.c`. En la telemetria **el texto que sale es el mismo**, asi
que Teleplot sigue leyendo numeros; en la OLED se conserva la alineacion usando `%5s` en vez de
`%5.2f`.

⚠️ **Si algun dia se vuelve a usar un `%f`**: hay que reponer `-Wl,-u,_printf_float`, porque **sin
el flag no falla la compilacion, simplemente los numeros salen mal**. Esta anotado junto a la linea
retirada, en `platformio.ini`.

📌 **Y lo que esto cambia**: el «presupuesto de Flash» deja de ser el cuello de botella. De 12,3 KB
libres se pasa a **21,8 KB**, que es lo que hace viable portar la capa de aplicacion (perfiles,
mapa y `flood fill`) sin tocar el banco de pruebas.

✅ **Y validado en el robot el mismo dia** (por el puente WiFi, con la orden `HELP`):

```
>VBAT:8.482      >VCELL1:4.153   >VCELL2:4.329
>VBAT_DESEQ:-0.176          <- el signo negativo
>BAT_K:0.302
>PID_KP:0.100  >PID_KI:0.500  >PID_KD:0.000   <- el cero CON decimales
>PID_VEL:300.000  >LED:0
```

El riesgo real de este cambio era que **newlib-nano no soportase el ancho por argumento**
(`%0*lu`, que es lo que pone los ceros a la izquierda de la fraccion): **queda descartado**, las
tres cifras salen. Y los enteros (`>LED:0`, `>PACK_PRESENTE:1`) siguen intactos.

### `extra_script.py`

`framework = stm32cube` **no respeta** `build_src_filter`, así que las fuentes de cada capa
se añaden con `env.BuildSources()`:

```python
add_layer_sources(env, ["core", "app", "control", "filter", "sensor", "hal", "hw"])
```

> Para una carpeta nueva, añádela a esa lista (y crea la carpeta, aunque esté vacía,
> o el script la saltará con `[SKIP]`).
>
> 📌 **`drivers/` todavía NO está en la lista, a propósito**: la capa existe (con su README) pero
> aún no tiene fuentes. Se añadirá cuando `core/motion` empiece a usar el contrato de
> `core/core_io.h` y haya algo que compilar.

---

## 4. Convenciones de código

### Includes

- Directorios de búsqueda: `-I.` (raíz `firmware/`) e `-Iinclude`.
- Las cabeceras se incluyen **con la capa como prefijo**:

```c
#include "peripherals.h"     /* include/ */
#include "pinout.h"
#include "hal/uart.h"
#include "hal/drv8833.h"
#include "sensor/encoder.h"
#include "app/telemetry.h"
```

### Estructura de un módulo

- `xxx.c` + `xxx.h` en la carpeta de su capa.
- Cabeceras con guardas `#ifndef XXX_H`.
- Documentación **Doxygen** al principio de cada función (ver «Comentarios y funciones», §4). Nada de
  banners `/* ===== */` dentro de un módulo: separar en funciones con nombre descriptivo.

### Reglas de dependencia

1. **La lógica de misión no toca el hardware.** `core/` (con `control/` y `filter/`) no incluye
   nada de `drivers/`, `sensor/`, `hal/` ni `hw/`: habla por el contrato
   [`core/core_io.h`](../firmware/core/core_io.h). Y es **comprobable en cada cambio**:
   `gcc -c -Wall -Wextra -I firmware firmware/core/*.c firmware/control/*.c firmware/filter/*.c`
   tiene que compilar **en el PC**, sin el STM32 y sin un aviso.
2. **`app/` es la excepción deliberada.** Es interfaz y banco (menú, botones, OLED, telemetría y
   las pruebas), así que **sí puede** tocar `hal/` y `sensor/`: `MX_*_Init()` en el arranque,
   `HAL_GetTick()`/`HAL_Delay()` para los tiempos, `HAL_GPIO_*` para el LED y los emisores. Lo
   que **no** puede es tener lógica de misión. Medido el 2026-10-10: `core`+`control`+`filter` =
   **0 símbolos de la HAL**; `app/`, 14 (ver [auditoria-capas.md](./auditoria-capas.md)).
3. `main.c` es mínimo: `APP_InitAll()` y `APP_Run()`.
4. Nada de `printf` en capas bajas: la salida va por `app/telemetry.c` y la entrada (órdenes del
   usuario) por `app/console.c`. Nadie más toca la UART.

---

### Comentarios y funciones

- **Un comentario por función, al principio**, y en formato **Doxygen** (`/** ... */` con `@brief`,
  `@param`, `@return`), para que funcionen las herramientas de documentación. Las funciones internas
  (`static`) se documentan igual.
- **Nada de comentarios línea a línea**: el nombre de la función y de las variables explican el *qué*;
  el comentario explica el ***porqué*** (razón compleja, decisión de diseño, limitación, unidades o un
  valor que viene de un datasheet). Si el comentario repite lo que ya dice el código, se borra.
- **Una función, una tarea**: el PID solo calcula la señal de control; aplicarla al motor es cosa de
  quien lo llama. Si una función hace dos cosas, se parte en dos con nombres descriptivos.
- Excepción: las **tablas de datos** admiten una anotación por fila (por ejemplo, los glifos de
  `app/oled.c`).

## 5. Telemetría

Formato **Teleplot**, emitido por **USART3** (PB10/PB11) a 115200 baudios:

```
>etiqueta:numero
```

> [!IMPORTANT] Solo números (decisión del usuario, 30-sep)
> Los estados y los veredictos van como **código entero** (`>BOOT:1`, `>IMU:1`, `>APP_EST:1`,
> `>ADC_EST0:3`), con su tabla en la documentación del módulo, y **los mensajes de ayuda no se mandan
> por telemetría** (están en `Documentacion/`). Así Teleplot dibuja todas las etiquetas en vez de
> descartar las que no son números. Para depurar está la traza de las FSM (`>APP_EST`, `>MENU_EST`).

| Función | Uso |
|:--------|:----|
| `TELEM_SendInt(label, value)` | Estados, códigos, cuentas y contadores |
| `TELEM_SendFloat(label, value)` | Valores analógicos (tensiones, velocidades) |
| `TELEM_SendIRValues(ir, count)` | Sensores de pared |
| `TELEM_SendIMU(ax, ay, az, gx, gy, gz)` | Acelerómetro y giroscopio |
| `TELEM_SendMotorEncoder(e1, v1, e2, v2)` | Encoders y velocidades |
| `TELEM_SendEncoderPulses(p1a, p1b, p2a, p2b)` | Pulsos por canal |
| `TELEM_SendDutyValues(d1a, d1b, d2a, d2b)` | Ciclos de trabajo del PWM |
| `TELEM_SendString(label, value)` | **Sin usar desde el 30-sep**: se dejó como utilidad, pero ningún valor debe ir como texto |

> `%f` en `printf` está habilitado con `-Wl,-u,_printf_float`. Si se quita ese flag,
> los `TELEM_SendFloat()` imprimirán mal.

### Entrada: órdenes por el mismo puerto

El robot también **recibe**: las órdenes se escriben como líneas de texto en el monitor serie o en el
cuadro de envío de Teleplot y las interpreta `app/console.c` (recepción **por interrupción** en
`hal/uart.c`, así que no se pierde nada mientras corre una prueba).

```
BAT LIST                → publica los umbrales de batería en uso
BAT CRIT_PACK 6000      → cambia el umbral (mV; con punto serían voltios: 6.0)
HELP                    → lista las órdenes
```

Protocolo completo y cómo añadir una orden: [comandos-serie.md](./comandos-serie.md).

---

## 6. Flujo de trabajo

```
1. Documentar el hardware/prueba   → Documentacion/hardware.md, pruebas-bringup.md
2. Clasificar el cambio por capas  → APP / CONTROL / FILTER / SENSOR / HAL / HW
3. Implementar el módulo + su App_Test_*
4. Compilar y flashear
5. Verificar por telemetría (10/10)
6. Integrar en el lazo de control
7. Actualizar estado               → Documentacion/estado-del-proyecto.md
```

Este flujo está empaquetado como skill del agente en
`.github/skills/micromouse-dev/` (se activa al pedir diseño, bring-up, control o
navegación del micromouse).

---

## 7. Problemas conocidos del entorno

| Problema | Solución |
|:---------|:---------|
| `platformio: command not found` | Usar `~/.platformio/penv/bin/pio` o activar la extensión |
| El puerto de subida no aparece | Comprobar `/dev/stlinkv2_2` y permisos sobre el USB |
| ⚠️ **`st-info` y `st-flash` no funcionan**: dan `flash: 0`, `sram: 0` y `unknown chip id! 0x410` aunque el ST-Link se detecte bien | El `stlink` instalado en `/usr/local` esta **incompleto**: el binario busca `/usr/local/share/stlink/config/chips` y los datos estan en otra ruta. **Usar `openocd`**, que trae sus propios scripts: `openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "init" -c "mdh <dir> <n>" -c "shutdown"` (solo lee, no flashea) |
| Hace falta saber con **que linker script** se enlaza | `pio run -v` **no lo imprime** si no hay nada que recompilar: forzar con `touch firmware/src/main.c && pio run -d firmware -v` y buscar el `.ld`. El `.map` no existe (no hay `-Wl,-Map`) y el `idedata.json` tampoco lo trae |
| El monitor no conecta | Puerto `/dev/ttyUSB0` ocupado por otro programa |
| El ST-Link no engancha tras un flasheo | Conectar **SWDIO (PA13), SWCLK (PA14), GND y 3.3 V**; si no responde, *connect under reset* |
| Se perdió el depurador definitivamente | Solo si alguien usó `__HAL_AFIO_REMAP_SWJ_DISABLE()`: recuperar con un borrado por *connect under reset* |
| Se regeneró HAL y "desapareció" código | CubeMX sobrescribe `gpio.c`, `spi.c`…: mantener los cambios propios en módulos aparte |
