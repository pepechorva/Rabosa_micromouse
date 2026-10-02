# 🏁 En el laberinto, SIN PC — chuleta para imprimir

> **Restricción del evento**: en el laberinto **no se puede usar nada externo al robot** — ni WiFi, ni
> cable UART, ni portátil. Todo lo de esta hoja se hace con **la OLED y los cuatro botones**.
>
> ⚠️ **Y el firmware está congelado**: lo que lleve el robot es lo que hay, porque allí **no se puede
> reflashear**. Un fallo que no sea de umbrales **no se arregla en el laberinto**.

---

## 0. Antes de salir de casa

| | Qué |
|:-:|:-----|
| ☐ | **Desconectar el ESP8266 de la UART.** Allí no solo no sirve: con la placa arrancando y el módulo cableado, su eco inunda el anillo de recepción |
| ☐ | Batería cargada. Por debajo de **6,00 V** el robot entra en **FAULT** (aviso a 6,60 V) |
| ☐ | **Anotar en papel** los umbrales de pared que funcionan en casa **y** los de fábrica, para saber de dónde se parte |
| ☐ | Imprimir esta hoja |

---

## 1. Los botones

| Botón | En el menú | Dentro de una prueba |
|:-----:|:-----------|:---------------------|
| **MENU** | vuelve atrás | **sale de la prueba** |
| **UP** | sube el cursor | según la prueba |
| **DOWN** | baja el cursor | según la prueba |
| **OK** | entra | según la prueba |

> **MENU va por interrupción**: sale de cualquier prueba aunque el robot esté andando. Es el botón del
> pánico. Si el robot se descontrola: **MENU**; si no responde, **PARAR** en el menú raíz.

---

## 2. Cómo llegar a cada sitio

El menú raíz es `TEST` · `EXPLORE` · `RESOLVE` · `SETTINGS` · `DIAG` · `PARAR`, y se entra desde
`RABOSA-MVR`:

| Quiero… | Pulsaciones desde `RABOSA-MVR` |
|:--------|:-------------------------------|
| **Ajustar los umbrales de pared** (`PAREDES`) | `OK` · `DOWN` · `OK` · `OK` |
| **Comprobar la batería** (`BATERIA`) | `OK` · `OK` · `DOWN` · `OK` |
| Alinear los focos IR (`EMISORES IR`) | `OK` · `DOWN` · `OK` · `DOWN` · `OK` |
| Comprobar el IR modulado (`IR MODULADO`) | `OK` · `DOWN` · `OK` · `DOWN`×7 · `OK` |
| Movimiento y giros (`MOVIMIENTO`) | `OK` · `DOWN` `DOWN` · `OK` · `OK` |
| PID (`PID VELOCIDAD`) | `OK` · `DOWN` `DOWN` · `OK` · `DOWN` · `OK` |
| **Recorrer el laberinto** (`WALL FOLLOWER`) | `DOWN` · `OK` · `OK` |

> 🔢 **El contador de la cabecera es la brújula.** Cuando una lista tiene más filas de las que caben,
> el título lleva `i/n` (`TEST IR 3/9`). **No cuentes pulsaciones: baja hasta que ponga la fila que
> quieres y pulsa OK.** Las filas del bloque **IR** son:
>
> `1` PAREDES · `2` EMISORES IR · `3` IR RECEPTORES · `4` VER R0 · `5` VER R1 · `6` VER R2 ·
> `7` VER R3 · `8` IR MODULADO · `9` CURVA IR
>
> ⚠️ **Ese bloque pasó de 3 a 9 filas el 2026-10-02** (se recuperaron `EMISORES IR`, `IR RECEPTORES`
> y `VER R0`..`VER R3`), así que **cualquier ruta apuntada de memoria antes de esa fecha está mal**.
> El contador no miente: si no dice lo que esperas, sigue bajando.

---

## 3. Ajustar los umbrales de pared (5 minutos, y hay que hacerlo)

**No se puede calibrar en casa**: las paredes de un laberinto ajeno pueden no reflejar como las tuyas.
Se hace **allí, con el robot quieto y en el pasillo donde va a correr**.

La pantalla:

```
PAREDES U 300     <- umbral de la pared seleccionada
>FRE 120 1        <- el '>' marca la seleccionada · medida · veredicto (1 = hay pared)
 IZQ  30 0
 DER 200 1
```

1. **OK** cambia de pared · **UP/DOWN** mueven su umbral.
2. Criterio: **con pared, `1`; sin pared, `0`**. Deja el umbral **en medio** de las dos medidas.
3. ⚠️ **Apunta los tres números en papel en cuanto funcionen.** Los umbrales **viven en RAM: un
   reinicio los devuelve a los de fábrica** y hay que volver a entrar aquí.

> Sin pared el valor de fondo también importa: si la medida «sin pared» ya está cerca del umbral, el
> robot verá paredes donde no las hay. **Súbelo** hasta que el pasillo abierto dé `0` con margen.

---

## 4. Recorrer el laberinto (`WALL FOLLOWER`)

```
WALL FOLLOWER >   <- la '>' dice que está andando
MANO DER C 12     <- mano y celdas recorridas
F1 I0 D1 AVANZA   <- las tres paredes y lo que decidió
OK:PARA MENU:SALIR
```

| Botón | Qué hace |
|:-----:|:---------|
| **OK** | arranca o para |
| **UP** | mano izquierda |
| **DOWN** | mano derecha |
| **MENU** | sale |

**La fila 3 es la que dice por qué se ha parado** — y es lo único que no se puede deducir de un vistazo:

| Fila 3 | Significa |
|:-------|:----------|
| `OK:ANDA UP:IZ DN:DE` | Parado **a mano** (OK). Puedes reanudar |
| `PARADO: ATASCO` | **Empujando contra algo**: un avance agotó sus 8 s sin llegar. **Umbral mal puesto** |
| `PARADO: FIN 250` | Recorrió el tope de 250 celdas. En un árbol **no debería pasar** |

> Cada vez que pulsas **OK** para arrancar, el contador de celdas **se pone a cero** y la traza se
> reinicia. Así `C` siempre cuenta **la pasada que estás viendo**.

---

## 5. Si algo va mal

| Síntoma | Qué mirar |
|:--------|:----------|
| Gira donde **no hay hueco** | Umbral **demasiado bajo**: el fondo se lee como pared. Vuelve a la §3 y súbelo |
| **No ve** una pared que está ahí | Umbral **demasiado alto**. Bájalo |
| `PARADO: ATASCO` en un pasillo abierto | Está viendo pared de frente donde no la hay ⇒ **umbral de FRENTE bajo** |
| Se para y la fila 3 no es `ATASCO` ni `FIN 250` | Lo has parado tú, o has pulsado **MENU** |
| La pantalla se queda **como estaba** y **ningún botón responde** | ⚠️ Es un **cuelgue**, no unos botones rotos. Reinicia la placa |
| Se reinicia solo (vuelve a `RABOSA-MVR`) | **Eléctrico**: batería baja o el cable del ST-Link. Y **pierdes los umbrales** |
| El LED parpadea a 4 Hz y la pantalla habla de batería | **FAULT** por pack bajo: cambia la batería y pulsa **OK** |

---

## 6. Consultar lo que vive en RAM (y qué se puede cambiar)

Casi todos los números del robot son `#define` **compilados** en el firmware: no se ven ni se tocan en
el laberinto (µm/cuenta, trocha, tamaño de celda, velocidad de crucero, duty máximo…). Estos son los
que **sí viven en RAM**, dónde se miran, y si hay forma de cambiarlos allí:

| Valor | Dónde se consulta | ¿Se puede cambiar sin PC? |
|:------|:------------------|:--------------------------|
| **Umbrales de pared** (los tres) | **`PAREDES`** (prueba 21): `PAREDES U <umbral>` arriba, y las tres medidas con su veredicto | ✅ **Sí**: `OK` cambia de pared, `UP/DOWN` mueven su umbral |
| **Ganancias del PID** (Kp, Ki, Kd) y la velocidad | **`PID VELOCIDAD`** (prueba 18), filas 0-1 | ✅ **Sí**: `OK` elige parámetro, `UP/DOWN` lo mueven |
| **Estado de la batería** (estado, `TOTAL`, `CEL1`, `CEL2`) | **`BATERIA`** (prueba 12) | ❌ No: sus umbrales se cambian con la orden `BAT`, que necesita PC |

### Cómo leer las pantallas

**Umbrales de pared** — la fila de arriba es el umbral **de la pared seleccionada** (la que lleva `>`):

```
PAREDES U 300     <- umbral de la pared marcada con '>'
>FRE 120 1        <- la seleccionada: medida · veredicto (1 = hay pared)
 IZQ  30 0
 DER 200 1
```

Cada pulsación de **OK** pasa a la siguiente pared, así que **las tres se leen en tres pulsaciones**.
Es el número que hay que apuntar en papel.

**PID** — la pantalla solo enseña **tres de los cuatro** valores a la vez (el marcado con `>` y dos más):

```
PID >KP 0.10      <- el '>' marca el que mueven UP/DOWN
KI 0.50 KD 0.00
IZQ 298 DER 301
SET 300 D 42 43
```

Hay que **pulsar OK para ir rotando** (`KP` → `KI` → `KD` → `VEL`) y apuntar los cuatro.

> ⚠️ **Todo esto se pierde al apagar o reiniciar.** El robot arranca siempre con los valores de
> fábrica ⇒ **apunta en papel los que funcionen, en cuanto funcionen**. Es lo único que convierte un
> reinicio en dos minutos de teclear botones en vez de una recalibración a ciegas.

> 💾 **La traza también está en RAM, pero no se puede leer aquí**: hace falta un PC para descargarla con
> la orden `TRACE`. En el laberinto, lo que se sabe de un recorrido es lo que se vio en la pantalla.

---

## 7. Lo que NO se puede hacer en el laberinto

- ❌ **Telemetría / Teleplot** — no hay cable ni WiFi.
- ❌ **Leer la traza** (`TRACE`) — está en RAM, pero para sacarla hace falta un PC.
- ❌ **Órdenes de consola** (`MOV`, `PID`, `BAT LIST`, `LED`) — necesitan el cable.
- ❌ **Reflashear** — cualquier arreglo de firmware tendría que haberse hecho antes.
- ❌ **Cambiar los umbrales desde otro sitio** — solo con los botones, en la prueba `PAREDES`.
