# 🔎 Auditoría de la frontera aplicación / drivers

> 📅 **2026-10-10** · Rama `develop`.
> Al estilo de la auditoría que trae `ukmars/mazerunner-core` en su carpeta `review/`:
> mirar la frontera entre capas, medirla y decir qué aguanta y qué no.

---

## 1. Por qué esta auditoría

La idea de partida es la que salió del análisis de `mazerunner-core`: **la capa de aplicación
debería poder vivir sin el microcontrolador y la de drivers sin la lógica**. En aquel proyecto
**también es así de hecho** —lo dice su propia auditoría: *«the application-layer classes
(`Mouse`, `Motion`, `Maze`, `Profile`) are MCU-agnostic and could be reused unchanged»*— pero
**no hay ninguna interfaz escrita que lo garantice**: el día que alguien incluya un registro
del AVR desde la aplicación, nadie se entera.

Aquí se hace al revés: **primero se escribe el contrato** ([`core/core_io.h`](../firmware/core/core_io.h))
y después el código que lo cumple. Esta auditoría comprueba **en qué punto está**.

---

## 2. La frontera, medida

| Capa | Líneas | Ficheros | Incluye de | Símbolos `HAL_`/`MX_` |
|:--|--:|--:|:--|--:|
| **`core/`** — perfiles, contrato | 419 | 3 | — | **0** ✅ |
| **`control/`** — PID, rampas, lazo | 792 | 6 | — | **0** ✅ |
| **`filter/`** — odometría | 197 | 2 | — | **0** ✅ |
| `app/` — menú, telemetría, consola, banco | **9 547** | 23 | control, filter, **hal**, **sensor** | **14** ⚠️ |
| `sensor/` — encoder, IMU, batería | 1 168 | 6 | hal | 15 |
| `hal/` — periféricos del MCU | 1 647 | 22 | — | 63 |
| `hw/` — registros y pines | 92 | 3 | — | 1 |

**La capa de aplicación portable son 1 408 líneas** (`core` + `control` + `filter`) y **no toca
la HAL en absoluto**. Lo que está acoplado es `app/`, que es otra cosa.

## 3. La prueba: compilan sin el STM32

No basta con que no aparezca la cadena `HAL_`; hay que comprobarlo. Con el `gcc` del PC:

```bash
gcc -c -Wall -Wextra -I firmware firmware/core/*.c firmware/control/*.c firmware/filter/*.c
```

```
controller.o   3 928 B
odometry.o     3 360 B
pid.o          3 320 B
profile.o      3 776 B
setpoint.o     2 264 B
```

✅ **Compilan sin el STM32, sin la HAL y sin un solo aviso.** La frontera no es una intención:
es un hecho comprobable, y se puede comprobar en cada cambio.

📌 Es lo que hace posible que existan `firmware/test/pid_sim.c`, `odometry_sim.c`,
`format_test.c` y `profile_test.c`. **Si algún día dejan de compilar, la frontera se ha roto.**

---

## 4. Hallazgos

| # | Gravedad | Hallazgo | Dónde |
|:-:|:--|:--|:--|
| 1 | ✅ **Bueno** | La aplicación portable **ya está limpia**: 1 408 líneas, 0 símbolos de la HAL | `core/`, `control/`, `filter/` |
| 2 | ⚠️ **Media** | **`app/` no cumple la regla de capas escrita**: la regla dice `APP → CONTROL → …` y `app/` **salta** a `hal/` y `sensor/` directamente | `app/application.c`, `buttons.c`, `oled.c`, `walls.c`, `console.c` |
| 3 | ⚠️ **Media** | **La capa `drivers/` no existe como código**: los drivers están repartidos entre `hal/` y `sensor/` | `hal/drv8833.c`, `sensor/*` |
| 4 | 📌 **Nota** | `core/core_io.h` es hoy **un contrato sin implementación**: nadie lo cumple todavía | `core/`, `drivers/` |
| 5 | 📌 **Nota** | `core/profile.c` **no ocupa Flash** todavía: nadie lo usa y `--gc-sections` lo tira | build |

### Sobre el hallazgo 2, que es el interesante

`app/` usa la HAL de forma **legítima**: arranca los periféricos (`MX_*_Init`), enciende el LED,
lee el reloj y sondea los botones. El problema no es que lo haga, es que **la regla del
repositorio dice que no debería** y por tanto **nadie sabe si es una excepción deliberada o un
descuido**.

Medido, son **14 símbolos distintos y ~140 usos**, y se reparten así:

| Uso | Cuántos | ¿Es una fuga? |
|:--|--:|:--|
| `HAL_GetTick()` / `HAL_Delay()` | ~110 |  **Sí, y fácil de cerrar**: son dos envoltorios (`TICK_Ms()`, `ESPERAR_ms()`) |
| `HAL_GPIO_WritePin/ReadPin` | ~16 | Sí: encender el LED o un emisor debería ser `LED_Set()` / `IR_Emisor_Set()` |
| `MX_*_Init()` en `APP_InitAll()` | 6 | **No es una fuga**: es la secuencia de arranque, que es de la APP por definición |
| `__HAL_RCC_*_CLK_ENABLE()`, `HAL_Init` | 3 | No: parte del arranque |

⚠️ **Y conviene mirar cuánto de eso está en el banco de pruebas**: buena parte de esos 110
`HAL_GetTick`/`HAL_Delay` viven en las 23 pruebas de `application.c` (4 783 líneas de las 9 547
de `app/`), que son **desechables**. La deuda real es bastante menor que el número bruto.

📌 **Decisión sugerida**: la regla de capas pasa a distinguir **dos cosas dentro de `app/`**:
`app/` **como interfaz y banco** puede tocar `hal/` (es su trabajo: menú, botones, OLED,
telemetría), y `core/` **como lógica de misión** no puede tocar nada por debajo de él. Hoy la
regla los mete en el mismo saco y por eso el hallazgo parece peor de lo que es.

---

## 5. Riesgos

| Riesgo | Estado | Comentario |
|:--|:--|:--|
| **Memoria** | ✅ holgado | 21,8 KB de Flash libres (66,7 %) y 6,4 KB de RAM de 20 KB. Tras el refactor del `%f` dejó de ser el cuello de botella |
| **Bloqueos** | ✅ | `core/` es matemática pura: `PROF_Update()` es O(1) y no llama a nada que pueda tardar |
| **Concurrencia** | ✅ | Los campos que cruzan a una ISR (`estado`, `velocidad`, `posicion`) son `volatile` y de 32 bits: se leen y escriben de una vez en un Cortex-M, así que no hace falta sección crítica |
| **`dt` variable** | ✅ | `PROF_Update()` **exige** el `dt` por parámetro; no lo supone. Es la lección del 2026-10-01 |
| **Errores** | ⚠️ | Las funciones de `core/` **no devuelven código de error**: `PROF_Start()` con datos absurdos los recorta en silencio (velocidad final mayor que la de crucero, distancia minúscula). Aceptable para lo que hay, **a revisar cuando crezca** |
| **Desbordamiento de buffers** | 🚨 **pendiente** | Es el fallo que la auditoría de mazerunner destaca en su propia cola del BFS: **acotada y con desbordamiento silencioso**. El mapa y el `flood fill` que vienen son exactamente el sitio donde eso vuelve a pasar ⇒ **cuando se escriban, con comprobación de desborde** |
| **Sin reloj de perro (*watchdog*)** | ⚠️ igual que mazerunner | Un lazo colgado exige cortar la alimentación. No está en esta auditoría, pero consta |

---

## 6. Qué hacer, por orden

| # | Qué | Por qué ahora |
|:-:|:--|:--|
| 1 | **`core/motion.c`** — perfiles + mezcla avance/rotación, usando `PROF_*` y el contrato | Es lo que da valor al perfil y lo que hará que `core_io.h` tenga quien lo cumpla |
| 2 | **`drivers/core_io.c`** — implementar el contrato sobre `hal/` y `sensor/` | Cierra el hallazgo 4 y hace que la capa de aplicación funcione **de verdad** |
| 3 | **Mover a `drivers/`** lo que es de dispositivo: `hal/drv8833.c` y los módulos de magnitud física de `sensor/` | Cierra el hallazgo 3 |
| 4 | **`TICK_Ms()` y `ESPERAR_ms()`** en `hal/` y sustituir los ~110 usos de la APP | Cierra media fuga del hallazgo 2 con muy poco trabajo |
| 5 | ✅ **HECHO el 2026-10-10**: la regla está reescrita en `.github/copilot-instructions.md`, `Layered Architecture.md` y [`entorno-desarrollo.md`](./entorno-desarrollo.md) §4, distinguiendo `app/` (interfaz) de `core/` (misión) | Para que el próximo hallazgo 2 no exista |

---

## 7. Relacionado

- [`../firmware/core/core_io.h`](../firmware/core/core_io.h) — el contrato entre capas
- [`../firmware/drivers/README.md`](../firmware/drivers/README.md) — la capa de dispositivo
- [`firmwares-referencia.md`](./firmwares-referencia.md) §2 — de dónde sale la idea, con la auditoría de mazerunner al lado
- [`entorno-desarrollo.md`](./entorno-desarrollo.md) §4 — las reglas de dependencia que hay escritas hoy
- [`estado-del-proyecto.md`](./estado-del-proyecto.md) — el estado del proyecto
