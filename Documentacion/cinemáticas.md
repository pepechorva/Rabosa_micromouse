# Cinemática del Movimiento: Posición, Velocidad, Aceleración y Jerk

> **Propósito:** Definir los conceptos cinemáticos fundamentales para el control de un robot Micromouse, explicar su relación matemática, y situarlos dentro de la arquitectura por capas del firmware.

---

## Índice

1. [Definiciones fundamentales](#1-definiciones-fundamentales)
2. [Relaciones matemáticas](#2-relaciones-matemáticas)
3. [Perfiles de movimiento típicos](#3-perfiles-de-movimiento-típicos)
4. [Parámetros cinemáticos del Micromouse](#4-parámetros-cinemáticos-del-micromouse)
5. [Ubicación en la arquitectura por capas](#5-ubicación-en-la-arquitectura-por-capas)
6. [Uso en el control del robot](#6-uso-en-el-control-del-robot)
7. [Ejemplo: perfil trapezoidal de velocidad](#7-ejemplo-perfil-trapezoidal-de-velocidad)
8. [Del jerk al PID: hoja de ruta](#8-del-jerk-al-pid-hoja-de-ruta)

---

## 1. Definiciones fundamentales

La cinemática describe el **movimiento** sin considerar las fuerzas que lo causan. Las cuatro magnitudes fundamentales son:

### 1.1 Posición $s(t)$

> **¿Dónde está el robot?**

Es la ubicación del robot en el espacio. En un Micromouse tenemos:

- **Posición lineal:** $(x, y)$ en metros [m], referida al centro del robot sobre el laberinto.
- **Orientación:** $\theta$ en radianes [rad], ángulo respecto al eje X del laberinto.

$$ \mathbf{p}(t) = \begin{bmatrix} x(t) \\ y(t) \\ \theta(t) \end{bmatrix} $$

### 1.2 Velocidad $v(t)$

> **¿Qué tan rápido se mueve?**

Es la **tasa de cambio** de la posición respecto al tiempo.

| Tipo | Definición | Unidades |
|:-----|:-----------|:---------|
| **Velocidad lineal** | $\displaystyle v(t) = \dot{x}(t) = \frac{dx}{dt}$ | m/s |
| **Velocidad angular** | $\displaystyle \omega(t) = \dot{\theta}(t) = \frac{d\theta}{dt}$ | rad/s |

En el Micromouse controlamos dos velocidades angulares de rueda ($\omega_d$, $\omega_i$) que se combinan en:

$$ v = \frac{r(\omega_d + \omega_i)}{2}, \qquad \dot{\theta} = \frac{r(\omega_d - \omega_i)}{L} $$

donde $r$ es el radio de rueda y $L$ la trocha (ver *modelado.md* sección 8).

### 1.3 Aceleración $a(t)$

> **¿Qué tan rápido cambia la velocidad?**

Es la **tasa de cambio** de la velocidad respecto al tiempo.

| Tipo | Definición | Unidades |
|:-----|:-----------|:---------|
| **Aceleración lineal** | $\displaystyle a(t) = \dot{v}(t) = \frac{dv}{dt} = \frac{d^2x}{dt^2}$ | m/s² |
| **Aceleración angular** | $\displaystyle \alpha(t) = \dot{\omega}(t) = \frac{d\omega}{dt} = \frac{d^2\theta}{dt^2}$ | rad/s² |

La aceleración está relacionada con el **torque** del motor mediante la segunda ley de Newton:

$$ \tau = J \cdot \alpha $$

donde $J$ es el momento de inercia total y $\alpha$ la aceleración angular del motor.

### 1.4 Jerk $j(t)$

> **¿Qué tan brusco es el movimiento? ¿Duele?** 🥴

Es la **tasa de cambio de la aceleración** respecto al tiempo. También llamado *sobreaceleración* o *sacudida*.

$$ j(t) = \dot{a}(t) = \frac{da}{dt} = \frac{d^2v}{dt^2} = \frac{d^3x}{dt^3} $$

| Tipo | Definición | Unidades |
|:-----|:-----------|:---------|
| **Jerk lineal** | $\displaystyle j(t) = \frac{da}{dt}$ | m/s³ |
| **Jerk angular** | $\displaystyle \zeta(t) = \frac{d\alpha}{dt}$ | rad/s³ |

#### ¿Por qué es importante el jerk en un Micromouse?

| Efecto | Causa |
|:-------|:------|
| **Oscilaciones del chasis** | Un jerk alto excita modos vibratorios de la estructura |
| **Deslizamiento de ruedas** | Cambios bruscos de aceleración rompen la adherencia |
| **Deriva en la IMU** | Aceleraciones bruscas saturan temporalmente el giroscopio/acelerómetro |
| **Consumo de corriente** | Picos de corriente menos agresivos = batería más estable |
| **Precisión de odometría** | Menor deslizamiento → mejor integración de posición |

> **💡 Regla práctica:** Limitar el jerk es lo que diferencia un robot que «tiembla» al moverse de uno que se desliza suave y precisamente.

---

## 2. Relaciones matemáticas

### 2.1 Cadena de derivación e integración

Las cuatro magnitudes forman una cadena continua en el tiempo:

```
                    derivar
    posición ───────────────► velocidad
    s(t)                      v(t) = ṡ(t)
      ▲                          │
      │                          │ derivar
      │                          ▼
      │                    aceleración
      │                     a(t) = v̇(t)
      │                          │
      │ integrar                 │ derivar
      │                          ▼
      └──────────────────────  jerk
                               j(t) = ȧ(t)
```

En formato compacto:

$$ s(t) \quad \xrightarrow{\frac{d}{dt}} \quad v(t) \quad \xrightarrow{\frac{d}{dt}} \quad a(t) \quad \xrightarrow{\frac{d}{dt}} \quad j(t) $$

$$ j(t) \quad \xrightarrow{\int dt} \quad a(t) \quad \xrightarrow{\int dt} \quad v(t) \quad \xrightarrow{\int dt} \quad s(t) $$

### 2.2 Ecuaciones del movimiento con jerk constante

Si el jerk $j$ se mantiene constante durante un intervalo $\Delta t = t - t_0$:

| Magnitud | Ecuación |
|:---------|:---------|
| Aceleración | $a(t) = a_0 + j \cdot \Delta t$ |
| Velocidad | $v(t) = v_0 + a_0 \cdot \Delta t + \displaystyle\frac{1}{2} j \cdot \Delta t^2$ |
| Posición | $s(t) = s_0 + v_0 \cdot \Delta t + \displaystyle\frac{1}{2} a_0 \cdot \Delta t^2 + \displaystyle\frac{1}{6} j \cdot \Delta t^3$ |

### 2.3 Ecuaciones con aceleración constante (jerk = 0)

Es el caso más simple, usado cuando no se controla jerk:

| Magnitud | Ecuación |
|:---------|:---------|
| Velocidad | $v(t) = v_0 + a \cdot \Delta t$ |
| Posición | $s(t) = s_0 + v_0 \cdot \Delta t + \displaystyle\frac{1}{2} a \cdot \Delta t^2$ |
| Velocidad (sin tiempo) | $v^2 = v_0^2 + 2a \cdot \Delta s$ |

---

## 3. Perfiles de movimiento típicos

### 3.1 Perfil trapezoidal de velocidad (con aceleración constante)

Es el más común en Micromouse por su simplicidad:

```
v(t)
  ▲
  │         ┌──────────────────┐
  │        ╱                    ╲
  │       ╱                      ╲
  │      ╱                        ╲
  │     ╱                          ╲
  │    ╱                            ╲
  │   ╱                              ╲
  │  ╱                                ╲
  │ ╱                                  ╲
  └─────────────────────────────────────────► t
     ↑        ↑         ↑         ↑
   t_acc    t_max    t_dec     t_fin
```

**Fases:**

| Fase | Descripción | Jerk |
|:-----|:------------|:----:|
| **Aceleración** | $a = a_{\text{max}}$ constante | $j = 0$ (infinito en transiciones) |
| **Velocidad constante** | $v = v_{\text{max}}$, $a = 0$ | $j = 0$ |
| **Deceleración** | $a = -a_{\text{max}}$ constante | $j = 0$ (infinito en transiciones) |

**Problema:** Las transiciones entre fases tienen jerk **infinito** (cambio instantáneo de aceleración). Esto provoca tirones en el robot.

### 3.2 Perfil en S (con limitación de jerk)

Suaviza las transiciones limitando el jerk a un valor finito $j_{\text{max}}$:

```
v(t)
  ▲                     ┌──────┐
  │                  ╱──        ──╲
  │              ╱──                ──╲
  │          ╱──                        ──╲
  │      ╱──                                ──╲
  │  ╱──                                        ──╲
  └──────────────────────────────────────────────────► t

a(t)
  ▲     ┌────┐
  │    ╱      ╲            ┌────┐
  │   ╱        ╲          ╱      ╲
  │  ╱          ╲        ╱        ╲
  │ ╱            ╲      ╱          ╲
  └──────────────────────────────────────► t
     ↑    ↑    ↑    ↑    ↑    ↑    ↑
    j>0  j=0  j<0  j=0  j<0  j=0  j>0
    a↑   a=max a↓   a=0  a↓  a=-max a↑
```

**Fases (7 segmentos):**

| Fase | Jerk | Aceleración | Velocidad |
|:-----|:----:|:-----------:|:---------:|
| 1. Acel. creciente | $+j_{\text{max}}$ | $0 \to a_{\text{max}}$ | Crec. cuadrático |
| 2. Acel. constante | $0$ | $a_{\text{max}}$ | Crec. lineal |
| 3. Acel. decreciente | $-j_{\text{max}}$ | $a_{\text{max}} \to 0$ | Crec. cuadrático |
| 4. Velocidad constante | $0$ | $0$ | Constante |
| 5. Decel. creciente | $-j_{\text{max}}$ | $0 \to -a_{\text{max}}$ | Dec. cuadrático |
| 6. Decel. constante | $0$ | $-a_{\text{max}}$ | Dec. lineal |
| 7. Decel. decreciente | $+j_{\text{max}}$ | $-a_{\text{max}} \to 0$ | Dec. cuadrático |

> **✅ Ventaja:** El robot acelera y frena de forma mucho más suave. La IMU no sufre picos de aceleración, y las ruedas deslizan menos.

### 3.3 Comparativa de perfiles

| Perfil | Jerk máximo | Oscilaciones | Deslizamiento | Complejidad | Uso típico |
|:-------|:-----------:|:------------:|:-------------:|:-----------:|:-----------|
| **Trapezoidal** | ∞ (picos) | Altas | Posible | Baja | Arranque rápido, prototipos |
| **En S (jerk finito)** | $j_{\text{max}}$ finito | Bajas | Mínimo | Media | Micromouse competitivo |
| **Triangular** | ∞ (picos) | Medias | Moderado | Baja | Distancias cortas (< 1 celda) |

---

## 4. Parámetros cinemáticos del Micromouse

### 4.1 Parámetros del robot (Rabosa-MVR)

| Símbolo | Descripción | Valor | Unidad |
|:-------:|:------------|:-----:|:------:|
| $r$ | Radio de rodadura de la rueda | **12.0** | mm |
| $L$ | Trocha efectiva (la **medida** en el robot, prueba 19) | **77.6** ✅ | mm |
| $v_{\text{max}}$ | Velocidad máxima objetivo | 1.5–2.0 | m/s |
| $a_{\text{max}}$ | Aceleración máxima | 2.0–4.0 | m/s² |
| $j_{\text{max}}$ | Jerk máximo (perfil en S) | 10–30 | m/s³ |
| $\omega_{\text{max}}$ | Velocidad angular máxima de giro | 6–10 | rad/s |
| $\alpha_{\text{max}}$ | Aceleración angular máxima | 20–40 | rad/s² |
| $\Delta t$ | Período del lazo de control | 1–10 | ms |

> [!WARNING] Valores provisionales
> $r = 10.5$ mm sale de la rueda Ø21 mm con goma (a confirmar al rediseñarla).
> $L = 83.6$ mm es la **distancia entre ejes de motor**: la trocha efectiva real hay que
> **medirla** girando el robot sobre sí mismo. Ver
> [hardware.md](./hardware.md#41-motores-transmisión-y-chasis).
>
> Con estos valores la relación geométrica es $L/r \approx 7.96$, es decir
> $\dot{\theta} \approx 0.126\,(\omega_d - \omega_i)$.

**Velocidad a máxima consigna** (rueda a 1500 RPM = 157 rad/s):

$$
v_{\text{max}} = r \cdot \omega = 0.0105 \times 157 \approx 1.65\ \text{m/s}
$$

### 4.2 Parámetros del laberinto

| Parámetro | Valor | Unidad |
|:----------|:-----:|:------:|
| Distancia entre celdas | 180 | mm |
| Ancho de pasillo | 168 | mm |
| Espesor de pared | 12 | mm |
| Tamaño del laberinto | 16×16 | celdas |

### 4.3 Parámetros derivados

| Relación | Fórmula | Ejemplo ($a=3\,\text{m/s}^2$) |
|:---------|:--------|:-----------------------------:|
| Tiempo para alcanzar $v_{\text{max}}$ | $t = v_{\text{max}} / a_{\text{max}}$ | $1.5 / 3 = 0.5$ s |
| Distancia de aceleración | $d = \frac{1}{2} a_{\text{max}} t^2$ | $0.5 \cdot 3 \cdot 0.5^2 = 0.375$ m |
| Distancia para frenar desde $v$ | $d_{\text{stop}} = v^2 / (2a_{\text{max}})$ | $1.5^2 / 6 = 0.375$ m |
| Tiempo de giro de 90° | $t_{\text{giro}} = \pi / (2\dot{\theta})$ | $1.57 / 8 = 0.196$ s |
| Velocidad en celda completa | $v_{\text{celda}} = \sqrt{2 a_{\text{max}} \cdot 0.18}$ | $\sqrt{2 \cdot 3 \cdot 0.18} \approx 1.04$ m/s |

---

## 5. Ubicación en la arquitectura por capas

### 5.1 Mapa de capas

La cinemática **no es un módulo independiente**, sino un conjunto de conceptos que **atraviesan varias capas**:

```
┌──────────────────────────────────────────────────────────────┐
│  APP                                                         │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ Planificador: "avanza 3 celdas, gira 90°, avanza 2..." │ │
│  │ Genera secuencias de posiciones objetivo                │ │
│  └─────────────────────────────────────────────────────────┘ │
│                              │ Δs, Δθ                        │
│                              ▼                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ CONTROL (setpoint.c)                                     │ │
│  │ Calcula perfiles: v_deseada(t), a_deseada(t), j_deseada │ │
│  │ Genera consignas suavizadas para el lazo PID             │ │
│  └─────────────────────────────────────────────────────────┘ │
│                              │ v_ref, ω_ref                   │
│                              ▼                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ CONTROL (pid.c)                                          │ │
│  │ Lazos PID: velocidad de cada rueda                       │ │
│  │ Error = ω_deseada - ω_encoder                            │ │
│  │ Salida = PWM para el driver                              │ │
│  └─────────────────────────────────────────────────────────┘ │
│                              │ PWM                          │
│                              ▼                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ FILTER (odometry.c)                                      │ │
│  │ Mide v_real, ω_real desde encoders → actualiza (x,y,θ)  │ │
│  │ Se puede derivar para obtener a_real y j_real            │ │
│  └─────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

### 5.2 ¿Dónde vive cada concepto?

| Concepto | Capa | Módulo | Qué hace |
|:---------|:-----|:-------|:---------|
| **Posición $(x,y,\theta)$** | `FILTER` | `odometry.c` | Integra velocidades → posición estimada |
| **Velocidad $v$, $\omega$** | `SENSOR` | `encoder.c` | Convierte pulsos de encoder → rad/s |
| **Velocidad $v$, $\dot{\theta}$** | `FILTER` | `odometry.c` | Cinemática directa: $\omega_{d,i} \to v,\dot{\theta}$ |
| **Aceleración $a$** | `CONTROL` | `setpoint.c` | Genera rampas de aceleración (consigna) |
| **Aceleración $a$** | `FILTER` | `filter.c` | Derivada numérica de $v$ para realimentación |
| **Jerk $j$** | `CONTROL` | `setpoint.c` | Genera perfiles en S (consigna) |
| **Jerk $j$** | `FILTER` | `filter.c` | Derivada segunda de $v$ (opcional) |

### 5.3 Flujo de datos cinemáticos

```
ENCÓDERS (pulsos/Δt)
    │
    ▼
ω_d, ω_i [rad/s]          ← encoder.c (SENSOR)
    │
    ├──► odometry_update()  → v, θ̇  → (x, y, θ)  ← odometry.c (FILTER)
    │
    ▼
PID del motor              ← pid.c (CONTROL)
    │
    ▼
PWM → DRV8833 → Motor
```

---

## 6. Uso en el control del robot

### 6.1 Bucle general de control cinemático

```
                  Perfilador
                  (setpoint.c)
                       │
             v_ref, ω_ref (consigna)
                       │
                       ▼
    ┌─────────────────────────────────────┐
    │         CONTROLADOR PID             │
    │                                     │
    │  ω_d_ref ──► PID_D ──► PWM_D       │
    │  ω_i_ref ──► PID_I ──► PWM_I       │
    │                                     │
    └─────────────────────────────────────┘
                       │
                       ▼
                  MOTORES + DRV8833
                       │
                       ▼
                  ENCÓDERS
                       │
             ω_d_real, ω_i_real
                       │
                       ▼
    ┌─────────────────────────────────────┐
    │         ODOMETRÍA                   │
    │                                     │
    │  v_real = r(ω_d + ω_i)/2           │
    │  θ̇_real = r(ω_d - ω_i)/L           │
    │  x += v·cos(θ)·Δt                  │
    │  y += v·sin(θ)·Δt                  │
    │  θ += θ̇·Δt                         │
    │                                     │
    └─────────────────────────────────────┘
                       │
             (x, y, θ)  para realimentación
                       │
                       ▼
                  PLANIFICADOR
            (siguiente objetivo)
```

### 6.2 Control de velocidad (lazo cerrado)

El objetivo del PID es que $\omega_{\text{real}}(t)$ siga a $\omega_{\text{ref}}(t)$:

$$ e(t) = \omega_{\text{ref}}(t) - \omega_{\text{real}}(t) $$

$$ \text{PWM}(t) = K_p e(t) + K_i \int e(t)\,dt + K_d \frac{de(t)}{dt} $$

La **aceleración** aparece implícitamente:

- Una rampa en $\omega_{\text{ref}}$ implica aceleración constante.
- El término derivativo $K_d$ responde a cambios en $e(t)$, que son variaciones de aceleración.

### 6.3 Control de posición (lazo externo)

Se puede añadir un lazo exterior de posición anidado sobre el de velocidad:

```
Posición deseada ──► Control de posición ──► Control de velocidad ──► Motor
                         (Kp_pos)                (PID velocidad)
                              ▲                        │
                              │                        ▼
                         Posición real           Velocidad real
                         (odometría)             (encoders)
```

Este esquema es útil para:
- **Precisión en celdas:** Frenar exactamente al borde de una celda.
- **Giros exactos:** Girar 90.0° exactos usando realimentación de $\theta$.
- **Centrado en pasillo:** Mantener $(y \approx \text{centro})$ mientras avanza en $x$.

### 6.4 Generación de consignas con limitación de jerk

```c
// Pseudocódigo: perfil en S para una transición de velocidad
// Entrada: v_actual, v_objetivo, a_max, j_max, dt
// Salida:  v_ref para el próximo ciclo

typedef struct {
    float v_ref;      // velocidad de consigna [m/s]
    float a_ref;      // aceleración de consigna [m/s²]
} profile_t;

profile_t profile_generate(float v_actual, float v_target,
                           float a_max, float j_max, float dt) {
    profile_t p;
    static float a_current = 0.0f;   // estado interno

    // 1. ¿Necesitamos acelerar o frenar?
    float v_error = v_target - v_actual;

    // 2. Distancia necesaria para frenar desde v_actual con a_max
    float stop_dist = (v_actual * v_actual) / (2.0f * a_max);

    // 3. Limitar jerk en la aceleración
    if (v_error > 0.1f && a_current < a_max) {
        // Acelerando: incrementar a suavemente
        a_current += j_max * dt;
        if (a_current > a_max) a_current = a_max;
    } else if (v_error < -0.1f && a_current > -a_max) {
        // Frenando: decrementar a suavemente
        a_current -= j_max * dt;
        if (a_current < -a_max) a_current = -a_max;
    } else {
        // Velocidad mantenida: reducir aceleración a cero
        if (a_current > 0.0f) {
            a_current -= j_max * dt;
            if (a_current < 0.0f) a_current = 0.0f;
        } else if (a_current < 0.0f) {
            a_current += j_max * dt;
            if (a_current > 0.0f) a_current = 0.0f;
        }
    }

    // 4. Integrar aceleración → velocidad
    p.a_ref = a_current;
    p.v_ref = v_actual + a_current * dt;

    return p;
}
```

### 6.5 Estimación de aceleración real desde encoders

Para observar la aceleración real del robot:

```c
// Estimación de aceleración por diferenciación numérica
// Llamar en cada ciclo de control

static float v_prev = 0.0f;

float estimate_acceleration(float v_current, float dt) {
    float a = (v_current - v_prev) / dt;   // [m/s²]
    v_prev = v_current;
    return a;
}
```

> **⚠️ Cuidado:** Derivar numéricamente amplifica el ruido. Es recomendable pasar la aceleración estimada por un filtro paso bajo (*filter.c*) antes de usarla.

---

## 7. Ejemplo: perfil trapezoidal de velocidad

### 7.1 Escenario

El robot debe avanzar **una celda** (180 mm) partiendo del reposo y terminando en reposo.

**Parámetros:**
- $v_{\text{max}} = 1.0$ m/s
- $a_{\text{max}} = 4.0$ m/s²
- Distancia total: $d = 0.18$ m

### 7.2 ¿Alcanza velocidad máxima?

Tiempo para acelerar a $v_{\text{max}}$:
$$ t_a = \frac{v_{\text{max}}}{a_{\text{max}}} = \frac{1.0}{4.0} = 0.25\ \text{s} $$

Distancia recorrida acelerando:
$$ d_a = \frac{1}{2} a_{\text{max}} t_a^2 = 0.5 \cdot 4.0 \cdot 0.25^2 = 0.125\ \text{m} $$

Distancia total acelerando + frenando:
$$ d_{\text{total}} = 2 \cdot d_a = 0.25\ \text{m} $$

**Conclusión:** $0.25\ \text{m} > 0.18\ \text{m}$ → **No alcanza $v_{\text{max}}$**. El perfil es triangular.

### 7.3 Perfil triangular resultante

Para un movimiento triangular, la velocidad máxima alcanzable es:

$$ v_{\text{pico}} = \sqrt{a_{\text{max}} \cdot d} = \sqrt{4.0 \cdot 0.18} = 0.848\ \text{m/s} $$

Tiempo total del movimiento:
$$ t_{\text{total}} = \frac{2 v_{\text{pico}}}{a_{\text{max}}} = \frac{2 \cdot 0.848}{4.0} = 0.424\ \text{s} $$

### 7.4 Mismo escenario con perfil en S

Si además limitamos $j_{\text{max}} = 20$ m/s³:

Tiempo para alcanzar aceleración máxima:
$$ t_j = \frac{a_{\text{max}}}{j_{\text{max}}} = \frac{4.0}{20} = 0.2\ \text{s} $$

La distancia extra perdida por el suavizado del jerk hace que la velocidad pico sea ligeramente menor. A cambio, el movimiento es más suave y preciso.

---

## 8. Del jerk al PID: hoja de ruta

### 8.1 Niveles de sofisticación en el control

| Nivel | Descripción | Conceptos usados | Cuándo implementar |
|:-----:|:------------|:-----------------|:-------------------|
| **0** | Motor ON/OFF | Solo PWM fijo | Primeras pruebas |
| **1** | Velocidad constante con PID | $v$, $\omega$, error | Sintonización inicial |
| **2** | Perfil trapezoidal | $v_{\text{ref}}$, $a_{\text{max}}$ | Movimiento básico en laberinto |
| **3** | Perfil en S (jerk limitado) | $a(t)$, $j_{\text{max}}$ | Carreras suaves, precisión |
| **4** | Control de posición anidado | $x,y,\theta$ realimentados | Giros exactos, centrado |
| **5** | Planificación de ruta óptima | Trayectorias con jerk mínimo | Competitivo |

### 8.2 Prioridad para Rabosa-MVR

Según la planificación actual:

```
Semana 4 (11 Ago)  →  Nivel 1: PID de velocidad
Semana 5 (18 Ago)  →  Nivel 2: Perfil trapezoidal + odometría
Semana 6 (25 Ago)  →  Nivel 3: Perfil en S (jerk)
Semana 7 (1 Sep)   →  Nivel 4: Control de posición
Semana 8 (8 Sep)   →  Nivel 5: Planificación de ruta
```

### 8.3 Relación con los módulos existentes

| Módulo existente | Concepto cinemático que implementa |
|:-----------------|:-----------------------------------|
| `encoder.c` | Mide $\omega$ [rad/s] desde pulsos |
| `odometry.c` | Integra $v$, $\dot{\theta}$ → $x, y, \theta$ |
| `pid.c` | Controla $\omega$ para que siga $\omega_{\text{ref}}$ |
| `setpoint.c` | Genera $\omega_{\text{ref}}(t)$ con perfiles $a$, $j$ |
| `filter.c` | Suaviza derivadas numéricas ($a$, $j$ medidas) |
| `controller.c` | Orquesta los lazos (posición → velocidad → PWM) |

---

## Resumen

| Concepto | Ecuación | Rol en el Micromouse |
|:---------|:---------|:---------------------|
| **Posición** $(x, y, \theta)$ | $\mathbf{p} = \int \mathbf{v}\,dt$ | Saber dónde está el robot en el laberinto |
| **Velocidad** $v$, $\omega$ | $v = \dot{x},\ \omega = \dot{\theta}$ | Magnitud que controla el PID |
| **Aceleración** $a$, $\alpha$ | $a = \dot{v},\ \alpha = \dot{\omega}$ | Limitar para no deslizar; consigna del perfil |
| **Jerk** $j$, $\zeta$ | $j = \dot{a},\ \zeta = \dot{\alpha}$ | Suavizar transiciones; evitar oscilaciones |

La cinemática conecta **lo que queremos** (ir del punto A al B) con **cómo se lo pedimos a los motores** (perfiles de velocidad suaves) y **cómo verificamos que lo hace bien** (odometría + PID).

---

> **Documentos relacionados:**
> - `modelado.md` — Modelo dinámico del motor DC y funciones de transferencia
> - `odometria.md` — Integración de posición desde velocidades de rueda
> - `planificacion.md` — Hoja de ruta y temporización de la implementación
> - `Layered Architecture.md` — Arquitectura por capas del firmware
