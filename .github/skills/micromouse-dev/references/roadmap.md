# Roadmap y estado — Rabosa-MVR

> Fuentes: [`Documentacion/planificacion.md`](../../../../Documentacion/planificacion.md) · [`Documentacion/estado-del-proyecto.md`](../../../../Documentacion/estado-del-proyecto.md)
> Hito MVP: **19 de septiembre de 2026** → **no alcanzado** (hardware validado; falta el control)
> Última revisión: 2026-09-30 · Menú y máquina de estados: [`Documentacion/maquina-estados-menu.md`](../../../../Documentacion/maquina-estados-menu.md)

## 1. Fases y puertas de salida

| Fase | Objetivo | Puerta de salida |
|---|---|---|
| **F0 · Bring-up** | Periféricos y sensores aislados funcionando | Las pruebas de banco dan salida coherente 10/10 · **actual: 17 de 19 escritas · 7 validadas en hardware** (la 3 se **retiró** el 28-sep y la 8 quedó sustituida por la 10; **escritas y sin ejecutar**: 7, 9 v2, 12 y **13–17**, estas últimas desde el 30-sep) |
| F1 · Salidas | OLED + menú navegable, robot con vida | ✅ **Menú y máquina de estados (28-sep)**, **FSM con el patrón `app/fsm.h` y árbol nuevo (30-sep)** y **consola de órdenes por USART3**: se lanzan y se salen las pruebas con los 4 botones, sin PC, y los umbrales se ajustan por el puerto serie. El **FAULT se abandona con OK**. Falta la fase **M6b**: pantallas de SETTINGS |
| **F2 · Sensores** | IMU + IR calibrados | **Pruebas 15–17** (ya escritas el 30-sep): IR miden 3–15 cm con error < 1 cm y aguantan la luz ambiente; IMU sin deriva a corto plazo |
| **F3 · Control** | PID de velocidad por rueda | ✅ implementado (28-sep) y validado en simulación · ⏳ **falta sintonizarlo en el robot** (prueba 18): escalón sin sobreimpulso > 20 % y ambos motores sincronizados |
| **F3b · Odometría** | Pose $(x,y,\theta)$ fiable | ✅ escrita (28-sep) y **validada en el robot (1-oct)**: recta de 1 m con **−0,3 %** de error y giro de 90° **clavado (+90° / −90°)** con la **trocha efectiva a 77,6 mm** (el giro medía un 7 % corto). ⏳ falta el cuadrado de 4×300 mm (cierre < 20 mm) |
| **F4 · Movimiento** | Recta, frenada y giros | 90° con error < 2°; celda de 18 cm con error < 5 mm |
| **F5 · Navegación** | Pasillo centrado + wall follower | Laberinto 4×4 resuelto 10/10 sin tocar paredes |
| **F6 · Futuro** | PCB, flood fill, ToF, velocidad | — |

## 2. Estado actual (consolidado del repo)

### Hecho

| Capa | Módulos |
|---|---|
| HW | `io.c/h`, `io_def.h` |
| HAL | `adc`, `drv8833`, `gpio`, `i2c`, `pwm`, `spi`, `systemclock`, `timers`, `uart` |
| SENSOR | `encoder.c/h` (TIM2/TIM3 modo encoder + `max_likelihood_counter_diff`) |
| SENSOR | `imu.c/h` (MPU6500 por SPI2) · `battery.c/h` (LiPo 2S: pack + celdas + aviso) |
| CONTROL | `pid.c/h` (anti-windup, derivada sobre la medida y filtrada, `dt` validado) · `setpoint.c/h` (rampas de consigna) · `controller.c/h` (lazo por rueda) — ✅ 28-sep |
| APP | `application.c/h` (FSM + banco de **19** rutinas), **`menu.c/h`** (menú por tablas `const`), `telemetry.c/h` (Teleplot), **`console.c/h`** (órdenes por USART3), `buttons.c/h` (4 botones; MENU por **interrupción** EXTI), `oled.c/h` (SSD1306 + glifos nuevos) |
| Build | `platformio.ini`, `extra_script.py`, `peripherals.h`, `pinout.h` |

### El stash es obsoleto (analizado el 2026-09-21)

`stash@{0}` se llama *«capas pendientes: control, filter, imu, sensor, menu, oled, spi»*, pero
`git stash show --name-only` **no devuelve ningún fichero de `control/`, `filter/`, `menu*` ni
`oled*`**: es una foto anterior a la arquitectura por capas, con versiones viejas de
`application.c`, `hal/*`, `src/main.c` y `encoder.c`. **Se ignora.**
CONTROL y FILTER se escriben de cero (como ya se escribieron `menu.c`, `oled.c`,
`buttons.c`, `battery.c` y `spi.c`, que funcionan en hardware).

### Pendiente de escribir

movimientos primitivos (`avanzar_celda`, `girar_90`, `centrar_pasillo`) · wall follower · flood fill ·
los filtros y el estimador de orientación de FILTER (`filter/adc_filter.c`, fusión con la IMU).

### Pendiente de hardware

Divisores de tensión de la batería (**22 kΩ/10 kΩ**, **sin condensadores**) y resistencias de los IR
(22 Ω + BC337 emisores · 3 kΩ receptores): ✅ **soldados (30-sep)**, pendientes de validar con las
pruebas 13–17 · **posición de los 4 canales IR** ·
rediseño de ruedas · laberinto 4×4 · fotos y exportación de los STEP.

✅ Ya montado: placa de prototipos, motores con encoder, DRV8833, LiPo 2S 450 mAh, OLED, botones,
IMU y los emisores/receptores IR comprados.

## 3. Siguientes pasos inmediatos

1. **Ejecutar el bloque analógico** (soldado el 30-sep, pruebas ya escritas): la **13** (chequeo de la
   soldadura) y después **14 → 15** (emisores y **matriz 4×4** emisor↔receptor con elección de $R_L$).
2. **Prueba 16** (modulado ON−OFF) y **prueba 17** (curva IR): dejan F2 cerrada.
3. **Prueba 12** + calibrar el divisor con el multímetro (`BAT CAL <mV>`).
4. Verificar el **PPR (7)** y el **µm/cuenta (235,6)** con la prueba 7 → base de la odometría.
5. **Menú**: ✅ hecho el 28-sep (`app/menu.c/h` + FSM). Las pruebas se lanzan y se salen con los botones.
7. ~~Escribir `control/` (PID de velocidad)~~ ✅ hecho (28-sep) y **validado en simulación**; queda **sintonizarlo con la prueba 18**.
8. ~~Escribir `filter/odometry.c`~~ ✅ hecho (28-sep) con banco en el PC; queda **la prueba 19 en el robot**
   (empujar a mano) y **calibrar la trocha efectiva $L$** — procedimiento en `pruebas-bringup.md` §15.

## 4. Semanas (resumen)

| Bloque | Semana | Foco |
|---|---|---|
| 0 | 23–25 jul | Estructura por capas y compilación |
| 1 | 28 jul–1 ago | OLED + menú |
| 2 | 3–8 ago | IMU + sensores IR |
| 3 | 11–15 ago | PID de velocidad |
| 3b | 18–22 ago | Filtros + recta y frenada |
| 4 | 25–29 ago | Pasillo + giros de 90° |
| 5 | 1–19 sep | Laberinto + wall follower + demo |

> Revisión de hitos los **viernes**: actualizar `planificacion.md` y ajustar la semana siguiente.
