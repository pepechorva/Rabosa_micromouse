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
| Límite de `Kp` | `≈ 0,2` · con `0,3` el lazo satura y oscila |

**Lo que limita `Kp` es el retardo del lazo, no la planta.** El retardo es de ~25 ms (la media móvil de
20 muestras del encoder más los 10 ms del lazo) y `tau ≈ 45 ms`, así que `T/tau ≈ 0,6`: el margen de
fase se agota en `K·Kp ≈ 3`, o sea `Kp ≈ 0,2`. Comprobado que `Kp = 0,3` **satura** (±60 % de duty, la
velocidad de −725 a +1112 mm/s y las dos ruedas desincronizadas).

**Corolario: el error de régimen lo quita el integrador, no la P.** Con `Kp` topado en 0,1 y sin `I`,
la velocidad se quedaba en **180 mm/s pidiendo 300** (un 40 % de error: es exactamente lo que predice
`v = K·Kp/(1+K·Kp)·r`). Poniendo el cero del PI una década por debajo del cruce (`Ki/Kp = 5 rad/s`
frente a `wc ≈ 25 rad/s`) el error baja al 0,5 % manteniendo un margen de fase de ~77°.

`Kd = 0` **a propósito**: la planta es de primer orden y el derivativo solo amplificaría el ruido de
cuantificación del encoder, que es de unos 7 mm/s.

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
