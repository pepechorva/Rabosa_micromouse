# 🔤 Glosario de tecnicismos

> **Por qué existe este documento**: en el proyecto se usan **tecnicismos en inglés** (duty, wall
> follower, anti-windup…), porque son los términos estándar del sector y así no hay que traducir
> datasheets, librerías ni foros. Este glosario es la **correspondencia** con el español y, sobre
> todo, **cómo se usa cada término aquí**.
>
> **Regla**: el término en inglés **se mantiene** (no se traduce) y, la primera vez que aparece en un
> documento, va enlazado o en cursiva. Lo que sí va en español es **todo lo demás**.
>
> Índice general: [README.md](./README.md)

---

## 0. Convenciones de escritura del repositorio

| Qué | Convención | Ejemplo |
|:----|:-----------|:--------|
| Documentación y comentarios de código | **Español** | `/* Rampa de consigna 0 → 100 % */` |
| Identificadores de código (ficheros, funciones, variables) | **Inglés**, con prefijo de módulo | `CTRL_Update()`, `TELEM_SendFloat()`, `App_Test_PID()` |
| Etiquetas de **telemetría** | MAYÚSCULAS, **sin acentos** y sin espacios | `>PID_SET:300`, `>VBAT:7.4` |
| Etiquetas del **menú de la OLED** | Sin acentos y **solo con glifos de la fuente** (ver `app/oled.h`) | `"PULSA UN BOTON"`, `"BATERIA"` |
| Nombres de fichero | minúsculas, con la capa como prefijo en los `#include` | `"control/controller.h"` |
| Ficheros y carpetas | Español en la documentación (`Documentacion/`), inglés en el código (`firmware/`) | — |

> ⚠️ La OLED **no tiene glifos con acentos**: `OLED_Print()` dibuja en blanco lo que no esté en su
> tabla. Por eso el menú dice `BATERIA` y no `BATERÍA`. Si hace falta un carácter, se añade a
> `oled_chars[]`/`oled_font[]` (5 bytes) y la comprobación de tamaño avisa si descuadra.

---

## 1. Micromouse y navegación

| Término | Significado | Uso en este proyecto |
|:--------|:------------|:---------------------|
| **micromouse** | Robot autónomo que resuelve un laberinto; también la competición | El robot es un *micromouse* (OSHWDem) |
| **maze** | Laberinto | `Documentacion/` habla de «laberinto 4×4»; el término inglés se usa poco |
| **cell** | Celda del laberinto (aquí de **18 cm**) | `avanzar_celda()` mueve una celda |
| **wall follower** | Algoritmo que recorre el laberinto pegado a una pared (aquí, *mano derecha*) | Submenú **EXPLORACION → MANO DERECHA** |
| **flood fill** | Relleno por inundación: calcula distancias al centro sobre el mapa para resolver el laberinto | Submenú **EXPLORACION → FLOOD FILL** |
| **run** | Una pasada de exploración o de resolución | Estado `APP_STATE_RUN`; se entra desde **EXPLORACION** o **RESOLUCION** |
| **dead reckoning** | Navegación por integración de movimientos, sin referencia externa | Equivale a **odometría**, que es el término que se usa aquí (`filter/odometry.c`) |

---

## 2. Control y regulación

| Término | Significado | Equivalente en español | Uso en este proyecto |
|:--------|:------------|:-----------------------|:---------------------|
| **PID** | Regulador proporcional-integral-derivativo | regulador PID | Lazo de velocidad por rueda (`control/pid.c`) |
| **Kp · Ki · Kd** | Ganancias proporcional, integral y derivativa | — | `PID_TEST_KP/KI/KD`; en %/(mm/s)… |
| **setpoint** | Valor deseado de la variable controlada | **consigna** | `control/setpoint.c` genera la consigna con rampa |
| **duty** / **duty cycle** | Fracción del periodo en que la señal PWM está activa | **ciclo de trabajo** | Duty del PWM en %, −100…100 (`DRV8833_Set_Speed`) |
| **overshoot** | Sobreimpulso: cuánto se pasa la respuesta del valor deseado | sobreimpulso | Puerta de CONTROL: **sin sobreimpulso > 20 %** |
| **steady-state error** | Error que queda cuando el transitorio ya ha pasado | error en régimen permanente | Criterio: < 2 % |
| **step** | Escalón: cambio brusco de la consigna | escalón | Modo 1 de la prueba 18 (para sintonizar) |
| **loop** | Lazo de control cerrado | lazo | «lazo de velocidad», «lazo lateral» |
| **anti-windup** | Recorte del término integral cuando la salida se satura, para que no se «cargue» | anti-saturación | Implementado en `PID_Update()` |
| **derivative kick** | Pico de derivada al cambiar la consigna, si se deriva el error | — | Evitado derivando **sobre la medida** |
| **feed-forward** | Término que se suma «a ciegas» según la consigna, sin esperar al error | prealimentación | Pendiente (ver `control-pid.md` §6) |
| **plant** | El sistema físico que se controla (motor + rueda + robot) | planta | El banco de pruebas del PC simula una planta de 1er orden |
| **Ziegler-Nichols** | Método clásico de sintonía a partir de la oscilación crítica (Ku, Tu) | — | **Descartado** para este robot (1-oct): la planta es de primer orden y no oscila · §4 de `control-pid.md` |
| **dt** | Intervalo entre dos pasos del lazo | — | `CTRL_Update(..., dt)` en segundos |
| **saturation** | Que la salida toca su tope | saturación | Duty topado por `PID_TEST_DUTY_MAX` |

---

## 3. Puesta en marcha y pruebas

| Término | Significado | Equivalente en español | Uso en este proyecto |
|:--------|:------------|:-----------------------|:---------------------|
| **bring-up** | Puesta en marcha de un periférico o módulo, paso a paso y aislado | puesta en marcha | `Documentacion/pruebas-bringup.md`, catálogo de **18 pruebas** |
| **bench** / **bench test** | Prueba de banco | banco de pruebas | «banco de pruebas» = las `App_Test_*` |
| **test** | Prueba | prueba | Se numeran (1…18) y se registran con fecha |
| **telemetry** | Datos que el robot envía para depurar | telemetría | Formato **Teleplot** por USART3, `>ETIQUETA:valor` |
| **Teleplot** | Aplicación que dibuja trazas a partir de esas líneas | — | Ver `entorno-desarrollo.md` |
| **gate** | Puerta de calidad: criterio medible para pasar de fase | puerta de calidad | Ver `pruebas-bringup.md` §2 y la skill |
| **snapshot** | Foto del estado en un momento dado | — | `estado-del-proyecto.md` es un *snapshot* |

---

## 4. Software y firmware

| Término | Significado | Equivalente en español | Uso en este proyecto |
|:--------|:------------|:-----------------------|:---------------------|
| **driver** | Módulo que maneja un periférico o chip | controlador | `hal/drv8833.c`, `sensor/imu.c` |
| **HAL** | *Hardware Abstraction Layer*: capa de abstracción del MCU | capa de abstracción de hardware | Carpeta `firmware/hal/` y las funciones `HAL_*` de ST |
| **callback** | Función que el sistema llama cuando ocurre un evento | — | `HAL_GPIO_EXTI_Callback()` en `hal/gpio.c` |
| **ISR** / **IRQ** | Rutina de servicio de interrupción / petición de interrupción | — | `EXTI15_10_IRQHandler()`; el botón MENU va por IRQ |
| **polling** | Preguntar el estado en bucle, en vez de esperar interrupción | sondeo | `ADC_ReadPin()`, `BUTTONS_Update()` |
| **debounce** | Filtrar los rebotes del contacto mecánico | antirrebote | 30 ms en `buttons.c`; 250 ms en la vía de la interrupción |
| **overflow** | Desbordamiento de un contador | desbordamiento | Contadores de 16 bits de los encoders; `max_likelihood_counter_diff()` |
| **framebuffer** | Memoria intermedia con la imagen a mostrar | — | 512 B en `app/oled.c` |
| **buffer / chunk** | Zona de memoria temporal / trozo de datos | — | El framebuffer se envía a la OLED en *chunks* de 32 B |
| **toolchain** | Conjunto de herramientas de compilación | — | `arm-none-eabi-gcc` (PlatformIO) |
| **build** | Compilación | compilación | `pio run -d firmware` |
| **flash** (verbo: *flashear*) | Grabar el firmware en el MCU | grabar | `pio run -t upload` por ST-Link |
| **boot** / **reset** | Arranque / reinicio | arranque / reinicio | `APP_STATE_BOOT`; `ENC_ResetCount1()` |
| **flag** | Variable que indica que algo ha ocurrido | bandera | Contador de pulsaciones del botón MENU |
| **hook** | Punto donde se puede enganchar código | — | `HAL_GPIO_EXTI_Callback()` es el *hook* del HAL |
| **stash** | Reserva temporal de Git | — | `stash@{0}` es **obsoleto** (ver `pruebas-pendientes.md` §8) |
| **refactor** | Reorganizar código sin cambiar lo que hace | refactorizar | La reorganización por capas de julio |
| **scale** (en gráficas) | Escala de los ejes | escala | El menú dibuja la OLED a *escala* 1 |

---

## 5. Electrónica y potencia

| Término | Significado | Equivalente en español | Uso en este proyecto |
|:--------|:------------|:-----------------------|:---------------------|
| **buck** | Convertidor conmutado que **baja** tensión | reductor | MP1584 5 V (batería → Blue Pill, OLED, encoders) |
| **boost** | Convertidor conmutado que **sube** tensión | elevador | MT3608 9 V para el `VM` del DRV8833 |
| **buck-boost** | Puede subir y bajar | — | Candidato para la PCB V2 (el buck puro corta a ≈6 V) |
| **LDO** | Regulador lineal de baja caída | regulador lineal | AMS1117 (en este robot **no regula**: es soporte del display) |
| **Iq** | Corriente de reposo del convertidor | — | Criterio para elegir el buck de la V2 (µA) |
| **NRND** | *Not Recommended for New Designs*: el fabricante desaconseja usarlo en diseños nuevos | — | El **MP1584** está NRND → MP2338 en la V2 |
| **pull-up / pull-down** | Resistencia que fija el nivel en reposo de una línea | — | Botones (internos), bus I2C (necesita **externos** en el F1) |
| **open-drain** / **colector abierto** | Salida que solo tira a GND; necesita pull-up | — | Salidas `C1`/`C2` de los encoders; I2C |
| **short brake** | Freno cortocircuitando el motor (ambas entradas a 1) | frenada en seco | `DRV8833_Brake()` |
| **coast** | Dejar el motor libre (ambas entradas a 0) | rueda libre | `DRV8833_Coast()` |
| **datasheet** | Hoja de datos del componente | hoja de datos | Carpeta `Datasheets/` (21 PDF) + índice |
| **breadboard / protoboard** | Placa de prototipos sin soldadura / con soldadura | — | Se descartó la *breadboard*: placa de prototipos |
| **LSB** | Bit menos significativo; equivale a un paso del ADC | — | 1 LSB ≈ 0.8 mV en el ADC (VREF 3.3 V / 4096) |
| **Vpp** | Tensión de pico a pico | — | Nivel de salida de los encoders medido: ≈3.6 Vpp |

---

## 6. Microcontrolador y buses

| Término | Significado | Uso en este proyecto |
|:--------|:------------|:---------------------|
| **GPIO** | Pin de propósito general | Pines de botones, emisores IR, LED |
| **ADC** | Convertidor analógico-digital | ADC1, 6 canales (4 paredes + 2 batería), 12 MHz, 239.5 ciclos |
| **PWM** | Modulación por ancho de pulso | TIM1 CH1…CH4 → DRV8833 (motores); futuro PWM de los emisores IR |
| **UART / USART** | Puerto serie asíncrono | USART3 (PB10/PB11) a 115200 para telemetría |
| **SPI** | Bus serie síncrono | SPI2 para la IMU (modo 0, /64) |
| **I2C** | Bus serie de dos hilos | I2C1 (PB6/PB7) para la OLED |
| **CS / SS** | *Chip Select* / *Slave Select*: selecciona el esclavo | `SPI2_CS_Low()` (PB12) |
| **MISO / MOSI / SCK** | Líneas del SPI (entrada, salida, reloj) | PB14 / PB15 / PB13 |
| **EXTI** | Interrupción externa por pin | Botón MENU (PC14), línea 14 |
| **NVIC** | Controlador de interrupciones del Cortex-M | `HAL_NVIC_EnableIRQ(EXTI15_10_IRQn)` |
| **DMA** | Acceso directo a memoria (transferencias sin CPU) | Pendiente para el ADC |
| **TIM** | Temporizador | TIM1 (PWM), TIM2/TIM3 (encoders) |
| **prescaler / autoreload** | Divisor de reloj / valor de recarga del temporizador | PWM de 1 kHz; TELEM: `Period` 1000, prescaler 71 |
| **TI12** | Modo *encoder* del timer que cuenta ambos canales (×4) | `TIM_ENCODERMODE_TI12` en TI2/TIM3 |
| **SysTick** | Temporizador del sistema (base de tiempo de la HAL) | Incrementa el *tick* de 1 ms (`HAL_GetTick`) |
| **tick** | Pulso de reloj; unidad de tiempo del sistema o de un encoder | «500 ticks de encoder = 1 s» en la prueba 9 |
| **JTAG / SWD** | Interfaces de depuración | **SWD solo**; JTAG desactivado para liberar PB3/PB4 |
| **watchdog** | Perro guardián: reinicia el MCU si el programa se cuelga | **No usado todavía** (candidato) |

---

## 7. Magnitudes y unidades del robot

| Término | Significado | Valor / uso aquí |
|:--------|:------------|:-----------------|
| **PPR** | *Pulses Per Revolution*: pulsos por vuelta del eje del motor | **7 PPR** (fabricante), por canal |
| **cuentas** (*counts*) | Pulsos tras la decodificación ×4 | **280 cuentas** por vuelta de rueda |
| **µm/cuenta** | Avance por cada cuenta | **287,0 µm**, medido el 1-oct (nominal 269,3 con rueda Ø24) |
| **ticks** | Cuentas del contador del encoder | Prueba 9: 1500 ticks por rampa (≈3 s) |
| **mm/s, m/s²** | Unidades de velocidad y aceleración usadas en CONTROL | Consigna de 300 mm/s; rampa de 600 mm/s² |
| **%** | Unidad del duty (y de la consigna de la prueba 9) | −100…100 % |
| **mAh / C** | Capacidad / tasa de descarga de la batería | LiPo 2S 450 mAh **80C** |
| **V / mV** | Tensión | Pack 2S: 8.4 V lleno, 6.6 V crítico |
| **lx** | Lux: iluminación (sensibilidad de los fototransistores) | ST-1KL3A: 5 mA a 200 lx |

---

## 8. Frases hechas del proyecto

| Frase | Qué quiere decir |
|:------|:-----------------|
| «**un periférico por prueba**» | Cada `App_Test_*` valida **una** cosa: si falla, el culpable es único |
| «**nada entra en el lazo de control sin prueba**» | Ningún sensor/módulo se usa en la misión antes de pasar su prueba de banco |
| «**la puerta de calidad 10/10**» | Criterio de salida del bring-up: la prueba se repite 10 veces con resultado correcto |
| «**banco de pruebas**» | El conjunto de rutinas `App_Test_*` que se lanzan desde el menú |
| «**raíles/rieles de alimentación**» | Los dos niveles de tensión del robot: 3.3 V (regulador de la Blue Pill) y 5 V (MP1584) |
| «**cabo suelto**» | Dato de hardware sin resolver (se apunta en `hardware.md` §6) |
| «**modo freno**» | `while(1){;}` en `main.c`: deja el robot parado mientras se toca el hardware |
