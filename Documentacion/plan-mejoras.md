# 🧭 Plan de mejoras — qué pasó y qué incorporamos

> 📅 **2026-10-05** · Estado: **propuesta v0**, pendiente de cerrar las decisiones de §6.
> Este documento nace del análisis de tres firmwares de referencia
> ([firmwares-referencia.md](./firmwares-referencia.md)) y del fallo medido en la competición.

La competición ya pasó y sirvió para lo que tenía que servir: **dijo dónde duele de verdad**, que es
distinto de donde suponíamos. Y ahora hay **tiempo** y **un laberinto 6×6 con medidas oficiales** en
camino, que es lo que faltaba para poder medir en condiciones.

---

## 1. Qué pasó, medido

El robot recorrió **un pasillo de 8 celdas «a tropezones»** (cada vez más torcido) hasta que se
encontró una pared delante y no pudo seguir.

Lo que está **medido y descartado**:

| Pieza | Estado | Medida |
|:--|:--:|:--|
| Giro de 90° | ✅ | 4 giros: 361,9° totales, **dispersión 2,1°** |
| Avance recto | ✅ | 177/180/179 mm, rumbo a **−0,9°** |
| Sensor frontal | ✅ | **0** sin pared, **179** con ella, umbral 75 |
| Decisión del *wall follower* | ✅ | ve la pared y elige `WF_DEC` correcto |
| Giro en el sitio | ✅ | `MOV_D` de 3-6 mm (pivote limpio) |

Lo que **falla**:

| # | Fallo | Evidencia |
|:-:|:--|:--|
| 1 | **`WF_Paso()` avanza la celda a ciegas** (180 mm sin volver a mirar) | 11 s empujando contra la pared con `>WF_ATASCO:1` |
| 2 | **Los laterales no ven nada** (crosstalk) | la derecha da 2687 con el robot **en el aire**; el umbral es 2050 ⇒ **«hay pared» siempre**. La izquierda, a 158 cuentas del corte |
| 3 | **No hay ninguna referencia absoluta** | la posición depende solo de odometría y giroscopio; ninguna de las dos acota el error |

### Las tres hipótesis que hay que medir antes de tocar nada

- **H1 — el error se acumula por celda.** El robot mide la pared una vez y recorre 180 mm a ciegas: cada
  celda acaba un poco descentrada y la siguiente parte peor. ⇒ **Medir el error por celda** en el 6×6.
- **H2 — el robot balancea porque el actuador es demasiado grueso.** El duty tiene **100 posiciones**
  (1 %) y 1 % son **15 mm/s**; el sensor resuelve **14 mm/s**. El PID pide valores que **no existen**, así
  que salta entre dos escalones y oscila. En las dos ruedas, con asimetría, **eso se ve como balanceo**.
  ⇒ **Medir el rizo de `>VEL_IZQ`/`>VEL_DER`** en un avance recto.
- **H3 — el robot no está centrado al empezar.** Si arranca pegado a una pared, todo lo que sigue hereda
  ese desplazamiento. ⇒ **Medir `>WF_X`/`>WF_Y`** celda a celda.

⚠️ **Ninguna de las tres está confirmada.** La tentación es arreglar las tres a la vez; la lección de la
noche del 2 de octubre es que **lo que falta no es una idea, es una prueba**.

---

## 2. Qué tienen los demás, filtrado a nuestro robot

De los tres firmwares analizados, esto es lo que **aplica** a un robot de 64 KB sin succión que va a
250 mm/s:

| Qué | Quién | Por qué nos sirve |
|:--|:--|:--|
| **Laterales que funcionan** | los tres | Es el **habilitador** de todo lo demás |
| **Pared como referencia absoluta** (壁切れ / *wall edge*) | ZoroBot3 y KERISE | Resetea una coordenada con el **borde** de un poste: **no arrastra error**. Es la respuesta directa al fallo #3 |
| **Sensores en distancia**, no hay/no hay | ZoroBot3 | Calibración no lineal (`a`, `b`, `c`) + offsets por sensor. Sin esto no se puede corregir **cuánto** |
| **Muestrear la pared al cruzar el borde de la celda** | UKMARSBOT | Su README lo dice: la señal «**puede doblarse en unos pocos mm**». Nuestro umbral de 75 se decidió a media celda |
| **Corregir con las paredes DURANTE el avance** | los tres | Nosotros solo corregimos el rumbo con el giroscopio |
| **Resolución del actuador** (PWM a 20 kHz) | ZoroBot3 | Ataca H2: de **1:1** a **34:1** frente al sensor |
| **Wall follower como `start`/`loop` no bloqueante** | ZoroBot3 (`handwall.c`) | Permite corregir **en cada tick**; el nuestro es bloqueante y por eso mide una vez |
| **Feedforward de fricción** | ZoroBot3 (duty mínimo medido) | Quita el mínimo de velocidad artificial del giro, y con él los rebotes |
| **Guardar las calibraciones de forma no volátil** | ZoroBot3 (EEPROM real) y KERISE (`SPIFFS`, **no portable a un F103**) | Hoy todo vive en RAM y se pierde al reiniciar; la opción barata en nuestro chip es RAM `.noinit` |
| **Arrancar la manga quitando la mano del frontal** | los tres | Trivial, y reutiliza un sensor que ya está |

## 3. Qué **no** incorporamos, y por qué

Decidir qué no se hace es la mitad del plan.

| Descartado | Motivo |
|:--|:--|
| **FreeRTOS** | El super-bucle de 1 kHz con cuatro tareas nos sobra. KERISE lo necesita porque su ESP32 lleva WiFi y tareas pesadas; **el F103 tiene 20 KB de RAM** y un contexto de RTOS cuesta |
| **Succión** | No hay hardware |
| **Perfiles en S con jerk limitado** | KERISE corre a **720 mm/s**; nosotros a 250. A 250 el tirón no se nota y el código es mucho más complejo |
| **Giros de 45° y diagonales** | Necesitan laterales, referencia absoluta y un perfil de giro. **Después**, no ahora |
| **Reescribir en C++ con clases** | Nuestro C por capas está documentado y funciona. Reescribir sería tirar el trabajo y perder la trazabilidad |
| **Cambiar de MCU (F4/ESP32)** | El cuello de botella es **el sensor y la resolución del PWM**, no la CPU: a 72 MHz vamos a 250 mm/s y el lazo va a 100 Hz |
| **Simulador** | Mucho trabajo. **El laberinto 6×6 es mejor instrumento que un simulador** para lo que nos falla |

---

## 4. El plan, por fases

Cada fase tiene **una puerta de salida medible**. No se pasa a la siguiente sin cerrarla.

### Bloque 1 — Encoders y PWM: que las dos ruedas vayan igual

**Es la base de todo lo demás**: mientras el actuador tenga 100 posiciones, cualquier medida de
rectitud está contaminada.

- **Subir la resolución del PWM**: `PSC = 0`, `ARR = 3599` (20 kHz) y **quitar el `(int16_t)`** de
  `DRV8833_Set_Speed`. De **15 mm/s de escalón a 0,42**.
- **Medir la planta de cada rueda POR SEPARADO y EN EL SUELO**: escalón de consigna → velocidad.
  Sacar $K$ (mm/s por %), `tau` y el **duty mínimo de arranque** de cada motor.
  ⚠️ Los valores actuales ($K \approx 15$, `tau` ≈ 45 ms) se midieron **con las ruedas en el aire**, y
  en el suelo la carga cambia $K$.
- **Verificar el µm/cuenta de cada rueda** por separado (hoy hay un valor común).
- **Ganancias independientes por rueda**: la izquierda es **~9 % más rápida** a lazo abierto (medido:
  `>COMP_P1:14387` frente a `>COMP_P2:13093`), así que la sintonía óptima **no es la misma**.
- **Feedforward de fricción**: el duty mínimo medido de cada motor, sumado al PID. Es lo que permite
  **quitar el mínimo de velocidad artificial** del giro (y con él el rebote).

> **Puerta**: con la misma consigna en las dos ruedas, **diferencia de velocidad < 2 %** en régimen,
> repetible, y el rizo de `>VEL_IZQ`/`>VEL_DER` **< 5 mm/s**.

### Bloque 2 — Sensores: ajuste y alineado de los emisores

**Va en paralelo con el bloque 1** (este es hardware y lleva compras e impresión).

- **Medir el crosstalk de los 4 canales con el robot en el aire.** Es el dato de referencia y hoy no se
  registra: medido a mano, **frente 0 ✅ · derecha 2687 ❌ · izquierda 742 ⚠️** (umbrales 75 / 2050 / 900).
- **Alinear los emisores**: con `EMISORES IR` (los cuatro encendidos) y un folio blanco, ver dónde
  apunta cada foco y corregir la mecánica.
- **Barrido de $R_L$** (1,5 k · 2 k · 3 k · 4,7 k · 10 k) midiendo **crosstalk y señal a 20/60/90/180 mm**.
  El criterio **no es la señal máxima, es la relación señal/crosstalk** (regla ya escrita en
  `calculos-diseno.md` §3.1).
- **Apantallar emisor↔receptor**: separador opaco. Es la solución más eficaz contra el crosstalk y
  ZoroBot3 la usa con **los mismos componentes**.
- **Ajustar el punto de muestreo**: medir a qué distancia la lectura tiene margen, y decidir la pared
  **al cruzar el borde de la celda** (UKMARSBOT).

> **Puerta**: los **4 canales** distinguen «sin pared» de «pared a 90 mm» con un factor **≥ 3**, y la
> lectura es repetible al **±5 %**.

### Bloque 3 — Movimiento: recto con PID y giroscopio

- **Re-sintonizar en el suelo** con la resolución nueva del bloque 1.
- **Medir el rizo de velocidad** en régimen (¿confirma H2?).
- **Medir el error por celda en 8 celdas rectas** en el laberinto 6×6: ¿es de rumbo, de distancia o
  lateral? (H1 y H3).
- Cerrar el rumbo con el giroscopio (ya se hace: `MOTION_GYRO_KP` = 12, tope 60 mm/s).

> **Puerta**: 8 celdas seguidas **en el 6×6** con error acumulado **< 15 mm** y rumbo **< 3°**.

### Bloque 4 — Aceleraciones

- **Perfil de avance** con aceleración y **frenada separadas** (acelerar suave, frenar fuerte) en vez de
  la pendiente única de `setpoint.c`.
- **Perfil de velocidad angular para el giro**: una rampa de velocidad angular con aceleración limitada
  (como ZoroBot3: `angular_accel`, `max_angular_speed`) en vez del lazo de posición con mínimo
  artificial. **Es lo que quita el rebote de raíz.**
- Jerk solo si a 250 mm/s se nota (probablemente no).

> **Puerta**: avance de celda **sin sobrepaso** y sin pico de corriente; giro **sin rebote**.

### Dependencias y orden

```
Bloque 1 (actuador) ──┐
                      ├──► Bloque 3 (movimiento) ──► Bloque 4 (aceleraciones)
Bloque 2 (sensores) ──┘
```

**1 y 2 en paralelo** (uno es software, el otro hardware y compras), y **3 y 4 después**: medir rectitud
con el actuador grueso es medir el ruido.

### Lo que queda fuera de este plan

Superado el bloque 4, y **solo con datos**:

- La **pared como referencia absoluta** (壁切れ) — es el siguiente salto, y necesita el bloque 2 hecho.
- `WF_Paso()` no bloqueante.
- *Flood fill*, giros de 45° y diagonales, guardar calibraciones en Flash.

---

## 5. El laberinto 6×6: cómo aprovecharlo

Es **el mejor instrumento que vamos a tener**, y cambia tres cosas:

1. **Paredes oficiales** ⇒ se puede calibrar el umbral contra el material y el color reales, que es
   justo lo que el `PAREDES` avisa que hay que hacer *in situ*.
2. **Se puede cerrar un recorrido entero** ⇒ el *wall follower* por fin se puede **completar**, que es
   algo que nunca ha pasado (el 4×4 de cartón nunca llegó a existir).
3. **Se puede medir el error acumulado** contra una referencia que no es el propio robot.

**Cómo usarlo, en orden:**

| Medida | Para qué |
|:--|:--|
| Umbral de pared contra las paredes nuevas | Saber si 75 sirve o hay que moverlo |
| Error por celda en 8 celdas rectas | H1: ¿el error se acumula? ¿dónde? |
| Rizo de velocidad en régimen | H2: ¿balancea por la cuantización? |
| `MOV_D` por giro (debe ser 3-6 mm) | Confirmar que el pivote sigue limpio |
| Cierre de un cuadrado de 4 celdas | Comparar odometría contra la realidad |

---

## 6. Decisiones abiertas

- **¿Fase 1 antes o después de la Fase 0?** La propuesta es **después**: si se cambia el actuador antes
  de medir el rizo, se pierde la medida de referencia.
- **¿Cuánto se sube el PWM?** Propuesta: **20 kHz** (`ARR = 3599`). Por encima de 50 kHz no se gana y
  suben las pérdidas de conmutación; por debajo de 16 kHz se oye.
- **¿Se acepta el coste de la Fase 2?** Es **hardware** (apantallar, cambiar $R_L$) y es la que
  desbloquea las otras dos. Sin ella, las fases 3 y 4 **no se pueden hacer**.
- **¿Cuándo se replantea el control por posición (UKMARSBOT)?** Propuesta: **no en este plan**. El
  feedforward de fricción de la Fase 1 ataca el mismo problema con una fracción del trabajo.

---

## 7. Relacionado

- [puesta-a-punto.md](./puesta-a-punto.md) — **el guion de la campaña**: procedimiento de cada bloque, puertas y tablas de resultados
- [firmwares-referencia.md](./firmwares-referencia.md) — el análisis de los otros tres firmwares
- [esquema-firmware.md](./esquema-firmware.md) — cómo está montado lo nuestro
- [estado-del-proyecto.md](./estado-del-proyecto.md) — qué está hecho
- [pruebas-bringup.md](./pruebas-bringup.md) — el catálogo de pruebas de banco
