# 📅 Historial de pruebas y avances

> Cronología de lo que se ha ido probando en el robot, día a día.
> **Cómo se ha reconstruido**: a partir del historial de sesiones de chat del proyecto
> (fechas reales de cada sesión) y del código de `firmware/app/application.c`.
> Las fechas son las de la sesión en que se trabajó el tema; donde no hay fecha exacta de banco
> se indica con ⚠️.
>
> - Catálogo y protocolo de las pruebas de banco: [pruebas-bringup.md](./pruebas-bringup.md)
> - Lo que falta por hacer: [pruebas-pendientes.md](./pruebas-pendientes.md)

---

## 1. Cronología

| Fecha | Qué se hizo | Cómo (rutina) | Resultado |
|:------|:------------|:--------------|:----------|
| **2026-07-17** | Primeros ensayos de motores y encoders | `App_RunLoop3()` (rampa 0→100 %), motores al 25 % avanzando 50 pulsos, contador de pulsos por teleplot (`1A/1B/2A/2B`), `calcularReductora()` | ✅ Los motores giran y los encoders cuentan. Nace la prueba 2 |
| **2026-07-17** | Lectura de la reducción del motor contando encoders en PA0/PA1 | `App_RunLoop_CalcularReductora()` | ⚠️ Sin resultado anotado; queda pendiente como prueba 7 |
| **2026-07-22 / 23** | Reorganización del firmware en **capas** (HW · HAL · SENSOR · APP) según *Layered Architecture* | Refactor de carpetas y `extra_script.py` | ✅ Estructura por capas funcionando |
| **2026-07-27** | Driver de encoders estilo **mmlib/UKMARSBOT**: modo hardware TI12 ×4, `max_likelihood_counter_diff()` para el *overflow* | `sensor/encoder.c` + [encoders.md](../firmware/sensor/encoders.md) | ✅ Base de la odometría |
| **2026-07-27 / 28** | Documentos teóricos: `odometria.md`, `cinemáticas.md`, 14 temas de estudio | — | ✅ Documentación |
| **2026-08-03** | Prueba de **PWM pura** en PB5/PB6/PB7 (barrido y rampas 0→500) | Rutina de PWM en `main` | ✅ El PWM funciona; predecesora de la prueba 9 |
| **2026-09-04** | **Bring-up de la IMU** por SPI2: telemetría `>AX >AY >AZ >GX >GY >GZ` pegada en el chat para ajustar | `App_Test_IMU()` (prueba 5) | ✅ IMU respondiendo (se identificó el cruce `SDA/ADO` como causa típica de fallo) |
| **2026-09-10** | Documentación estructurada (`hardware.md`, `pruebas-bringup.md`, `estado-del-proyecto.md`), skill del agente, **lista de hardware confirmada**, y se marcan como validadas las pruebas 1-6 | Pruebas 1-6 | ✅ 1, 2, 4, 5, 6 validadas en banco · ❌ 3 sin ejecutar |
| **2026-09-10** | Se escribe la **prueba 7** (calibración de encoders: PPR, reducción, µm/cuenta) y se asignan los 4 botones | `App_Test_CalibracionEncoders()` | ❌ Pendiente de ejecutar |
| **2026-09-18** | **Prueba 9 v1**: rampa de PWM en lazo abierto sobre **PA8/PA9**, con timing en segundos | `App_Test_PWM_Rampa()` | ✅ **Rueda izquierda** (PA8/PA9) identificada; se repite con PA10/PA11 (derecha) |
| **2026-09-18** | **Encoders conectados y confirmados**: izquierdo en A0/A1 (TIM2), derecho en A6/A7 (TIM3) | Se mueve cada motor por separado con la prueba 9 y se ve qué rueda gira y qué contador sube | ✅ Coincidía con lo que ya esperaba `sensor/encoder.c` |
| **2026-09-19** | **Prueba 9 v2**: la rampa pasa a medirse en **ticks de encoder** (500 ticks = 1 s) con protecciones anti-atasco | `pwm_rampa()` reescrita | ⚠️ Compilada y lista; ejecución pendiente |
| **2026-09-20** | **Botones intercambiados** (OK↔MENU y UP↔DOWN) y telemetría de eventos `>BTN:n` (potencias de dos: 1/2/4/8) | `BUTTONS_GetEvent()` + `TELEM_SendButton()` | ✅ Cableado y numeración cuadrados |
| **2026-09-20** | **Driver del OLED SSD1306 128×32** por I2C1 escrito desde cero (framebuffer, fuente 5×7) y prueba 10 (botón → nombre + pin en pantalla) | `app/oled.c` + `App_Test_BotonesOLED()` | ✅ Funcionando; se giró la pantalla 180° (`OLED_ROTACION_180`) |
| **2026-09-21** | **Diagnóstico del bus I2C1**: niveles en flotante + escaneo de direcciones | `App_Test_I2C_Scan()` (prueba 11) | ✅ `SCL=1 SDA=1` y `I2C_FOUND:60` → **el módulo OLED sí lleva pull-ups** y responde en 0x3C |
| **2026-09-21** | **Corregido el reloj del ADC**: prescaler a 12 MHz (estaba en 36 MHz, fuera de especificación) y muestreo a 239.5 ciclos | `hal/adc.c` | ✅ Admite fuentes de alta impedancia (divisores e IR) |
| **2026-09-21** | **Divisores de batería calculados** (100 kΩ/56 kΩ) + módulo `sensor/battery.c` + **aviso por LED y pantalla** (el filtro de 100 nF se retiró el mismo día por falta de espacio) | `App_Test_Bateria()` (prueba 12) | ❌ Pendiente: faltan los divisores |
| **2026-09-21** | **Datasheets descargados** del emisor (SFH 4550) y receptor (ST-1KL3A) y **resistencias IR calculadas** | `Datasheets/` + [calculos-diseno.md](./calculos-diseno.md) | ✅ 220 Ω emisores · R_L 4,7 kΩ receptores |
| **2026-09-21** | **Especificaciones del motor y del encoder volcadas al repo** (imágenes incluidas) y constantes al código | `images/motor-encoder/` + `sensor/encoder.h` | ✅ Reductora **1:10** y encoder **7 PPR** (fabricante) ⇒ **280 cuentas** y **235,6 µm/cuenta** |
| **2026-09-21** | **Medición del nivel de salida de los encoders** (van a 5 V) con el polímetro | Sonda en `C1`/`C2` girando la rueda | ✅ **≈3,6 V de pico a pico** ⇒ el módulo limita internamente la salida y queda dentro del máximo absoluto (4,0 V) de PA0/PA1/PA6/PA7 |

---

## 2. Estado de las 12 pruebas de banco

| # | Prueba | Rutina | Estado | Fecha |
|:-:|:-------|:-------|:------:|:-----:|
| 1 | ADC + sensores IR + telemetría | `App_Test_ADC()` | ✅ Validada | ≤ 2026-09-10 ⚠️ |
| 2 | Motores + encoders en bucle cerrado | `App_Test_MotoresEncoders()` | ✅ Validada | 2026-07-17 |
| 3 | ADC → PWM directo | `App_Test_ADC_PWM()` | ❌ No ejecutada | — |
| 4 | Encoders solo lectura | `App_Test_Encoders()` | ✅ Validada | 2026-07-17 |
| 5 | IMU MPU-6500 por SPI2 | `App_Test_IMU()` | ✅ Validada | 2026-09-04 |
| 6 | Loopback SPI2 (diagnóstico) | `App_Test_SPI2_Loopback()` | ✅ Validada | ≤ 2026-09-10 ⚠️ |
| 7 | Calibración de encoders (PPR / µm por cuenta) | `App_Test_CalibracionEncoders()` | ❌ Pendiente | — |
| 8 | Botones + antirrebote | `App_Test_Botones()` | ➖ Sustituida por la 10 | — |
| 9 | Rampa de PWM (izq. PA8/PA9 · dcha. PA10/PA11) | `App_Test_PWM_Rampa()` | ⚠️ v1 (temporal) validada con la izquierda · v2 (ticks) pendiente | 2026-09-18 |
| 10 | Botones → pantalla OLED | `App_Test_BotonesOLED()` | ✅ Validada | 2026-09-20 |
| 11 | Diagnóstico del bus I2C1 | `App_Test_I2C_Scan()` | ✅ Validada (`>I2C_FOUND:60`) | 2026-09-21 |
| 12 | Batería 2S (pack + celdas) + aviso | `App_Test_Bateria()` | ❌ Faltan los divisores | — |

> ⚠️ Las fechas marcadas así son anteriores al registro por sesiones: se sabe que se validaron
> antes del 2026-09-10, pero no el día exacto. Si las recuerdas, se corrigen.

---

## 3. Hitos y bloqueos

- **Hasta 2026-09-10**: capas HW/HAL y buena parte de SENSOR funcionando, con 5 pruebas validadas.
- **2026-09-18 → 2026-09-21**: se cerró el bloque de *entradas* (encoders confirmados, botones,
  pantalla, diagnóstico I²C) y se corrigió un fallo de fondo del ADC. **El robot ya "siente" y
  "habla"**, pero todavía no tiene lazo cerrado ni navegación.
- **Bloqueo actual**: no hay `CONTROL` ni `FILTER` en el firmware (los únicos "pendientes" que
  quedan de la antigua reorganización se perdieron en un *stash* obsoleto: ver
  [pruebas-pendientes.md](./pruebas-pendientes.md) §7).
- **Siguiente hito**: montar los divisores y los sensores IR, calibrar (PPR y µm/cuenta) y
  **cerrar el primer PID de velocidad**.
