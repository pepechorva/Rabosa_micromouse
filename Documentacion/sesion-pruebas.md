# 🎯 Sesión de pruebas: la última tarde (2026-10-02)

> Lo que queda por ejecutar, **en orden de lo que bloquea la competición**, no por número de prueba.
> Cada bloque dice qué hacer, qué mirar y **cuándo darlo por bueno**. Los procedimientos largos están
> en [pruebas-bringup.md](./pruebas-bringup.md); esto es el guion de la sesión.

**Lo que se persigue hoy**: que el *wall follower* haga **4 o 5 celdas con dos giros, incluido un
cruce, sin tocar nada y sin `PARADO: ATASCO`**. Todo lo que sea más, premio.

---

## 0. Montar el sitio (media hora, y condiciona todo lo demás)

| Qué | Cómo |
|:----|:-----|
| **Un pasillo de 180 mm** | Dos listones, libros o cajas. **Ancho interior exacto**: es la celda del laberinto |
| **Un cruce** | El pasillo, más **una celda lateral** que salga a un lado. Sin cruce no hay decisión que probar |
| **Paredes altas y mates** | ~15 cm y **blancas o claras, sin brillo**. Una pared negra o brillante devuelve otra cosa: los umbrales que saques hoy valdrán menos |
| **Suelo liso y nivelado** | La fricción estática depende del reparto de peso; una mesa inclinada ya falseó unas medidas de duty el otro día |
| **Batería, no fuente** | Las pruebas de motor se hacen con la LiPo. La fuente se hunde con los escalones de corriente |

> **Antes de empezar:** reflashea (la fila 3 del *wall follower* y la prueba 12 cambiaron), y
> **levanta el puente** (`TEST → VARIOS → WIFI AL PC`) con el servidor del PC ya escuchando.
>
> ⚠️ **Y el orden importa: flashea PRIMERO y levanta el puente DESPUÉS.** Al resetear el MCU el ESP
> **sale del modo transparente**, así que **cada reflash tumba el puente**: el socket queda `ESTAB`
> pero mudo (`ss -tn` lo enseña con `Recv-Q 0`) y **no llega ni un byte**, ni el `>BOOT:1`.
> Consecuencia práctica: **no dar por hecha ninguna prueba que solo hable por telemetría hasta ver
> el primer byte llegar**, o se repite el trabajo (el 2-oct se perdió una curva IR entera así).

---

## 1. Prueba 16 · IR MODULADO — 15 min

**Ruta**: `OK` `DOWN` `OK` `DOWN` `OK`

| | |
|:--|:--|
| **Qué hacer** | Robot quieto con las paredes del pasillo puestas. Encender y apagar la luz de la sala |
| **Mirar** | **En la pantalla**, de dos en dos: `R0`/`R1` en una fila y `R2`/`R3` en la siguiente (recuerda: `R0` frontal dcho · `R1` lateral dcho · `R2` lateral izq · `R3` frontal izq). Con puente: `>R0_DIF`..`>R3_DIF` y `>R0_VAR`..`>R3_VAR` |
| **Criterio** | La `DIF` de cada canal cambia **menos del ±10 %** al cambiar la luz |

**Por qué primero**: la detección de paredes es **ON − OFF**. Si esa diferencia se mueve con la luz, todo
lo de después se apoya en un número que baila.

**Si sale mal** (una `VAR` alta, o la `DIF` se va): apunta **qué canal** y **cuánto**, y seguimos. No se
arregla cambiando firmware (sería cosa de la resistencia de carga), pero hay que saberlo para
interpretar lo de después.

---

## 2. Prueba 21 · PAREDES — 30 min ← **la más importante de la tarde**

**Ruta**: `OK` `DOWN` `OK` `OK`

| | |
|:--|:--|
| **Qué hacer** | Robot **dentro del pasillo**, centrado en un tramo recto, **quieto** |
| **Mirar** | `>WALL_0` `>WALL_1` `>WALL_2` (medidas), `>WALL_n_UMB` (umbrales) y `>WALL_n_HAY` (veredictos) |
| **Botones** | **OK** cambia de pared · **UP/DOWN** mueven **su** umbral |

`0` = **frente** · `1` = **derecha** · `2` = **izquierda**

### El ajuste, en un paso

Con el robot en el pasillo: **IZQ = 1 · DER = 1 · FRENTE = 0**.

Deja cada umbral **en medio** entre la medida con pared y la medida de fondo del mismo canal: así el
valor no está al filo. Y súbelo hasta que el pasillo abierto dé `0` **con margen**.

### ⚠️ La comprobación que hay que hacer sí o sí

**Si `FRENTE` sale `1` en un pasillo abierto, los frontales están viendo las paredes laterales.** Van
montados en «V» y se validaron contra una pared **de frente a 10 cm**, nunca dentro de un pasillo de
180 mm. Es la sospecha número uno de esta tarde.

- Se arregla **subiendo el umbral de FRENTE** (que deje de ver los laterales).
- Y hay que comprobarlo en los dos casos: **pasillo abierto** (frente `0`) y **fondo de saco** (frente `1`).

**Apunta los tres umbrales en papel en cuanto funcionen.** Viven en RAM: un reinicio los borra.

---

## 3. Prueba 22 · WALL FOLLOWER — el resto de la tarde

**Ruta**: `DOWN` `OK` `OK`

**Antes**: robot al principio del pasillo, **centrado y mirando por el pasillo**, y **reflashear la
sesión** (cada OK arranca el contador y la traza de cero).

```
WALL FOLLOWER >   <- la '>' dice que anda
MANO DER C 12     <- mano y celdas
F1 I0 D1 AVANZA   <- las tres paredes y lo que decidió
OK:PARA MENU:SALIR
```

| Botón | Qué hace |
|:-----:|:---------|
| **OK** | arranca o para |
| **DOWN** | mano derecha (por defecto) |

### Criterio

**4-5 celdas con dos giros, un cruce por medio, sin tocar y sin `PARADO: ATASCO`.**

### Qué mirar mientras anda

| Etiqueta | Qué es |
|:---------|:-------|
| `>WF_CELDAS` | Celdas recorridas |
| `>WF_DEC` | Decisión tomada (1 avanza · 2 gira izq · 3 gira der · 4 vuelta) |
| `>WF_FRENTE` `>WF_IZQ` `>WF_DER` | Las tres paredes **según el umbral** |
| `>WF_ANG` | Rumbo acumulado **según la odometría** |
| `>WF_GYR` | Rumbo acumulado **según el giroscopio** (no depende de las ruedas) |
| `>WF_GYROK` | 1 = el rumbo lo corrige el giroscopio · 0 = respaldo por odometría |

> **`>WF_ANG` es el termómetro del residuo de los giros.** Cada giro se deja 15-19 mm y ±4-7°, y eso
> **se suma**: si `ANG` crece sin parar, el robot irá descentrándose en el pasillo hasta ver una pared
> que no existe. Es el fallo más probable y el que menos se ve leyendo el código.
>
> ⚠️ Y **`ANG` ya no es el único número**: desde el 2026-10-02 **el rumbo del avance y el ángulo de los
> giros** los cierra el giroscopio, así que si `>WF_GYR` se queda cerca de cero mientras `>WF_ANG` se va,
> el que se equivoca es la odometría (µm por cuenta o trocha), no el lazo. Medido el 02-oct: en el
> **avance** los dos coinciden (+2,3° frente a −0,1°) y en el **giro** la odometría sobreestima un 44 %
> (125° frente a 87°) — de ahí que el giro también se cierre con el giroscopio, y que el residuo de
> posición de 15-19 mm por giro siga siendo el de siempre.

### Si para

| Fila 3 | Qué hacer |
|:-------|:----------|
| `PARADO: ATASCO` | **MENU** para salir, y **manda `TRACE`**. Es el caso a analizar |
| `PARADO: FIN 250` | Recorrió el tope: estaba «viendo» celdas que no existen ⇒ umbral de **FRENTE** bajo |
| (sigue `OK:ANDA…`) | Lo has parado tú: OK reanuda |

---

## 4. Prueba 20b · giros con el corrector de centro a 8 — solo si sobra tiempo

`MOTION_GIRO_CENTRO_KP` se subió de 4 a **8** y **nunca se ha vuelto a medir**. Por el puente:

```
MOV UP 4        (cuatro giros de +90°)
MOV DOWN 4      (cuatro de -90°)
```

**Criterio**: giro < 2° y traslación pequeña. Lo último medido fue **+83,3° / +90,7°** con 15-19 mm de
deriva.

---

## Anexo A · Cómo se lee la traza (esto es lo que necesito yo)

La traza va en **RAM** y se descarga con la orden **`TRACE`**, que **bloquea 5,12 s**. Se manda desde el
menú (fuera de la prueba 22), y cae en el registro del puente.

```
python3 /tmp/rabosa_traza.py            # por defecto lee /tmp/puente.log
```

El analizador tabula **una fila por celda** (medidas + decisión) y, sobre todo, **deduce qué umbrales
explican las decisiones tomadas**:

- **Sale una banda** (p. ej. `96..260`) ⇒ la regla funciona; si el robot se equivocó, fue por umbral o
  por geometría.
- **Sale «NINGÚN umbral»** ⇒ el problema **no son los umbrales**: es la regla de decisión o la mano, y
  hay que mirar `wf_decidir()`. Eso ahorra media tarde de ajustar botones en balde.

## Anexo B · Qué contarme

En cada paso, con esto me basta:

1. **Qué hace el robot** (aunque te parezca que «no hace nada raro»).
2. **Qué pone la pantalla** (las cuatro líneas).
3. **El trozo de registro** del puente, o el nombre del fichero si es largo.

No hace falta que interpretes nada: el «hace algo raro» ya es el dato.
