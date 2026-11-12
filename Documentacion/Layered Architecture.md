---
tags:
  - arquitectura-software
  - firmware
  - capas
  - stm32
  - obsidian
---

# Layered Architecture (Arquitectura por Capas)

> [!INFO] Regla de aislamiento
> **La lógica de misión (`core/`, junto con `control/` y `filter/`) no puede tocar `drivers/`,
> `sensor/`, `hal/` ni `hw/`.** Habla con el mundo por el contrato `core/core_io.h`, y quien
> lo implementa es `drivers/`.
> *Ejemplo:* ¿Cambias la IMU del hardware? Se toca el driver y nada más.
>
> ⚠️ **`app/` es la excepción deliberada**: es interfaz y banco (menú, botones, OLED,
> telemetría y las pruebas de banco), y **sí puede tocar `hal/` y `sensor/`** — arrancar los
> periféricos, encender el LED, leer el reloj—. Lo que no puede es tener lógica de misión.
> Medido el 2026-10-10: `core` + `control` + `filter` = **0 símbolos de la HAL**; `app/`, 14.
> Ver [auditoria-capas.md](./auditoria-capas.md).

---

## 🏗️ Esquema Visual por Capas

<div style="display: flex; flex-direction: column; gap: 10px; font-family: monospace; font-size: 0.9em;">

  <!-- CAPA APP -->
  <div style="display: flex; background-color: #FCE4EC; border-radius: 8px; padding: 10px; align-items: center; border-left: 4px solid #EC407A;">
    <div style="writing-mode: vertical-lr; transform: rotate(180deg); font-weight: bold; padding-right: 10px; color: #880E4F;">APP</div>
    <div style="display: flex; gap: 10px; width: 100%;">
      <div style="flex: 1; background: #FFF0F5; padding: 8px; border-radius: 6px; border: 1px solid #F8BBD0;">
        <b>Application</b><br>application.c
      </div>
      <div style="flex: 1; background: #FFF0F5; padding: 8px; border-radius: 6px; border: 1px solid #F8BBD0;">
        <b>Telemetry</b><br>telemetry.c
      </div>
    </div>
  </div>

  <!-- CAPA CONTROL -->
  <div style="display: flex; background-color: #FFF8E1; border-radius: 8px; padding: 10px; align-items: center; border-left: 4px solid #FFCA28;">
    <div style="writing-mode: vertical-lr; transform: rotate(180deg); font-weight: bold; padding-right: 10px; color: #F57F17;">CONTROL</div>
    <div style="display: flex; gap: 10px; width: 100%;">
      <div style="flex: 1; background: #FFFDE7; padding: 8px; border-radius: 6px; border: 1px solid #FFE082;">
        <b>Controller</b><br>controller.c
      </div>
      <div style="flex: 1; background: #FFFDE7; padding: 8px; border-radius: 6px; border: 1px solid #FFE082;">
        <b>PID</b><br>pid_controller.c
      </div>
      <div style="flex: 1; background: #FFFDE7; padding: 8px; border-radius: 6px; border: 1px solid #FFE082;">
        <b>Setpoints</b><br>setpoint.c
      </div>
    </div>
  </div>

  <!-- CAPA FILTER -->
  <div style="display: flex; background-color: #E3F2FD; border-radius: 8px; padding: 10px; align-items: center; border-left: 4px solid #42A5F5;">
    <div style="writing-mode: vertical-lr; transform: rotate(180deg); font-weight: bold; padding-right: 10px; color: #0D47A1;">FILTER</div>
    <div style="display: flex; gap: 10px; width: 100%;">
      <div style="flex: 1; background: #EBF5FB; padding: 8px; border-radius: 6px; border: 1px solid #90CAF9;">
        <b>Filter</b><br>filter.c
      </div>
      <div style="flex: 1; background: #EBF5FB; padding: 8px; border-radius: 6px; border: 1px solid #90CAF9;">
        <b>IEKF</b><br>iekf.c
      </div>
      <div style="flex: 1; background: #EBF5FB; padding: 8px; border-radius: 6px; border: 1px solid #90CAF9;">
        <b>Odometry</b><br>odometry.c
      </div>
    </div>
  </div>

  <!-- CAPA SENSOR -->
  <div style="display: flex; background-color: #FBE9E7; border-radius: 8px; padding: 10px; align-items: center; border-left: 4px solid #FF7043;">
    <div style="writing-mode: vertical-lr; transform: rotate(180deg); font-weight: bold; padding-right: 10px; color: #BF360C;">SENSOR</div>
    <div style="display: flex; gap: 10px; width: 100%;">
      <div style="flex: 1; background: #FFF3E0; padding: 8px; border-radius: 6px; border: 1px solid #FFAB91;">
        <b>Sensor</b><br>sensor.c
      </div>
      <div style="flex: 1; background: #FFF3E0; padding: 8px; border-radius: 6px; border: 1px solid #FFAB91;">
        <b>ENCODER</b><br>encoder.c (MPU6050)
      </div>
      <div style="flex: 1; background: #FFF3E0; padding: 8px; border-radius: 6px; border: 1px solid #FFAB91;">
        <b>IMU Driver</b><br>imu.c (MPU6050)
      </div>
    </div>
  </div>

  <!-- CAPA HAL -->
  <div style="display: flex; background-color: #F3E5F5; border-radius: 8px; padding: 10px; align-items: center; border-left: 4px solid #AB47BC;">
    <div style="writing-mode: vertical-lr; transform: rotate(180deg); font-weight: bold; padding-right: 10px; color: #4A148C;">HAL</div>
    <div style="display: flex; gap: 6px; width: 100%;">
      <div style="flex: 1; background: #FAF0CA; padding: 6px; border-radius: 6px; border: 1px solid #CE93D8; font-size: 0.85em;">
        <b>I2C</b><br>i2c.c
      </div>
      <div style="flex: 1; background: #FAF0CA; padding: 6px; border-radius: 6px; border: 1px solid #CE93D8; font-size: 0.85em;">
        <b>RADIO</b><br>radio.c
      </div>
      <div style="flex: 1; background: #FAF0CA; padding: 6px; border-radius: 6px; border: 1px solid #CE93D8; font-size: 0.85em;">
        <b>USART</b><br>usart.c
      </div>
      <div style="flex: 1; background: #FAF0CA; padding: 6px; border-radius: 6px; border: 1px solid #CE93D8; font-size: 0.85em;">
        <b>Timer/PWM</b><br>timer.c
      </div>
      <div style="flex: 1; background: #FAF0CA; padding: 6px; border-radius: 6px; border: 1px solid #CE93D8; font-size: 0.85em;">
        <b>Status</b><br>monitor.c
      </div>
    </div>
  </div>

  <!-- CAPA HW -->
  <div style="display: flex; background-color: #EDE7F6; border-radius: 8px; padding: 10px; align-items: center; border-left: 4px solid #7E57C2;">
    <div style="writing-mode: vertical-lr; transform: rotate(180deg); font-weight: bold; padding-right: 10px; color: #311B92;">HW</div>
    <div style="display: flex; gap: 10px; width: 100%;">
      <div style="flex: 1; background: #F3E5F5; padding: 8px; border-radius: 6px; border: 1px solid #B39DDB;">
        <b>Register IO</b><br>io.c · io_def.h
      </div>
      <div style="flex: 1; background: #F3E5F5; padding: 8px; border-radius: 6px; border: 1px solid #B39DDB;">
        <b>STM32L4xx</b><br>system_stm32l4xx.c
      </div>
    </div>
  </div>

</div>

---

## 📑 Detalle por Capas

### 1. `APP` (Capa de Aplicación)
* **`application.c`**: Lógica principal de ejecución y máquina de estados.
* **`telemetry.c`**: Gestión y empaquetado de datos transmitidos.

### 2. `CONTROL` (Capa de Control)
* **`controller.c`**: Gestor de los lazos de control.
* **`pid_controller.c`**: Algoritmos de control PID.
* **`setpoint.c`**: Puntos de consigna/referencia deseada.

### 3. `FILTER` (Capa de Filtrado)
* **`filter.c`**: Filtros digitales pasa-bajas/pasa-altas.
* **`iekf.c`**: Filtro de Kalman Extendido Invariante (*Invariant Extended Kalman Filter*).

### 4. `SENSOR` (Capa de Sensores)
* **`sensor.c`**: Abstracción e interfaz unificada de sensores.
* **`imu.c`**: Driver de la IMU (MPU6050).

### 5. `HAL` (Hardware Abstraction Layer)
* **`i2c.c`** | **`radio.c`** | **`usart.c`** | **`timer.c`** | **`monitor.c`**
  Controladores de periféricos físicos del microcontrolador.

### 6. `HW` (Capa de Hardware Físico / Registros)
* **`io.c` / `io_def.h`**: Control directo de registros de entrada/salida.
* **`system_stm32l4xx.c`**: Archivo de arranque y reloj del chip STM32.