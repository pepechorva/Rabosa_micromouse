# 🔁 El lazo de control en interrupción (TIM1)

> **Estado:** 🚧 **EN CURSO** al 2026-10-11. **Etapa 1 hecha y verificada en el robot** (la
> interrupción corre, no regula nada todavía); faltan la **etapa 2** (el PID dentro) y la **etapa 3**
> (ganancias por rueda). El diseño y las medidas están aquí; el código, en `firmware/app/control_loop.c`.
> Contexto: [control-pid.md](./control-pid.md) · [comandos-serie.md](./comandos-serie.md) ·
> [esquema-firmware.md](./esquema-firmware.md) · [estado-del-proyecto.md](./estado-del-proyecto.md)

---

## 1. El problema, con números

El 2026-10-11 se añadió un cronómetro al firmware (TIM4, 2 µs por cuenta; orden `LAZO` de la consola) y
se midió **en el robot**, con la prueba 18 en marcha y las ruedas en el aire:

| Tramo | Medido | Cada cuánto |
|:--|--:|:--|
| Una vuelta normal del bucle | **1,9 ms** | — |
| `OLED_Update()` (552 B por I²C a 100 kHz) | **52,8 ms** | cada 200 ms |
| Una línea de telemetría (≈21 B a 115200) | **1,8 ms** | ~8 ms por bloque de cinco, cada 50 ms |
| **La vuelta peor** | **68,8 ms** | cada 200 ms |

**36 veces la vuelta normal.** Y no es un caso raro: son ~40 % del tiempo sin muestrear, repartido en
un pico periódico de 52,8 ms cada 200 ms (la pantalla) y otro de ~8 ms cada 50 ms (la telemetría).

Por qué pasa: **el bucle de control comparte hilo con todo lo demás**. `OLED_Update()` y
`UART_SendData()` son **bloqueantes** (el primero espera a que salgan 552 bytes por el bus; el segundo
usa `HAL_UART_Transmit` con `HAL_MAX_DELAY`), así que mientras cualquiera de los dos está dentro, el
PID **no corre**.

### 🤦 Y hay algo peor, que salió de leer el estimador con estos números delante

La media móvil de la velocidad son **20 muestras**, y su ventana de tiempo es **la suma de los `dt`**
(`velocity_suma_dt`), no 20 ms. O sea que **la constante de tiempo del filtro depende de cada cuánto
se llame a `update_encoder_readings()`**:

| Dónde | Cada cuánto se llama | Ventana | Retraso del filtro $(N-1)/2 \times T$ |
|:--|--:|--:|--:|
| Prueba 18 (banco) | ~1,9 ms | ~38 ms | **~18 ms** |
| **`MOTION_Avanzar` / `MOTION_Girar` (misión)** | **~10 ms** | **~200 ms** | **~95 ms** ⚠️ |

⚠️⚠️ **En la misión la velocidad se mide con un retraso de casi 100 ms**, cinco veces el de la prueba
18. Y eso tiene dos consecuencias graves:

1. **`Kp` se sintonizó en el banco**, con ~18 ms de retraso, y se aplica en la misión con ~95 ms. No es
   que la ganancia esté «conservadora»: está calculada para otro lazo.
2. El filtro de 9,5 ms que se citaba el 1-oct **solo valía para el banco**. La misión nunca ha tenido
   ese número.

📌 **Lección**: un promedio móvil de N **muestras** no tiene una constante de tiempo conocida hasta
saber **a qué ritmo se le dan** las muestras. «Medir el `dt`» arregla el *escalado* de la velocidad,
pero **no** la duración de la ventana.

⇒ Esto, y no la OLED, es el argumento más fuerte para el ISR: **muestrear a 1 kHz fijo devuelve la
ventana a 20 ms y el filtro a 9,5 ms, siempre el mismo**, en el banco y en la misión.

### Qué consecuencias tiene

1. **Es lo que limita `Kp`.** El techo de ~0,2 que se calculó el 1-oct salía de un retardo de ~25 ms;
   con picos de 68,8 ms el margen de fase es mucho menor. La ganancia que hoy funciona está
   **conservadora** para el lazo nominal y **justa** en el peor caso.
2. **Es una perturbación de 5 Hz.** El pico no es aleatorio: llega cada 200 ms, con el duty congelado
   durante 52,8 ms. Un lazo con más ganancia entraría en oscilación justo a esa frecuencia.
3. **Y contamina el banco**: la prueba 18 es donde se juzga `Kp`, y es donde más se pinta en pantalla.
   Se está sintonizando con un instrumento que se rompe solo.

> ⚠️ **Lo bueno**: la misión (`app/motion.c`, `app/wall_follower.c`) **no toca la OLED**, así que en
> el laberinto el pico grande no está. Las ganancias que salen del banco son conservadoras para la
> misión, no optimistas. Pero el pico de telemetría sí está en la misión.

---

## 2. Por qué un ISR, y por qué no bastaba con lo evidente

El 2026-10-11 se planteó mover el lazo a una interrupción de timer. Conviene dejar escrito **qué
arregla y qué no**, porque la intuición falla en un punto:

| | ¿Lo arregla? | Por qué |
|:--|:-:|:--|
| El **pico de la OLED** (52,8 ms) | ✅ | La interrupción **adelanta** al bucle principal: la pantalla puede tardar lo que quiera |
| El **pico de la telemetría** (~8 ms cada 50 ms) | ✅ | Lo mismo |
| El **jitter** de los instantes de muestreo | ✅ | El evento de TIM1 lo pone el hardware, no el código |
| El **retraso del filtro**: de ~95 ms a **9,5 ms** | ✅ | **Aquí está la sorpresa**: no era del algoritmo sino del **ritmo**. Con la ventana llena a 1 kHz fijo baja una década |
| Los 9,5 ms que **quedan** de filtro | ❌ | Eso sí es del algoritmo: $(N-1)/2 \times 1\ ms$. Bajarlo es el estudio aparte (M/T o *feedforward*) |

📌 **Mover solo el *muestreo* no habría servido de nada**: si el PID sigue en el bucle principal, sigue
esperando a la OLED. Lo que hay que mover es **el lazo entero** — muestrear, calcular y escribir el
duty — y todo eso junto cabe en decenas de microsegundos.

Y el filtro sigue ahí: eso es el estudio aparte (método M/T o *feedforward* del modelo), no esto.

---

## 3. El diseño

```
   TIM1 (PWM de motores, 1 kHz)
      │  evento de actualización: 1 por período de PWM
      ▼
   ┌──────────────────────────────────────────────────────────────┐
   │  LOOP_IrqHandler()          ← prioridad 1 (la más alta)      │
   │    1. update_encoder_readings()   lee TIM2/TIM3, estima mm/s │
   │    2. CTRL_Update() × 2           rampa + PID → duty         │
   │    3. DRV8833_Set_Speed() × 2     escribe los CCR del PWM    │
   └──────────────────────────────────────────────────────────────┘
      │  consignas (volatile)                    ▲  medidas (volatile)
      ▼                                          │
   ┌──────────────────────────────────────────────────────────────┐
   │  Bucle principal                                             │
   │    · odometría      · giroscopio    · primitivas de movimiento│
   │    · menú y FSM     · telemetría    · OLED                    │
   └──────────────────────────────────────────────────────────────┘
```

### Por qué el evento de actualización de TIM1 y no otro timer

- **TIM1 ya es el PWM de los motores, a 1 kHz** (`PSC` 71, `ARR` 1000). Actualizar el duty **en su
  evento de actualización** es lo correcto: el nuevo `CCR` se aplica al principio de un período, así
  que ningún pulso sale a medias. Un timer independiente daría un lazo **asíncrono con el PWM**, con
  un pulso mordido de vez en cuando.
- **No hay que gastar ningún timer**: TIM1 es el PWM, TIM2/TIM3 son los encoders y TIM4 es el
  cronómetro. Los cuatro están ocupados y no hace falta un quinto.

### El período y el `dt`

`ARR` = 1000 ⇒ el evento llega cada **1001 cuentas de 1 µs**, o sea a **999 Hz**. El `dt` que se le
pasa al PID es **esa constante**, no una medida:

```c
#define LOOP_DT_S   (1001.0e-6f)
```

⚠️ **Medirlo sería un error.** Los instantes del evento los fija el hardware y son exactos *aunque la
interrupción se retrase* — que es justo lo que pasa cuando la OLED tiene el bus ocupado. Cronometrar
con TIM4 metería el **jitter de entrada** en el propio `dt`, que es lo contrario de lo que se busca.
El error de tomar 1001 µs por 1000 es del 0,1 %, y el integrador lo absorbe.

### Qué se queda en el bucle principal

Todo lo que **no** es la regulación de velocidad: odometría, giroscopio, primitivas de movimiento,
menú, FSM, telemetría y OLED. Siguen tardando lo que tardan —eso no cambia— pero ya **no arrastran al
PID**.

---

## 4. El registro de controladores

El ISR tiene que saber **a quién** regular, y en este firmware hay dos dueños distintos de los lazos:
las primitivas (`Motion_t`) y la prueba 18 (`lazo_izq`/`lazo_der` en su propia pila). Además hay
pruebas que mueven los motores **sin PID** (`UM/CUENTA`, `PPR A MANO`), y ésas no pueden tener a nadie
regulando por detrás.

Por eso el lazo se **activa y se desactiva**:

```c
void LOOP_Activar(Controller_t *izq, Controller_t *der,
                  DRV8833_Motor_t *motor_izq, DRV8833_Motor_t *motor_der);
void LOOP_Desactivar(void);
```

- **Activado**: el ISR regula esas dos ruedas y escribe el PWM.
- **Desactivado** (por defecto): el ISR **sigue muestreando los encoders** —la odometría y el banco lo
  agradecen— pero **no toca el PWM**, así que las pruebas en lazo abierto siguen mandando ellas.

Lo activa `MOTION_Init()` y la prueba 18 al entrar, y lo desactiva quien sale. Es **un solo
controlador a la vez**: no tiene sentido regular dos pares de ruedas.

---

## 5. Las secciones críticas (y por qué hacen falta)

El bucle principal y el ISR comparten los `Controller_t`. Con las consignas no hay problema: son
`float` alineados y en Cortex-M3 **una escritura de 32 bits es atómica**, así que el ISR lee el valor
viejo o el nuevo, y los dos son válidos.

⚠️ **Pero `CTRL_Parar()` no es una escritura.** Toca cinco cosas (`parado`, `duty`, `PID_Reset()`, el
objetivo y el salto de la rampa). Si el ISR entra a mitad, puede calcular un duty con el integrador a
medio resetear y **escribirlo después** de que el principal haya puesto `duty = 0` ⇒ **la parada no
para**. Es el peor fallo posible en un robot con motores.

⇒ Toda mutación del estado del lazo desde el bucle principal va **con la interrupción de control
pausada**:

```c
LOOP_Pausa();     /* enmascara el evento de TIM1 */
CTRL_Parar(&m->izq);
CTRL_Parar(&m->der);
LOOP_Reanuda();
```

Son unos microsegundos y pasa muy de vez en cuando (una vez por movimiento), así que no cuesta nada.
Afecta a `CTRL_Parar()`, `CTRL_SetConsigna*()`, `CTRL_SetGanancias()` y `CTRL_Init()`.

---

## 6. Qué NO cambia

- **Las ganancias** (`Kp` 0,10 · `Ki` 0,50 · `Kd` 0). Un lazo más rápido **sube** el margen de fase, así
  que las de ahora siguen valiendo: se quedan conservadoras. Re-sintonizar será para **aprovechar** el
  margen nuevo, no para arreglar nada.
- **La capa CONTROL no toca la HAL**, ni antes ni después: `CTRL_Update()` recibe la medida y `dt`, y
  devuelve el duty. Sigue siendo matemática pura y sigue compilando en el PC.
- **La odometría**: sigue integrándose a partir de los µm por rueda; solo cambia **quién** los pide.
- **Las pruebas en lazo abierto** (`UM/CUENTA`, `PPR A MANO`, `PWM_RAMPA`…): funcionan igual, porque
  sin registro el ISR no escribe el PWM.

### Y por qué el fichero va en `app/`

`LOOP_IrqHandler()` lee los timers de los encoders, llama a `control/` y escribe los registros del
PWM. Eso son **las tres capas a la vez**, y ninguna de las de abajo puede hacerlo:

- `control/` tiene **0 símbolos de la HAL** por regla y no puede tocar TIM1 ni el DRV8833.
- `drivers/` está **por debajo** de `control/` en la pila: si el ISR viviera ahí, `drivers/` estaría
  llamando **hacia arriba**, que es lo que la regla de capas prohíbe.
- `app/` es **la única capa autorizada a tocar `hal/` y `sensor/`**, y ya hace exactamente este
  intercalado en `app/motion.c` (excepción declarada del bring-up).

Así que va en `app/control_loop.c`, con la misma excepción declarada que `motion.c`. Cuando `drivers/`
tenga contenido (el `drv8833` y los encoders movidos ahí), se puede revisar: lo natural será que
`drivers/` se quede con **muestrear y escribir el duty** y que el paso de control se le inyecte desde
arriba. Hoy eso sería invertir la pila para ganar una pureza que todavía no existe.

---

## 7. Cómo se mide que funciona

| Etiqueta | Antes | Ahora | Objetivo |
|:--|--:|--:|:--|
| `>ISR_MAX` | — | **22 µs** ✅ | muy por debajo de 1001 µs: es el paso de control dentro de la interrupción |
| `>LAZO_MAX` | 68 848 µs | 0 (la etapa 1 no instrumenta el bucle principal) | el bucle **principal**, que ya no lleva el control: seguirá teniendo picos, y es correcto |
| `>OLED_MAX` | 52 800 µs | 52 960 µs | igual: es del bucle principal |
| `>UART_MAX` | 1 830 µs | 1 132 µs (línea de 13 B) | igual |

### ✅ Etapa 1 verificada (2026-10-11)

Grabada y leída **por SWD, sin tocar un botón** —la interrupción arranca con el robot, así que su
contador y su máximo están ahí desde el primer milisegundo—:

- **8834 pasos** desde el arranque (~8,8 s) ⇒ **~1004 Hz**, el ritmo diseñado (TIM1 da 999 Hz). El
  contador es de 32 bits, así que no da la vuelta.
- **22 µs por paso**: el **2,2 %** del tiempo de CPU, con un margen de **45×** antes de quedarse sin
  período. Y eso llevando ya dos lecturas de contador, la media móvil de 20 muestras y aritmética de
  `float`. Con los dos PID dentro debería quedar en 30-40 µs.

⚠️ **`>LAZO_MAX` cambia de significado**: era «la vuelta del lazo de control» y pasa a ser «la vuelta
del **bucle principal**». Seguirá midiendo 68 ms, y eso ya no significa que el robot vaya a la deriva.

Lo que hay que mirar de verdad es **`>ISR_MAX`**: si se acerca a 1001 µs, el paso de control no cabe
en su período y hay que partirlo. Con lo que hace (leer dos contadores, una media móvil de 20
muestras, dos PID con `float` y cuatro escrituras de registro) debería quedarse en decenas de µs.

#### ✅ Confirmado el 2026-10-11 con la segunda grabación

- **16 853 pasos** en 16,9 s ⇒ **~1000 Hz** otra vez, y **22 µs** de `>ISR_MAX`: el coste es estable.
- **La ventana del filtro ya es la de diseño, y se ve en la telemetría.** Las velocidades salen
  **cuantizadas en escalones de 14,336 mm/s**, que es exactamente `287 µm / 20,02 ms` (una cuenta de
  encoder dividida por las 20 muestras de 1001 µs). ⇒ El `dt` constante **llega hasta el final**: la
  ventana dura 20 ms aquí **y en la misión**, que era el objetivo de todo esto, y no hace falta
  ningún instrumento para comprobarlo: se lee en los números.
- **El límite de `Kp` apenas se ha movido**: 0,10 va bien y **0,20 oscila** (medido). En el **banco**
  el bucle ya iba a 1,9 ms, así que la ventana ya duraba 20 ms y lo único que quitó la interrupción
  fueron los 10 ms del lazo: el retardo pasó de ~20 a ~15 ms, solo **1,3×**. ⇒ **El premio del ISR no
  está en el banco, está en la misión** (200 → 20 ms). Los números en
  [control-pid.md](./control-pid.md) §4.

---

## 8. Lo que queda pendiente después de esto

1. **La etapa 2: meter el PID dentro de la interrupción.** Hoy la interrupción **muestrea** —y eso ya
   ha arreglado la ventana del filtro, que era el problema gordo— pero el PID sigue en el bucle
   principal, a 10 ms. Ganancia **medida**: el retardo bajaría de ~15 a ~10,5 ms, un **30 %**.
   ⚠️ **Pero medido el 2026-10-11, el límite de `Kp` depende del retardo y ese 30 % lo mueve poco**:
   acortar la ventana para bajar el retardo tampoco sube el límite, porque la cuantización se come lo
   que se gana. ⇒ **La etapa 2 deja de ser prioridad** (ver
   [control-pid.md](./control-pid.md) §4). Cuando se haga, hacerlo bien: `LOOP_Desactivar()` antes de
   frenar, porque si no el ISR reescribe el freno con un duty nuevo.
2. **La UART sin bloqueo** (anillo de TX o DMA): deja de gastar ~8 ms cada 50 ms **del bucle
   principal**. Ya no afecta al control, pero sí a la odometría, al giroscopio y a los botones.
3. **La I²C a 400 kHz** (`I2C1_CLOCK_HZ`): 52,8 ms → ~13 ms. Comodidad del banco.
4. **El estimador** (método M/T o *feedforward* del modelo): es lo único que **rompe el acoplamiento
   resolución↔retardo** (da medida fina *y* ventana corta a la vez), y el experimento del 11-oct
   demuestra que ese acoplamiento es real y que la ventana actual está cerca del óptimo. ⚠️ El premio
   esperable en `Kp` es de un factor **~1,5-2**, no un cambio de escala ⇒ es una **mejora de segundo
   orden**, no la respuesta a los errores que tiene el robot hoy (los giros de ±4-7°). Estudio aparte,
   con banco en el PC; la planta ya está **medida** (unos 71 mm/s por punto de duty con un **escalón
   muerto de ~10 %**, ver [control-pid.md](./control-pid.md) §4), que es la mitad del *feedforward*.
