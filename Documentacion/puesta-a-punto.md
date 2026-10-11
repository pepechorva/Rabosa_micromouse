# 🔧 Puesta a punto — el guion de la campaña

> 📅 **2026-10-05** · Estado: **protocolo escrito, todavía SIN ejecutar**.
> La competición pasó y dejó tres cosas medidas y una cuarta que duele. Ahora hay **tiempo** y
> viene un **laberinto 6×6 con medidas oficiales**, que es el instrumento que faltaba.

Este documento es el **guion**: qué se mide, en qué orden, con qué se compara y **cuándo se da un
bloque por cerrado**. Reparte el trabajo así:

| Documento | Qué contiene |
|:--|:--|
| **Este** (`puesta-a-punto.md`) | El **guion**: orden, puertas de salida y tablas de resultados |
| [plan-mejoras.md](./plan-mejoras.md) | El **por qué**: qué falló, qué hacen los demás y qué NO copiamos |
| [pruebas-bringup.md](./pruebas-bringup.md) | El **cómo**: procedimiento de cada prueba de banco |
| [firmwares-referencia.md](./firmwares-referencia.md) | El análisis de los tres firmwares de referencia |

---

## 0. Cómo se usa este guion

**Tres reglas, y la tercera es la que más cuesta:**

1. **Un bloque no se abre sin cerrar el anterior.** El orden es
   `1+2 en paralelo → 3 → 4` (ver el grafo de dependencias en `plan-mejoras.md` §4).
2. **Cada medida se apunta en su tabla con la fecha, aunque salga mal.** Un dato que no se apunta
   no existe: la noche del 2 de octubre se perdieron dos horas por dar por bueno un número que
   nadie había medido con una referencia externa.
3. **La puerta se comprueba contra una referencia EXTERNA al robot.** La odometría y el giroscopio
   del robot no sirven para validar la odometría y el giroscopio del robot. Ya pasó: `MOV_GYR` daba
   89-91° por esquina **con cualquier escala**, porque el lazo cerraba sobre el propio `gyro_total`.
   Sirven: una cinta en el suelo, el laberinto, un metro, un cronómetro.

**Marcas de estado** en las tablas: `⏳` pendiente · `✅` superado · `❌` no superado.

---

## 1. Trabajo previo: rehabilitar las pruebas de banco

⚠️ **Antes de empezar hay que tocar el firmware.** Los bloques 1 y 3 necesitan pruebas que están
**retiradas del menú** (`firmware/app/application.c`, tabla `menu_test_motores[]`), porque en su día
se midieron y sus números quedaron grabados en los ajustes. Pero ahora cambia **el actuador**
(PWM a 20 kHz), y con él **todo lo medido deja de valer**: hay que volver a medirlas con la
resolución nueva.

| Entrada de menú | Test | Para qué la necesita este guion | Hoy |
|:--|:-:|:--|:--|
| `MIN M1` / `MIN M2` / `MIN AMBOS` | 2, 3 | **Duty mínimo de arranque de cada motor** → el *feedforward* de fricción del bloque 1 | ⛔ retirada |
| `COMPARA M` | — | Que las dos ruedas tiren igual: `COMP_P1`, `COMP_P2`, `COMP_DESV` | ⛔ retirada |
| `RAMPA PWM` | 9 | Planta de cada motor en lazo abierto, rueda a rueda | ⛔ retirada |
| `MOTORES → CALIBRAR → UM/CUENTA` | — | µm/cuenta **por rueda** (hoy hay un valor común) | ✅ reactivada |
| `MOTORES → CALIBRAR → PPR A MANO` | — | PPR y reducción por rueda, por si el µm/cuenta no cuadra | ✅ reactivada |
| `ODOMETRIA` | 19 | Trocha efectiva, cuadrado de 4 × 300 mm y **contraste del giroscopio** con las cuentas | ✅ **reactivada el 2026-10-11**, con `>ODO_GYR` |
| `IR → UMBRALES` | 21 | Umbral de pared (bloque 2) | ✅ activa |
| `IR → ENCENDER` | 14 | Alineado del apuntado de los cuatro focos (bloque 2) | ✅ activa |
| `IR → LECTURA` | — | Matriz 4×4 emisor↔receptor: **crosstalk** (bloque 2) | ✅ activa |
| `IR → CURVA` | 17 | Curva distancia↔ADC y calibración en mm (bloque 2) | ✅ activa |
| `IR → MODULADO` | 16 | Modulación ON−OFF (bloque 2) | ✅ activa |
| `MOTORES → SINTONIA` | 18 | Escalón de consigna: `K`, `tau` y rizo de velocidad (bloques 1 y 3) | ✅ activa |
| `MOTORES → MOVIMIENTO` | 20 | Celda y giro de 90° (bloques 3 y 4) | ✅ activa |
| `EXPLORACION → MANO DERECHA` | 22 | Recorrido completo en el 6×6 | ✅ activa |

**Órdenes de consola disponibles** (ver [comandos-serie.md](./comandos-serie.md)), que evitan
reflashear durante la campaña:

| Orden | Efecto |
|:--|:--|
| `PID` · `PID LIST` | Publica `PID_KP`, `PID_KI`, `PID_KD`, `PID_VEL` |
| `PID KP\|KI\|KD <valor>` | Sintoniza el lazo de velocidad en caliente |
| `PID VEL <mm/s>` | Cambia la consigna de la prueba 18 |
| `GYRO LIST` · `GYRO KP\|KI\|KD\|ESC\|VMIN\|VARR\|VDPS <valor>` | Ganancias del giro en caliente |
| `GYRO ODOM\|GIRO\|FUSION` | Modo de referencia del giro |
| `MOV …` | Primitivas de movimiento (solo **dentro** de la prueba 20) |
| `BAT` · `BAT LIST` · `BAT CAL <mV>` · `BAT <UMBRAL> <mV>` | Batería |
| `TRACE` | Descarga la traza de las FSM |
| `LED 0\|1\|TOGGLE` | **Prueba de vida de la consola**: si esto no responde, el problema es la recepción |
| `HELP` · `?` | Publica batería + PID + LED |

> ⚠️ **Cada reflasheo tumba el puente WiFi.** Hay que volver a entrar en `TEST → VARIOS → WIFI AL PC`
> (prueba 24) antes de fiarse de una telemetría. Si dudas, `LED 1` lo dice en un segundo.

---

## 2. Bloque 1 — Encoders y PWM: que las dos ruedas vayan igual

### Objetivo

Que la misma consigna dé la **misma velocidad** en las dos ruedas, y que la velocidad **no dé
saltos**. Es la base de todo: mientras el actuador tenga 100 posiciones, cualquier medida de
rectitud está midiendo el actuador, no el lazo.

### Por qué

El duty tiene **100 posiciones** (1 % = 15 mm/s) y el sensor resuelve **14 mm/s**: la relación es
**1:1**. El PID pide valores que **no existen**, así que salta entre dos escalones. Con las dos
ruedas descompensadas (~9 %: `COMP_P1:14387` frente a `COMP_P2:13093`), eso se ve como balanceo.

### Cambio de firmware asociado

| Fichero | Cambio |
|:--|:--|
| `firmware/hal/timers.c` | TIM1: `PSC = 0`, `ARR = 3599` ⇒ **20 kHz**, 3600 pasos (de 15 mm/s por escalón a **0,42**) |
| `firmware/hal/drv8833.c` | Quitar el `(int16_t)` del interface: hoy el duty se trunca a **% enteros** |
| `firmware/control/controller.c` | Ganancias y *feedforward* **por rueda** |
| `firmware/sensor/encoder.c` | `micrometers_per_count` **por rueda** (hoy uno común) |

⚠️ Al subir la frecuencia, **re-sintonizar el PID**: el mismo `Kp` ya no da la misma respuesta.

### Secuencia

| # | Paso | Prueba | Se registra |
|:-:|:--|:--|:--|
| 1.1 | Subir el PWM a 20 kHz y comprobar que los motores siguen girando y **no zumban** | `RAMPA PWM` (9) | — |
| 1.2 | µm/cuenta de **cada rueda** por separado (empujando y con motores) | `UM MOTORES`, `PPR A MANO` | `CAL_UM_CUENTA`, `CAL_UM_ESP`, `CAL_PPR1` |
| 1.3 | **Duty mínimo de arranque** de cada motor, rueda a rueda | `MIN M1`, `MIN M2`, `MIN AMBOS` | `ARR_M1`, `ARR_M2`, `ARR_DUTY` |
| 1.4 | **Planta en el suelo**: escalón de consigna → velocidad, **cada rueda por separado** | `PID VELOCIDAD` (18) | `DUTY_IZQ`, `DUTY_DER`, `VEL_IZQ`, `VEL_DER`, `PID_SET` |
| 1.5 | ¿Tira igual un motor que el otro a lazo abierto? | `COMPARA M` | `COMP_P1`, `COMP_P2`, `COMP_DESV` |
| 1.6 | **Rizo** de velocidad en régimen (H2 del plan) | `PID VELOCIDAD` (18) | `VEL_IZQ`, `VEL_DER` a 100 Hz |
| 1.7 | Ganancias **independientes** por rueda + *feedforward* de fricción | `PID KP\|KI\|KD <valor>` | `PID_KP`, `PID_KI`, `PID_KD` |

**Cómo se mide el rizo (1.6)**, que es la medida que decide si H2 es cierta:

1. Teleplot grabando, `PID VEL = 250 mm/s`, robot **en el suelo** y con sitio para 1 m.
2. Capturar **2 s en régimen** (descartar el primer medio segundo, que es la rampa).
3. Calcular la desviación típica de `VEL_IZQ` y de `VEL_DER` por separado.
4. **Repetir 3 veces.** Si el rizo baja al subir la resolución, H2 era cierta.

### Puertas

> - **P1.1** — Con la misma consigna en las dos ruedas, **diferencia de velocidad < 2 %** en régimen,
>   y **repetible** en 3 intentos.
> - **P1.2** — Rizo de `VEL_IZQ` y `VEL_DER` **< 5 mm/s** (desviación típica en régimen).
> - **P1.3** — µm/cuenta de las dos ruedas **coherente** con el diámetro nominal y con la reducción
>   (± 5 %).

### Resultados

| Fecha | Medida | Rueda izq. | Rueda der. | Desviación | ✅/❌ |
|:--|:--|--:|--:|--:|:-:|
| ⏳ | µm/cuenta | | | | |
| ⏳ | Duty mínimo de arranque | | | | |
| ⏳ | $K$ (mm/s por %) en el suelo | | | | |
| ⏳ | `tau` (ms) | | | | |
| ⏳ | Velocidad a la misma consigna (mm/s) | | | | |
| ⏳ | Rizo en régimen (mm/s) | | | | |
| ⏳ | Ganancias finales Kp/Ki/Kd | | | | |

---

## 3. Bloque 2 — Sensores: ajuste y alineado de los emisores

### Objetivo

Que los **cuatro** canales distingan «no hay pared» de «hay pared», y que la lectura signifique
**una distancia**. Hoy solo funciona el frontal.

### Por qué

Medido el 2026-10-02 con el robot **en el aire** (sin nada delante):

| Canal | Lectura en el aire | Umbral | Veredicto |
|:--|--:|--:|:--|
| Frente | **0** | 75 | ✅ correcto |
| Derecha | **2687** | 2050 | ❌ **siempre ve pared** |
| Izquierda | **742** | 900 | ⚠️ a **158 cuentas** del corte |

Eso es **crosstalk**: el receptor ve el emisor de al lado (o el propio, por luz que se cuela por el
montaje). El frontal está bien porque está lejos del resto.

⚠️ Como el *wall follower* solo usa el frontal, **los 11 rebotes contra la pared del 5 de octubre
son lógica, no sensores**. Pero sin laterales no hay corrección de posición, ni centrado en
pasillo, ni pared como referencia absoluta.

### Por qué este bloque puede esperar al rediseño

El robot actual va **soldado sobre protoboard y cableado con wrapping**, y el usuario va a
rediseñarlo en KiCad. El apantallado y el cambio de resistencias son **de hardware**: tiene sentido
hacerlos **en la placa nueva**, no sobre el protoboard. Lo que **sí** conviene adelantar son las
**compras** (§7), porque tienen plazo de entrega.

### Secuencia

| # | Paso | Prueba | Se registra |
|:-:|:--|:--|:--|
| 2.1 | **Crosstalk de los 4 canales con el robot en el aire.** Es el dato de referencia, y hoy no se apunta en ningún sitio | `PAREDES` (21) | `FRENTE`, `DERECHA`, `IZQUIERDA` |
| 2.2 | **Alinear el apuntado** de los cuatro focos con un folio blanco y la cámara del móvil | `EMISORES IR` (14) | — (visual) |
| 2.3 | **Matriz 4×4**: encender un emisor y ver qué recibe cada receptor | `IR RECEPTORES` | matriz |
| 2.4 | **Barrido de $R_L$**: 1,5 k · 2 k · 3 k · 4,7 k · 10 k, midiendo **crosstalk y señal a 20/60/90/180 mm** | `PAREDES` (21) + regla | tabla |
| 2.5 | Apantallar emisor↔receptor (separador opaco) y repetir 2.1 | `PAREDES` (21) | `FRENTE`, `DERECHA`, `IZQUIERDA` |
| 2.6 | Curva distancia↔ADC de cada canal, con el material del 6×6 | `CURVA IR` (17) | `IR_CAL_CM`, `IR_CAL_CANAL` |
| 2.7 | Decidir el umbral y **dónde se muestrea** la pared | `PAREDES` (21) | umbrales |

⚠️ **El criterio de 2.4 no es la señal máxima, es la relación señal/crosstalk.** Está escrito en
[calculos-diseno.md](./calculos-diseno.md) §3.1 y conviene releerlo antes de comprar resistencias:
una resistencia que dispara la señal un 30 % pero multiplica por 3 el crosstalk es **peor** que la
que da la mitad de señal con el crosstalk a cero.

**El punto de muestreo (2.7)** sale de UKMARSBOT, que lo dice con todas las letras:

> *«Detectar una pared frontal de forma fiable puede ser sorprendentemente difícil. Para máxima
> fiabilidad, **muestrea al cruzar el borde de la celda**. Unos pocos milímetros de recorrido
> pueden significar el doble de señal.»*

Nuestro umbral de 75 se decidió **a media celda**. Hay que medir a qué distancia de la pared la
lectura tiene margen y mover el muestreo al borde.

### Puerta

> **P2.1** — Los **4 canales** distinguen «sin pared» de «pared a 90 mm» con un factor **≥ 3**.
> **P2.2** — La lectura es repetible al **± 5 %** en 10 medidas.
> **P2.3** — Con el robot en el aire, **los 4 canales por debajo de su umbral** (hoy falla la
> derecha y la izquierda está al límite).

### Resultados

| Fecha | Canal | En el aire | Pared a 20 mm | 60 mm | 90 mm | 180 mm | $R_L$ | Umbral |
|:--|:--|--:|--:|--:|--:|--:|--:|--:|
| ⏳ | Frente | | | | | | | |
| ⏳ | Derecha | | | | | | | |
| ⏳ | Izquierda | | | | | | | |
| ⏳ | R0 (frontal der.) | | | | | | | |
| ⏳ | R3 (frontal izq.) | | | | | | | |

---

## 4. Bloque 3 — Movimiento: recto con PID y giroscopio

### Objetivo

Saber **de qué** es el error que se acumula al recorrer un pasillo: de rumbo, de distancia o
lateral. Y cuantificarlo por celda.

### Por qué

El robot recorrió **8 celdas «a tropezones»**, cada vez más torcido. Es el fallo medido del 5 de
octubre. Tres hipótesis, ninguna confirmada:

- **H1** — el error se acumula por celda (se mide la pared una vez y se avanza a ciegas).
- **H2** — el robot balancea porque el actuador es grueso (va en el bloque 1).
- **H3** — el robot no arranca centrado y todo lo que sigue hereda el desplazamiento.

⚠️ **No se empieza este bloque hasta cerrar el 1**: medir rectitud con el actuador grueso es medir
el ruido.

### Secuencia

| # | Paso | Prueba | Se registra |
|:-:|:--|:--|:--|
| 3.1 | Re-sintonizar el lazo de velocidad **en el suelo** con la resolución nueva | `PID VELOCIDAD` (18) | `PID_KP`, `PID_KI`, `PID_KD` |
| 3.2 | 3 celdas rectas en el 6×6, apuntando rumbo, distancia y desviación lateral | `MOVIMIENTO` (20) | `MOV_X`, `MOV_Y`, `MOV_ANG`, `MOV_D`, `MOV_GYR` |
| 3.3 | **8 celdas rectas** en el 6×6, apuntando la posición después de **cada** celda | `MOVIMIENTO` (20) | `MOV_X`, `MOV_Y`, `MOV_ANG` por celda |
| 3.4 | Repetir 3.3 con el robot **arrancando descentrado** a propósito (H3) | `MOVIMIENTO` (20) | lo mismo |
| 3.5 | Cuadrado de 4 celdas: ¿cierra? | `MOVIMIENTO` (20) | `MOV_X`, `MOV_Y`, `MOV_ANG` finales |
| 3.6 | Repetir 3.3 **midiendo con un metro** la posición real al final | cinta + `MOVIMIENTO` | referencia **externa** |

⚠️ **3.6 es el paso que decide.** La odometría del robot no puede validarse a sí misma: hay que
poner el robot donde empieza, marcar con cinta **dónde debería acabar** (8 × 180 mm) y medir con un
metro dónde acabó de verdad.

### Puerta

> **P3.1** — 8 celdas seguidas **en el 6×6** con error acumulado **< 15 mm** (medido con cinta) y
> rumbo **< 3°**.
> **P3.2** — El error **por celda no crece** de forma monótona (eso confirmaría H1).

### Resultados

| Fecha | Celda | `MOV_X` | `MOV_Y` | `MOV_ANG` | Real (cinta) | Error | ✅/❌ |
|:--|:-:|--:|--:|--:|--:|--:|:-:|
| ⏳ | 1 | | | | | | |
| ⏳ | 2 | | | | | | |
| ⏳ | 3 | | | | | | |
| ⏳ | 4 | | | | | | |
| ⏳ | 5 | | | | | | |
| ⏳ | 6 | | | | | | |
| ⏳ | 7 | | | | | | |
| ⏳ | 8 | | | | | | |

---

## 5. Bloque 4 — Aceleraciones

### Objetivo

Que el robot arranque y frene **sin sobrepaso** y que el giro **no rebote**.

### Por qué

El rebote del giro se ha peleado toda la noche del 2 de octubre **con un mínimo de velocidad
artificial** (`MOTION_GIRO_VEL_MIN_RODANDO_MM_S` = 35 mm/s aplicado siempre). Eso existe porque el
motor no arranca por debajo de cierto duty. El **feedforward de fricción** del bloque 1 elimina la
causa, y el **perfil de velocidad angular** (una rampa con aceleración limitada, como el
`angular_accel` / `max_angular_speed` de ZoroBot3) elimina el síntoma: **el giro deja de ser un
lazo de posición con un mínimo artificial y pasa a ser un perfil que se sigue**.

⚠️ Es el bloque que **más código nuevo** necesita, y por eso va el último: solo se hace si los
bloques 1 a 3 dejan claro que el rebote sigue ahí.

### Secuencia

| # | Paso | Prueba | Se registra |
|:-:|:--|:--|:--|
| 4.1 | Avance de celda con **aceleración y frenada separadas** | `MOVIMIENTO` (20) | `MOV_D`, `MOV_X`, `VEL_IZQ`, `VEL_DER` |
| 4.2 | Comprobar que **no hay sobrepaso** (¿se pasa de 180 mm y vuelve?) | `MOVIMIENTO` (20) | `MOV_D` frente a `TR_SET` |
| 4.3 | Perfil de **velocidad angular** con aceleración limitada, sustituyendo el mínimo artificial | `MOVIMIENTO` (20) | `MOV_GYR`, `MOV_ANG`, `MOV_D` |
| 4.4 | Giro de 90° desde parado y **encadenado** (sin parar) | `MOVIMIENTO` (20) | `MOV_GYR`, `MOV_D` |
| 4.5 | Consumo: ¿aparece un pico de corriente al arrancar? | `BATERIA` + `VBAT` | `VBAT`, `VBAT_DESEQ` |

### Puerta

> **P4.1** — Avance de celda **sin sobrepaso** (la posición no pasa del objetivo) y **sin pico** de
> `VBAT_DESEQ`.
> **P4.2** — Giro de 90° **sin rebote**: `MOV_GYR` entra en la banda de ±1,5° y **no la vuelve a
> cruzar** más de una vez.
> **P4.3** — Cuatro giros encadenados con **dispersión < 2°** (hoy: 2,1°), y `MOV_D` entre 3 y 6 mm
> (pivote limpio, no arrastre).

### Resultados

| Fecha | Prueba | Medida | Valor | ✅/❌ |
|:--|:--|:--|--:|:-:|
| ⏳ | 4.1 | Sobrepaso (mm) | | |
| ⏳ | 4.1 | Pico de `VBAT_DESEQ` (V) | | |
| ⏳ | 4.4 | Cruces de banda por giro | | |
| ⏳ | 4.4 | `MOV_D` (mm) | | |
| ⏳ | 4.4 | Dispersión de 4 giros (°) | | |

---

## 6. Resumen de puertas

| Puerta | Criterio | Estado |
|:--|:--|:-:|
| **P1.1** | Misma consigna ⇒ diferencia de velocidad < 2 % | ⏳ |
| **P1.2** | Rizo de velocidad < 5 mm/s | ⏳ |
| **P1.3** | µm/cuenta coherente con diámetro y reducción (± 5 %) | ⏳ |
| **P2.1** | 4 canales distinguen «sin pared» de «pared a 90 mm» con factor ≥ 3 | ⏳ |
| **P2.2** | Lectura repetible al ± 5 % | ⏳ |
| **P2.3** | Robot en el aire ⇒ los 4 canales por debajo de su umbral | ⏳ |
| **P3.1** | 8 celdas con error < 15 mm y rumbo < 3° | ⏳ |
| **P3.2** | El error por celda no crece de forma monótona | ⏳ |
| **P4.1** | Avance sin sobrepaso ni pico de corriente | ⏳ |
| **P4.2** | Giro sin rebote (un cruce de banda como máximo) | ⏳ |
| **P4.3** | 4 giros encadenados con dispersión < 2° y `MOV_D` de 3-6 mm | ⏳ |

---

## 7. Material y compras

Del bloque 2, que es el único que necesita comprar:

| Qué | Para qué | Nota |
|:--|:--|:--|
| Resistencias de $R_L$: 1,5 k · 2 k · 3 k · 4,7 k · 10 k | Barrido de 2.4 | Incluir la del valor actual para tener la referencia |
| Lámina opaca fina (plástico negro, cartulina gruesa) | Separador emisor↔receptor (2.5) | Se corta a tijera y se pega; no hace falta imprimir |
| Folio blanco mate | Alinear el apuntado (2.2) | — |
| Metro o cinta de 3 m | Puerta P3.1 | **La referencia externa** del bloque 3 |
| Cinta de carrocero | Marcar las distancias en el suelo | Que no deje residuo |

Ver el detalle de componentes y la lista completa en [calculos-diseno.md](./calculos-diseno.md).

---

## 8. Relacionado

- [plan-mejoras.md](./plan-mejoras.md) — el porqué de cada bloque, qué copiamos y qué no
- [pruebas-bringup.md](./pruebas-bringup.md) — el procedimiento de cada prueba de banco
- [firmwares-referencia.md](./firmwares-referencia.md) — UKMARSBOT, ZoroBot3 y KERISE
- [esquema-firmware.md](./esquema-firmware.md) — cómo está montado el firmware
- [comandos-serie.md](./comandos-serie.md) — las órdenes de consola al detalle
- [estado-del-proyecto.md](./estado-del-proyecto.md) — el estado del proyecto
- [calculos-diseno.md](./calculos-diseno.md) — los cálculos de los sensores y el material
