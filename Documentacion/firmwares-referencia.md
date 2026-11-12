# 🔍 Informe: firmwares de micromouse de referencia

Análisis del **código** (no de los README) de los firmwares de micromouse que merece la pena mirar,
comparado con el firmware de Rabosa-MVR. El objetivo es ver **qué hacen distinto** y **qué merece la
pena adoptar**.

> 📅 Análisis: **2026-10-05** (KERISE, ZoroBot3 y UKMARSBOT) y **2026-10-10**, cuando se añade
> **`ukmars/mazerunner-core`**, que resulta ser **el software de verdad del UKMARSBOT**.
> Repos clonados en `/tmp/refs` (no se han copiado al repo).
> ⚠️ **Licencias**: `micromouse-kerise-*`, `ukmarsbot` y **`mazerunner-core`** son **MIT** (permisivas:
> se puede reutilizar citando); **ZoroBot3 es GPL-3.0**. Desde el **2026-10-30** este proyecto es
> **GPL-3.0-or-later** en software y **CERN-OHL-S-2.0** en hardware (ver `LICENSE` y
> `LICENSE-HARDWARE`), así que reutilizar lo MIT es compatible, y de ZoroBot3 se toman **ideas** —
> su código obligaría a mantener la GPL y atribuir.

## Qué se ha analizado

| Proyecto | Repos | Lenguaje |
|:--|:--|:--|
| **KERISE** (Ryotaro Onuki, Japón) | `kerise-firmware`, `control-module`, `maze-library` | C++17 |
| **ZoroBot3** (OPRobots, España) | `ZoroBot3` (todo dentro) | C |
| **UKMARSBOT** (UK Micromouse Society) | `ukmarsbot` (solo HW), `ukmarsbot-examples`, `ukmarsbot-mazerunner`, **`ukmars/mazerunner-core`** | C++ / Arduino |

⚠️ **`micromouseonline/ukmarsbot` NO llevaba software**: es el hardware (PCB, CAD, docs). El código está
en `ukmarsbot-examples` (11 sketches numerados, pensados como tutorial), en `ukmarsbot-mazerunner` (el
primero que resolvía laberintos) y en **`ukmars/mazerunner-core`**, que es **el bueno**: la versión
mantenida, la que corrió en competición en **octubre de 2023**.

📌 **`mazerunner-core` es el software del UKMARSBOT**, así que se integra en la ficha de abajo en lugar
de ser un cuarto firmware: es el mismo robot, con el código de verdad. Y aporta algo que **ninguno de los
otros tres tiene**: una **auditoría de código propia** en `review/` (2 419 líneas, revisando seguridad de
memoria, errores, *timing* y abstracción de hardware).

---

## 1. Fichas técnicas

| | **Rabosa-MVR** | **KERISE** | **ZoroBot3** | **UKMARSBOT** |
|:--|:--|:--|:--|:--|
| MCU | STM32F103C8T6 @72 MHz | **ESP32** (pico32) | **STM32F405RG** @168 MHz | **ATmega328P** (Nano) |
| Flash / RAM | 64 KB / 20 KB | 4 MB / 520 KB | 1 MB / 192 KB | 32 KB / 2 KB |
| Framework | PlatformIO + **STM32Cube** | PlatformIO + **ESP-IDF** | PlatformIO + **libopencm3** | **Arduino IDE** |
| Lenguaje | **C puro** | **C++17** | C | C++/Arduino |
| RTOS | **No** (super-bucle) | **FreeRTOS** | **No** (super-bucle) | **No**: ISR de control a **500 Hz** + bucle principal |
| Nº de módulos | ~40 | ~25 clases | 30 `.c` | 18 cabeceras (*header-only*) |
| Líneas | ~9 000 (con banco) | ~4 500 + librerías | **8 382** | **5 735** |
| Config multi-robot | defines sueltos | — | — | **3 ejes compilados** (placa → laberinto → robot), ver §2 |
| Licencia | — | MIT | **GPL-3.0** | MIT |
| Succión | No | **Sí** (con rampa propia) | **Sí** | No |
| Simulador | No | **pybind11 + LaTeX** | **MMSIM** (`lib/mmsim_api`) | No |

Nada más ver la tabla se entiende una cosa: **los tres son robots de otra liga**. Dos de ellos (KERISE y
ZoroBot3) tienen succión y compiten a nivel internacional con varios primeros puestos; UKMARSBOT es
deliberadamente un robot **didáctico** de 8 bits, y aun así es más capaz que el nuestro en navegación.

⚠️ **Y fíjate en la última columna**: hace todo eso —perfiles trapezoidales, PD de posición con tres
términos de *feedforward*, control de dirección por pared, mapa 16×16 y *flood fill*— **en 2 KB de RAM**.
Nosotros tenemos **diez veces más** y no llegamos. La diferencia no es el microcontrolador: es el diseño.

---

## 2. Arquitectura

### Rabosa-MVR (lo nuestro)

Cinco capas estrictas `APP → CONTROL → FILTER → SENSOR → HAL → HW`, dependencia en un solo sentido,
`C` sin asignación dinámica ni RTOS, y **super-bucle de 1 kHz**: `BUTTONS_Update()`,
`BATTERY_Update()`, `CONSOLE_Update()` y la FSM de la APP. La aplicación principal y el banco de pruebas
conviven en el mismo binario y se eligen por menú.

### KERISE — orientado a objetos, con RTOS y librerías externas

```
src/
├── app/          app_mouse.cpp (20 líneas) · app_devkit.cpp (50)  ← una app por objetivo
├── agents/       maze_robot.h (484) · move_action.h (1094)        ← navegación y movimiento
├── machine/      machine.h                                        ← el robot entero
├── drivers/      as5048a · ma730 · icm20602 · vl6180x             ← un fichero por chip
├── peripheral/   esp · spiffs · ui · buzzer · fan · led
├── freertospp/   envoltorio C++ de FreeRTOS
└── config/       model.h · slalom_shapes.h
```

Lo interesante es **dónde vive la lógica**: las `app_*.cpp` son de **20 y 50 líneas** porque todo el
trabajo está en clases (`MoveAction`, `MazeRobot`, `Machine`). Y el movimiento se apoya en **tres
librerías externas** que se compilan aparte:

- `micromouse-control-module` → `include/ctrl/` con `trajectory_tracker.h`, `feedback_controller.h`,
  `accel_designer.h`, `accel_curve.h`, `pose.h`, `state.h`, `polar.h` y subcarpetas **`slalom/`** y
  **`straight/`**. Tiene **bindings de Python (pybind11)**, ejemplos y documentación en LaTeX ⇒ **se
  puede diseñar y simular una trayectoria en el PC antes de flashearla**.
- `micromouse-maze-library` → **dos ficheros**: `Maze.h` y `StepMap.h`. Es C++ de PC (`<fstream>`,
  `<iostream>`, colores ANSI, `std::chrono` para medir tiempos) ⇒ **el laberinto se prueba en el
  ordenador**, no en el robot.

### ZoroBot3 — plano, un `.c` por responsabilidad

```
source_code/
├── main.c        sólo el bucle principal y el reparto de algoritmos
├── setup.c       inicialización          control.c   los lazos (622 líneas)
├── move.c        movimientos (1996)      sensors.c   sensores (550)
├── floodfill.c   flood fill (1964)       handwall.c  wall follower
├── maze.c · macroarray.c · floodfill_weigths.c
├── menu.c · menu_configs.c · menu_run.c · timetrial.c
└── leds.c · buttons.c · usart.c · eeprom.c · battery.c · lsm6dsr.c · rc5.c · debug.c
```

**Sin capas formales** (todo es plano), pero **cada fichero tiene una responsabilidad clara** y hay una
separación que nosotros no tenemos: **`menu.c` (interfaz), `menu_configs.c` (parámetros) y `menu_run.c`
(la carrera) son tres ficheros distintos**. Y `eeprom.c` para guardar calibraciones.

El reparto de tareas periódicas es un **tick** del sistema, no un bucle con `HAL_Delay`:

```c
void sys_tick_handler(void) {
  clock_tick();  update_encoder_readings();  update_sensors_magics();
  update_battery_voltage();  check_leds_while();  check_buttons();  lsm6dsr_update();
}
```

### UKMARSBOT — la separación más parecida a la nuestra

```
mazerunner-core/                        18 ficheros, 5 735 lineas, TODO en cabeceras
├── config.h                            elige la variante; TurnParameters; PERSISTENT
├── config-ukmarsbot.h                  HARDWARE: pines y canales ADC
├── config-robot-orion.h                ROBOT: geometria, calibracion y modelo del motor
├── config-robot-osmium.h               ROBOT: otro
├── mouse.h                             la mision: explorar, correr, calibrar, arranque por gesto
├── maze.h                              mapa 16x16 y flood fill
├── motion.h  profile.h                 perfiles trapezoidales (avance Y giro, misma clase)
├── sensors.h  adc.h                    sensores y secuenciador de doble muestreo
├── motors.h  encoders.h                motores (PD + feedforward) y encoders
├── cli.h  reporting.h  switches.h      consola, telemetria, interruptores por ADC
└── systick.h  queue.h  battery.h       tick de 500 Hz, cola sin heap, bateria
```

Fíjate: **`cli` + `reporting` + `mouse` + los `config`** — es el mismo reparto que nuestro `console.c`,
`telemetry.c`, `application.c` y los defines. **Su arquitectura de módulos es la misma idea que la
nuestra**, con dos diferencias: ellos tienen `profile.h` (que a nosotros nos falta por completo) y
**todo el código vive en cabeceras incluidas desde un único `.ino`**.

### UKMARSBOT — **tres ejes de configuración** en la misma compilación ⭐

Esto responde directamente a la pregunta de **cómo versionar el robot**, y está resuelto en **144 líneas**
(`config.h`). No hay ramas ni repos por versión: hay **tres `#define` y tres carpetas de cabeceras**.

| Eje | Elige | Fichero | Qué cambia |
|:--|:--|:--|:--|
| **`HARDWARE`** | la **placa** | `config-ukmarsbot.h` | **pines y canales del ADC**; si no coincide, `#error "NO HARDWARE DEFINED"` |
| **`EVENT`** | el **laberinto** | dentro del robot | **calibración de sensores por recinto** y dónde está la meta |
| **`ROBOT`** | la **variante física** | `config-robot-orion.h` | geometría, reductora, **polaridad** de motores y encoders, modelo del motor |

```cpp
#define HARDWARE HARDWARE_UKMARSBOT_1_3A     // pines y canales
#define EVENT    EVENT_HOME                  // calibracion por laberinto
#define ROBOT    ROBOT_ORION                 // geometria y modelo del motor
```

Y el eje **`EVENT`** no es teoría: en `config-robot-orion.h` hay **cuatro juegos completos de
calibración** (HOME, UK, PORTUGAL, APEC) del **mismo robot**, para cuatro recintos distintos. Cada juego
lleva sus `LEFT_CALIBRATION`, `RIGHT_CALIBRATION`, `FRONT_LINEAR_CONSTANT`, `TURN_THRESHOLD_SS90E` y
`EXTRA_WALL_ADJUST`.

📌 **Esto es exactamente lo que necesitamos nosotros**, y ya nos ha mordido: nuestra prueba 21 existe
porque «el laberinto del sábado es ajeno, y si sus paredes no reflejan como las de casa, el umbral de
casa no vale». Ellos lo tienen resuelto **compilando otro juego de números**, en vez de reajustar a ojo
sobre el robot con el cable puesto.

📌 Y el eje **`ROBOT`** lleva **dos µm/cuenta distintos**: `MM_PER_COUNT_LEFT = 500.0/1918` y
`MM_PER_COUNT_RIGHT = 500.0/1927`, medidos sobre 500 mm. **Por rueda**, no uno común — que es justo lo
que propone el bloque 1 de [puesta-a-punto.md](./puesta-a-punto.md).

📌 Y una coincidencia tranquilizadora: su `MOUSE_RADIUS = 38.70` mm ⇒ **trocha efectiva 77,4 mm**,
frente a los **77,6 mm** que medimos nosotros con la cinta en el suelo. Dos robots distintos del mismo
tamaño convergen al mismo número, así que el nuestro está bien medido.

---

## 3. El control de movimiento: aquí está la diferencia gorda

Los tres atacan el movimiento de tres formas distintas, y **ninguna se parece a la nuestra**.

### Rabosa-MVR (lo nuestro)

`MOTION_Avanzar(mm)` y `MOTION_Girar(grados)` **bloqueantes**: piden una distancia o un ángulo, y
dentro de un bucle de 10 ms calculan un error, un **PID sobre la velocidad** de cada rueda y un lazo
externo sobre el giroscopio. El avance tiene `MOTION_AVANCE_VEL_MIN_MM_S` (50 mm/s) y el giro tiene
`MOTION_GIRO_VEL_MIN_RODANDO_MM_S` (35) porque **por debajo de esas velocidades el motor no vence la
fricción estática y la rueda se queda clavada**.

### UKMARSBOT — control por **posición** con *profiler* ⭐

Su README lo dice sin rodeos:

> *"The previous code used an underlying velocity control model and it was not proving stable. This code
> has a position control model for the motors."*

El mecanismo:

1. Un **profiler** genera, en **cada tick**, la posición que la rueda *debería* tener. A 1000 mm/s y
   1000 Hz son 2 mm por tick. Forward y rotación tienen **su propio profiler y su propio controlador**.
2. El controlador compara esa posición con la de los encoders y aplica **un PD, sin integral**:
   *"there is no need for integral terms. Because of the simplicity of the technique there is no need for
   a full-blown PID controller class and the control code is just a couple of lines."*
3. Las salidas se mezclan (avance + rotación) y se aplican a cada motor.

> ⭐ **Y esto explica el problema que nos costó la noche del 2 de octubre.** Nuestro mínimo de arranque
> existe porque con **control de velocidad** hay que vencer la fricción estática con un término integral
> (o con un mínimo artificial), y ahí nacen el rebote y el atasco. **Con control de posición no hace
> falta nada de eso**: si la rueda no se mueve, el error de posición crece sin límite y el propio término
> P la arranca. No es un ajuste mejor, es **el problema deja de existir**.

⚠️ **No hay ningún "avanza 180 mm"** en su código. El robot no va a ciegas en ningún momento: la
consigna de posición se recalcula cada milisegundo y, si algo va mal, el error lo dice enseguida.

#### Y el detalle que lo hace funcionar: *feedforward* del **modelo**, no del PID

`mazerunner-core` no confía al integrador la tarea de vencer la fricción. Antes del PID **calcula la
tensión que el motor necesita** para la velocidad y la aceleración pedidas, a partir del **modelo del
motor medido**:

```cpp
// config-robot-orion.h — el modelo, medido y guardado como constantes
const float FWD_KM   = 475.0;      // mm/s por voltio    (la K del motor)
const float FWD_TM   = 0.190;      // constante de tiempo (s)
const float SPEED_FF = (1.0 / FWD_KM);
const float ACC_FF   = (FWD_TM / FWD_KM);
```

```cpp
// motors.h — tres terminos, POR RUEDA
float leftFeedForward(float speed) {
  float leftFF = speed * SPEED_FF;           // 1) velocidad
  if (speed > 0)      leftFF += BIAS_FF;     // 2) friccion estatica, CON SIGNO
  else if (speed < 0) leftFF -= BIAS_FF;
  float acc = (speed - m_old_left_speed) * LOOP_FREQUENCY;
  m_old_left_speed = speed;
  return leftFF + ACC_FF * acc;              // 3) aceleracion
}
```

Tres cosas que conviene copiar tal cual:

1. **El *bias* de fricción lleva signo.** No es una constante que siempre suma: suma rodando hacia
   adelante y resta hacia atrás. Un mínimo de velocidad **sin signo**, como el nuestro, es una fuente de
   asimetría entre el avance y el retroceso.
2. **Hay *feedforward* de aceleración.** Es lo que permite perfilar sin que el controlador vaya siempre
   por detrás; sin él, cada tramo de aceleración es un error de régimen.
3. **Es por rueda**, igual que los µm/cuenta. El comentario lo dice: *«The drive train is not symmetric
   and there is significant stiction»*.

> ⚠️ **Y una matización honesta de la propia fuente**: *«If used with PID, a simpler, single value will be
> sufficient»*. Es decir: si se cierra el lazo con un integrador (**como hacemos nosotros**), el *bias*
> de fricción puede ser **un solo número**; lo que no se puede quitar es la parte de **velocidad y
> aceleración**, porque son las que le quitan trabajo al PID y las que hacen que se pueda sintonizar.

Y el detalle **más elegante del repo**: el control trabaja **en voltios, no en *duty***
(`MAX_MOTOR_VOLTS = 6.0`). El *duty* sale de dividir la tensión pedida entre **la tensión de batería
medida**. Con eso la compensación de batería no es un extra que se añade: **está dentro del control por
construcción**.

### ZoroBot3 — velocidad + **seis lazos** y un perfil de giro

`control.c` mantiene **seis errores con su integral y su derivada** (son PIDs completos, con sus
`kp/ki/kd` en dos juegos, uno para explorar y otro para correr):

| Lazo | `kp` | `ki` | `kd` | Para qué |
|:--|:--:|:--:|:--:|:--|
| `KPI_LINEAR` | 0,008 | 0,0005 | 0 | velocidad de avance |
| `KPI_ANGULAR` | 0,5 | 0,015 | 0 | ángulo |
| `KPI_SIDE_SENSORS` | 0,03 | 0,002 | 0 | **posición lateral con los sensores de lado** |
| `KPI_FRONT_ANGLE` | 0,02 | 0 | 0,12 | ángulo con los frontales |
| `KPI_FRONT_DISTANCE` | 0,05 | 0,0002 | 0,25 | distancia a la pared de enfrente |
| `KPI_FRONT_DIAGONAL` | 0,001 | 0,04 | 0,04 | corrección en diagonal |

Y el movimiento no es "gira 90°": es una **lista de 27 movimientos** con parámetros de perfil:

```c
static struct inplace_params turns_inplace[] = {
    [MOVE_LEFT_INPLACE] = { .radians = PI/2, .angular_accel = 350,
                            .max_angular_speed = 10, .sign = -1 },
    ...
};
```

Es decir, **el giro es un perfil de velocidad angular con aceleración limitada**, no un lazo de posición
que corta al llegar. Hay movimientos `MOVE_DIAGONAL`, `MOVE_LEFT_TO_45`, `MOVE_FROM_45_180`… ⇒ **compiten
con giros de 45° y diagonales**. Y compensan el PWM con la tensión de batería:

```c
static int32_t voltage_to_motor_pwm(float voltage) {
  return voltage / get_battery_voltage() * MOTORES_MAX_PWM;
}
```

### KERISE — trayectorias pregrabadas + *tracker*

`ctrl/slalom/` y `ctrl/straight/` contienen **formas de trayectoria** (`config/slalom_shapes.h`), y un
`trajectory_tracker.h` las sigue. Los parámetros de marcha:

```c
float v_max = 720;      // mm/s
float a_max = 3600;     // mm/s²
float j_max = 240000;   // mm/s³   ← jerk limitado: perfil en S de verdad
float v_search = 330;   // mm/s en exploración
float fan_duty = 0.3f;  // succión
```

**`j_max`**: nosotros tenemos un documento entero (`cinemáticas.md`) hablando de perfiles trapezoidales
y en S; ellos lo tienen **implementado y con el jerk limitado a 240 000 mm/s³**.

Y hay tres tipos de tarea: `TaskActionSearchRun` (explorar), `TaskActionFastRun` (la vuelta rápida) y
`TaskActionPositionRecovery` ⇒ **la recuperación de posición es una tarea de primera clase**.

---

## 4. Los sensores de pared

Los tres tienen **cuatro** canales: dos frontales y dos laterales. Nosotros también (R0/R3 frontales,
R1/R2 laterales) — pero **los nuestros no funcionan** y los suyos sí.

Un dato importante: **ZoroBot3 usa exactamente los mismos componentes que nosotros**
(SFH-4550 como emisor y ST-1KL3A como receptor, según su README). O sea que **el hardware da para
esto**; lo que falla es la puesta a punto.

Lo que hacen distinto:

### Convierten a **distancia**, no comparan con un umbral

```c
struct sensors_distance_calibration { float a, b, c; };
int16_t sensors_distance_offset[NUM_SENSORES];
const float ln_lookup[...];   // tabla de logaritmos para linealizar
```

Tienen una **calibración no lineal por sensor** (`a`, `b`, `c`) más una **tabla de logaritmos** para
linealizar, y **offsets individuales**. Del ADC pasan a **milímetros**. Y la calibración **se guarda por
robot y se carga de la EEPROM** (`set_sensors_robot_calibration(ZOROBOT3_A/B/C)`, `eeprom_load()`)
porque cada unidad tiene sus valores.

> ⭐ Nosotros, en cambio, comparamos la resta `ON − OFF` contra un umbral único por pared. Es más simple y
> **funciona** (el frontal da 0 sin pared y 179 con ella), pero no da distancia, así que no se puede
> corregir la posición con las paredes: solo se puede decir «hay / no hay».

### Miden con emisor apagado **y** encendido

```c
static volatile uint16_t sensors_off[NUM_SENSORES];
static volatile uint16_t sensors_on[NUM_SENSORES];
```

**Lo mismo que hacemos nosotros** (`ON − OFF`). ✅ Aquí vamos bien encaminados: es la técnica que
cancela la luz ambiente y la usan los tres.

### La ventana de muestreo (UKMARSBOT)

Su README tiene un párrafo que es **exactamente el problema que vimos el sábado**:

> *"Detecting a front wall reliably can be surprisingly hard… the front sensor only gives a normalised
> reading of 20-30 when the wheels are 20 mm short of the cell boundary and there is a wall ahead. At
> that point, with a detection threshold of only 20, it is possible to miss a wall… **For best
> reliability, sample as the robot crosses the cell boundary.** Just those few mm of travel can mean
> double the returned signal."*

Y normalizan las lecturas a **100** en una posición conocida (`FRONT_CALIBRATION`, `LEFT_CALIBRATION`,
`RIGHT_CALIBRATION`), de forma que los umbrales se expresan en **porcentaje**: laterales al **50 %** del
valor de referencia, frontal con un umbral bajo (15) por la poca señal.

---

## 5. La navegación y —lo más importante— **la posición**

| | Wall follower | Flood fill | Referencia absoluta de posición |
|:--|:--|:--|:--|
| **Rabosa-MVR** | Sí, `frente → mano → atrás` | ❌ pendiente | **Solo odometría y giroscopio** |
| **ZoroBot3** | `handwall.c` (`start`/`loop`) | **Sí, por pesos** | **Pérdida de pared lateral** (wall edge) |
| **KERISE** | dentro de `maze_robot.h` | Sí, `StepMap.h` | **壁切れ** (corte de pared), opcional |
| **UKMARSBOT** | Sí | Sí | Bordes de pared al cruzar la celda |

### El detalle que explica las ocho celdas «a tropezones»

**ZoroBot3 hace un *reset* de posición cuando pierde una pared lateral.** Lo dice su README:
*"Se realiza un reseteo de posición por pérdida de pared lateral para mejorar la navegación."*
KERISE tiene lo mismo y lo llama **壁切れ** (`side_wall_cut_enabled`).

Es un concepto que nosotros **no tenemos en ninguna parte**: cuando el robot va por un pasillo pegado a
una pared con el sensor lateral, en el momento en que la pared **termina** (el borde de un poste), la
lectura lateral cae de golpe — y ese instante marca **una coordenada de celda exacta**. Es una
**referencia absoluta** que no arrastra error, al contrario que la odometría.

> ⭐ Con odometría sola, cada celda suma su error y a las ocho celdas el robot va torcido y desplazado:
> eso es exactamente lo que se vio. **Los tres proyectos usan el borde de la pared como referencia**;
> nosotros solo llevamos la cuenta de las ruedas.

Y ZoroBot3 va más allá: como tiene los **cuatro** sensores buenos, usa los laterales para corregir
**posición y ángulo**, y los frontales para la **distancia a la pared de enfrente** — con tres PIDs
dedicados (`KPI_SIDE_SENSORS`, `KPI_FRONT_ANGLE`, `KPI_FRONT_DISTANCE`).

### El arranque de la carrera

Los dos competidores arrancan **quitando la mano del sensor frontal**: UKMARSBOT lo describe en el README
(«place your hand just in front of the front sensor… when you take it away again, the test will run») y
ZoroBot3 lo implementa con `check_start_run()` y `SENSOR_FRONT_LEFT_WALL_ID`. Nosotros dependemos de los
**botones**, y dentro del laberinto eso está bien, pero es un dato: ellos reutilizan un sensor que ya
tienen.

---

### El mapa: 16×16, `flood fill` en 7 ms y **sobrevive al reset**

Detalles de `maze.h` (556 líneas) que nos van a hacer falta en el 6×6:

- **Un byte por celda** con un bit por pared (N=0, E=1, S=2, W=3), en un **array de una dimensión**
  (celda 0 = esquina suroeste). Como la pared Norte de una celda *es* la Sur de la vecina, **el dato se
  guarda duplicado** y hay que actualizar las dos al escribir.
- **El mapa vive en RAM que no se borra al resetear** (`__attribute__((section(".noinit")))`): se puede
  explorar, chocar, pulsar *reset* —o conectar el cable serie, que en un Arduino también reinicia— y
  **seguir donde estabas**. La disciplina obligatoria está escrita en mayúsculas en el propio código:
  ***«CLEAR THE MAZE BEFORE EVERY CONTEST»*** (se limpia manteniendo pulsado el botón al resetear).
- **El `flood fill` completo tarda ~7 ms** en un ATmega328 ⇒ se puede repetir **en cada celda** durante la
  exploración e ir eligiendo siempre el mejor camino, en vez de explorar a ciegas.
- ⚠️ La cola del BFS está acotada a **64 posiciones** y marcada como *«unproven»*, y **su desbordamiento
  es silencioso**: el propio autor lo señala. Un `Queue` sin comprobación de desborde da mapas de coste
  corruptos sin avisar.
- La meta se define con **un `#define` y el `EVENT`** (`Location(1,0)` en casa, `Location(7,7)` en
  concurso), con un aviso que vale su peso en oro: *«más de un concursante se ha sorprendido al descubrir
  que su robot buscaba y corría rápidamente hacia un sitio que no era la meta»*.

### La corrección lateral: el error se disfraza de giro ⭐

Y aquí está **la idea más barata y más potente del repo**, que contesta a «cómo me centro en el pasillo
sin un lazo nuevo». `documents/steering.md` lo explica en tres pasos:

1. **Los sensores no miden distancia**: miden luz reflejada, con respuesta muy **no lineal**. Se
   **normalizan** por calibración para que den **100** cuando el robot está centrado entre dos paredes
   — así el controlador siempre ve un rango conocido, aunque los sensores sean más o menos sensibles.
2. El error es el **CTE** (*cross-track error*): `cte = right_sensor − left_sensor`. Si falta una pared,
   se usa la que hay **y se dobla**, porque solo una está contribuyendo.
3. **Y ese error se convierte en un error ANGULAR que se suma al ángulo del robot.** No hay un lazo de
   posición lateral: se le dice al controlador de rumbo «vas torcido» y él corrige con lo que ya sabe
   hacer.

```cpp
// sensors.h
float pTerm = STEERING_KP * m_cross_track_error;
float dTerm = STEERING_KD * (m_cross_track_error - m_last_steering_error);
float adjustment = pTerm + dTerm * LOOP_FREQUENCY;
adjustment = constrain(adjustment, -STEERING_ADJUST_LIMIT, STEERING_ADJUST_LIMIT);
```

📌 **Para nosotros son tres líneas en `motion.c`**: ya tenemos un lazo que corrige el rumbo con el
giroscopio (`MOTION_GYRO_KP`). Bastaría **sumarle** `KP·CTE` (en grados) con un tope, y el robot
**empezaría a centrarse en el pasillo** — que es exactamente la «referencia absoluta» que nos falta. El
propio docstring avisa de que **el término P puede bastar** y de que el tope es necesario.

⚠️ **Y avisan del modo de fallo que hay que conocer**: *«al barrer un borde de pared, la respuesta cae de
golpe y los sensores dan la impresión de que el robot se está yendo de la pared; el controlador puede
entonces intentar **seguir el borde** y desviarse del camino»*. Es un fallo real, y **nuestro *wall
follower* barre bordes de poste en cada celda**. Se combate con el **tope** del ajuste y con una sintonía
no agresiva: *«aim for a robot that will correct modest errors within **1 to 2 cells** of travel»*.

⚠️⚠️ **Pero ojo: la mejora necesita los laterales**, que hoy no funcionan. La idea es barata, y está
**bloqueada por el bloque 2** de [puesta-a-punto.md](./puesta-a-punto.md) igual que todo lo demás. Lo que
cambia es que ahora sabemos **qué** hacer con ellos en cuanto funcionen, y que **no hace falta un lazo
nuevo**.

---

## 6. Herramientas: el banco y el simulador

| | Banco de pruebas | Simulador | Guardar calibración |
|:--|:--|:--|:--|
| **Rabosa-MVR** | **24 pruebas en menú OLED** ✅ + telemetría Teleplot + `TRACE` en RAM | ❌ | ❌ (todo en RAM) |
| **ZoroBot3** | menú + `debug.c` | **MMSIM** (`lib/mmsim_api`, `.gitignore` lo integra) | **EEPROM** |
| **KERISE** | menú con 16 modos + `app_devkit` | **pybind11** (control) y maze-library en el PC | **SPIFFS** |
| **UKMARSBOT** | `tests.cpp` + `reports.cpp`, elegidos con **dip switches** | ❌ | `config.h` recompilando |

**Aquí no vamos por detrás**: nuestro banco de pruebas con menú OLED, telemetría numérica y traza en RAM
está **a la altura** de cualquiera de los tres, y la traza en RAM para depurar **sin cable** es una
solución buena. Lo que no tenemos es **simulador** ni **memoria no volátil para los ajustes**.

---

## 7. Resumen: en qué estamos a la par y en qué no

### ✅ Estamos bien (o mejor)

- **Arquitectura por capas explícita.** Ellos tienen módulos; nosotros tenemos una **regla de dependencia
  escrita y verificable**. `Layered Architecture.md` no existe en ninguno de los tres.
- **Banco de pruebas y telemetría.** A la altura, y la traza en RAM sin cable es nuestra.
- **Cerrar el giro con el giroscopio.** Lo hacemos y está medido (dispersión 2,1°); ZoroBot3 y KERISE
  usan encoder magnético de alta resolución (AS5145B, AS5048A) y **no necesitan** giroscopio para girar.
- **Medir con emisor ON/OFF** para cancelar la luz ambiente: igual que ellos.
- **Documentación.** Nuestro `Documentacion/` (con historial de decisiones y gotchas medidos) es más
  completo que el de los tres.

### ⚠️ Nos falta, por orden de impacto

| # | Qué | Quién lo tiene | Coste |
|:--:|:--|:--|:--|
| 1 | **Referencia absoluta por borde de pared** (壁切れ) | ZoroBot3 y KERISE | Necesita los laterales |
| 2 | **Laterales que funcionen** (hoy: crosstalk) | los tres | Hardware: apantallar y/o bajar $R_L$ |
| 3 | **Sensores que den distancia**, no hay/no hay | ZoroBot3 (calib log + offsets) | Calibración |
| 4 | **Control por posición con profiler** en vez de velocidad + PID | UKMARSBOT | Reescribir `motion.c` |
| 5 | **Corrección lateral durante el avance** — y **no hace falta un lazo nuevo**: el error de las paredes se suma al error de giro y lo corrige el lazo que ya existe (ver §5) | los tres | Depende de 1 y 2 |
| 6 | **Compensar el PWM con la tensión de batería** | ZoroBot3 | ~5 líneas |
| 7 | **Guardar las calibraciones de forma no volátil** ⚠️ en un F103 **no hay EEPROM** y `SPIFFS` es cosa de ESP32: ver la nota de abajo | ZoroBot3 (EEPROM real) y KERISE (SPIFFS) | **10 líneas** si es RAM `.noinit`; un chip I2C si tiene que sobrevivir al apagón |
| 8 | **Perfil de giro** con aceleración angular limitada | ZoroBot3, KERISE y UKMARSBOT | Reescribir `MOTION_Girar` |
| 8b | ***Feedforward* del modelo** (velocidad + aceleración + *bias* con signo) en vez del mínimo artificial: **quita el rebote de raíz** (ver §3) | UKMARSBOT | Media tarde |
| 9 | **Simulador** | ZoroBot3 y KERISE | Mucho |
| 10 | Giros de 45° y diagonales | ZoroBot3 y KERISE | Solo con 1, 2 y 8 |

> ⚠️ **Nota sobre el punto 7 — guardar las calibraciones. La técnica no se puede copiar tal cual.**
> `SPIFFS` es el sistema de ficheros de **ESP8266/ESP32** (flash externa con VFS) y la `EEPROM` del
> ATmega328 son **1 KB de EEPROM de verdad dentro del chip**. El **STM32F103C8T6 no tiene EEPROM**, así
> que lo que se copia es la **idea**. Las opciones reales en nuestro chip son cuatro, y la decisión ya
> está razonada en [comandos-serie.md](./comandos-serie.md) §1 (*«Por qué RAM y no Flash»*):
>
> | Opción | Capacidad | Sobrevive a | Coste |
> |:--|:--|:--|:--|
> | **RAM en sección `.noinit`** (lo que usa `mazerunner-core`) | la que quieras (sobran ~18 KB) | **reset** — y al *reflashear*, si no se corta la alimentación | ~10 líneas |
> | **Registros de backup** (`BKP_DR1..DR10`) | **20 bytes** | reset; apagón **solo con VBAT respaldado** | ~20 líneas |
> | **Flash interna emulada** | KB | todo | alto: borrado por páginas de 1 KB, **desgaste** (unos 10 000 ciclos) y **la rutina debe correr desde RAM**, porque en el F1 no se puede escribir y leer flash a la vez |
> | **EEPROM por I2C** (24C02/24C32) | KB, ~1 000 000 de ciclos | todo | un chip barato — y **el I2C ya está montado** (la OLED cuelga de él) |
>
> 📌 **Y el dato que decide**: para lo que de verdad nos molesta —**recalibrar después de cada
> flasheo**— basta la primera opción, porque un reflasheo **no corta la alimentación** y la RAM no se
> inicializa si el arranque no la toca. Lo único que hace falta es un **número mágico** al principio y
> reinicializar si no cuadra.

### La lectura de fondo: **el robot no se fía de sus ruedas para saber
dónde está**. Nosotros llevamos la cuenta de la odometría y del giroscopio, y **ninguna de las dos es una
referencia absoluta**: la odometría acumula y el giroscopio deriva. Por eso ocho celdas se convierten en
«a tropezones»: **no hay nada que diga «estás exactamente aquí»**.

Ellos sí lo tienen, y lo sacan de la **pared**: del borde de un poste lateral (壁切れ) o de la distancia
frontal. Y de ahí sale todo lo demás: la corrección de la posición, la del ángulo, y los giros de 45°.

**Para Rabosa-MVR, el camino no es el flood fill ni la diagonal: es conseguir que los laterales vean la
pared y usar su borde como referencia.** Sin eso, cualquier algoritmo que se añada encima heredará el
mismo error acumulado.

---

## 8. Dónde está el código, si se quiere mirar

Los repos están clonados en `/tmp/refs` (fuera del repo, para no ensuciarlo). Ficheros que merece la pena
leer tal cual:

| Qué | Fichero |
|:--|:--|
| **Los tres ejes de configuración** ⭐ | `/tmp/refs/mazerunner-core/mazerunner-core/config.h` (144 líneas) |
| **Calibración por laberinto** (4 eventos) | `/tmp/refs/mazerunner-core/mazerunner-core/config-robot-orion.h` |
| ***Feedforward* del modelo, por rueda** | `/tmp/refs/mazerunner-core/mazerunner-core/motors.h` (`leftFeedForward`) |
| **Control de dirección por CTE** ⭐ | `/tmp/refs/mazerunner-core/mazerunner-core/sensors.h` (`calculate_steering_adjustment`) |
| **Perfiles**: la teoría, muy clara | `/tmp/refs/mazerunner-core/documents/profiles.md` y `profile.h` |
| **Dirección**: qué es el error y por qué | `/tmp/refs/mazerunner-core/documents/steering.md` |
| **Mapa y flood fill** | `/tmp/refs/mazerunner-core/documents/maze.md` y `maze.h` |
| **La auditoría de código** | `/tmp/refs/mazerunner-core/review/04-final-report.md` (3 MEDIUM, 7 LOW) |
| Control por posición con profiler | `/tmp/refs/ukmarsbot-mazerunner/mazerunner/profile.h` y `motion.cpp` |
| El README que explica ese control | `/tmp/refs/ukmarsbot-mazerunner/mazerunner/README.MD` |
| Seis lazos y perfil de giro | `/tmp/refs/ZoroBot3/source_code/src/move.c` y `control.c` |
| Sensores con calibración a distancia | `/tmp/refs/ZoroBot3/source_code/src/sensors.c` |
| Flood fill por pesos | `/tmp/refs/ZoroBot3/source_code/src/floodfill_weigths.c` |
| Wall follower mínimo | `/tmp/refs/ZoroBot3/source_code/include/handwall.h` |
| Trayectorias de slalom | `/tmp/refs/micromouse-control-module/include/ctrl/slalom/` |
| Laberinto en solo dos ficheros | `/tmp/refs/micromouse-maze-library/include/MazeLib/` |
| Movimiento con perfiles en S | `/tmp/refs/micromouse-kerise-firmware/src/agents/move_action.h` |

## 9. Relacionado

- [estado-del-arte.md](./estado-del-arte.md) — **la vista de decisión**: matriz de capacidades, qué
  funciona medido y las fases F1–F9 con su puerta de salida
- [esquema-firmware.md](./esquema-firmware.md) — cómo está montado lo nuestro
- [estado-del-proyecto.md](./estado-del-proyecto.md) — qué está hecho y qué falta
- [cinemáticas.md](./cinemáticas.md) — perfiles trapezoidal y en S (teoría que KERISE ya implementa)
