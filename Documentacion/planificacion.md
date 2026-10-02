# 🗺️ Planificación Rabosa-MVR — MVP hasta 19 de Septiembre

> **Fecha:** 23 de julio de 2026  
> **Hito final:** 19 de septiembre de 2026 (sábado) — demo del MVP  
> **Régimen de trabajo:** Trabajo continuo entre semana, con **revisión de hitos los viernes**  
> **Duración:** 9 semanas restantes

> [!NOTE] Documento histórico
> Este plan se redactó el 23 de julio de 2026 y **no se reescribe**: refleja lo que se sabía
> entonces (por ejemplo, menciona comprar un TB6612FNG cuando el driver real es un **DRV8833**).
> Para el hardware real y el estado actual, consulta [hardware.md](./hardware.md) y
> [estado-del-proyecto.md](./estado-del-proyecto.md).

---

## 📍 Situación Actual (23 Julio)

### Arquitectura del Firmware

Se ha migrado de estructura plana (`src/` + `include/`) a **arquitectura por capas** según `Layered Architecture.md`.
Construcción de abajo arriba: primero las capas inferiores, luego las superiores.

```
Working set actual:
  APP     → application.c  (banco de pruebas), telemetry.c
  SENSOR  → encoder.c
  HAL     → adc.c, drv8833.c, gpio.c, i2c.c, pwm.c,
            reset.c, systemclock.c, timers.c, uart.c
  HW      → io.c, io_def.h

En stash (pendientes de integrar según plan):
  CONTROL → pid.c, setpoint.c, controller.c
  FILTER  → filter.c, adc_filter.c, iekf.c
  SENSOR  → imu.c (MPU6500), sensor.c (unificado)
  APP     → oled.c (SSD1306), menu.c (menú + botón + encoder)
  HAL     → spi.c
```

---

### ✅ Completado (23 Julio)

#### Código — Capas HW, HAL, SENSOR y APP base
| Capa | Módulo | Descripción |
|------|--------|-------------|
| HW | `io.c/h`, `io_def.h` | Registros rápidos GPIO (BSRR, ODR) |
| HAL | `adc.c/h` | ADC1, 6 canales (PA2-PA5, PB0-PB1), polling |
| HAL | `drv8833.c/h` | Driver motor: Init, SetSpeed(-100..100), Brake, Coast |
| HAL | `gpio.c/h` | Configuración completa de pines |
| HAL | `i2c.c/h` | I2C1 maestro 100 kHz: WriteReg, ReadReg, WriteMulti, ReadMulti |
| HAL | `pwm.c/h` | PWM 4 canales TIM1 |
| HAL | `reset.c/h` | Causa del último reinicio, leída de las banderas de `RCC->CSR` (`>RESET:n`) |
| HAL | `systemclock.c/h` | Reloj 72 MHz (HSE 8 MHz + PLL x9) |
| HAL | `timers.c/h` | TIM1 modo PWM |
| HAL | `uart.c/h` | USART3 + helpers SendString, SendData, ReceiveByte |
| SENSOR | `encoder.c/h` | TIM2/TIM3 modo encoder hardware + flancos software |
| APP | `application.c/h` | Banco de pruebas con 4 rutinas seleccionables |
| APP | `telemetry.c/h` | Formato Teleplot por UART |
| - | `platformio.ini` | Configurado con rutas de todas las capas |
| - | `extra_script.py` | Script para compilar por capas |
| - | `main.c` | Punto de entrada mínimo |
| - | `peripherals.h` / `pinout.h` | Headers comunes |
| - | `README.md` | Documentación de la estructura |
| - | `.gitignore` | Actualizado con nuevas carpetas |

---

### 🗂️ Stash — Código desarrollado pendiente de integrar

`stash@{0}: capas pendientes: control, filter, imu, sensor, menu, oled, spi`

| Módulo | Capa | Archivos | Previsto integrar |
|--------|------|----------|-------------------|
| OLED SSD1306 | APP | `app/oled.c/h` | Semana 29 Jul |
| Menú + botón + encoder | APP | `app/menu.c/h` | Semana 29 Jul |
| IMU MPU6500 (giroscopio) | SENSOR | `sensor/imu.c/h` | Semana 4 Ago |
| Sensor unificado | SENSOR | `sensor/sensor.c/h` | Semana 11 Ago |
| PID | CONTROL | `control/pid.c/h` | Semana 11 Ago |
| Setpoint | CONTROL | `control/setpoint.c/h` | Semana 11 Ago |
| Controller | CONTROL | `control/controller.c/h` | Semana 11 Ago |
| Filter | FILTER | `filter/filter.c/h` | Semana 18 Ago |
| ADC Filter | FILTER | `filter/adc_filter.c/h` | Semana 18 Ago |
| IEKF | FILTER | `filter/iekf.c/h` | Semana 18 Ago |
| SPI | HAL | `hal/spi.c/h` | Cuando se necesite |

---

### ❌ Pendiente de desarrollar

#### Prioridad esta semana (23-25 Jul)
- [ ] Compilar la estructura por capas sin errores
- [ ] Probar en hardware: ADC, encoders, motores
- [ ] **Revisión del viernes 25 Jul** → repasar estado

#### Prioridad inmediata (próximos 15 días)
- [ ] **OLED** — recuperar del stash `app/oled.c/h`, probar display 128×32
- [ ] **Menú + máquina de estados** — recuperar `app/menu.c/h`, integrar botón y encoder
- [ ] **Módulo giróscopio** — recuperar `sensor/imu.c/h`, probar MPU6500 por I2C
- [ ] DMA para ADC (lectura continua sin bloquear CPU)

#### Hardware y compras
- [ ] **Hacer lista de componentes**: N20, driver, batería, sensores
- [ ] **Encargar 4 módulos para el laberinto** (pedido, esperando respuesta)
- [ ] Comprar motores N20 con encoder
- [ ] Comprar driver TB6612FNG
- [ ] Comprar batería LiPo 2S + conector JST
- [ ] Comprar fototransistores IR + resistencias
- [ ] Montar breadboard + chasis provisional

#### Calibración y control
- [ ] Calibrar sensores IR (emisor PWM + receptor ADC + curva distancia)
- [ ] PID de velocidad sintonizado en ambos motores
- [ ] Robot avanza en línea recta
- [ ] Robot frena ante pared
- [ ] PID lateral (centrado en pasillo)
- [ ] Giros de 90° exactos
- [ ] Mini-laberinto 4×4
- [ ] Algoritmo Wall Follower (Mano Derecha)
- [ ] Consistencia 10/10

### 🔮 Futuro (Fase 2)
- [ ] PCB personalizada
- [ ] Flood Fill (resolución óptima)
- [ ] Sensores ToF
- [ ] Mejoras de velocidad

---

## 🗓️ Planificación Semanal (23 Jul → 19 Sep)

> **Formato de las revisiones semanales (viernes):**
> 1. Revisar hitos de la semana — ✅ / ❌
> 2. Actualizar `planificacion.md` con el progreso real
> 3. Ajustar tareas de la semana siguiente según retrasos/adelantos
> 4. Decidir qué recuperar del stash y qué nuevo código escribir
> 5. Si hay demo, mostrar el avance en hardware

---

### 🟣 BLOQUE 0: PUESTA EN MARCHA DE LA ARQUITECTURA
*Semana del 23 al 25 Jul — Compilar y validar la estructura por capas*

---

### 📅 Viernes 25 de Julio

| Hito de la semana | Tareas offline | Estado |
|-------------------|----------------|--------|
| Estructura por capas compila | [x] Reestructuración a capas completada | ✅ |
| ADC + encoders + motores funcionan | [ ] Compilar estructura | ⏳ |
| | [ ] Probar `App_Test_ADC()` → lecturas IR por UART | ⏳ |
| | [ ] Probar `App_Test_Encoders()` → pulsos al girar ruedas | ⏳ |
| | [ ] Probar `App_Test_MotoresEncoders()` → motor 50 pulsos | ⏳ |
| | [ ] Probar `App_Test_ADC_PWM()` → potenciómetro→motor | ⏳ |

**🎯 Hito de la semana:** Verificar que las 4 pruebas de `application.c` funcionan en hardware.

**📝 Para la revisión:** Repasar la arquitectura de capas, mostrar el código organizado,
decidir el orden de integración de las siguientes capas.

---

### 🟣 BLOQUE 1: PERIFÉRICOS DE SALIDA (OLED + MENÚ)
*Semana del 28 Jul al 1 Ago — Display, botón, encoder de navegación*

Objetivo: el robot cobra "vida" con pantalla y menú navegable.

| Capa afectada | Módulo | Acción |
|---------------|--------|--------|
| APP | `app/oled.c/h` | Recuperar del stash ✅ |
| APP | `app/menu.c/h` | Recuperar del stash ✅ |
| APP | `application.c/h` | Reescribir: máquina de estados con menú |

---

### 📅 Viernes 1 de Agosto

**Tareas de la semana:**
- [ ] Recuperar `app/oled.c/h` y `app/menu.c/h` del stash
- [ ] Probar OLED: mostrar texto, contadores, dibujar líneas
- [ ] Probar botón (GPIO) + encoder → navegar menú en OLED
- [ ] Rediseñar `application.c` como máquina de estados:
  - `APP_STATE_BOOT` → mostrar "Rabosa MVR"
  - `APP_STATE_MENU` → menú navegable
  - `APP_STATE_RUN` → ejecutar pruebas/control
- [ ] **Hacer lista de componentes** para la compra

**🎯 Hito:** Display OLED funcional. Menú navegable con botón + encoder.

---

### 🟡 BLOQUE 2: SENSORES (IMU + IR)
*Semana del 3 al 8 de Agosto — Giroscopio y sensores de distancia*

| Capa afectada | Módulo | Acción |
|---------------|--------|--------|
| SENSOR | `sensor/imu.c/h` | Recuperar del stash ✅ |
| SENSOR | `sensor/sensor.c/h` | Recuperar del stash ✅ |
| HAL | Ninguno nuevo | (I2C ya está) |

---

### 📅 Viernes 8 de Agosto

**Tareas de la semana:**
- [ ] Recuperar `sensor/imu.c/h` del stash
- [ ] Probar MPU6500: whoAmI, leer giroscopio y acelerómetro
- [ ] Mostrar datos de IMU en OLED
- [ ] Encender emisores IR con PWM/GPIO
- [ ] Leer receptores por ADC (PA2-PA5, PB0)
- [ ] Crear función de calibración `distancia_cm(raw)`
- [ ] Probar con regla: 3-15 cm, error < 1 cm

**🎯 Hito:** IMU comunicando por I2C. Sensores IR calibrados midiendo distancia.

---

### 🔵 BLOQUE 3: CONTROL DE MOTORES (PID)
*Semana del 11 al 15 de Agosto — Lazo cerrado de velocidad*

| Capa afectada | Módulo | Acción |
|---------------|--------|--------|
| CONTROL | `control/pid.c/h` | Recuperar del stash ✅ |
| CONTROL | `control/setpoint.c/h` | Recuperar del stash ✅ |
| CONTROL | `control/controller.c/h` | Recuperar del stash ✅ |

---

### 📅 Viernes 15 de Agosto

**Tareas de la semana:**
- [ ] Recuperar `control/pid.c/h`, `setpoint.c/h`, `controller.c/h` del stash
- [ ] Integrar PID en la máquina de estados de `application.c`
- [ ] Calcular RPM a partir de pulsos de encoder
- [ ] Fijar setpoint y cerrar lazo
- [ ] Sintonizar con Ziegler-Nichols (Ku, Tu)
- [ ] Probar respuesta: cambios de velocidad, frenado

**🎯 Hito:** PID de velocidad sintonizado en ambos motores.

---

### 🟠 BLOQUE 3b: FILTROS + MOVIMIENTO BÁSICO
*Semana del 18 al 22 de Agosto — Filtrar sensores, recta y frenada*

| Capa afectada | Módulo | Acción |
|---------------|--------|--------|
| FILTER | `filter/filter.c/h` | Recuperar del stash ✅ |
| FILTER | `filter/adc_filter.c/h` | Recuperar del stash ✅ |
| FILTER | `filter/iekf.c/h` | Recuperar del stash ✅ |

---

### 📅 Viernes 22 de Agosto

**Tareas de la semana:**
- [ ] Recuperar `filter/` del stash
- [ ] Integrar filtros en la cadena sensor→control
- [ ] Montar breadboard (BluePill + driver + sensores + batería)
- [ ] Probar `adelante()`, `atras()`, `pivotar()`, `parar()`
- [ ] Sincronizar motores con PID → línea recta
- [ ] Añadir lógica: si sensor frontal < 5 cm → frenar

**🎯 Hito:** Robot avanza en línea recta y frena ante pared.

---

### 🔴 BLOQUE 4: NAVEGACIÓN EN PASILLO
*Semana del 25 al 29 de Agosto — PID lateral y giros*

---

### 📅 Viernes 29 de Agosto

**Tareas de la semana:**
- [ ] Construir pasillo de cartón pluma de 20 cm de ancho
- [ ] Implementar PID lateral: `error = dist_izq - dist_der`
- [ ] Ajustar constante: PID lateral más lento que PID velocidad
- [ ] Programar `girar_90(grados, direccion)` con odometría + IMU
- [ ] Probar giros, medir error (< 2°)
- [ ] Añadir rampa de aceleración (soft start)

**🎯 Hito:** Robot centrado en pasillo. Giros de 90° exactos.

---

### 🏁 BLOQUE 5: LABERINTO
*Semanas del 1 al 19 de Septiembre — Mini-laberinto + Wall Follower*

---

### 📅 Viernes 5 de Septiembre

**Tareas de la semana:**
- [ ] Construir laberinto 4×4 en cartón pluma (cúter + cinta)
- [ ] Implementar máquina de estados Wall Follower (Mano Derecha)
- [ ] Estados: BUSCAR_PARED → AVANZAR → GIRAR
- [ ] Primera prueba completa: robot recorre laberinto
- [ ] Ajustar umbrales: pared frontal 8 cm, lateral 12 cm

**🎯 Hito:** Robot resuelve el laberinto 4×4 con Mano Derecha.

---

### 📅 Viernes 12 de Septiembre

**Tareas de la semana:**
- [ ] Semana colchón: absorber retrasos acumulados
- [ ] Optimizar sensores: histéresis, filtro antirrebote
- [ ] Mejorar tracción (añadir peso si necesario)
- [ ] 5/5 intentos → 10/10 en el laberinto
- [ ] Cronometrar intentos
- [ ] Ajustar velocidad máxima fiable

**🎯 Hito:** Consistencia 10/10. MVP validado.

---

### 📅 Sábado 19 de Septiembre 🏆 **Demo del MVP**

| Actividad | Descripción |
|-----------|-------------|
| 🎬 Demo en vivo | Robot resolviendo el laberinto |
| 📊 Repaso de métricas | Tiempo por intento, consistencia, velocidad |
| 🗺️ Roadmap Fase 2 | PCB personalizada, Flood Fill, ToF |
| 🎉 Agradecimientos | A la comunidad, colaboradores |

---

## 📋 Checklist General del Proyecto

### 🏗️ Código — Por capas (de abajo arriba)

#### ✅ HW — Completado
- [x] `hw/io.c/h` — Registros rápidos GPIO

#### ✅ HAL — Completado (pendiente SPI)
- [x] `hal/adc.c/h`
- [x] `hal/drv8833.c/h`
- [x] `hal/gpio.c/h`
- [x] `hal/i2c.c/h`
- [x] `hal/pwm.c/h`
- [x] `hal/reset.c/h` — por qué se ha reiniciado la placa (diagnóstico de los reinicios del banco)
- [x] `hal/systemclock.c/h`
- [x] `hal/timers.c/h`
- [x] `hal/uart.c/h`
- [x] `hal/spi.c/h` — escrito desde cero (IMU por SPI2, prueba 5)

#### ✅ SENSOR — IMU validada
- [x] `sensor/encoder.c/h`
- [x] `sensor/imu.c/h` — validada (prueba 5)
- [x] `sensor/battery.c/h` — escrito desde cero (batería 2S, prueba 12)
- [ ] `sensor/sensor.c/h` — pendiente (unificar medidas)

#### ⏳ FILTER — Por escribir desde cero
> ⚠️ **No están en el stash** (verificado el 2026-09-21 con `git stash show --name-only`: no
> aparece ningún fichero de `filter/`, `control/` ni `menu*`/`oled*`). Hay que escribirlos de nuevo.
- [ ] `filter/filter.c/h`
- [ ] `filter/adc_filter.c/h`
- [ ] `filter/iekf.c/h`

#### ⏳ CONTROL — Por escribir desde cero
> ⚠️ Mismo caso: el stash no trae `control/`.
- [ ] `control/pid.c/h`
- [ ] `control/setpoint.c/h`
- [ ] `control/controller.c/h`

#### ✅ APP — OLED hecho, menú pendiente
- [x] `app/application.c/h` — banco de pruebas con 12 rutinas
- [x] `app/telemetry.c/h`
- [x] `app/buttons.c/h` — 4 botones con pull-up interno
- [x] `app/oled.c/h` — escrito desde cero (prueba 10)
- [ ] `app/menu.c/h` — pendiente

### 🔧 Hardware
- [ ] Hacer lista de componentes (N20, driver, batería, sensores)
- [ ] Comprar motores N20 con encoder
- [ ] Comprar driver TB6612FNG
- [ ] Comprar batería LiPo 2S
- [ ] Comprar sensores IR
- [ ] Montar breadboard
- [ ] Construir chasis provisional
- [ ] Construir laberinto 4×4

### 🧪 Pruebas y Validación
- [ ] Compilar estructura por capas → **Semana del 25 Jul**
- [ ] OLED + menú funcional → **Semana del 1 Ago**
- [ ] IMU + sensores IR calibrados → **Semana del 8 Ago**
- [ ] PID velocidad sintonizado → **Semana del 15 Ago**
- [ ] Línea recta + frenado → **Semana del 22 Ago**
- [ ] PID lateral + giros 90° → **Semana del 29 Ago**
- [ ] Wall Follower funcional → **Semana del 5 Sep**
- [ ] Consistencia 10/10 → **Semana del 12 Sep**
- [ ] MVP presentado → **19 Sep**

---

## 🎯 Resumen de hitos por semana

| Sem | Fecha | Hito | Bloq |
|:---:|-------|------|------|
| 1 | **Vie 25 Jul** | Capas HW+HAL+SENSOR compilan y funcionan en hardware | 🟣 0 |
| 2 | **Vie 1 Ago** | OLED + menú navegable con botón y encoder | 🟣 1 |
| 3 | **Vie 8 Ago** | IMU (giroscopio) + sensores IR calibrados | 🟡 2 |
| 4 | **Vie 15 Ago** | PID de velocidad sintonizado | 🔵 3 |
| 5 | **Vie 22 Ago** | Línea recta + frenada ante pared | 🟠 3b |
| 6 | **Vie 29 Ago** | Centrado en pasillo + giros 90° | 🔴 4 |
| 7 | **Vie 5 Sep** | Laberinto 4×4 resuelto (Mano Derecha) | 🏁 5 |
| 8 | **Vie 12 Sep** | Consistencia 10/10 | 🏁 5 |
| 9 | **Sáb 19 Sep** | **Demo final del MVP** | 🏁 |

---

## 📦 Gestión del Stash

```bash
# Ver qué hay en el stash
git stash list

# Recuperar todo (todas las capas pendientes)
git stash pop

# Recuperar un módulo concreto (ej: solo el OLED)
git checkout stash@{0} -- firmware/app/oled.c firmware/app/oled.h
git reset  # para moverlo al working tree

# Después de recuperar, actualizar application.c con los nuevos includes
```

---

## 🔗 Enlaces Útiles

| Recurso | Ruta |
|---------|------|
| Índice de la documentación | [`README.md`](./README.md) |
| Arquitectura por capas | [`Layered Architecture.md`](./Layered%20Architecture.md) |
| Estado real del proyecto | [`estado-del-proyecto.md`](./estado-del-proyecto.md) |
| Hardware real (pines y módulos) | [`hardware.md`](./hardware.md) |
| Cálculos de diseño | [`calculos-diseno.md`](./calculos-diseno.md) |
| Pruebas: hechas / pendientes | [`historial-pruebas.md`](./historial-pruebas.md) · [`pruebas-pendientes.md`](./pruebas-pendientes.md) |
| Código fuente | [`firmware/`](../firmware/) |
| Datasheets | [`Datasheets/`](../Datasheets/) |
| Parte mecánica | [`mechanical/`](../mechanical/) |

---

*Última actualización: 23 de julio de 2026*

### 🔧 Hardware
- [ ] Comprar motores N20 con encoder
- [ ] Comprar driver TB6612FNG
- [ ] Comprar batería LiPo 2S
- [ ] Comprar sensores IR
- [ ] Montar breadboard
- [ ] Construir chasis provisional
- [ ] Construir laberinto 4×4

### 🧪 Pruebas y Validación
- [ ] Compilar estructura por capas → Semana 23 Jul
- [ ] Sensores IR calibrados → Semana 26 Jul
- [ ] PID velocidad sintonizado → Semana 15 Ago
- [ ] Línea recta + frenado → Semana 15 Ago
- [ ] PID lateral funcional → Semana 22 Ago
- [ ] IMU comunicando → Semana 22 Ago
- [ ] Giros 90° exactos → Semana 29 Ago
- [ ] OLED + menú → Semana 29 Ago
- [ ] Wall Follower funcional → Semana 5 Sep
- [ ] Consistencia 10/10 → Semana 12 Sep
- [ ] MVP presentado → 19 Sep

---

## 📦 Stash del Proyecto — descartado

> El stash **no contiene** CONTROL ni FILTER (ver arriba). Se ignora: el módulo `oled.c/h` y el
> `spi.c/h` que mencionaba el plan ya se han escrito de cero y funcionan.
> Cuando ya no haga falta, `git stash drop` para limpiar la lista de stashes.
>
> Decision y evidencia: [pruebas-pendientes.md](./pruebas-pendientes.md) §8.

---

## 🔗 Enlaces Útiles

| Recurso | Ruta |
|---------|------|
| Índice de la documentación | [`README.md`](./README.md) |
| Arquitectura por capas | [`Layered Architecture.md`](./Layered%20Architecture.md) |
| Estado real del proyecto | [`estado-del-proyecto.md`](./estado-del-proyecto.md) |
| Hardware real (pines y módulos) | [`hardware.md`](./hardware.md) |
| Cálculos de diseño | [`calculos-diseno.md`](./calculos-diseno.md) |
| Pruebas: hechas / pendientes | [`historial-pruebas.md`](./historial-pruebas.md) · [`pruebas-pendientes.md`](./pruebas-pendientes.md) |
| Código fuente | [`firmware/`](../firmware/) |
| Datasheets | [`Datasheets/`](../Datasheets/) |
| Parte mecánica | [`mechanical/`](../mechanical/) |

---

*Última actualización: 23 de julio de 2026*

---

### 🏁 HITO FINAL — 19 de Septiembre 🎉

Presentación del MVP funcionando. Demo en vivo del robot resolviendo el laberinto, repaso de métricas y hoja de ruta para la Fase 2.

---

## 📊 Resumen de Hitos

| # | Semana | Hito | Estado |
|---|--------|------|--------|
| 1 | 17 Jul | Encoders hardware + estructura PID | **ESTA SEMANA** |
| 2 | 24 Jul | PID de velocidad sintonizado (Ziegler-Nichols) | ⏳ Pendiente |
| 3 | 1 Ago | Sensores IR calibrados + compra componentes | ⏳ Pendiente |
| 4 | 8 Ago | Robot montado en breadboard, movimientos básicos | ⏳ Pendiente |
| 5 | 15 Ago | Línea recta + parada ante pared | ⏳ Pendiente |
| 6 | 22 Ago | PID lateral: robot centrado en pasillo | ⏳ Pendiente |
| 7 | 29 Ago | Giros de 90° exactos (< 2° error) | ⏳ Pendiente |
| 8 | 5 Sep | Robot resuelve laberinto 4×4 (Mano Derecha) | ⏳ Pendiente |
| 9 | 12 Sep | Consistencia 10/10 intentos | ⏳ Pendiente |
| 🏁 | **19 Sep** | **Demo del MVP** | ❌ **No alcanzado** |

> [!IMPORTANT] Realidad a 21 de septiembre de 2026
> El hito del 19-sep **no se alcanzó**, pero el camino real ha sido otro y conviene tenerlo claro:
> **todo el hardware está validado** (motores, encoders, IMU, botones, OLED, bus I2C y ADC) y el
> firmware tiene un **banco de 12 pruebas de bring-up**. Lo que falta es la **capa de control**:
> PID de velocidad y lateral, odometría, giros de 90° y navegación.
> → Estado real: [estado-del-proyecto.md](./estado-del-proyecto.md) ·
> hecho: [historial-pruebas.md](./historial-pruebas.md) ·
> pendiente: [pruebas-pendientes.md](./pruebas-pendientes.md)

---

## 🔗 Referencias

- [Índice de la documentación](./README.md)
- [Hardware y pinout real](./hardware.md)
- [Cálculos de diseño (resistencias, PID)](./calculos-diseno.md)
- [Pruebas de bring-up](./pruebas-bringup.md) · [historial](./historial-pruebas.md) · [pendientes](./pruebas-pendientes.md)
- [Cinemática y odometría](./cinemáticas.md)
- [Arquitectura por capas](./Layered%20Architecture.md)
- [Parte mecánica](../mechanical/README.md)
