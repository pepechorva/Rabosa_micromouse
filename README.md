# Rabosa-MVR


Rabosa en valenciano significa zorro, un animal inteligente que es capaz de meterse por madrigueras.


Rabosa-MVR (Minimum Viable Robot) es el robot micromouse con el que quiero presentarme a la OSHWDem, y se irá iterando en este repositorio para siguientes encuentros y/o competiciones.

Este robot está muy basado en el de UKMARSBOT (https://github.com/micromouseonline/ukmarsbot), y las siguientes iteraciones irán inspirándose hacia Zorobot de OPRobots (https://github.com/OPRobots/ZoroBot3) y Green Giant 5.19V de Green Ye (https://greenye.net/).

---


## 📋 Características

### Plataforma y Tecnología
- **Microcontrolador**: STM32F103C8T6 (Bluepill)
- **Frecuencia**: 72 MHz
- **Framework**: STM32CubeMX/HAL
- **Lenguaje**: C puro
- **Herramienta de build**: PlatformIO

## 📁 Estructura del Proyecto

```
Rabosa-MVR/
├── firmware/                 Firmware STM32 (PlatformIO, arquitectura por capas)
│   ├── platformio.ini        Configuración de compilación
│   ├── extra_script.py       Añade las capas al build (BuildSources)
│   ├── include/
│   │   ├── peripherals.h     HAL STM32 + includes globales
│   │   └── pinout.h          Pines del robot
│   ├── src/
│   │   └── main.c            Punto de entrada (delega en la capa APP)
│   ├── app/                  APP      application.c · telemetry.c · buttons.c · oled.c
│   ├── control/              CONTROL   (por escribir: pid · setpoint · controller)
│   ├── filter/               FILTER   (por escribir: filter · iekf · odometry)
│   ├── sensor/               SENSOR   encoder.c · imu.c · battery.c · encoders.md
│   ├── hal/                  HAL      adc · drv8833 · gpio · i2c · pwm · spi ·
│   │                                  systemclock · timers · uart
│   └── hw/                   HW       io.c · io_def.h
│
├── Documentacion/            Documentación técnica (índice en su README.md)
├── Datasheets/               Hojas de datos (DRV8833, STM32F103, SFH 4550, ST-1KL3A)
├── mechanical/               Piezas 3D: FreeCAD propio (versionado) + STEP de GrabCAD
├── images/                   Imágenes y diagramas (fotos del motor y del encoder)
├── .github/                  Instrucciones del repositorio y skill del agente
└── Rabosa-MVR.code-workspace Carpetas del workspace de VS Code
```

### 🗂️ ¿Por qué `firmware/` aparece dos veces en el explorador?

**Solo hay una carpeta `firmware/` en el disco**: `Rabosa-MVR/firmware/`. Lo que se duplica es la
vista, porque `Rabosa-MVR.code-workspace` declara **dos raíces**:

```jsonc
{ "folders": [ { "path": "." }, { "path": "firmware" } ] }
```

| Raíz del workspace | Apunta a | Para qué |
|:-------------------|:---------|:---------|
| `.` | `Rabosa-MVR/` | El proyecto completo: documentación, mecánica, firmware |
| `firmware` | `Rabosa-MVR/firmware/` (**la misma carpeta**) | Que la extensión **PlatformIO** reconozca el proyecto: PIO busca un `platformio.ini` en la raíz de alguna carpeta del workspace |

Por eso el explorador enseña `firmware/` una vez anidada dentro del proyecto y otra como raíz
suelta. **No hay copia duplicada y no se debe crear una**: cualquier cambio hecho en una entrada
aparece en la otra, porque es el mismo directorio.

Dónde está todo de verdad:

| Cosa | Ruta |
|:-----|:-----|
| Raíz del repositorio git | `Rabosa-MVR/` |
| Código del firmware | `Rabosa-MVR/firmware/` (`platformio.ini`, `app/`, `sensor/`, `hal/`…) |
| Resultado de compilación | `Rabosa-MVR/firmware/.pio/build/bluepill_f103c8_128k/` |

Si molesta verla duplicada, en el `.code-workspace` hay dos ajustes posibles:

- **Renombrar la raíz** (recomendado: no se pierde la integración con PIO):
  `{ "path": "firmware", "name": "firmware (PlatformIO)" }`
- **Quitarla**: se ve un solo árbol, pero la extensión PlatformIO puede dejar de reconocer el
  proyecto; compilar desde el terminal seguiría funcionando igual.

> **Decisión (2026-09-21):** se deja **como está** — la extensión PlatformIO reconoce el proyecto y
> la duplicidad en el explorador no molesta.

## 🔧 Configuración del Hardware

El firmware está configurado para:
- **Board**: Bluepill STM32F103C8T6
- **Upload Protocol**: ST-Link v2
- **Upload Port**: `/dev/stlinkv2_2`
- **Clock**: 72 MHz
- **framework**: stm32cube

Esta configuración está por defecto en `firmware/platformio.ini`, no necesitas tocar nada. 
Si quieres usar otro framework, como Arduino, no resolveré dudas, así que hazlo bajo tu responsabilidad.

### Diagrama de bloques del micromouse:

``` mermaid
graph TD
    %% Estilos de los Bloques
    classDef bateria fill:#2ecc71,stroke:#27ae60,stroke-width:2px,color:#fff;
    classDef potencia fill:#e74c3c,stroke:#c0392b,stroke-width:2px,color:#fff;
    classDef power fill:#304030,stroke:#2c3e50,stroke-width:2px,color:#fff;
    classDef logica fill:#3498db,stroke:#2980b9,stroke-width:2px,color:#fff;
    classDef sensores fill:#9b59b6,stroke:#8e44ad,stroke-width:2px,color:#fff;
    classDef debug fill:#d35400,stroke:#e67e22,stroke-width:2px,color:#fff;
    
    %% Bloque de Energía Principal
    BAT["🔋 LiPo 2S 7.4V 450mAh"]:::bateria

    %% Etapa de Distribución Conmutada
    BOOST["⚡ MT3608 boost 9V"]:::potencia
    BUCK["📉 MP1584 buck 5V"]:::power

    %% Componentes de Potencia
    DRV["🎛️ Driver DRV8833"]:::potencia
    M1["⚙️ N20 GA12-N20 6V izq"]:::potencia
    M2["⚙️ N20 GA12-N20 6V der"]:::potencia
    WH["🛞 Ruedas Ø21mm"]:::potencia

    %% Componentes Lógicos y Sensores
    BP["🧠 Blue Pill STM32F103<br>regulador 3.3V"]:::logica
    LEDS["🚨 Emisores IR<br>SFH 4550"]:::sensores
    IR_REC["👁️ Fototransistores<br>ST-1KL3A"]:::sensores
    IMU["🛰️ IMU MPU-9250/6500<br>SPI2"]:::sensores
    ENC["🔢 Encoders M1/M2<br>TIM2/TIM3"]:::sensores
    OLED["🖵 OLED SSD1306<br>I2C1"]:::sensores
    BTNS["🔘 Botones<br>MENU/UP/DOWN/OK"]:::sensores

    %% Debug / Telemetría de la Bluepill
    BT["🔌 Conversor TTL<-->USB"]:::debug
    BLE["📶 BLE HJ-580XP<br>futuro"]:::debug
    
    %% Conexiones de Energía (Líneas Continuas)
    BAT -->|7.4V| BOOST
    BAT -->|7.4V| BUCK
    BAT -->|ADC PB0/PB1| BP
    
    BOOST -->|9V| DRV
    DRV -->|Canal A| M1
    DRV -->|Canal B| M2
    M1 --- WH
    M2 --- WH
    
    BUCK -->|5V| BP
    BUCK -->|5V| OLED
    BUCK -->|5V| ENC
    
    BP -->|3.3V| IMU
    BP -->|3.3V| IR_REC
    BP -->|3.3V| LEDS
    BP -->|3.3V| BT
    
    %% Conexiones de Señal / Control (Líneas Discontinuas)
    BP -.->|PWM| DRV
    BP -.->|GPIO directo| LEDS
    IMU -.->|SPI2| BP
    ENC -.->|Timer Encoder Mode| BP
    IR_REC -.->|ADC| BP
    OLED <-.->|I2C1| BP
    BTNS -.->|GPIO pull-up| BP
    BT <-.->|USART3| BP
    BLE <-.->|USART3| BP
```

## 📊 Estado del Proyecto

**Fase**: bring-up de módulos terminado — lo que falta es la capa de control

- ✅ Arquitectura por capas establecida y compilando
- ✅ HW / HAL / SENSOR + **banco de 12 pruebas de bring-up**
- ✅ Motores (PA8–PA11 con DRV8833) e **identificación izquierda/derecha**
- ✅ **Encoders**: 7 PPR y reductora 1:10 ⇒ **280 cuentas** y **235,6 µm** por vuelta de rueda
- ✅ IMU por SPI2 · telemetría Teleplot por USART3 · reloj del ADC corregido (12 MHz)
- ✅ **Interfaz de usuario**: 4 botones (pull-up interno) + **OLED SSD1306** por I2C1
- ✅ Lista de hardware cerrada, con enlaces de compra (ver `hardware.md` §7)
- ✅ Módulo de batería 2S (divisores calculados; pendientes de montar)
- ❌ **CONTROL y FILTER**: por escribir **de cero** (el antiguo *stash* es obsoleto)
- ❌ Odometría, máquina de estados, giros de 90° y navegación
- ✅ **Trocha efectiva medida (1-oct): 77,6 mm** · el radio de rodadura ya va dentro de los µm/cuenta

> Estado detallado y siguientes pasos: [estado-del-proyecto.md](Documentacion/estado-del-proyecto.md)

## 📄 Documentación

Índice completo en [`Documentacion/README.md`](Documentacion/README.md).

| Documento | Contenido |
|:----------|:----------|
| [hardware.md](Documentacion/hardware.md) | Componentes, pinout, chasis, alimentación, BOM real con enlaces |
| [pruebas-bringup.md](Documentacion/pruebas-bringup.md) | Protocolo de pruebas de banco y calibraciones |
| [historial-pruebas.md](Documentacion/historial-pruebas.md) | Cronología: qué se probó cada día y con qué resultado |
| [pruebas-pendientes.md](Documentacion/pruebas-pendientes.md) | Backlog por fases: hardware, calibración, control, movimiento y navegación |
| [calculos-diseno.md](Documentacion/calculos-diseno.md) | Cálculos de diseño (resistencias IR, divisores, ADC, PID) |
| [calibracion-encoders.md](Documentacion/calibracion-encoders.md) | PPR, reducción y µm por cuenta: teoría y procedimiento |
| [entorno-desarrollo.md](Documentacion/entorno-desarrollo.md) | Compilar, flashear, telemetría y convenciones |
| [estado-del-proyecto.md](Documentacion/estado-del-proyecto.md) | Qué está hecho y qué toca ahora |
| [planificacion.md](Documentacion/planificacion.md) | Plan hasta el hito MVP (documento histórico) |
| [Layered Architecture.md](<Documentacion/Layered Architecture.md>) | Arquitectura por capas |
| [modelado.md](Documentacion/modelado.md) · [cinemáticas.md](<Documentacion/cinemáticas.md>) · [odometria.md](Documentacion/odometria.md) | Teoría y algoritmos |
| [Pinout (PDF)](Documentacion/Pinout.pdf) | Pinout oficial de la Blue Pill |

### Datasheets

**Índice completo (20 PDF, con fuente y notas):** [`Datasheets/README.md`](Datasheets/README.md)

- **MCU**: [STM32F103C8](Datasheets/stm32f103c8.pdf) · [RM0008 (manual de referencia)](Datasheets/stm32f103-rm0008.pdf) · [UM1850 (HAL)](Datasheets/um1850-description-of-stm32f1-hal-and-lowlayer-drivers-stmicroelectronics.pdf) · [AN2586 (hardware)](Datasheets/stm32f103-an2586-hardware.pdf) · [AN4899 (GPIO)](Datasheets/stm32f103-an4899-gpio.pdf) · [ST-LINK/V2 (UM1075)](Datasheets/stlinkv2-um1075.pdf)
- **Motores**: [familia N20 de ASLONG](Datasheets/aslong-n20-gearmotors.pdf) · nuestro **GA12-N20 con encoder** → [imágenes del fabricante](images/motor-encoder/)
- **Sensores**: [MPU-9250](Datasheets/mpu-9250-datasheet.pdf) + [mapa de registros](Datasheets/mpu-9250-register-map.pdf) · [AK8963](Datasheets/ak8963.pdf) · [SFH 4550](Datasheets/sfh4550.pdf) · [ST-1KL3A](Datasheets/st-1kl3a.pdf)
- **Alimentación, driver y pantalla**: [MT3608](Datasheets/mt3608.pdf) · [MP1584](Datasheets/mp1584.pdf) · [AMS1117](Datasheets/ams1117.pdf) · [DRV8833](Datasheets/drv8833.pdf) · [SSD1306](Datasheets/ssd1306.pdf) · [JST XH](Datasheets/jst-xh.pdf) · [DA14580](Datasheets/da14580.pdf)

> ⚠️ El buck **MP1584** está **NRND**: su propia hoja de datos lleva el sello «NOT RECOMMENDED FOR
> NEW DESIGNS · **REFER TO MP2338**» ⇒ **a sustituir en la PCB V2** por el MP2338 (criterios en
> [`hardware.md`](Documentacion/hardware.md) §4.6).

---
---

## Requisitos
- PlatformIO CLI o VS Code con extensión PlatformIO
- STM32CubeMX (opcional, para generación de código HAL)
- ST-Link v2

> Si `platformio` no está en el `PATH`, el CLI vive en `~/.platformio/penv/bin/pio`.

### Compilar el Proyecto
```bash
cd firmware
platformio run
```

### Cargar el Firmware
```bash
cd firmware
platformio run -t upload
```

### Monitoreo Serial
```bash
cd firmware
platformio device monitor -b 115200
```



