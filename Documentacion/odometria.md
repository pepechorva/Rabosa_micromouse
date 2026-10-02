# Odometría — Ubicación en la Arquitectura por Capas

> Basado en `Layered Architecture.md` y `modelado.md`
>
> ⚠️ `Layered Architecture.md` es **aspiracional** y está desactualizado (habla de STM32L4xx, de
> MPU6050 por I2C y de `radio.c`/`monitor.c`, que no existen). Lo que manda es el estado real del
> firmware.

---

## 1. Ubicación: Capa `FILTER`

La odometría pertenece a la capa **FILTER** porque es un **estimador de estado**: combina las
distancias recorridas por cada rueda (y, más adelante, la IMU) para producir la posición
$(x, y, \theta)$. No es una medición directa (eso iría en `SENSOR`) ni una acción de control (eso iría
en `CONTROL`).

```
APP          (application.c, telemetry.c)
   ↑
CONTROL      (controller.c, pid.c, setpoint.c)
   ↑
FILTER  ←──  odometry.c      ←── Δs_i, Δs_d de los encoders
   ↑
SENSOR       (encoder.c, imu.c, battery.c)
   ↑
HAL          (i2c.c, timers.c, spi.c, uart.c…)
   ↑
HW           (io.c, registros)
```

---

## 2. Desglose por subfunciones

| Función | Responsabilidad | Ubicación | Estado |
|:--------|:----------------|:----------|:-------|
| $\Delta s = f(\Delta\text{cuentas})$ | Convertir cuentas de encoder a distancia | `sensor/encoder.c` | ✅ Hecho (`get_encoder_*_micrometers()`) |
| $v,\dot\theta = f(\Delta s_i, \Delta s_d)$ | Cinemática directa (tracción diferencial) | `filter/odometry.c` | ✅ 28-sep (`ODOM_Update()`) |
| $x,y,\theta$ por integración de arcos | Integración de posición (odometría pura) | `filter/odometry.c` | ✅ 28-sep |
| Fusión encoder + IMU | Estimador de orientación / IEKF | `filter/iekf.c` | ❌ **No existe** (era aspiracional) |

---

## 3. Interfaz del módulo `odometry.c`

El módulo es **matemática pura**: no lee encoders, no mira el reloj y no sabe nada del laberinto.
Recibe los **incrementos de arco** de cada rueda y mueve la pose. Quien lee los encoders y llama es la
capa APP, igual que en CONTROL (el `controller` tampoco lee encoders ni escribe PWM).

```c
// filter/odometry.h

typedef struct {
    float x;         // posición X [m]
    float y;         // posición Y [m]
    float theta;     // orientación [rad], en [-π, π]
    float distance;  // camino recorrido acumulado [m]
} Odometry_t;

void  ODOM_Init(Odometry_t *odo, float x0_m, float y0_m, float theta0_rad);
void  ODOM_Reset(Odometry_t *odo);
void  ODOM_Update(Odometry_t *odo, float ds_left_m, float ds_right_m);
void  ODOM_SetTrocha(float trocha_mm);
float ODOM_GetTrocha(void);
```

> ✅ **Escrito el 28-sep** (`firmware/filter/odometry.{h,c}`). El sistema de referencia es **relativo
al arranque**: origen donde empieza la prueba y $\theta=0$ mirando al frente. Las coordenadas de
laberinto (celdas de 180 mm, origen en la esquina) son cosa de APP/planificación, no de FILTER.

---

## 4. Algoritmo: incrementos de arco

**Se integran distancias, no velocidades.** La odometría consume $\Delta s$ de cada rueda, y eso:

- elimina la dependencia de `dt` (inmune al *jitter* del bucle);
- permite la solución **exacta** del arco en vez de la aproximación de Euler;
- no duplica trabajo: la velocidad la sigue calculando `sensor/encoder.c` para el PID.

Con $ds = (ds_d + ds_i)/2$ y $d\theta = (ds_d - ds_i)/L$ (positivo = giro a la **izquierda**):

- Si $|d\theta| < 10^{-4}$ rad, el tramo es recto: $x \mathrel{+}= ds\cos\theta$, $y \mathrel{+}= ds\sin\theta$
- Si no, el robot gira alrededor del centro de rotación instantáneo, de radio $R = ds/d\theta$:
  $x \mathrel{+}= R(\sin(\theta+d\theta)-\sin\theta)$, $y \mathrel{-}= R(\cos(\theta+d\theta)-\cos\theta)$

Después, $\theta \mathrel{+}= d\theta$ normalizado a $[-\pi, \pi]$. El camino recorrido acumula
$|ds|$, así que no decrece al retroceder.

> Se descarta la integración con $v\cdot dt$ que proponía la primera versión de este documento: para
> el mismo movimiento da más error y depende del instante exacto en que se llame.

⚠️ Los dos encoders deben contar **positivo al avanzar**. Si uno va invertido, la pose gira sola
(aunque el robot vaya recto).

---

## 5. Verificación

| Nivel | Dónde | Qué comprueba |
|:------|:------|:--------------|
| Lógica (PC) | `firmware/test/odometry_sim.c` | Recta, cuadrado, círculo y normalización de $\theta$, con error de cierre nulo |
| Hardware | Prueba 19 `App_Test_Odometria()` | Que la pose estimada coincide con lo recorrido en el suelo, empujando a mano |

```bash
gcc -I firmware -O2 -Wall -o /tmp/odometry_sim firmware/test/odometry_sim.c \
    firmware/filter/odometry.c -lm
/tmp/odometry_sim
```

Telemetría de la prueba 19 (20 Hz): `>ODO_X`, `>ODO_Y` en mm, `>ODO_TH` en grados, `>ODO_D` (camino,
mm) y `>ODO_UM_L` / `>ODO_UM_R` (µm de cada rueda, para detectar un encoder invertido). El botón **OK
pone el origen a cero** sin salir de la prueba.

Puertas de calidad (procedimiento completo en [pruebas-bringup.md](./pruebas-bringup.md) §15):

| Maniobra | Tolerancia |
|:---------|:-----------|
| Recta de 1 m | error < 10 mm (1 %) |
| Giro de 360° sobre sí mismo | error < 5° |
| Cuadrado de 4 × 300 mm | cierre < 20 mm y < 5° de orientación |

---

## 6. Calibración de la trocha efectiva $L$

Los µm por cuenta salen de la prueba 7, así que el **único** parámetro geométrico que queda es $L$: el
radio de rueda ya va dentro del factor de calibración.

### Valor medido

**$L$ = 77,6 mm** (medido el **2026-10-01** con la prueba 19). El punto de partida era la distancia
entre ejes de motor, **83,6 mm**, y con ella un giro de 90° a mano se estimaba en 83° —un 7 % corto—,
tanto a la izquierda como a la derecha. Ese sesgo **simétrico** descarta un encoder invertido y señala
directamente a $L$:

$$L = L_{usado}\cdot\frac{\theta_{medido}}{\theta_{real}} = 83{,}6 \times \frac{83{,}5}{90} = 77{,}6\ \text{mm}$$

> Sale **menor que la trocha geométrica**, y es lo esperable: al girar sobre sí mismo las ruedas
deslizan, recorren menos arco del que corresponde a la separación real y el ángulo estimado se queda
corto. La trocha efectiva no es una distancia que se pueda medir con el calibre: es el parámetro que
hace que el modelo cuadre con el suelo.

### Refinarla con 5 vueltas

El método de los 90° es rápido pero hereda el error de alineación a mano. Para afinar:

1. Marca el robot y el suelo para referenciar el arranque y la orientación.
2. Con la prueba 19 activa, gira el robot **5 vueltas exactas sobre sí mismo** (a mano o con los
   motores) hasta que vuelva a la marca de orientación.
3. Anota $\Delta s_i$ y $\Delta s_d$ de la telemetría y despeja:
   $$L = \frac{\Delta s_d - \Delta s_i}{5 \cdot 2\pi}$$
4. Escribe el valor en `ODOM_TROCHA_MM_DEF` o llámalo con `ODOM_SetTrocha()`.

Con 5 vueltas, un error de alineación de 3° se queda en el 0.2 %.

> La alternativa "girar 720° y leer el *yaw* de la IMU" hoy no sirve: `sensor/imu.c` solo da el
giroscopio en crudo, sin ángulo fusionado.

---

## 7. Fusión con la IMU (futuro)

Cuando exista un estimador de orientación en FILTER, la odometría será su **paso de predicción** y el
*yaw* de la IMU la **corrección**:

```
Encoders ──► ODOM_Update() ──► pose estimada (predicción)
                                   │
IMU ──► yaw ──► corrección ────────┘
                     ▼
             pose fusionada (x, y, θ)
```

⚠️ Este documento (y `modelado.md`) citaban un `filter/iekf.c` **existente**: no existe, es
aspiracional. Hoy hay odometría pura + IMU en crudo.

---

## 8. Ficheros

| Archivo | Ruta | Estado |
|:--------|:-----|:-------|
| `odometry.h` | `firmware/filter/odometry.h` | ✅ 28-sep |
| `odometry.c` | `firmware/filter/odometry.c` | ✅ 28-sep |
| `odometry_sim.c` | `firmware/test/odometry_sim.c` | ✅ 28-sep (banco del PC) |

> La conversión pulsos → distancia ya la hace `sensor/encoder.c`
> (`get_encoder_left_micrometers()`).
