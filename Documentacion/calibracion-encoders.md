# 🎯 Calibración de los encoders — teoría y procedimiento

> **Documento de estudio.** Recoge el porqué, las cuentas y el procedimiento de la
> **prueba 7** (`App_Test_PPR()` y `App_Test_UmCuenta()`, los dos modos del menú).
> Prueba relacionada: [pruebas-bringup.md](./pruebas-bringup.md#2-catálogo-de-pruebas) ·
> Hardware: [hardware.md](./hardware.md#42-encoders)

---

## 1. Qué queremos averiguar (y por qué no lo dice el datasheet)

Del conjunto motor + encoder + rueda hay **tres números** que la odometría necesita:

| # | Magnitud | Qué es | Dato del fabricante (21-sep) | ¿Se puede dar por bueno? |
|:-:|:---------|:-------|:------------------------------|:-------------------------|
| 1 | **PPR** | Pulsos por vuelta del **eje del motor** (antes de la reductora) | **7** por canal | ✅ Casi: la medición lo verifica (y tiene que salir **entero**) |
| 2 | **n** (reducción) | Vueltas del motor por vuelta de la rueda | **1:10** (caja GA12) | ✅ Confirmado en la propia tabla del fabricante |
| 3 | **µm/cuenta** | Lo que avanza el robot por cada cuenta | 235,6 µm con rueda Ø21 mm | ❌ **No**: depende del radio **de rodadura**, no del geométrico — la goma se deforma y las ruedas deslizan |

> [!IMPORTANT] El problema de fondo
> El PPR y la reducción ya los publica el fabricante, pero **el factor de distancia no**: depende
> del radio de rodadura. Y una sola medida no separa $n$ de PPR (solo da el producto
> $n \cdot \text{PPR}$), así que conviene girar la rueda para verificar el producto **y** medir la
> distancia con la regla. De ahí que la prueba tenga dos modos.

---

## 2. El modelo de conteo

El contador de TIM2/TIM3 está en modo **encoder TI12**, también llamado *x4*: cuenta **todos**
los flancos de los dos canales (subida y bajada de A, y subida y bajada de B).

Por cada vuelta **del eje del motor** el canal A genera $\text{PPR}$ pulsos ⇒ el contador da
$4 \times \text{PPR}$ cuentas.

Por cada vuelta **de la rueda** el motor gira $n$ veces:

$$\boxed{\ \text{cuentas por vuelta de rueda} = 4 \times n \times \text{PPR}\ }$$

Y para un número cualquiera de vueltas $V$ giradas a mano:

$$\text{cuentas} = 4 \times n \times \text{PPR} \times V$$

Despejando el PPR:

$$\text{PPR} = \frac{\text{cuentas}}{4 \times n \times V}$$

### Con los números de nuestro robot

Con $V = 10$ vueltas y la hipótesis $n = 10$:

$$\text{PPR} = \frac{\text{cuentas}}{4 \times 10 \times 10} = \frac{\text{cuentas}}{400}$$

| Si lees… | …el PPR es |
|:--------:|:----------:|
| 2800 | **7** |
| 4400 | 11 |
| 5200 | 13 |

> [!TIP] La regla de oro
> El PPR **tiene que salir entero**. Si te sale 6.98 o 7.03, perfecto (es ruido de contar).
> Si te sale 3.5 o 7.5, **la reducción asumida es falsa**: repite el cálculo ajustando
> `CAL_REDUCCION` hasta que el PPR salga entero. La reducción es siempre un entero pequeño
> (5, 10, 20, 30, 50, 100, 150, 300…).

---

## 3. Las dos medidas

### 3.1 Medida A — a mano (modo 1)

**Objetivo:** PPR y verificación de la reducción. No mueve motores.

Giras la rueda $V$ vueltas exactas y lees cuántas cuentas ha acumulado el contador. Como no hay
motores implicados, no hay deslizamiento ni inercia: es la medida **más limpia** de las dos.

Ventaja añadida: si haces la prueba **con el robot en el suelo**, la rueda rueda de verdad y
puedes medir con la regla lo que avanza. Eso te da el PPR *y* el µm/cuenta en un solo paso.

### 3.2 Medida B — con motores (modo 2)

**Objetivo:** µm/cuenta, que es lo que realmente usa la odometría.

El robot avanza a lazo abierto (PWM fijo, sin PID) y se cuenta la distancia. El problema es que
dos motores iguales **no giran igual**: a la misma consigna, uno tira más que el otro y el robot
describe un arco. Y una distancia medida sobre un arco no sirve.

**El truco que usa la prueba:** cada rueda se detiene **cuando ella misma** alcanza el objetivo
de cuentas. No se para el conjunto, se paran por separado. Así las dos ruedas recorren la misma
distancia total y el robot tiende a ir recto, **sin necesidad de PID**. Es un lazo abierto
"igualador" muy simple y suficiente para calibrar.

```
while (alguna rueda siga girando) {
    si (rueda_izq < objetivo)  motor_izq = DUTY;  si no → frenar motor_izq
    si (rueda_der < objetivo)  motor_der = DUTY;  si no → frenar motor_der
}
```

Después se dejan 500 ms contando: **las ruedas siguen girando durante la frenada y ese tramo
también es distancia recorrida**. Contarlo es lo que hace coherentes la regla y el contador.

---

## 4. Los números esperados del Rabosa-MVR

Datos de partida ([hardware.md](./hardware.md#41-motores-transmisión-y-chasis)):

| Dato | Valor |
|:-----|:------|
| Diámetro de rueda con goma | 21 mm ⇒ radio 10.5 mm |
| Perímetro de la rueda | $\pi \times 21 \approx 65.97$ mm |
| **PPR del encoder** (fabricante) | **7** pulsos por canal y vuelta del eje del motor |
| **Reducción** (fabricante) | **1:10** |
| Objetivo de la prueba | 2800 cuentas por rueda (≈10 vueltas) |

$$\text{µm/cuenta} = \frac{\text{distancia}}{\text{cuentas}} = \frac{10 \times 65.97\,\text{mm} \times 1000}{2800} \approx 236\ \text{µm}$$

La predicción completa:

$$\boxed{\ 2800\ \text{cuentas} \Rightarrow 10\ \text{vueltas} \Rightarrow 660\ \text{mm}\ }$$

| Escenario | cuentas/vuelta de rueda | µm/cuenta | Distancia para 2800 cuentas |
|:----------|:-----------------------:|:---------:|:---------------------------:|
| **PPR = 7, n = 10** ← *lo que dice el fabricante* | **280** | **236 µm** | **660 mm** |
| PPR = 11, n = 10 | 440 | 150 µm | 420 mm |
| PPR = 13, n = 10 | 520 | 127 µm | 356 mm |
| PPR = 7, n = 30 | 840 | 79 µm | 220 mm |

> [!NOTE] Qué significa "236 µm por cuenta"
> Es una resolución bastante **gruesa**: cada cuenta equivale a casi un cuarto de milímetro.
> A 1 kHz de muestreo y 1 m/s el contador avanza ≈4.2 cuentas por milisegundo, así que se puede
> medir bien la velocidad; pero **el error de cuantización por muestra (~0.24 mm) es el suelo
> del error de la odometría**. Si hiciera falta más resolución habría que ir a un encoder con
> más PPR o a relaciones de reducción mayores.

---

## 5. Procedimiento paso a paso

### Paso 1 — `PPR A MANO` (entrada del menú)

1. La prueba se lanza desde el menú (**TEST → MOTORES → `PPR A MANO`**), no hay que tocar `APP_Run()`
   ni recompilar: los dos modos están en el menú.
2. `cd firmware && platformio run -t upload`
3. `platformio device monitor -b 115200` (o Teleplot).
4. Marca la rueda con cinta y haz una marca fija en el chasis.
5. Gira la rueda **10 vueltas exactas**, despacio y **siempre en el mismo sentido**. En la OLED se ve
   el número de pulsos y el PPR **en vivo**.
6. Lee `CAL_P1` al terminar. La prueba ya imprime `CAL_PPR1` calculado.

### Paso 2 — `UM MOTORES` (entrada del menú)

1. Menú: **TEST → MOTORES → `UM MOTORES`** (⚠️ el robot avanza).
2. Marca en el suelo la posición de partida.
3. Ejecutar: el robot avanza 2800 cuentas por rueda y frena; la pantalla se queda con `P1`/`P2` y
   `MIDE CON REGLA`.
4. Mide con la regla la distancia desde la marca.
5. Pon ese valor (en mm) en `CAL_DISTANCIA_MEDIDA_MM`, reflashea y ejecuta de nuevo: imprime
   `CAL_UM_CUENTA`.
6. Ese valor es el que se pasa a `set_micrometers_per_count()`.

> [!WARNING] Siempre una pasada en el aire primero
> Antes de dejarlo correr por el suelo, sujétalo con las **ruedas en el aire** y comprueba que
> las dos giran hacia delante y que los pulsos suben. Si una gira al revés, hay que invertirla
> en `DRV8833_Set_Speed()`.

---

## 6. Fuentes de error

| Fuente | Efecto | Cómo se detecta |
|:-------|:-------|:----------------|
| **Deslizamiento** de rueda | Distancia real < predicción | La distancia medida sale corta de forma sistemática |
| **Overshoot** al frenar | Distancia real > predicción | Sale larga; reducir `CAL_DUTY_MOTOR` |
| **Reducción mal asumida** | PPR no entero | El PPR da decimales "feos" (3.5, 7.5) |
| **Vueltas contadas a mano** | Todo el cálculo se desplaza | PPR con error constante; repetir 3 veces |
| **Radio de rodadura ≠ geométrico** | µm/cuenta desviado | La medida con regla no cuadra con la geometría |
| **No girar siempre en el mismo sentido** | Cuentas que se restan | `CAL_P1` final menor de lo esperado |
| **Saturar la UART** | Valores perdidos | La prueba imprime a 10 Hz por este motivo |

---

## 7. Qué se hace con el resultado

1. `set_micrometers_per_count(<CAL_UM_CUENTA>)` antes de usar `update_encoder_readings()`.
   Sin ese valor, `micrometers_per_count` vale 0 y las distancias son basura (la función lo
   sustituye por 1.0 para no dividir por cero, pero el resultado no significa nada).
2. Anotar en [hardware.md](./hardware.md) el PPR, la reducción y el µm/cuenta **medidos**,
   sustituyendo las hipótesis.
3. Actualizar [estado-del-proyecto.md](./estado-del-proyecto.md) y el registro de
   [pruebas-bringup.md](./pruebas-bringup.md).

### Lo que aún faltará para la odometría

Con esto quedan resueltos el radio de rodadura $r$ y el factor de conversión. Pero la cinemática
diferencial necesita además la **trocha efectiva $L$**, que se mide de otra forma:

$$L = \frac{2\pi \cdot d}{\theta} \quad\text{(girando el robot sobre sí mismo: distancia } d \text{ por rueda, ángulo } \theta \text{ girado)}$$

Esa es la siguiente calibración pendiente. Ver [cinemáticas.md](./cinemáticas.md#4-parámetros-cinemáticos-del-micromouse).

---

## 8. Referencias

- [pruebas-bringup.md](./pruebas-bringup.md) — protocolo y registro de pruebas
- [hardware.md](./hardware.md) — motores, encoders y geometría
- [odometria.md](./odometria.md) — dónde entra el resultado
- [cinemáticas.md](./cinemáticas.md) — parámetros $r$, $L$ y perfiles de movimiento
- [modelado.md](./modelado.md) — modelado del motor y de la reductora
