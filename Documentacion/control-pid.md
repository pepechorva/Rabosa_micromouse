# 🎛️ CONTROL — lazo de velocidad (diseño e implementación)

> **Estado:** ✅ **implementado el 2026-09-28** y **verificado en el PC** (simulación).
> Pendiente de **sintonizar sobre el robot** con la prueba 18
> ([pruebas-bringup.md](./pruebas-bringup.md) §14).
> Capa: `control/`. Contrato de capas: [Layered Architecture.md](./Layered%20Architecture.md).

---

## 1. Qué hay

| Fichero | Qué hace |
|:--------|:---------|
| `control/pid.c/h` | PID genérico: **anti-windup**, derivada **sobre la medida** y filtrada, `dt` validado |
| `control/setpoint.c/h` | **Rampa de consigna** (limitador de pendiente) |
| `control/controller.c/h` | El lazo de **una rueda**: consigna + PID → duty |

**Unidades acordadas**: velocidad en **mm/s** y duty en **% (−100…100)**. Por tanto `Kp` va en
%/(mm/s), `Ki` en %/(mm/s·s), etc.

⚠️ `controller.c` **no lee encoders ni escribe PWM**: entra la velocidad medida y sale el duty. Así
CONTROL no conoce ni el HAL ni el sensor, el lazo se puede probar en el PC y no se rompe la regla
de capas. El cableado (leer encoder → llamar al lazo → escribir el driver) lo hace la capa **APP**,
que durante el bring-up es la única autorizada a saltar capas.

---

## 2. El lazo

```mermaid
graph LR
    SP["Rampa de consigna"] --> SUM["Error = consigna menos medida"]
    SUM --> PID["PID: Kp Ki Kd"]
    PID --> LIM["Tope de duty"]
    LIM --> DRV["DRV8833"]
    DRV --> MOT["Motor y rueda"]
    MOT --> ENC["Encoder en mm/s"]
    ENC --> SUM
```

Cada rueda tiene **su propio lazo** (su PID y su rampa): es la forma de que el robot vaya recto
aunque los dos motores no sean idénticos.

---

## 3. Decisiones de diseño

| Decisión | Por qué |
|:---------|:--------|
| **Anti-windup por recorte del integrador** | Si la salida se satura (duty al tope, rueda atascada), el término I se recorta para no quedarse «cargado»: al salir de la saturación el duty responde en milisegundos, no en segundos |
| **Derivada sobre la MEDIDA**, no sobre el error | Cambiar la consigna de golpe no produce el pico clásico (*derivative kick*) |
| **Derivada filtrada** (α = 0.2, media exponencial) | La velocidad del encoder es ruidosa: derivarla en crudo mete ese ruido en el duty |
| **`dt` validado** | Si el bucle se queda parado (un `HAL_Delay` largo, un breakpoint del depurador), no se integra ni se deriva: solo actúa la P. Evita que un `dt` enorme dispare el integrador |
| **Rampa de consigna obligatoria** | Un escalón pide un pico de corriente que la LiPo de 450 mAh y el DRV8833 notan, y hace patinar las ruedas → falsea la odometría. La rampa es la regla del proyecto |
| **Lazo a 100 Hz con encoders a 1 kHz** | Suficiente para el ancho de banda del motor y deja CPU libre para el resto del robot |

---

## 4. Ganancias sintonizadas

**`Kp = 0.10` · `Ki = 0.50` · `Kd = 0.00`**, con duty máximo del 60 % durante la prueba (70 % en las
primitivas de `app/motion.c`). Medidas sobre el robot el **2026-10-01**.

### Lo que se midió (y por qué estos valores)

| Medida | Valor |
|:-------|:------|
| Planta duty → velocidad (ruedas libres) | `K ≈ 15 mm/s` por 1 % de duty · `tau ≈ 45 ms` |
| Error de régimen con estas ganancias | **0,5 %** (298,5 mm/s de mediana pidiendo 300) |
| Asentamiento tras un escalón | 0,6 s |
| Límite de `Kp` | entre **0,10** (bien) y **0,20** (oscila) · medido el 11-oct ya con el lazo en interrupción |

**Lo que limita `Kp` es el retardo del lazo, no la planta.** El retardo es de ~25 ms (la media móvil de
20 muestras del encoder más los 10 ms del lazo) y `tau ≈ 45 ms`, así que `T/tau ≈ 0,6`: el margen de
fase se agota en `K·Kp ≈ 3`, o sea `Kp ≈ 0,2`. Comprobado que `Kp = 0,3` **satura** (±60 % de duty, la
velocidad de −725 a +1112 mm/s y las dos ruedas desincronizadas).

**Corolario: el error de régimen lo quita el integrador, no la P.** Con `Kp` topado en 0,1 y sin `I`,
la velocidad se quedaba en **180 mm/s pidiendo 300** (un 40 % de error: es exactamente lo que predice
`v = K·Kp/(1+K·Kp)·r`). Poniendo el cero del PI una década por debajo del cruce (`Ki/Kp = 5 rad/s`
frente a `wc ≈ 25 rad/s`) el error baja al 0,5 % manteniendo un margen de fase de ~77°.

`Kd = 0` **a propósito**: la planta es de primer orden y el derivativo solo amplificaría el ruido de
cuantificación del encoder, que es de **14,3 mm/s** (una cuenta de 287 µm dividida por la ventana de
20,02 ms; medido el 11-oct).

### Ganancias por rueda (medido el 2026-10-11)

Las ganancias se guardan **una por rueda** (`KP IZQ`, `KP DER`…) porque los dos N20 nunca tienen por
qué ser iguales. **Al medirlo, resulta que estos dos sí lo son**, y conviene dejar el número escrito:

| Medida (ruedas en el aire) | IZQ | DER | Diferencia |
|:--|--:|--:|--:|
| duty a 80 mm/s (con integrador), n = 180 | 10,931 % | 10,959 % | **−0,028 % ± 0,181** (0,2 σ) |
| duty a 300 mm/s, n = 826 | 12,469 % | 12,408 % | **+0,061 % ± 0,113** (0,5 σ) |
| régimen a 300 mm/s, n = 321 | 176,76 mm/s | 177,12 mm/s | **0,36 mm/s** (error de la media ±0,47) |

⇒ **Los dos motores piden el mismo duty para la misma velocidad, dentro del ruido** (0,2-0,5 σ). El
método *detectaría* la diferencia: un 9 % de planta daría **+6,3 mm/s** a 300 mm/s, diez veces por
encima del error de la media. **Por eso las dos ruedas llevan hoy las mismas ganancias**: no hay nada
que compensar, y escribir una asimetría que no existe sería peor que no tocar nada.

> [!WARNING] Lo que esta medida **no** cubre
> Todas las pruebas de este proyecto se hacen **con las ruedas en el aire** (instrucción vigente), y en
> el aire **no hay carga**. La asimetría de unos N20 aparece sobre todo **con carga** (roce del eje,
> puntos duros de la reductora, y sobre todo el duty de arranque). ⇒ La estructura por rueda está
> puesta y **probada** para el día en que las pruebas pasen al suelo: ese día se repite esta medida con
> el robot pesando y, si sale diferencia, se escribe en `PID_TEST_*_DEF` (`app/application.c`).

#### La planta, medida en tres puntos

| Punto | duty | velocidad |
|:--|--:|--:|
| Baja velocidad (con integrador; error −2,6 mm/s) | 10,93 % | 77,4 mm/s |
| Equilibrio del lazo **P** (`Ki = 0`, `Kp = 0,1`, pidiendo 300) | 12,32 % | 176,8 mm/s |
| Régimen a 300 mm/s | 13,63 % | 300,5 mm/s |

Los dos primeros dan la recta $v \approx 71{,}5\,(duty - 9{,}85\,\%)$: **unos 71,5 mm/s por punto de
duty con un escalón muerto de ~10 %**. Es la «rampa de cada motor» y explica por qué a duty bajo el
motor solo **zumba**: por debajo del escalón muerto no hay par que venza la fricción.

⚠️ El tercer punto se sale de esa recta un **10 %**, así que el modelo afín es **una aproximación, no
una ley**: sirve para el *feedforward* (compensar el escalón muerto), **no** para calcular la
velocidad a lazo abierto.

> ℹ️ El duty de trabajo a 300 mm/s es **13,6 %**, muy por debajo del **21-25 %** que se midió como
> «duty mínimo de arranque» sobre el suelo. No se contradicen: son cosas distintas (una rueda **que ya
gira** con el robot en el aire, frente a **arrancar parada** con el peso del robot encima).

#### `Kp = 0,20` sigue siendo inestable (comprobado el 2026-10-11)

Con la interrupción puesta se esperaba poder subir `Kp`. **No se puede, y el porqué es lo interesante**:

| `Kp` | σ(v) en régimen | σ(duty) | Veredicto |
|:--:|--:|--:|:--|
| **0,10** | 10,3 y 13,2 mm/s (**0,7 y 0,9 cuentas**) | 1,2 % | ✅ quieto: el residuo está por debajo del escalón del encoder |
| 0,20 | **151 y 83 mm/s** (10,5 y 5,8 cuentas) | **28 %** | ❌ oscila (picos de 817 mm/s pidiendo 300) |

El motivo: en el **banco** el bucle ya iba a **1,9 ms**, así que la media móvil ya duraba sus 20 ms y
lo único que el ISR quitó fue el retardo del lazo (de 10 a 0,5 ms). El retardo total pasó de ~20 a
~15 ms, solo **1,3×**, y el límite de `Kp` se mueve con él. **El premio del ISR no está en el banco,
está en la misión**, donde ese mismo filtro pasaba de 200 a 20 ms (10×).

### Ziegler-Nichols se descartó

La velocidad de una rueda es un sistema de **primer orden**, y un lazo proporcional sobre un sistema de
primer orden **no oscila nunca**, suba lo que suba `Kp`. El barrido automático de `PID_TEST_MODO 3`
llegó a `Kp = 2,0` sin encontrar la oscilación crítica: el método necesita **fase**, y aquí la única que
hay son los ~10 ms de la media móvil del encoder. La sintonía se hizo **por respuesta al escalón**
(procedimiento en [pruebas-bringup.md](./pruebas-bringup.md) §14).

### ⚠️ Dos trampas que costaron la tarde del 2026-10-01

1. **El integrador del PID guarda un término de SALIDA en %, no un acumulador del error.** Con `Ki = 0`
   deja de acumular pero **conserva su último valor**, y ese valor **sigue sumándose a la salida**: con
   `KP = KI = KD = 0` el duty se quedaba en 17,6 % y 60 % en vez de irse a cero. Es decir, «dejar solo
   la P» **no dejaba solo la P**. `PID_SetGanancias()` ahora llama a `PID_Reset()`.
2. **Los motores van en espejo**: en un diferencial los dos giran al revés entre sí para el mismo
   voltaje, así que **solo uno** necesita `invertido = 1`. Y ojo: **cruzar los canales del PWM e
   invertir el signo son la misma inversión**; hacer las dos cosas a la vez las cancela y el motor se
   queda como estaba.

> **Lección de método**: no se diagnostica un lazo **oscilando** con una captura de telemetría. Con el
> sistema saturado, el duty y la velocidad de un mismo bloque pueden ser de instantes distintos. Para
> averiguar un signo hay que **congelar el lazo** (ganancias a cero) y medir en régimen.

### Se ajustan en caliente (2026-10-01)

Las ganancias y la consigna **no exigen recompilar**: la consola las cambia con el lazo corriendo
(`PID KP 0.35`, `PID KI 0.8`, `PID VEL 300`; ver [comandos-serie.md](./comandos-serie.md)).

El reparto es el que ya estaba acordado: la **APP** es la dueña del ajuste (`APP_PID_SetGanancia()`),
la consola solo lo deja escrito, y es el **lazo** quien lo empuja a CONTROL con `CTRL_SetGanancias()`
en su paso siguiente. Así la consola no toca el lazo por su cuenta ni se salta ninguna capa.

Ziegler-Nichols necesita **barrer `Kp` muchas veces**; sin esto, cada tanteo son un recompilar, un
flashear y un reinicio que borra la observación que se estaba haciendo.

---

## 5. Verificación en el PC (sin hardware)

Como la capa es matemática pura, se puede validar **antes** de tener el robot montado. El banco de
pruebas del PC está en **`firmware/test/pid_sim.c`** (no entra en el build del firmware) y simula una
planta de motor de primer orden (10 mm/s por cada 1 % de duty, τ = 0.15 s):

```bash
gcc -I firmware -O2 -Wall -o /tmp/pid_sim firmware/test/pid_sim.c \
    firmware/control/pid.c firmware/control/setpoint.c firmware/control/controller.c -lm
/tmp/pid_sim
```

**Resultado del 2026-09-28:**

| Caso | Sobreimpulso | Error final | Veredicto |
|:-----|:------------:|:-----------:|:---------:|
| Rampa de consigna (0 → 300 mm/s) | −0.2 % | 0.2 % | ✅ |
| Escalón puro de consigna | −0.1 % | 0.1 % | ✅ |
| Consigna inalcanzable (anti-windup) | — | — | ✅ el duty vuelve a < 1 % en **20 ms** después de 5 s saturado |

> La simulación valida la **lógica**, no la sintonía: sobre el robot real hay rozamiento,
> deslizamiento, batería que cae y motores distintos entre sí.

---

## 6. Qué falta (siguientes fases)

- **PID lateral** (error = distancia_izq − distancia_der): necesita los sensores IR (pruebas 15–17)
  y debe ser **más lento** que el de velocidad.
- Repetir la sintonía si la batería cambia mucho de tensión.
- **Odometría** (`filter/odometry.c`) para posición y giros de 90°.
- **Feed-forward**: sumar un duty aproximado a la consigna y dejar al PID solo el error (el duty
  necesario depende de la batería; el PID solo no distingue «cuesta abajo» de batería baja).
- Límite de **aceleración** por batería: si el pack está bajo, bajar la consigna máxima.
