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

Medido el 2026-10-02 sobre el firmware real (65 536 B de Flash):

| Configuración | Flash | Veredicto |
|:--------------|------:|:----------|
| **`-Os` (actual)** | **48 644 B (74,2 %)** | ✅ lo que hay |
| `-Os` sin `_printf_float` | 34 676 B (52,9 %) | grifo pendiente: el `%f` cuesta 8 596 B |
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

**Grifo pendiente: 8 596 B del `%f`.** No lo usa solo la telemetría —lo usan las **pantallas de la
OLED** (`%.2f V`, las ganancias del PID, `UM/C`, `PPR`)—, así que quitarlo **no es un flag**: hay que
formatear con aritmética entera (`%d.%02d`) y cambiar `%f` por `%s`. Es el mayor ahorro que queda,
pero es un refactor con **salida visible**: con el 66 % hay 22 KB libres y no es cosa de la víspera
del evento.

### `extra_script.py`

`framework = stm32cube` **no respeta** `build_src_filter`, así que las fuentes de cada capa
se añaden con `env.BuildSources()`:

```python
add_layer_sources(env, ["app", "control", "filter", "sensor", "hal", "hw"])
```

> Para una carpeta nueva, añádela a esa lista (y crea la carpeta, aunque esté vacía,
> o el script la saltará con `[SKIP]`).

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

1. Cada capa solo llama a la inmediatamente inferior (ver [Layered Architecture.md](./Layered%20Architecture.md)).
2. `main.c` es mínimo: `APP_InitAll()` y `APP_Run()`.
3. Nada de `HAL_*` fuera de la capa HAL (salvo `HAL_Delay`/`HAL_GetTick` en APP).
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
| El monitor no conecta | Puerto `/dev/ttyUSB0` ocupado por otro programa |
| El ST-Link no engancha tras un flasheo | Conectar **SWDIO (PA13), SWCLK (PA14), GND y 3.3 V**; si no responde, *connect under reset* |
| Se perdió el depurador definitivamente | Solo si alguien usó `__HAL_AFIO_REMAP_SWJ_DISABLE()`: recuperar con un borrado por *connect under reset* |
| Se regeneró HAL y "desapareció" código | CubeMX sobrescribe `gpio.c`, `spi.c`…: mantener los cambios propios en módulos aparte |
