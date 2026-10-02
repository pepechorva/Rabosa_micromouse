---
name: micromouse-dev
description: 'Workflow incremental para diseñar y programar el micromouse Rabosa-MVR (STM32F103C8T6, PlatformIO/stm32cube, C puro, arquitectura por capas APP→CONTROL→FILTER→SENSOR→HAL→HW). Úsala cuando haya que decidir en qué capa va un cambio, dar de alta hardware nuevo o documentarlo, escribir/ejecutar pruebas de banco (bring-up) de un periférico, interpretar telemetría Teleplot por USART3, calibrar sensores IR o encoders, tunear PID de velocidad/lateral, implementar odometría, giros de 90°, wall follower o flood fill. Incluye el contrato de documentación incremental (Documentacion/ + references/).'
argument-hint: 'Describe el hardware nuevo, la prueba hecha o el módulo a implementar'
---

# Desarrollo Micromouse — Rabosa-MVR

## Qué produce esta skill

Un cambio **trazable y verificado** en el robot micromouse, siguiendo tres invariantes:

1. **Arquitectura por capas** — cada capa solo llama a la inmediatamente inferior.
2. **Bring-up antes que sistema** — ningún módulo entra en el lazo de control sin haber sido validado aislado con una prueba de banco y telemetría.
3. **Documentación incremental** — hardware, pruebas y estado se documentan *antes* o *junto* al código, nunca después.

## Cuándo usar

| Disparador | Ir a |
|---|---|
| "Voy a usar / he cambiado este hardware" | [Paso 2](#paso-2--dar-de-alta-el-hardware-y-la-prueba) |
| "Quiero probar X periférico" | [Paso 3](#paso-3--bring-up-una-cosa-a-la-vez) |
| "¿Dónde meto este código?" | [Paso 1](#paso-1--clasificar-el-cambio) |
| "El robot va torcido / derrapa / mide mal" | [Paso 4](#paso-4--estimar-filter) y [Paso 5](#paso-5--controlar-control) |
| "Implementar wall follower / flood fill" | [Paso 6](#paso-6--navegar-app) |
| "¿Cómo compilo/flasheo/veo telemetría?" | [`entorno-desarrollo.md`](../../../Documentacion/entorno-desarrollo.md) |

## Modelo mental: las 6 capas

```
APP      Misión y secuencias      application.c · telemetry.c · menu.c · oled.c
   ↓  solo llama hacia abajo
CONTROL  Consignas y lazos        controller.c · pid.c · setpoint.c
   ↓
FILTER   Estimación de estado     odometry.c (✅) · filter.c · adc_filter.c · iekf.c
   ↓
SENSOR   Lectura del mundo        sensor.c · imu.c · encoder.c
   ↓
HAL      Periféricos del MCU      adc · drv8833 · gpio · i2c · pwm · spi · timers · uart
   ↓
HW       Registros y pines        io.c · io_def.h
```

Detalle del contrato, mapa de ficheros y convenciones de include: [`references/arquitectura.md`](./references/arquitectura.md).

---

## Procedimiento

### Paso 1 — Clasificar el cambio

Antes de escribir código, decide la capa con estas preguntas (en orden):

| Pregunta | Capa | Ejemplo |
|---|---|---|
| ¿Es un bit, un registro o un pin? | **HW** | `io_def.h`, máscaras GPIO |
| ¿Es un periférico del MCU (ADC, I2C, SPI, TIM, UART)? | **HAL** | `hal/spi.c` |
| ¿Es una **medida cruda** del mundo físico? | **SENSOR** | `encoder.c`, `imu.c` |
| ¿Es una **estimación** o fusión de medidas? | **FILTER** | `odometry.c`, `iekf.c` |
| ¿Es una **decisión** de consigna o actuación? | **CONTROL** | `pid.c`, `setpoint.c` |
| ¿Es una **secuencia de misión** o presentación? | **APP** | `application.c`, `telemetry.c` |

⚠️ Si un cambio toca dos capas, **divídelo en dos cambios**. Nunca importes una capa superior desde una inferior.

### Paso 2 — Dar de alta el hardware y la prueba

El usuario irá especificando hardware y pruebas poco a poco. Cada entrada nueva se registra en **dos sitios**:

| Qué llega | Doc humana (canónica) | Digest de la skill |
|---|---|---|
| Hardware nuevo o modificado | `Documentacion/hardware.md` | [`references/hardware.md`](./references/hardware.md) |
| Prueba de banco nueva o resultado | `Documentacion/pruebas-bringup.md` | [`references/bringup.md`](./references/bringup.md) |
| Hito o cambio de plan | `Documentacion/planificacion.md` + `estado-del-proyecto.md` | [`references/roadmap.md`](./references/roadmap.md) |

Plantillas y convenciones: [`references/hardware.md`](./references/hardware.md#dar-de-alta-hardware-nuevo).

### Paso 3 — Bring-up: una cosa a la vez

Orden **obligatorio** (cada paso depende del anterior):

1. Reloj (`SystemClock_Config`, 72 MHz) — sin esto nada cuadra.
2. UART + telemetría — sin esto no ves nada.
3. GPIO / LED — prueba de vida.
4. ADC (sensores IR, batería).
5. PWM → driver de motor → motores.
6. Timers en modo encoder → `encoder.c`.
7. I2C / SPI → IMU.
8. Sensores IR completos (emisor + receptor + calibración).

Reglas del bring-up:
- **Un solo periférico por prueba.** Si falla, sabes quién fue.
- Toda prueba **imprime por telemetría** (USART3, formato Teleplot) o parpadea un LED. Una prueba sin salida observable no es una prueba.
- Si un periférico no responde, **aísla antes de culpar al módulo**: loopback del bus, comprobación de cableado, lectura de registro WHO_AM_I.
- Selección de la rutina: **menú de la OLED** (`app/menu.c`), con los 4 botones; el botón **MENU sale** de la prueba (va por interrupción). Ya no se comenta/descomenta nada en `APP_Run()`.

Catálogo de pruebas existentes, formato de telemetría y cómo añadir una prueba nueva: [`references/bringup.md`](./references/bringup.md).

### Paso 4 — Estimar (FILTER)

Solo cuando las medidas crudas son fiables:
1. Filtro de la señal (media móvil / mediana / LPF) — `filter/`.
2. Cinemática directa y odometría (`odometry.c`): ✅ **hecha el 28-sep**, integra **incrementos de arco**
   (sin `dt`) y se valida en el PC con `firmware/test/odometry_sim.c`.
3. Fusión encoder + IMU (`iekf.c`) como paso de predicción + corrección — **no existe todavía**.

⚠️ La odometría es matemática pura: **no lee encoders**. La APP le pasa los incrementos de
`get_encoder_*_micrometers()` (excepción de capas declarada, igual que en el PID).

Contrato de interfaces y parámetros: [`references/arquitectura.md`](./references/arquitectura.md#interfaces-entre-capas).

### Paso 5 — Controlar (CONTROL)

- Primero **PID de velocidad por rueda** (error = ω_ref − ω_encoder → PWM).
- Después **PID lateral** (error = distancia_izq − distancia_der), más lento que el de velocidad.
- Toda consigna pasa por `setpoint.c` (rampas) antes de llegar al PID: nunca un escalón directo a los motores.
- Sintonía con Ziegler-Nichols y validación con escalón registrado por telemetría.

### Paso 6 — Navegar (APP)

`application.c` deja de ser banco de pruebas y pasa a **máquina de estados**:

```
BOOT → MENU → RUN → (WALL_FOLLOW | FLOOD_FILL) → STOP
```

Movimientos primitivos a implementar y validar en orden: `avanzar_celda()`, `girar_90()`, `parar()`, `centrar_pasillo()`.

---

## Puertas de calidad

No se pasa a la fase siguiente sin cumplir la puerta.

| Fase | Puerta de salida (criterio medible) |
|---|---|
| Bring-up | Cada prueba de banco produce salida coherente por telemetría, repetible 10/10 |
| FILTER | Pose con error < 5 % frente a medida manual: **prueba 19** — recta de 1 m con error < 1 % (✅ −0,3 % el 1-oct) y giros con error < 2° (✅ 90° clavado) — con la **trocha efectiva calibrada** (✅ 77,6 mm) |
| CONTROL | Escalón de velocidad sin sobreimpulso > 20 %; ambos motores a la misma velocidad ⇒ recta |
| Movimiento | 90° con error < 2°; celda de 18 cm con error < 5 mm |
| Navegación | Wall follower completa el laberinto 10/10 sin tocar paredes |

## Protocolo de ampliación

Cuando el usuario aporte información nueva, **no la dejes solo en el chat**:

1. Clasifica (Paso 1).
2. Actualiza la doc canónica en `Documentacion/` (sección correspondiente).
3. Actualiza el digest en `references/` de esta skill.
4. Si implica código, añade o ajusta la prueba de banco (Paso 3).
5. Si cambia el plan, actualiza `estado-del-proyecto.md` y `references/roadmap.md`.

> **Autorización permanente**: el usuario ha pedido que estas actualizaciones se hagan
> **directamente, sin pedir confirmación previa**. Al terminar, resume qué ficheros has
> tocado y qué has cambiado.

## Referencias

- [`references/arquitectura.md`](./references/arquitectura.md) — contrato de capas, mapa de ficheros, includes, build.
- [`references/hardware.md`](./references/hardware.md) — MCU, pinout, alimentación, cableado crítico, alta de hardware.
- [`references/bringup.md`](./references/bringup.md) — catálogo de pruebas, telemetría, calibración.
- [`references/roadmap.md`](./references/roadmap.md) — fases, hitos y estado actual.
- [`Documentacion/glosario.md`](../../../Documentacion/glosario.md) — **glosario** de tecnicismos en inglés (*duty*, *setpoint*, *wall follower*…) y convenciones de escritura.
- `Documentacion/` — documentación canónica del proyecto (índice en `Documentacion/README.md`).

## Anti-patrones

- ❌ Llamar a `HAL_*` directamente desde `CONTROL` o `APP` (salta capas).
- ❌ Meter un sensor nuevo directamente en el lazo de control sin prueba de banco.
- ❌ Tunear un PID sobre una señal sin filtrar o sin calibrar.
- ❌ Documentar "al final": la doc de hardware/pruebas se escribe al darlo de alta.
- ❌ Un `App_Test_*` que no imprime nada ni cambia un LED.
- ❌ Cambiar la configuración de un pin en dos sitios (p. ej. `gpio.c` y `hal/spi.c`) sin dejarlo anotado en `hardware.md`.
