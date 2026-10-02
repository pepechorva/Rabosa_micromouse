# Rabosa MVR — Firmware

Firmware para robot Micromouse basado en **STM32F103C8T6** (Blue Pill) con arquitectura por capas.

---

## 🏗️ Arquitectura por Capas

Cada capa **solo se comunica directamente con la capa inferior.**
*Ejemplo:* ¿Cambias la IMU del hardware? Únicamente se modifica `imu.c`.

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
      <div style="flex: 1; background: #FFF0F5; padding: 8px; border-radius: 6px; border: 1px solid #F8BBD0;">
        <b>Menu</b><br>menu.c
      </div>
      <div style="flex: 1; background: #FFF0F5; padding: 8px; border-radius: 6px; border: 1px solid #F8BBD0;">
        <b>OLED</b><br>oled.c (SSD1306)
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
        <b>PID</b><br>pid.c
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
        <b>ADC Filter</b><br>adc_filter.c
      </div>
      <div style="flex: 1; background: #EBF5FB; padding: 8px; border-radius: 6px; border: 1px solid #90CAF9;">
        <b>IEKF</b><br>iekf.c
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
        <b>IMU Driver</b><br>imu.c (MPU6500)
      </div>
      <div style="flex: 1; background: #FFF3E0; padding: 8px; border-radius: 6px; border: 1px solid #FFAB91;">
        <b>Encoder</b><br>encoder.c
      </div>
    </div>
  </div>

  <!-- CAPA HAL -->
  <div style="display: flex; background-color: #F3E5F5; border-radius: 8px; padding: 10px; align-items: center; border-left: 4px solid #AB47BC;">
    <div style="writing-mode: vertical-lr; transform: rotate(180deg); font-weight: bold; padding-right: 10px; color: #4A148C;">HAL</div>
    <div style="display: flex; gap: 6px; width: 100%; flex-wrap: wrap;">
      <div style="flex: 1; min-width: 70px; background: #FAF0CA; padding: 6px; border-radius: 6px; border: 1px solid #CE93D8; font-size: 0.85em;">
        <b>ADC</b><br>adc.c
      </div>
      <div style="flex: 1; min-width: 70px; background: #FAF0CA; padding: 6px; border-radius: 6px; border: 1px solid #CE93D8; font-size: 0.85em;">
        <b>DRV8833</b><br>drv8833.c
      </div>
      <div style="flex: 1; min-width: 70px; background: #FAF0CA; padding: 6px; border-radius: 6px; border: 1px solid #CE93D8; font-size: 0.85em;">
        <b>GPIO</b><br>gpio.c
      </div>
      <div style="flex: 1; min-width: 70px; background: #FAF0CA; padding: 6px; border-radius: 6px; border: 1px solid #CE93D8; font-size: 0.85em;">
        <b>I2C</b><br>i2c.c
      </div>
      <div style="flex: 1; min-width: 70px; background: #FAF0CA; padding: 6px; border-radius: 6px; border: 1px solid #CE93D8; font-size: 0.85em;">
        <b>PWM</b><br>pwm.c
      </div>
      <div style="flex: 1; min-width: 70px; background: #FAF0CA; padding: 6px; border-radius: 6px; border: 1px solid #CE93D8; font-size: 0.85em;">
        <b>SPI</b><br>spi.c
      </div>
      <div style="flex: 1; min-width: 70px; background: #FAF0CA; padding: 6px; border-radius: 6px; border: 1px solid #CE93D8; font-size: 0.85em;">
        <b>Timer</b><br>timers.c
      </div>
      <div style="flex: 1; min-width: 70px; background: #FAF0CA; padding: 6px; border-radius: 6px; border: 1px solid #CE93D8; font-size: 0.85em;">
        <b>UART</b><br>uart.c
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
    </div>
  </div>

</div>

---

## 📁 Estructura de Directorios

```
firmware/
├── platformio.ini          ← Configuración de compilación
├── README.md               ← Este archivo
│
├── app/                    ← 🟥 CAPA DE APLICACIÓN
│   ├── application.c       ←   Máquina de estados principal
│   ├── application.h       │
│   ├── telemetry.c         ←   Telemetría (formato Teleplot)
│   ├── telemetry.h         │
│   ├── buttons.c           ←   Lectura + antirrebote de MENU/UP/DOWN/OK
│   ├── buttons.h           │
│   ├── menu.c              ←   Menú interactivo (botón + encoder + OLED)
│   ├── menu.h              │
│   ├── oled.c              ←   Driver OLED SSD1306 128×32 vía I2C
│   └── oled.h              │
│
├── control/                ← 🟨 CAPA DE CONTROL
│   ├── controller.c        ←   Gestor de lazos de control
│   ├── controller.h        │
│   ├── pid.c               ←   PID con anti-windup y filtro derivativo
│   ├── pid.h               │
│   ├── setpoint.c          ←   Rampas y perfiles de consigna
│   └── setpoint.h          │
│
├── filter/                 ← 🟦 CAPA DE FILTRADO
│   ├── filter.c            ←   Filtros genéricos (LPF, media móvil, mediana)
│   ├── filter.h            │
│   ├── adc_filter.c        ←   Filtros para sensores IR (ADC)
│   ├── adc_filter.h        │
│   ├── iekf.c              ←   Filtro de Kalman Extendido Invariante
│   └── iekf.h              │
│
├── sensor/                 ← 🟧 CAPA DE SENSORES
│   ├── encoder.c           ←   Encoders de ruedas (TIM2 / TIM3)
│   ├── encoder.h           │
│   ├── imu.c               ←   Driver MPU6500 (I2C)
│   ├── imu.h               │
│   ├── sensor.c            ←   Interfaz unificada de sensores
│   └── sensor.h            │
│
├── hal/                    ← 🟪 CAPA DE ABSTRACCIÓN DE HARDWARE
│   ├── adc.c               ←   ADC (conversor analógico-digital)
│   ├── adc.h               │
│   ├── drv8833.c           ←   Driver motor DRV8833
│   ├── drv8833.h           │
│   ├── gpio.c              ←   Configuración de pines GPIO
│   ├── gpio.h              │
│   ├── i2c.c               ←   Protocolo I2C maestro
│   ├── i2c.h               │
│   ├── pwm.c               ←   Señales PWM
│   ├── pwm.h               │
│   ├── spi.c               ←   Protocolo SPI maestro
│   ├── spi.h               │
│   ├── systemclock.c       ←   Configuración de reloj (72 MHz)
│   ├── systemclock.h       │
│   ├── timers.c            ←   Temporizadores (TIM1 PWM)
│   ├── timers.h            │
│   ├── uart.c              ←   UART (USART3)
│   └── uart.h              │
│
├── hw/                    ← 🟫 CAPA DE HARDWARE (REGISTROS)
│   ├── io.c               ←   Acceso rápido a registros GPIO
│   ├── io.h               │
│   └── io_def.h           ←   Definiciones de pines aceleradas
│
├── include/               ← Cabeceras comunes del proyecto
│   ├── peripherals.h      ←   HAL STM32 + macros globales
│   └── pinout.h           ←   Definición de pines del robot
│
├── src/
│   └── main.c             ← Punto de entrada (mínimo: delega en APP)
│
└── test/
    └── README
```

---

## 📋 Detalle por Capas

### 🟥 APP — Aplicación

| Archivo | Descripción |
|---------|-------------|
| `application.c` | Máquina de estados: `INIT → IDLE → CALIBRATE → RUN → ERROR`. Orquesta todas las capas inferiores. |
| `telemetry.c` | Envío de datos en formato Teleplot por UART para visualización en PC. |
| `menu.c` | Menú interactivo. Usa el botón (OK/seleccionar) y el encoder de ruedas para navegar opciones en la OLED. |
| `oled.c` | Driver del display OLED SSD1306 de 128×32 píxeles vía I2C. Buffer completo de 512 bytes, fuente 5×7, soporta texto, números, líneas y rectángulos. |

### 🟨 CONTROL — Control

| Archivo | Descripción |
|---------|-------------|
| `controller.c` | Gestor de lazos de control. Coordina PIDs de velocidad y orientación según el modo (IDLE, SPEED, POSITION, ORIENTATION, WALL_FOLLOW). |
| `pid.c` | Controlador PID con ganancias Kp/Ki/Kd, anti-windup por saturación del integrador y filtro derivativo pasa-bajas. |
| `setpoint.c` | Generación de consignas con perfiles: escalón, rampa lineal, trapezoidal (acel-crucero-decel) y curva suave. |

### 🟦 FILTER — Filtrado

| Archivo | Descripción |
|---------|-------------|
| `filter.c` | Filtros digitales genéricos: pasa-bajas exponencial (Q16 y float), media móvil simple y filtro mediano. |
| `adc_filter.c` | Filtros específicos para los 5 sensores IR. Cada canal tiene su propio pasa-bajas + mediana. |
| `iekf.c` | Filtro de Kalman Extendido Invariante. Estima orientación (roll, pitch, yaw) fusionando giroscopio y acelerómetro. |

### 🟧 SENSOR — Sensores

| Archivo | Descripción |
|---------|-------------|
| `encoder.c` | Encoders de cuadratura. Motor 1 → TIM2 (PA0/PA1), Motor 2 → TIM3 (PA6/PA7). Incluye conteo hardware y por flanco individual. |
| `imu.c` | Driver MPU6500 (no MPU6050). WhoAmI = 0x70. Lectura de acelerómetro, giroscopio y temperatura vía I2C. Calibración de offset del giroscopio. |
| `sensor.c` | Interfaz unificada. `SENSOR_ReadAll()` llena una estructura con IR + encoders + IMU de una sola llamada. |

### 🟪 HAL — Abstracción de Hardware

| Archivo | Descripción |
|---------|-------------|
| `adc.c` | ADC1 con 6 canales (PA2-PA5, PB0-PB1). Lectura por polling. |
| `drv8833.c` | Driver del DRV8833. Control de 2 motores DC con PWM. Funciones: Init, SetSpeed (-100..100), Brake, Coast. |
| `gpio.c` | Inicialización de todos los pines GPIO del sistema. |
| `i2c.c` | I2C1 maestro a 100 kHz. Operaciones: WriteReg, ReadReg, WriteMulti, ReadMulti, WriteRaw. |
| `pwm.c` | Control de los 4 canales PWM del TIM1. |
| `spi.c` | SPI1 maestro a ~4.5 MHz. Operaciones con CS gestionado por GPIO. |
| `systemclock.c` | Reloj del sistema: HSE 8 MHz → PLL x9 → SYSCLK 72 MHz. |
| `timers.c` | Inicialización del TIM1 en modo PWM. |
| `uart.c` | USART3 (PB10=TX, PB11=RX) con helpers SendString, SendData, ReceiveByte. |

### 🟫 HW — Hardware (Registros)

| Archivo | Descripción |
|---------|-------------|
| `io.c` | Funciones de acceso rápido a registros GPIO (BSRR, ODR) para operaciones críticas. |
| `io_def.h` | Definiciones de pines con índices para acceso por array. |

---

## ⚙️ Compilación

El proyecto usa **PlatformIO** con el framework **STM32Cube**.

```bash
# Compilar
pio run

# Subir al dispositivo
pio run --target upload

# Monitor serie
pio device monitor
```

La configuración de `platformio.ini` ya incluye las rutas de todas las capas:

```ini
src_filter = +<src/> +<app/> +<control/> +<filter/> +<sensor/> +<hal/> +<hw/>
build_flags = -D STM32F1
  -Iinclude -Iapp -Icontrol -Ifilter -Isensor -Ihal -Ihw
```

---

## 🔌 Dependencias entre Capas

```
APP
 ├── CONTROL
 │    ├── PID
 │    └── Setpoint
 ├── FILTER
 │    ├── ADC Filter
 │    └── IEKF
 ├── SENSOR
 │    ├── Encoder
 │    ├── IMU (MPU6500)
 │    └── Sensor (unificado)
 └── HAL
      ├── ADC
      ├── DRV8833
      ├── GPIO
      ├── I2C
      ├── OLED (SSD1306)
      ├── PWM
      ├── SPI
      ├── SystemClock
      ├── Timer
      └── UART
            └── HW (Registros GPIO)
```

**Regla de oro:** una capa `N` puede usar símbolos de la capa `N-1`, pero nunca de la capa `N+1`.  
*Ejemplo:* `CONTROL` usa `FILTER` y `SENSOR`, pero nunca llama directo a `HAL`.

---

## 🧪 Tests

Los tests unitarios se encuentran en `test/`. Para ejecutarlos:

```bash
pio test
```

---

> 📌 **Nota:** Este README refleja la estructura del firmware según la [Layered Architecture](../Documentacion/Layered%20Architecture.md).
