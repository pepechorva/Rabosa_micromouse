# Documentación Técnica: Modelado y Control de un Robot Micromouse

> **Propósito:** Este documento guía desde los fundamentos de sistemas dinámicos hasta el modelo completo de
> un robot Micromouse, con todas las fórmulas, ejemplos numéricos y pasos de implementación práctica.

---

## Índice

1. [Fundamentos: ¿Qué es un sistema dinámico?](#1-fundamentos-qué-es-un-sistema-dinámico)
2. [Identificación del orden de un sistema](#2-identificación-del-orden-de-un-sistema)
3. [Modelado del motor DC](#3-modelado-del-motor-dc)
4. [Efecto de la caja reductora (gearbox)](#4-efecto-de-la-caja-reductora-gearbox)
5. [Momento de inercia: cálculo completo](#5-momento-de-inercia-cálculo-completo)
6. [Descomposición sistémica del Micromouse](#6-descomposición-sistémica-del-micromouse)
7. [Funciones de transferencia del conjunto](#7-funciones-de-transferencia-del-conjunto)
8. [Cinemática del robot de tracción diferencial](#8-cinemática-del-robot-de-tracción-diferencial)
9. [Odometría: integración de posición](#9-odometría-integración-de-posición)
10. [Guía de caracterización experimental](#10-guía-de-caracterización-experimental)
11. [Ejemplo numérico completo](#11-ejemplo-numérico-completo)
12. [Implementación en microcontrolador](#12-implementación-en-microcontrolador)
13. [Hoja de ruta: del modelo al PID](#13-hoja-de-ruta-del-modelo-al-pid)

---

## 1. Fundamentos: ¿Qué es un sistema dinámico?

Un **sistema dinámico** es aquel cuya salida $y(t)$ evoluciona en el tiempo en respuesta a una entrada $u(t)$, y su estado actual depende de estados pasados (tiene **memoria**).

### 1.1 Representación en ecuaciones diferenciales

La forma más general de un sistema lineal e invariante en el tiempo (LTI) es:

$$a_n \frac{d^n y}{dt^n} + a_{n-1} \frac{d^{n-1} y}{dt^{n-1}} + \dots + a_0 y = b_m \frac{d^m u}{dt^m} + \dots + b_0 u$$

donde $n$ es el **orden del sistema**.

### 1.2 Representación en función de transferencia

Aplicando la transformada de Laplace (asumiendo condiciones iniciales cero):

$$G(s) = \frac{Y(s)}{U(s)} = \frac{b_m s^m + \dots + b_0}{a_n s^n + \dots + a_0}$$

El **orden** del sistema es el grado del polinomio del denominador.

### 1.3 ¿Por qué es importante el orden?

Porque determina:
- **Número de variables de estado** que necesitas para describir el sistema.
- **Tipo de controlador** necesario (un PID puede bastar para 1.er orden, pero sistemas de 2.º orden pueden necesitar control más sofisticado).
- **Comportamiento transitorio** (sobreimpulso, tiempo de establecimiento, etc.).

---

## 2. Identificación del orden de un sistema

Existen **tres metodologías** para determinar el orden, y lo ideal es usarlas de forma complementaria.

### 2.1 Por la ecuación diferencial

El orden es el **exponente de la derivada más alta**:

| Orden | Ecuación diferencial típica |
|:-----:|:---------------------------|
| **1.º** | $\displaystyle \frac{dy}{dt} + a_0 y = b_0 u$ |
| **2.º** | $\displaystyle \frac{d^2 y}{dt^2} + a_1 \frac{dy}{dt} + a_0 y = b_0 u$ |
| **3.º** | $\displaystyle \frac{d^3 y}{dt^3} + a_2 \frac{d^2 y}{dt^2} + a_1 \frac{dy}{dt} + a_0 y = b_0 u$ |

**Ejemplo:** La ley de Newton $F = m \frac{d^2 x}{dt^2}$ tiene derivada segunda → **2.º orden**.

### 2.2 Por la función de transferencia $G(s)$

El orden es el **grado del denominador** (número de polos):

| Orden | Forma estándar | Polos |
|:-----:|:---------------|:------|
| **1.º** | $\displaystyle G(s) = \frac{K}{\tau s + 1}$ | $s = -1/\tau$ (1 polo real) |
| **2.º** | $\displaystyle G(s) = \frac{K \omega_n^2}{s^2 + 2\zeta\omega_n s + \omega_n^2}$ | $s = -\zeta\omega_n \pm \omega_n \sqrt{\zeta^2 - 1}$ (2 polos, reales o complejos) |
| **3.º** | $\displaystyle G(s) = \frac{K}{(s + p_1)(s^2 + 2\zeta\omega_n s + \omega_n^2)}$ | 1 polo real + 2 polos complejos (o 3 reales) |

**Ejemplo:** $G(s) = \frac{5}{s^2 + 3s + 2} = \frac{5}{(s+1)(s+2)}$ → denominador de grado 2 → **2.º orden**.

### 2.3 Por la respuesta temporal (prueba de escalón)

Este es el método **más usado en la práctica** porque no requiere modelos matemáticos previos.

| Característica | 1.er orden | 2.º orden | 3.er orden |
|:---------------|:----------:|:---------:|:----------:|
| Forma de la curva | Exponencial pura | Oscilatoria o sigmoide | "S" pronunciada |
| Sobreimpulso | ❌ No | ✅ Sí (si $\zeta < 1$) | ⚠️ Posible |
| Punto de inflexión inicial | ❌ (tangente máxima en $t=0$) | ❌ (tangente horizontal o no) | ✅ Sí (2 cambios de concavidad) |
| Tiempo para alcanzar el 63.2% | Coincide con $\tau$ | No coincide con ninguna constante simple | No aplica |

#### 2.3.1 Respuesta de 1.er orden

$$y(t) = K u_0 \left(1 - e^{-t/\tau}\right)$$

- La **tangente inicial** es máxima (la curva arranca con pendiente pronunciada).
- Alcanza el **63.2%** del valor final en $t = \tau$.
- Alcanza el **98%** del valor final en $t = 4\tau$ (tiempo de establecimiento).
- **Nunca** sobrepasa el valor final.

#### 2.3.2 Respuesta de 2.º orden (subamortiguado, $\zeta < 1$)

$$y(t) = K u_0 \left[1 - \frac{e^{-\zeta\omega_n t}}{\sqrt{1-\zeta^2}} \sin\left(\omega_d t + \phi\right)\right]$$

donde $\omega_d = \omega_n \sqrt{1-\zeta^2}$ es la frecuencia natural amortiguada.

- La **tangente inicial es horizontal** (arranque lento).
- Presenta **sobreimpulso** (overshoot) si $\zeta < 1$.
- El sobreimpulso máximo (%OS) se calcula como:
  $$\%OS = 100 \cdot e^{-\pi \zeta / \sqrt{1-\zeta^2}}$$

#### 2.3.3 Respuesta de 3.er orden

$$y(t) = K u_0 \left[1 - A e^{-p_1 t} + B e^{-\zeta\omega_n t}\sin(\omega_d t + \phi)\right]$$

- Tiene **dos puntos de inflexión** (la pendiente aumenta, luego disminuye, luego vuelve a aumentar).
- El arranque es **más lento aún** que en 2.º orden.
- Difícil de distinguir de 2.º orden a simple vista: se necesita un **análisis de residuos** o ajuste de curva.

### 2.4 Método práctico: análisis de la curva de velocidad

Para identificar el orden de tu motor, sigue estos pasos:

1. **Aplica un escalón de voltaje** al motor (por ejemplo, pasar de 0% a 50% PWM instantáneamente).
2. **Registra la velocidad** con el encoder a una tasa de muestreo alta (≥ 500 Hz).
3. **Normaliza la curva** dividiendo por el valor final.
4. **Observa la tangente en $t=0$**:
   - Si la pendiente es máxima en $t=0$ → **1.er orden**.
   - Si la pendiente en $t=0$ es cero (arranque horizontal) → **2.º orden o superior**.
5. **Cuenta los puntos de inflexión** (cambios de signo de la segunda derivada):
   - 0 puntos de inflexión → **1.er orden**.
   - 1 punto de inflexión → **2.º orden**.
   - 2 puntos de inflexión → **3.er orden o superior**.

> **💡 Regla práctica para motores DC pequeños:** La mayoría de motores DC con escobillas se comportan como **1.er orden** porque la inductancia $L$ es despreciable (del orden de microhenrios). Si usas motores brushless (BLDC), la conmutación electrónica introduce dinámica adicional → podrías tener **2.º orden**.

---

## 3. Modelado del motor DC

### 3.1 Modelo físico completo

Un motor DC tiene dos ecuaciones acopladas:

**Circuito eléctrico (malla):**
$$V(t) = L \frac{di(t)}{dt} + R\,i(t) + e(t)$$

donde $e(t) = K_e \, \omega(t)$ es la fuerza contraelectromotriz (FEM).

**Circuito mecánico (torques):**
$$J \frac{d\omega(t)}{dt} + B\,\omega(t) = \tau(t)$$

donde $\tau(t) = K_t \, i(t)$ es el torque generado.

**Variables:**
- $V$: voltaje aplicado [V]
- $i$: corriente de armadura [A]
- $R$: resistencia de armadura [Ω]
- $L$: inductancia de armadura [H]
- $K_e$: constante de velocidad (FEM) [V·s/rad]
- $K_t$: constante de torque [N·m/A], con $K_t \approx K_e$ en unidades SI
- $J$: momento de inercia total [kg·m²]
- $B$: coeficiente de fricción viscosa [N·m·s/rad]
- $\omega$: velocidad angular [rad/s]

### 3.2 Función de transferencia (2.º orden, con inductancia)

Aplicando Laplace al sistema de ecuaciones:

$$V(s) = (L s + R) I(s) + K_e \Omega(s)$$

$$(J s + B) \Omega(s) = K_t I(s)$$

Despejando $I(s)$ de la segunda y sustituyendo en la primera:

$$G_m(s) = \frac{\Omega(s)}{V(s)} = \frac{K_t}{(L s + R)(J s + B) + K_e K_t}$$

Como $K_e = K_t$, queda:

$$G_m(s) = \frac{K_t}{L J \, s^2 + (L B + R J) s + (R B + K_t^2)}$$

**Orden:** **2.º orden** (denominador de grado 2).

### 3.3 Modelo reducido (1.er orden, inductancia despreciable)

En motores DC pequeños, $L$ es muy baja (~10-100 µH). La constante de tiempo eléctrica $\tau_e = L/R$ es del orden de microsegundos, mientras que la constante de tiempo mecánica $\tau_m = J/B$ es del orden de milisegundos. Por tanto, se puede despreciar $L \approx 0$:

$$G_m(s) = \frac{K_t}{R J \, s + (R B + K_t^2)}$$

Que en forma estándar de 1.er orden:

$$G_m(s) = \frac{K}{\tau_m s + 1}$$

donde:

$$K = \frac{K_t}{R B + K_t^2} \quad \text{[rad/s/V]}$$

$$\tau_m = \frac{R J}{R B + K_t^2} \quad \text{[s]}$$

### 3.4 Obtención de parámetros del motor desde el datasheet

El fabricante suele proporcionar estos datos:

| Símbolo | Descripción | Unidades típicas |
|:-------:|:------------|:----------------:|
| $V_n$ | Voltaje nominal | V |
| $\omega_0$ | Velocidad sin carga | RPM o rad/s |
| $I_0$ | Corriente sin carga | A |
| $\tau_s$ | Torque de arranque (stall) | g·cm, kg·cm o N·m |
| $I_s$ | Corriente de arranque (stall) | A |
| $R$ | Resistencia de armadura | Ω (a veces no dada) |
| $J_m$ | Inercia del rotor | g·cm² |

**Paso a paso:**

1. **Resistencia de armadura:**
   $$R = \frac{V_n}{I_s}$$

2. **Constante de torque/velocidad ($K_t = K_e$):**
   $$K_t = \frac{\tau_s}{I_s} \quad [\text{N·m/A}]$$

3. **Constante de velocidad alternativa (si dan RPM):**
   $$K_e = \frac{V_n - I_0 R}{\omega_0} = \frac{V_n}{\omega_0}\,(\text{aproximadamente}) \quad [\text{V·s/rad}]$$

4. **Coeficiente de fricción viscosa $B$:**
   En vacío, el torque generado solo vence la fricción:
   $$\tau_0 = K_t I_0 = B \omega_0 \quad \Rightarrow \quad B = \frac{K_t I_0}{\omega_0}$$

5. **Inercia total $J$:**
   Si no se da $J_m$, se puede estimar del torque de arranque y la aceleración máxima, pero es más fiable medirla (ver [sección 5](#5-momento-de-inercia-cálculo-completo)).

---

## 4. Efecto de la caja reductora (gearbox)

Los motores N20 y similares incorporan una **caja reductora** que multiplica el torque y reduce la velocidad.

### 4.1 Parámetros de la reductora

- $n$: relación de reducción (ej: 30:1 → $n = 30$)
- $\eta$: eficiencia de la transmisión (típicamente 0.7–0.9)

### 4.2 Efecto en velocidad y torque

$$ \omega_{\text{eje}} = \frac{\omega_{\text{motor}}}{n} $$

$$ \tau_{\text{eje}} = \tau_{\text{motor}} \cdot n \cdot \eta $$

### 4.3 Efecto en la inercia reflejada

La inercia del motor vista desde el eje de salida:

$$ J_{\text{reflejada}} = J_{\text{motor}} \cdot n^2 $$

La inercia de la rueda vista desde el motor:

$$ J_{\text{rueda}\to\text{motor}} = \frac{J_{\text{rueda}}}{n^2} $$

### 4.4 Función de transferencia con reductora

Si el modelo del motor es $G_m(s)$ y la reductora es un bloque de ganancia $1/n$ (en velocidad), la función de transferencia desde $V$ hasta $\omega_{\text{eje}}$ es:

$$G_{\text{total}}(s) = \frac{\Omega_{\text{eje}}(s)}{V(s)} = \frac{1}{n} \cdot G_m(s)$$

---

## 5. Momento de inercia: cálculo completo

### 5.1 Inercia de la rueda

**Caso 1: Aproximación como cilindro macizo** (simplificado):

$$ J_r = \frac{1}{2} m_r \, r^2 $$

**Caso 2: Aproximación como cilindro hueco** (más realista, la mayoría de ruedas tienen radios interior y exterior):

$$ J_r = \frac{1}{2} m_r (R_{\text{ext}}^2 + R_{\text{int}}^2) $$

**Caso 3: Aro delgado** (ruedas tipo «aro de goma»):

$$ J_r = m_r \, r^2 $$

**Unidades:** $m_r$ [kg], $r$ [m] → $J_r$ [kg·m²]

### 5.2 Inercia del rotor del motor

Si el datasheet proporciona $J_m$ en g·cm²:

$$ J_m \, [\text{kg·m}^2] = J_m \, [\text{g·cm}^2] \times 10^{-7} $$

### 5.3 Inercia reflejada de la masa del robot (efecto suelo)

Cuando el robot está en el suelo, **toda su masa** $M$ se convierte en una carga inercial para los motores. La energía cinética del robot en movimiento lineal es:

$$ E_c = \frac{1}{2} M v^2 $$

Y la energía cinética rotacional de una rueda es:

$$ E_c = \frac{1}{2} J_{\text{eq}} \, \omega^2 $$

Como $v = \omega \, r$, igualando:

$$ \frac{1}{2} M (\omega r)^2 = \frac{1}{2} J_{\text{eq}} \, \omega^2 $$

Por tanto, la **inercia equivalente** de la masa del robot vista desde **cada rueda** es:

$$ J_{\text{robot}} = \frac{1}{2} M \, r^2 $$

> **Nota:** El factor $1/2$ aparece porque la masa se reparte entre las dos ruedas. Si el robot tuviera una sola rueda motriz sería $M r^2$.

### 5.4 Inercia total por eje

Hay que considerar **tres contribuciones**:

1. Inercia del rotor del motor: $J_m$
2. Inercia de la rueda reflejada al motor: $J_r / n^2$
3. Inercia del robot reflejada al motor: $J_{\text{robot}} / n^2$

$$ J_{\text{total}} = J_m + \frac{J_r}{n^2} + \frac{J_{\text{robot}}}{n^2} $$

### 5.5 Inercia en vacío (rueda al aire)

Si el robot está elevado (ruedas sin contacto), solo contribuyen el rotor y la rueda:

$$ J_{\text{vacío}} = J_m + \frac{J_r}{n^2} $$

---

## 6. Descomposición sistémica del Micromouse

El robot se descompone en **5 subsistemas** (no 4), porque el driver de potencia es un subsistema con su propia función. Además, hay que considerar la reductora como subsistema separado si aplica.

### 6.1 Diagrama de bloques completo

```
                    ┌──────────────────────────────────────────────────────────────┐
                    │                  SUBSISTEMA DEL ROBOT                        │
                    │                                                              │
  V_d[V] ──►[Driver D]──►[Motor DC]──►[Reductora]──►[Rueda D]──► ω_d[rad/s] ──┐  │
                    │                                                              │  │
                    │                                              ┌──────────┐   │  │
                    │                                              │          │   │  │
                    │                                              │Cinématica│──►▼  │
                    │                                              │Diferenc. │  v   │
                    │                                              │          │  ˙θ  │
                    │                                              └──────────┘   │  │
                    │                                                              │  │
  V_i[V] ──►[Driver I]──►[Motor I]──►[Reductora]──►[Rueda I]──► ω_i[rad/s] ──┘  │  │
                    │                                                              │  │
                    │                                              ┌──────────┐   │  │
                    │                                              │          │◄──┘  │
                    │                                              │Odometría │      │
                    │                                              │          │──► (x,y,θ)
                    │                                              └──────────┘      │
                    └──────────────────────────────────────────────────────────────┘
```

### 6.2 Subsistema 0: Driver de Potencia (Puente H)

| Propiedad | Descripción |
|:----------|:------------|
| **Entrada** | Señal PWM (0–100%), usualmente un valor de 16 bits (0–65535) o 8 bits (0–255) |
| **Salida** | Voltaje aplicado al motor: $V_{\text{motor}} = D \cdot V_{\text{batería}}$ |
| **Modelo** | $V_{\text{motor}}(s) = V_{\text{bat}} \cdot D(s)$ |
| **Orden** | **Cero** (instantáneo, la frecuencia PWM ≫ dinámica mecánica) |
| **Ejemplo** | DRV8833, TB6612FNG |

### 6.3 Subsistema 1 y 2: Motor DC + Reductora + Rueda (x2)

| Propiedad | Descripción |
|:----------|:------------|
| **Entrada** | Voltaje $V_d$ [V] y $V_i$ [V] |
| **Salida** | Velocidad angular $\omega_d$ y $\omega_i$ [rad/s] del eje de la rueda |
| **Modelo** | $G_m(s) = \dfrac{K}{\tau_m s + 1}$ (1.er orden) o $G_m(s) = \dfrac{K_t}{L J s^2 + (LB+RJ)s + (RB+K_t^2)}$ (2.º orden) |
| **Cantidad** | 2 unidades independientes |

### 6.4 Subsistema 3: Cinemática del robot (tracción diferencial)

| Propiedad | Descripción |
|:----------|:------------|
| **Entrada** | $\omega_d$, $\omega_i$ [rad/s] |
| **Salida** | $v$ [m/s], $\dot{\theta}$ [rad/s] |
| **Modelo** | Ecuaciones algebraicas (orden cero) |
| **Orden** | **Cero** |

### 6.5 Subsistema 4: Odometría

| Propiedad | Descripción |
|:----------|:------------|
| **Entrada** | $v$ [m/s], $\dot{\theta}$ [rad/s] |
| **Salida** | $x(t), y(t)$ [m], $\theta(t)$ [rad] |
| **Modelo** | Integradores puros |
| **Orden** | **1.er orden** (integrador) |

---

## 7. Funciones de transferencia del conjunto

### 7.1 De voltaje a velocidad de rueda (modelo reducido 1.er orden)

$$G_{\text{rueda}}(s) = \frac{\Omega_{\text{eje}}(s)}{V(s)} = \frac{1}{n} \cdot \frac{K}{\tau_m s + 1}$$

donde $K$ y $\tau_m$ incorporan la inercia total del robot (en condiciones de carrera).

Si además consideramos que el driver PWM convierte el duty cycle $D$ a voltaje:

$$G_{\text{total}}(s) = \frac{\Omega_{\text{eje}}(s)}{D(s)} = V_{\text{bat}} \cdot \frac{1}{n} \cdot \frac{K}{\tau_m s + 1}$$

### 7.2 De velocidad de ruedas a velocidad del robot (cinemática)

$$\begin{bmatrix} v(s) \\ \dot{\theta}(s) \end{bmatrix} = \begin{bmatrix} r/2 & r/2 \\ r/L & -r/L \end{bmatrix} \begin{bmatrix} \Omega_d(s) \\ \Omega_i(s) \end{bmatrix}$$

### 7.3 De velocidad del robot a posición (odometría)

El sistema es **no lineal** porque $v$ se proyecta según $\theta$, por lo que no se puede representar con funciones de transferencia lineales. Linealizando alrededor de una orientación constante $\theta_0$:

$$X(s) = \frac{v(s) \cos\theta_0}{s}, \quad Y(s) = \frac{v(s) \sin\theta_0}{s}, \quad \Theta(s) = \frac{\dot{\theta}(s)}{s}$$

Para el caso general con $\theta$ variable, ver la integración numérica en la [sección 9](#9-odometría-integración-de-posición).

---

## 8. Cinemática del robot de tracción diferencial

### 8.1 Geometría del robot

```
        ○ ←──── L ────→ ○
        ↑                ↑
      Rueda D          Rueda I
        │                │
        └── r ────── r ──┘
```

Donde:
- $r$: radio de las ruedas [m]
- $L$: distancia entre centros de las ruedas (trocha) [m]

### 8.2 Velocidad lineal del robot

La velocidad lineal avance del centro del robot es el promedio de las velocidades tangenciales de ambas ruedas:

$$v = \frac{r(\omega_d + \omega_i)}{2}$$

### 8.3 Velocidad angular del robot (giro)

La velocidad de giro se debe a la diferencia de velocidades entre ambas ruedas:

$$\dot{\theta} = \frac{r(\omega_d - \omega_i)}{L}$$

**Deducción:** Si la rueda derecha va más rápido que la izquierda, el robot gira a la izquierda. La velocidad angular es la diferencia de velocidades tangenciales dividida por la distancia entre ruedas:

$$\dot{\theta} = \frac{v_d - v_i}{L} = \frac{r\omega_d - r\omega_i}{L}$$

### 8.4 Casos particulares

| Movimiento | $\omega_d$ | $\omega_i$ | $v$ | $\dot{\theta}$ |
|:-----------|:----------:|:----------:|:---:|:--------------:|
| Avance recto | $\omega$ | $\omega$ | $r\omega$ | 0 |
| Giro sobre el eje | $\omega$ | $-\omega$ | 0 | $\dfrac{2r\omega}{L}$ |
| Giro con un radio $R$ | $\dfrac{v + \dot{\theta}L/2}{r}$ | $\dfrac{v - \dot{\theta}L/2}{r}$ | — | — |
| Curva suave | $\omega + \Delta$ | $\omega - \Delta$ | $r\omega$ | $\dfrac{2r\Delta}{L}$ |

### 8.5 Matriz de transformación (formulación compacta)

$$ \begin{bmatrix} v \\ \dot{\theta} \end{bmatrix} = \begin{bmatrix} \frac{r}{2} & \frac{r}{2} \\[4pt] \frac{r}{L} & -\frac{r}{L} \end{bmatrix} \begin{bmatrix} \omega_d \\ \omega_i \end{bmatrix} $$

E inversamente (para calcular las consignas de velocidad de cada rueda a partir de $v$ y $\dot{\theta}$ deseados):

$$ \begin{bmatrix} \omega_d \\ \omega_i \end{bmatrix} = \begin{bmatrix} \frac{1}{r} & \frac{L}{2r} \\[4pt] \frac{1}{r} & -\frac{L}{2r} \end{bmatrix} \begin{bmatrix} v \\ \dot{\theta} \end{bmatrix} $$

---

## 9. Odometría: integración de posición

### 9.1 Modelo continuo

La posición y orientación del robot evolucionan según:

$$\dot{x}(t) = v(t) \cos\theta(t)$$
$$\dot{y}(t) = v(t) \sin\theta(t)$$
$$\dot{\theta}(t) = \omega_{\text{robot}}(t)$$

### 9.2 Integración numérica (método de Euler)

Dado un período de control $\Delta t$, y conociendo $v_k$ y $\dot{\theta}_k$ en el instante $k$:

**Paso 1:** Actualizar orientación
$$\theta_{k+1} = \theta_k + \dot{\theta}_k \cdot \Delta t$$

**Paso 2:** Actualizar posición usando la orientación actualizada (Euler de orden 1, el más común en microcontroladores)
$$x_{k+1} = x_k + v_k \cdot \cos(\theta_{k+1}) \cdot \Delta t$$
$$y_{k+1} = y_k + v_k \cdot \sin(\theta_{k+1}) \cdot \Delta t$$

### 9.3 Método mejorado (Euler con promediado)

Para mayor precisión sin aumentar la complejidad, se usa el promedio de orientaciones:

$$\bar{\theta} = \theta_k + \frac{\dot{\theta}_k \cdot \Delta t}{2}$$

$$\theta_{k+1} = \theta_k + \dot{\theta}_k \cdot \Delta t$$
$$x_{k+1} = x_k + v_k \cdot \cos(\bar{\theta}) \cdot \Delta t$$
$$y_{k+1} = y_k + v_k \cdot \sin(\bar{\theta}) \cdot \Delta t$$

### 9.4 Implementación en C para el STM32

```c
#include <math.h>

typedef struct {
    float x;        // [m]
    float y;        // [m]
    float theta;    // [rad]
} odometry_t;

void odometry_update(odometry_t *odo, float v, float omega, float dt) {
    // Método del promedio de orientación (Runge-Kutta de 2.º orden)
    float theta_mid = odo->theta + 0.5f * omega * dt;
    odo->theta += omega * dt;
    odo->x += v * cosf(theta_mid) * dt;
    odo->y += v * sinf(theta_mid) * dt;

    // Normalizar theta a [-PI, PI] para evitar errores de acumulación
    if (odo->theta > M_PI)       odo->theta -= 2.0f * M_PI;
    if (odo->theta < -M_PI)      odo->theta += 2.0f * M_PI;
}
```

### 9.5 Errores comunes en odometría

| Error | Causa | Consecuencia |
|:------|:------|:-------------|
| Radio de rueda incorrecto | Estimación visual, no medición real | Error sistemático de escala en $x$, $y$ |
| Trocha ($L$) incorrecta | No medir entre centros de contacto | Error en el giro estimado |
| Deriva angular | Pequeños errores acumulados en $\theta$ | El robot «pierde el norte» en pocos metros |
| Deslizamiento | El modelo asume rodadura perfecta | Error no modelado, crece con aceleraciones |
| Aliasing del encoder | Muestreo demasiado lento | Subestimación de velocidad, sobre todo en curvas |

---

## 10. Guía de caracterización experimental

### 10.1 Ruta experimental: identificación del motor (recomendada)

#### Material necesario
- Microcontrolador (STM32) con PWM y timer en modo encoder
- Fuente de alimentación o batería
- Software para capturar datos (PuTTY, serial plotter, o script Python)

#### Procedimiento

**Paso 1: Montaje**
1. Fija el motor con la rueda **al aire** (sin contacto con el suelo).
2. Conecta el encoder a un timer del STM32 en modo encoder.
3. Prepara un script que envíe los datos por UART a 115200 baud (o superior).

**Paso 2: Aplicar escalón de voltaje**
1. Envía un comando para poner el PWM al 50% instantáneamente (por ejemplo, pasar de duty cycle 0 a 5000 en un timer de 10000).
2. Muestra la velocidad del encoder cada 1 ms.
3. Captura al menos 2 segundos de datos.

**Paso 3: Procesar los datos**
1. Convierte los pulsos del encoder a rad/s:
   $$\omega \, [\text{rad/s}] = \frac{\text{cuentas por segundo} \times 2\pi}{\text{PPR} \times 4}$$
   donde **PPR** (Pulses Per Revolution) es el número de pulsos base del encoder (p.ej., 7 pulsos por revolución) y el factor 4 es por el cuádruple conteo de flancos (A↑, A↓, B↑, B↓).

   > ⚠️ **Atención con la nomenclatura:** Algunos fabricantes llaman **CPR** (Counts Per Revolution) al valor que ya incluye el ×4. Si tu encoder especifica CPR = 28, entonces usa directamente: $\omega = \dfrac{\text{cuentas/s} \times 2\pi}{\text{CPR}}$.

2. Normaliza la curva: $\omega_{\text{norm}}(t) = \omega(t) / \omega_{\text{max}}$.

**Paso 4: Extraer $K$ y $\tau$**
1. **Ganancia $K$:**
   $$K = \frac{\omega_{\text{max}}}{V_{\text{aplicado}}} = \frac{\omega_{\text{max}}}{D \cdot V_{\text{bat}}}$$
   
   Donde $D$ es el duty cycle en tanto por uno (0.5 para 50%) y $V_{\text{bat}}$ el voltaje de la batería.

2. **Constante de tiempo $\tau$:**
   - Encuentra el tiempo $t_{63\%}$ donde $\omega(t) = 0.632 \cdot \omega_{\text{max}}$.
   - Entonces $\tau_m = t_{63\%}$.

3. **Verificación:** A $t = 4\tau$, la velocidad debe ser $\approx 98\%$ del valor final.

#### Ejemplo de captura de datos

```c
// Código de ejemplo para capturar respuesta al escalón (pseudocódigo)
#define SAMPLE_RATE 1000  // 1 kHz
#define BUFFER_SIZE 2000  // 2 segundos

uint32_t encoder_buffer[BUFFER_SIZE];
uint32_t t_buffer[BUFFER_SIZE];

void capture_step_response(void) {
    // Configurar PWM al 0%
    set_pwm_duty(0);
    delay_ms(100);

    // Iniciar captura
    for (int i = 0; i < BUFFER_SIZE; i++) {
        t_buffer[i] = get_us_tick();
        encoder_buffer[i] = get_encoder_count();
        if (i == 0) set_pwm_duty(5000);  // Escalón en t=0
        delay_us(1000);  // 1 ms entre muestras
    }

    // Enviar por UART
    for (int i = 0; i < BUFFER_SIZE; i++) {
        printf("%lu,%lu\r\n", t_buffer[i], encoder_buffer[i]);
    }
}
```

### 10.2 Ruta analítica (desde datasheet)

Si no puedes hacer mediciones (o como verificación cruzada), usa las fórmulas de la [sección 3.4](#34-obtención-de-parámetros-del-motor-desde-el-datasheet).

**Procedimiento resumido:**

1. Obtén $R$, $K_t$, $B$ del datasheet o calcúlalos.
2. Calcula $J_{\text{total}}$ según la [sección 5.4](#54-inercia-total-por-eje).
3. Calcula $K$ y $\tau_m$:
   $$K = \frac{K_t}{R B + K_t^2}, \quad \tau_m = \frac{R J_{\text{total}}}{R B + K_t^2}$$
4. Si hay reductora, aplica:
   $$K_{\text{eje}} = \frac{K}{n}, \quad \tau_{m,\text{eje}} = \tau_m$$

---

## 11. Ejemplo numérico completo

### 11.1 Datos del motor (N20 Micro Metal Gearmotor)

Supongamos un motor N20 con reductora 10:1. Es importante distinguir si un valor está medido en el **eje del motor** o en el **eje de salida** (tras la reductora).

| Parámetro | Valor | Unidad | Medido en |
|:----------|:-----:|:-------|:----------|
| $V_n$ | 6 | V | Motor |
| $\omega_0$ (motor) | 15000 | RPM | Eje motor |
| $\omega_0$ (eje) | 1500 | RPM | Eje salida |
| $I_0$ | 0.06 | A | Motor |
| $\tau_s$ (eje) | 0.33 | kg·cm | Eje salida |
| $I_s$ | 1.2 | A | Motor |
| $J_m$ (rotor) | 0.5 | g·cm² | Motor |
| $n$ (reductora) | 10 | — | — |
| $\eta$ (eficiencia) | 0.75 | — | Estimada |

> ⚠️ **Regla de oro:** Trabaja siempre con valores **del lado del motor** en los cálculos eléctricos y mecánicos (sección 3). Refleja al eje de salida solo al final.

### 11.2 Datos del robot

| Parámetro | Valor | Unidad |
|:----------|:-----:|:-------|
| $M$ (masa total) | 0.180 | kg |
| $r$ (radio rueda) | 0.016 | m |
| $L$ (trocha) | 0.050 | m |
| $m_r$ (masa rueda) | 0.003 | kg |
| $V_{\text{bat}}$ | 7.4 | V (2S LiPo) |

> [!NOTE] Ejemplo didáctico, no los valores del Rabosa-MVR
> Este capítulo resuelve un **ejemplo numérico** con valores redondos para que las cuentas sean
> fáciles de seguir. Los parámetros reales del Rabosa-MVR son $r = 10.5$ mm y $L \approx 83.6$ mm
> (ver [hardware.md](./hardware.md) y [cinemáticas.md](./cinemáticas.md)).
> Las **fórmulas** son las que se reutilizan; los números, no.

### 11.3 Paso 1: Convertir a unidades SI y reflejar al motor

**Velocidad del motor (sin carga):**
$$\omega_{0,\text{motor}} = 15000 \times \frac{2\pi}{60} = 1570.8\ \text{rad/s}$$

**Inercia del rotor:**
$$J_m = 0.5\ \text{g·cm}^2 \times 10^{-7} = 5 \times 10^{-8}\ \text{kg·m}^2$$

**Verificación cruzada con el torque de arranque:**
El torque en el eje de salida ($\tau_{s,\text{eje}} = 0.33\text{ kg·cm}$) reflejado al motor es:
$$\tau_{s,\text{motor}} = \frac{\tau_{s,\text{eje}}}{n \cdot \eta} = \frac{0.33 \times 9.81 \times 10^{-2}}{10 \times 0.75} = \frac{0.0324}{7.5} = 0.00432\ \text{N·m}$$

Esto nos da un $K_t$ alternativo: $K_t = 0.00432/1.2 = 0.00360\ \text{N·m/A}$, muy cercano al obtenido por vacío ($0.00363$). ✅

### 11.4 Paso 2: Calcular parámetros del motor (lado motor)

**Resistencia de armadura:**
$$R = \frac{V_n}{I_s} = \frac{6}{1.2} = 5\ \Omega$$

**Constante de torque/velocidad ($K_t = K_e$):**
El método más fiable es usar la **condición de vacío**, donde el voltaje aplicado equilibra la fuerza contraelectromotriz más la caída resistiva:

$$K_e = \frac{V_n - I_0 R}{\omega_{0,\text{motor}}} = \frac{6 - (0.06 \times 5)}{1570.8} = \frac{5.7}{1570.8} = 3.63 \times 10^{-3}\ \text{V·s/rad}$$

$$K_t = K_e = 3.63 \times 10^{-3}\ \text{N·m/A}$$

**Coeficiente de fricción viscosa $B$:**
En vacío, el torque del motor solo vence la fricción: $K_t I_0 = B \, \omega_0$

$$B = \frac{K_t I_0}{\omega_{0,\text{motor}}} = \frac{3.63 \times 10^{-3} \times 0.06}{1570.8} = 1.39 \times 10^{-7}\ \text{N·m·s/rad}$$

### 11.5 Paso 3: Calcular inercias (reflejadas al motor)

**Inercia de la rueda** (cilindro macizo, en el eje de salida):
$$J_r = \frac{1}{2} m_r r^2 = \frac{1}{2} \times 0.003 \times (0.016)^2 = 3.84 \times 10^{-7}\ \text{kg·m}^2$$

**Inercia del robot en cada rueda** (en el eje de salida):
$$J_{\text{robot}} = \frac{1}{2} M r^2 = \frac{1}{2} \times 0.180 \times (0.016)^2 = 2.304 \times 10^{-5}\ \text{kg·m}^2$$

**Inercia total reflejada al motor:**
$$J_{\text{total}} = J_m + \frac{J_r}{n^2} + \frac{J_{\text{robot}}}{n^2}$$

$$J_{\text{total}} = 5 \times 10^{-8} + \frac{3.84 \times 10^{-7}}{10^2} + \frac{2.304 \times 10^{-5}}{10^2}$$

$$J_{\text{total}} = 5 \times 10^{-8} + 3.84 \times 10^{-9} + 2.304 \times 10^{-7}$$

$$J_{\text{total}} \approx 2.842 \times 10^{-7}\ \text{kg·m}^2$$

> **💡 Observación:** Con reductora 10:1, la masa del robot ($2.304 \times 10^{-7}$) domina sobre el rotor ($5 \times 10^{-8}$). La rueda aporta muy poco ($3.84 \times 10^{-9}$).

### 11.6 Paso 4: Calcular $K$ y $\tau_m$ (lado motor)

$$K = \frac{K_t}{R B + K_t^2} = \frac{3.63 \times 10^{-3}}{5 \times 1.39 \times 10^{-7} + (3.63 \times 10^{-3})^2}$$

$$K = \frac{3.63 \times 10^{-3}}{6.95 \times 10^{-7} + 1.32 \times 10^{-5}} = \frac{3.63 \times 10^{-3}}{1.39 \times 10^{-5}} = 261\ \text{rad/s/V}$$

$$\tau_m = \frac{R J_{\text{total}}}{R B + K_t^2} = \frac{5 \times 2.842 \times 10^{-7}}{1.39 \times 10^{-5}} = \frac{1.421 \times 10^{-6}}{1.39 \times 10^{-5}} = 0.102\ \text{s} = 102\ \text{ms}$$

### 11.7 Paso 5: Reflejar al eje de salida (con reductora)

$$K_{\text{eje}} = \frac{K}{n} = \frac{261}{10} = 26.1\ \text{rad/s/V}$$

$$\tau_{m,\text{eje}} = \tau_m = 102\ \text{ms}$$

### 11.8 Paso 6: Incluir el driver PWM

$$G_{\text{final}}(s) = \frac{\Omega_{\text{eje}}(s)}{D(s)} = V_{\text{bat}} \cdot K_{\text{eje}} \cdot \frac{1}{\tau_m s + 1}$$

$$G_{\text{final}}(s) = 7.4 \times 26.1 \times \frac{1}{0.102\,s + 1}$$

$$G_{\text{final}}(s) = \frac{193}{0.102\,s + 1}$$

**Verificación con la velocidad sin carga:**
Con $V_{\text{bat}} = 7.4$ V y $D = 1.0$, la velocidad predicha es $\omega = 193$ rad/s ≈ 1844 RPM en el eje de salida. La velocidad máxima real del motor a 7.4 V sería:

$$\omega_{0,\text{eje}} \times \frac{V_{\text{bat}}}{V_n} = 1500 \times \frac{7.4}{6} = 1850\ \text{RPM}$$

Ambos valores coinciden (1844 vs 1850 RPM) → **el modelo es consistente** ✅

### 11.9 Paso 7: Velocidad lineal del robot

Con $D = 0.5$ en ambas ruedas (avance recto):

$$\omega_{\text{eje}} = D \cdot V_{\text{bat}} \cdot K_{\text{eje}} = 0.5 \times 7.4 \times 26.1 = 96.6\ \text{rad/s} \approx 922\ \text{RPM}$$

$$v = r \cdot \omega_{\text{eje}} = 0.016 \times 96.6 = 1.55\ \text{m/s}$$

**Velocidad máxima (D = 1.0):**
$$v_{\text{max}} = 0.016 \times 193 = 3.09\ \text{m/s}$$

> **✅ Resultado realista:** Un micromouse con reductora 10:1 alcanza ~1.5 m/s al 50% PWM y hasta ~3 m/s a máxima potencia. Los micromouse competitivos suelen operar en el rango de 2–4 m/s.

**Resumen del modelo final:**

$$G_{\text{final}}(s) = \frac{193}{0.102\,s + 1}$$

Con $D$ en tanto por uno (0–1) y $\Omega_{\text{eje}}$ en rad/s. La constante de tiempo $\tau_m = 102$ ms indica que el motor responde en ~102 ms al 63.2% y en ~408 ms al 98% de su velocidad final.

---

## 12. Implementación en microcontrolador

### 12.1 Estructura general del firmware

```
┌─────────────────────────────────────────────────────────────────────┐
│                        MAIN LOOP (1 kHz)                            │
│                                                                     │
│  ┌──────────┐   ┌──────────┐   ┌───────────┐   ┌───────────┐      │
│  │  Leer    │   │  Control │   │  Aplicar  │   │ Odometría │      │
│  │ Encoders │──►│   PID    │──►│   PWM     │   │ (por debajo) │   │
│  └──────────┘   └──────────┘   └───────────┘   └───────────┘      │
│       │                                                            │
│       ▼                                                            │
│  ┌──────────┐                                                      │
│  │ Calcular │                                                      │
│  │  ω_d, ω_i│                                                      │
│  └──────────┘                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 12.2 Cálculo de velocidad desde encoder

```c
// Configuración: timer en modo encoder, contando flancos A y B (x4)
// PPR = pulsos base del encoder (p.ej., encoder magnético de 7 pares de polos)
// Cuentas por revolución = PPR × 4 (cuádruple conteo: A↑, A↓, B↑, B↓)
#define PPR_ENCODER     7    // pulsos base por revolución
#define CUENTAS_POR_REV (PPR_ENCODER * 4)  // 28 cuentas por revolución (quadrature ×4)
#define RAD_POR_CUENTA  (2.0f * M_PI / CUENTAS_POR_REV)

float encoder_to_omega(int32_t delta_cuentas, float dt) {
    // delta_cuentas: diferencia de cuentas del timer encoder entre dos lecturas
    // dt: tiempo entre lecturas en segundos
    return (float)delta_cuentas * RAD_POR_CUENTA / dt;
}
```

### 12.3 Bucle de control completo

```c
#define DT             0.001f    // 1 ms
#define RADIO_RUEDA    0.016f    // metros
#define TROCHA         0.050f    // metros

// Estados
static odometry_t odo = {0};
static float omega_d = 0, omega_i = 0;
static int32_t last_enc_d = 0, last_enc_i = 0;

void main_loop(void) {
    uint32_t last_tick = get_tick_us();

    while (1) {
        uint32_t now = get_tick_us();
        float dt = (now - last_tick) / 1000000.0f;
        last_tick = now;

        // 1. Leer encoders
        int32_t enc_d = get_encoder_d();
        int32_t enc_i = get_encoder_i();

        // 2. Calcular velocidades
        omega_d = encoder_to_omega(enc_d - last_enc_d, dt);
        omega_i = encoder_to_omega(enc_i - last_enc_i, dt);
        last_enc_d = enc_d;
        last_enc_i = enc_i;

        // 3. Cinemática directa
        float v = RADIO_RUEDA * (omega_d + omega_i) / 2.0f;
        float theta_dot = RADIO_RUEDA * (omega_d - omega_i) / TROCHA;

        // 4. Odometría
        odometry_update(&odo, v, theta_dot, dt);

        // 5. Control PID (a implementar)
        // set_pwm_derecha(pid_d_update(reference_d, omega_d));
        // set_pwm_izquierda(pid_i_update(reference_i, omega_i));

        // 6. Sincronización (opcional, podría usar un timer)
        while ((get_tick_us() - now) < (uint32_t)(DT * 1000000));
    }
}
```

### 12.4 Consideraciones sobre el tiempo de muestreo

- **Frecuencia de control recomendada:** 500 Hz – 1 kHz (DT = 1–2 ms).
- **Frecuencia mínima:** $10 \times$ el ancho de banda del sistema. Con $\tau_m = 102$ ms del ejemplo, el ancho de banda es $f_c = 1/(2\pi\tau_m) \approx 1.56$ Hz → necesitarías muestrear solo a **≥ 16 Hz**. Una frecuencia de control de **500 Hz–1 kHz** es más que suficiente.
- **En la práctica:** Con $\tau_m = 102$ ms, la dinámica del motor con reductora 10:1 es bastante lenta, por lo que un control a **500 Hz** es más que suficiente. Los micromouse competitivos suelen ejecutar su lazo de control a 1 kHz.

---

## 13. Hoja de ruta: del modelo al PID

### 13.1 Resumen de la secuencia completa

| Paso | Acción | Fórmula clave | Resultado |
|:----:|:-------|:--------------|:----------|
| 1 | Obtener datos del motor | $R = V_n/I_s$, $K_t = \tau_s/I_s$, $B = K_t I_0/\omega_0$ | Parámetros eléctricos y mecánicos |
| 2 | Calcular inercias | $J_{\text{total}} = J_m + \frac{J_r + J_{\text{robot}}}{n^2}$ | Inercia equivalente |
| 3 | Calcular $K$ y $\tau_m$ | $K = \frac{K_t}{RB+K_t^2}$, $\tau_m = \frac{RJ}{RB+K_t^2}$ | Modelo de 1.er orden |
| 4 | Reflejar al eje | $K_{\text{eje}} = K/n$ | Modelo con reductora |
| 5 | Verificar experimentalmente | Escalón de voltaje, medir $\omega(t)$ | Validar $K$ y $\tau$ |
| 6 | Sintonizar PID | Método de Ziegler-Nichols o lugar de las raíces | Controlador |

### 13.2 Sintonización del PID (próximo paso)

Una vez que tengas $G(s) = \frac{K}{\tau s + 1}$, puedes sintonizar un PI (no hace falta D para 1.er orden) mediante:

**Método de Ziegler-Nichols en lazo abierto:**

1. Aplica un escalón y registra la respuesta.
2. Identifica $K$ (ganancia), $\tau$ (constante de tiempo) y $L$ (tiempo muerto, si existe).
3. Si $L \approx 0$ (tu caso), usa el método de cancelación de polos:
   $$K_p = \frac{\tau}{K \cdot T_c}, \quad T_i = \tau$$
   donde $T_c$ es la constante de tiempo deseada en lazo cerrado (normalmente $T_c \approx \tau/5$ a $\tau/10$).

### 13.3 Próximos pasos recomendados

1. ✅ **Modelar** los subsistemas (este documento).
2. **Medir** los parámetros reales de tus motores N20.
3. **Implementar** la captura de respuesta al escalón.
4. **Sintonizar** el PI de velocidad para cada rueda.
5. **Implementar** el control de velocidad en lazo cerrado.
6. **Añadir** el lazo de posición (control de trayectoria).
7. **Implementar** el algoritmo de Wall Follower o resolución de laberinto.

