# 🎛️ Comandos por el puerto serie — Rabosa-MVR

Cómo se le **envían órdenes** al robot por el mismo puerto de la telemetría (**USART3**, PB10/PB11,
115200 baudios), para ajustar cosas que dependen del montaje sin recompilar ni flashear.

| Sentido | Quién | Formato |
|:--------|:------|:--------|
| Salida (robot → PC) | `app/telemetry.c` | `>ETIQUETA:valor` (Teleplot) |
| **Entrada** (PC → robot) | `app/console.c` | línea de texto terminada en `\n` |

La recepción va **por interrupción** (`hal/uart.c`, buffer circular de 64 B): los bytes que llegan se
guardan aunque el firmware esté dentro del bucle de una prueba, y `CONSOLE_Update()` los procesa
cuando puede. Se atiende en el **menú** (1 kHz) y dentro de las pruebas cuya configuración se toca en
vivo: la **12** (umbrales de batería, viendo la tensión), la **18** (ganancias del PID, viendo la
respuesta del motor) y la **20** (movimientos, con la orden `MOV`).

> ⚠️ Una prueba que no llame a `CONSOLE_Update()` en su bucle deja la consola **muda** mientras corre:
> las órdenes se quedan en el anillo hasta que se vuelve al menú, que es justo cuando ya no sirven.
> Es lo primero que hay que añadir al escribir una prueba nueva que se sintonice en vivo.

---

## 1. Órdenes

| Orden | Qué hace |
|:------|:---------|
| `HELP` o `?` | Publica el estado de la batería **y** del lazo de velocidad |
| `BAT LIST` | Publica los 4 umbrales, el factor del divisor (`>BAT_K`) y la tensión medida (`>VBAT`) |
| `BAT <UMBRAL> <valor>` | Cambia un umbral y publica los valores |
| `BAT CAL <valor>` | **Calibra el divisor**: le dice al firmware cuál es la tensión REAL del pack ahora mismo |
| `PID LIST` | Publica las ganancias y la consigna del lazo de velocidad (prueba 18) |
| `PID KP\|KI\|KD <valor> [IZQ\|DER]` | Cambia una ganancia **con el lazo corriendo**; sin rueda, en las dos |
| `PID VEL <mm/s>` | Cambia la consigna de velocidad de la prueba 18 |
| `GYRO [LIST]` | Publica la **referencia y las siete ganancias del lazo de giro** |
| `GYRO ODOM\|GIRO\|FUSION` | Elige con qué se cierra el **giro**: odometría, giroscopio o la media de los dos |
| `GYRO KP\|KI\|KD\|ESC\|VMIN\|VARR\|VDPS <valor>` | Cambia una ganancia del giro **sin reflashear** |
| `MOV OK\|UP\|DOWN [n]` | Lanza `n` movimientos en la **prueba 20** (primitivas de `app/motion.c`): celda de 180 mm o giros de 90° |
| `TRACE` | Vuelca la **traza en RAM** del lazo por telemetría (`>TR_SET`, `>TR_IZQ`, `>TR_DER`, `>TR_FIN`) |
| `LAZO` | Publica los **tiempos medidos** del lazo, de la OLED y de la UART, y reinicia sus máximos |
| `TEST <etiqueta>` | **Lanza una prueba del banco** por su nombre del menú (`TEST SINTONIA`) |
| `SALIR` o `MENU` | Sale de la prueba en curso, igual que el botón MENU |
| `LED <0\|1>` o `LED TOGGLE` | Enciende, apaga o conmuta el **LED de la placa** (PC13) y publica `>LED` |

`HELP` no imprime una ayuda de texto: **toda la telemetría es numérica** (decisión del 30-sep), así que
responde con los valores. La lista de órdenes es este documento.

### Cómo se manda una orden

Hay que **escribir la línea en un programa que tenga abierto el puerto serie**, terminada con `\n`. El
**Teleplot de VS Code** vale para las dos cosas a la vez: dibuja la telemetría y trae su caja para
enviar, así que no hace falta cambiar de herramienta.

> ⚠️ **Un puerto serie lo abre un programa, y el resto se queda fuera**: si otro programa está leyendo
> el mismo `/dev/ttyUSB*` (un monitor serie, una sesión de `picocom`), el envío no saldrá. **No tiene
> nada que ver con el OLED**, que va por I²C: el conflicto, si lo hay, es entre dos programas y el mismo
> puerto serie.

> [!TIP] Con el cable puesto, la consola manda sobre los botones: se pueden pedir **valores exactos**
> (`PID KP 0.37`) en vez de ir a saltos. Los botones existen para cuando el robot está **en el suelo**,
> que es donde la sintonía vale de verdad.

### Si la consola no responde

Se diagnostica en **30 segundos con una sola orden**:

```
LED 1
```

Enciende el LED de la placa (PC13). No toca nada del robot, se ve a simple vista y no necesita Teleplot,
así que **separa los dos fallos posibles**:

| Resultado | Qué significa | Dónde mirar |
|:----------|:--------------|:------------|
| El **LED se enciende** y sale `>LED:1` | ✅ **La consola funciona** | El fallo estaba en la orden que probabas: revisa el nombre y los parámetros (`>CMD_ERR` avisa) |
| **No pasa nada** | ❌ **No llega la orden** | Ver la lista de abajo |

Si no llega, por orden de probabilidad:

1. **Falta el salto de línea.** La consola ejecuta la orden al recibir `\n` o `\r`; si el programa del PC
   la manda sin terminar, el firmware se queda esperando y **no ejecuta nada**. Es el fallo más común
   con las cajas de envío de los monitores.
2. **No se está atendiendo la consola.** `CONSOLE_Update()` se llama en cada paso de `APP_Update()`
   (1 kHz), **antes** de dar el paso a la máquina de estados, así que se atiende en todo lo que no
   bloquee:

   | Dónde estés | ¿Se atiende? |
   |:------------|:------------:|
   | **Menú** (raíz y cualquier submenú, navegando) | ✅ 1 kHz |
   | Pantallas de `STOP`, `FAULT` y arranque | ✅ |
   | Dentro de las pruebas **12**, **18** y **19** | ✅ (esas pruebas la llaman) |
   | Dentro de **cualquier otra prueba** | ❌ se queda en el anillo hasta salir al menú |
   | Durante la **cuenta atrás** de una prueba | ❌ |

   > [!TIP] Los valores se pueden **preparar desde el menú** y entrar después en la prueba
   > (`PID KI 0`, `PID VEL 150`…) con todo ya puesto. Es lo más cómodo para no pelearse con los
   > botones dentro de la prueba.

3. **Puerto, baudios o cable.** Tiene que ser el **USART3** (PB10/PB11) a **115200**, y ser el único
   programa que lo tiene abierto.

Tras cualquier cambio se publican **los cuatro umbrales** (`>BAT_AVISO_PACK`, `>BAT_CRIT_PACK`,
`>BAT_AVISO_CELDA`, `>BAT_DESEQ`), así que en Teleplot se ve el estado completo sin pedirlo.

### Umbrales de la batería

Los de `sensor/battery.c`. Los tres primeros en voltios; `DESEQ` es la **diferencia** entre celdas.

| Nombre | Por defecto | Qué dispara |
|:-------|:-----------:|:------------|
| `AVISO_PACK` | 6.60 V | `BATTERY_AVISO` (LED a 1 Hz + el estado en la pantalla) |
| `CRIT_PACK` | 6.00 V | `BATTERY_CRITICA` ⇒ **FAULT** (freno total) |
| `AVISO_CELDA` | 3.30 V | Aviso **informativo** (rótulo `CELDA`, LED a 1 Hz) |
| `DESEQ` | 0.40 V | Aviso **informativo** de desequilibrio entre celdas |

> [!NOTE] Por qué estos valores (30-sep)
> Los de aviso bajaron de **7,00 a 6,60 V** de pack (de 3,45 a **3,30 V/celda**) porque avisaban con la
> batería sana, y el desequilibrio pasó de **0,15 a 0,40 V** porque 0,17 V —una batería normal—
> encendía el aviso. Ahora solo se avisa cuando alguna celda se acerca de verdad a los 3,30 V.

> Solo **`CRIT_PACK`** bloquea. La celda es monitorización: una batería de otra química, una fuente de
> laboratorio o un divisor sin montar no pueden encerrar el robot.

### Cómo se escribe la tensión

| Se escribe | Se interpreta | Por qué |
|:-----------|:--------------|:--------|
| `7.4` | 7400 mV | **con punto = voltios** |
| `7400` | 7400 mV | **sin punto = milivoltios** |
| `6.6`, `3.3`, `0.4` | 6600, 3300, 400 mV | lo mismo, con decimales |

Se admite entre **0,10 y 15,00 V**. Si el valor es incoherente (el crítico del pack tiene que quedar
por debajo del de aviso) o está fuera de rango, se rechaza con `>CMD_ERR:<código>` y los umbrales no
cambian.

| `>CMD_ERR` | Significado |
|:----------:|:------------|
| 1 | Umbral desconocido |
| 2 | Valor no válido (no es un número) |
| 3 | Valor rechazado (fuera de rango o incoherente) |
| 4 | Orden desconocida |
| 5 | No se pudo calibrar (no hay medida del pack, o la tensión no cuadra con un divisor) |

### Ganancias del PID de velocidad (prueba 18)

Los valores de `app/application.c`. Se escriben **siempre con punto** (`0.35`): aquí no hay la
ambigüedad de unidades de la batería, porque no son tensiones.

| Nombre | Por defecto | Rango aceptado | Unidad |
|:-------|:-----------:|:--------------:|:-------|
| `KP` | 0.10 | 0 … 10 | %/(mm/s) |
| `KI` | 0.50 | 0 … 50 | %/(mm/s·s) |
| `KD` | 0.00 | 0 … 5 | %·s/(mm/s) |
| `VEL` | `300` | 0 … 1000 | mm/s |

Las ganancias son **una por rueda**: `PID KP 0.12 DER` toca **solo** la derecha y `PID KP 0.10` las
dos. Los valores por defecto son los **medidos en el robot** (ver
[control-pid.md](./control-pid.md) §4): dan un **0,15 % de error de régimen** a 300 mm/s.

> [!NOTE] Por qué hay una ganancia por rueda, si hoy valen lo mismo
> Los dos N20 no tienen por qué ser iguales, así que la estructura está puesta desde el 11-oct.
> **Medido con las ruedas en el aire, estos dos motores piden el mismo duty para la misma velocidad
dentro del ruido** (0,2-0,5 σ a 80 y a 300 mm/s), así que hoy las dos ruedas llevan los mismos
> valores. Los números, y lo que esa medida **no** cubre (la carga), están en
> [control-pid.md](./control-pid.md) §4.

Funciona **de las dos maneras**: se puede dejar puesta la ganancia antes de entrar en la prueba (se
aplica al arrancar) o cambiarla **mientras corre** (se aplica en el paso siguiente del lazo, a
100 Hz). Sin esto, cada tanteo de sintonía son un recompilar, un flashear y un reinicio que borra la
observación en curso.

> ⚠️ **`PID KP 0` solo deja la P si el integrador se resetea.** El integrador guarda un **término de
> salida en %**, no un acumulador del error: con `KI = 0` conserva su último valor y **ese valor sigue
> sumándose a la salida**, así que las ruedas siguen girando. `PID_SetGanancias()` llama a
> `PID_Reset()`, y aun así **conviene comprobarlo**: si al poner las tres ganancias a cero el duty no
> se va a 0, algo se ha quedado cargado (pasó el 2026-10-01).

Con cada cambio se publican las **seis ganancias y la consigna** —`>PID_KP_I`, `>PID_KP_D`, `>PID_KI_I`,
`>PID_KI_D`, `>PID_KD_I`, `>PID_KD_D` y `>PID_VEL`—, las mismas etiquetas que manda la prueba al
arrancar, para que el registro de Teleplot diga con qué ganancias se hizo cada tramo. **Una etiqueta
por rueda**: con ganancias por rueda, `PID_KP` a secas ya no identificaría nada.

> ⚠️ **No se guarda en Flash**: al reiniciar vuelven los valores de `PID_TEST_*_DEF`
> (`app/application.c`).

### Sintonía del lazo de giro (`GYRO`)

Existe para **no reflashear**, y no es una comodidad: cada flasheo cuesta tres minutos y, además,
**reinicia el robot y con él la calibración del giroscopio**, así que dos tandas seguidas no se podrían
ni comparar. Con esta orden se cambia un número y se repite la maniobra en segundos.

| Suborden | Qué toca | Arranque |
|:---------|:---------|:--------:|
| `GYRO` o `GYRO LIST` | Publica las siete y la referencia | — |
| `GYRO ODOM\|GIRO\|FUSION` | Con qué se cierra el giro | `GIRO` |
| `GYRO KP <mm/s por °>` | Proporcional al error de ángulo | 4,0 |
| `GYRO KI <mm/s por (°·s)>` | Integral, con anti-windup a 30 mm/s | 0,0 |
| `GYRO KD <mm/s por °/s>` | Amortiguación (frena según la velocidad angular) | 0,3 |
| `GYRO ESC <adimensional>` | **Escala del giroscopio** | 1,01 |
| `GYRO VMIN <mm/s>` | Mínimo de velocidad **con el robot girando** | 35,0 |
| `GYRO VARR <mm/s>` | Mínimo de velocidad **para arrancar del todo** | 40,0 |
| `GYRO VDPS <°/s>` | A partir de qué velocidad se considera que «ya rueda» | 0,0 |

Contesta con `>GYR_KP`, `>GYR_KI`, `>GYR_KD`, `>GYR_ESC`, `>GYR_VMIN`, `>GYR_VARR`, `>GYR_VDPS` y
`>GYRO_MODO`. Valor fuera de rango: `>CMD_ERR:3`. Parámetro desconocido: `>CMD_ERR:1`.

> ✅ **Los siete valores están fijados en `app/motion.h`**, así que **un reinicio no los pierde** — a
> diferencia de `PID`, que vuelve a sus `_DEF` al arrancar. La orden los cambia en RAM, pero el punto de
> partida ya es el bueno. Se midieron el **2026-10-02** (lazo, amortiguación y mínimos) y la **escala el
> 2026-10-11**, que pasó de 1,20 a **1,01** al comprobarla contra una marca en el suelo: el detalle está en
> [pruebas-bringup.md §16](./pruebas-bringup.md).

#### Cómo se diagnostica con estos números

⚠️ **Lo primero: `>MOV_GYR` NO sirve para calibrar `ESC`.** El lazo cierra sobre `gyro_total`, **que ya
está escalado**: al subir la escala el robot gira menos, pero el lazo sigue cortando cuando su cuenta
llega a 90 y la telemetría **vuelve a decir 90**. La referencia se valida a sí misma, y con cualquier
escala `>MOV_GYR` da 89-91° por esquina.

Para la escala hace falta una referencia **externa**, y es **una cinta en el suelo**: cuatro giros de
90° y la distancia del morro a la cinta, $d = 2L\sin(\Delta/2)$ con $L$ = 62 mm (0 mm son 360°,
~47 mm son 405°).

Con el ángulo en la mano, el síntoma dice **qué** ganancia tocar:

| Síntoma | Qué es | Qué tocar |
|:--------|:-------|:----------|
| El error es **proporcional** al ángulo (90°→89°, 360°→356°) | Escala | `ESC` |
| El error es **constante** en grados | Rozamiento o tolerancia de parada | `KP` |
| El robot **llega y vuelve hacia atrás** | El D manda más que el P | bajar `KD` |
| **No completa** el giro y una rueda se queda quieta | El mínimo no vence la fricción | subir `VMIN` |
| **Cruza el objetivo** de largo | El mínimo es demasiado alto | bajar `VMIN` |

> ⚠️ **Y una rueda dura invalida cualquier calibración.** Mientras una rueda arrastre, el robot pivota
> **sobre ella** y el ángulo que acaba teniendo **no es función de ninguna ganancia**: el chasis gira 90
> y el giroscopio lo mide bien, pero el morro acaba en otro sitio. Medido el 2026-10-02: el eje
> izquierdo iba duro, y en cuanto se arregló **la medida empezó a responder de forma monótona a `ESC`**.

### Mando de las primitivas de movimiento (prueba 20)

| Orden | Acción | `>MOV_CMD` |
|:------|:-------|:----------:|
| `MOV OK [n]` | Avanza `n` celdas de 180 mm | 1 |
| `MOV UP [n]` | Gira `n` × 90° a la **izquierda** | 2 |
| `MOV DOWN [n]` | Gira `n` × 90° a la **derecha** | 3 |

`n` es opcional (por defecto 1) y vale de 1 a `APP_MOV_REP_MAX` (20). Al aceptar la orden se publica
`>MOV_CMD` y `>MOV_REP`, y al terminar cada movimiento salen los cinco valores de siempre
(`>MOV_D`, `>MOV_ANG`, `>MOV_E_D`, `>MOV_E_ANG`, `>MOV_OK`).

```
MOV OK       -> una celda
MOV OK 4     -> cuatro celdas seguidas (cuatro movimientos, no uno de 720 mm)
MOV UP 4     -> 360° a la izquierda
```

> ⚠️ **La orden solo vale DENTRO de la prueba 20.** Es la única que llama a `CONSOLE_Update()` en su
> bucle y la única que recoge la petición; en cualquier otro sitio se responde `>CMD_ERR:3`. Aceptarla
> y no hacer nada haría creer que se ha perdido.

> ⚠️ **Un puerto serie solo lo abre un programa.** El bucle de la prueba atiende la consola **entre**
> movimiento y movimiento, así que mientras el robot anda (≈2,2 s por celda) la orden se queda en el
> anillo de recepción. Conviene **esperar al resultado antes de mandar la siguiente**: así el registro
> dice qué se pidió y qué salió, en orden.

**Por qué existe**: las primitivas son **bloqueantes** y los botones son físicos. Un cuadrado de cuatro
celdas y cuatro giros son ocho pulsaciones que hay que repetir enteras cada vez que se toca una
constante de `app/motion.h`, y no se puede hacer con el robot lejos. Con esto se guioniza desde el PC.

Los botones **siguen funcionando exactamente igual**: las dos vías llaman al mismo `mov_test_ejecutar()`,
que es lo que garantiza que lo que se mide por consola sea lo mismo que lo que se mide con el dedo.

> [!NOTE] Otras pruebas bloqueantes (la 18) sí atienden la consola durante el lazo, pero **solo la 18 y
> la 20** la atienden dentro de una prueba. En las demás las órdenes **no se leen** hasta volver al
> menú: ver el aviso de la cabecera de este documento.

### Traza del lazo (`TRACE`)

`app/trace.c` guarda en **RAM** las últimas 512 muestras del lazo (consigna, rueda izquierda y rueda
derecha, una cada 10 ms) y las vuelca cuando se pide. Existe porque el robot tiene que estar **en el
suelo** para que la sintonía valga —y ahí el cable estorba— y porque **un puerto serie solo lo abre un
programa a la vez**: si el que lee la telemetría no deja escribir, la orden `TRACE` no se puede mandar.

```
TRACE
>TR_SET:300
>TR_IZQ:298
>TR_DER:301
>TR_SET:300
...
>TR_FIN:1
```

**Por qué RAM y no Flash**: el STM32F103 **no tiene EEPROM**. Emularla obliga a borrar páginas de 1 KB,
desgasta la memoria y añade gestión de sectores, y la Flash del programa ya está al 81 %. En RAM sobran
18 KB.

> La descarga va **a velocidad real** (una muestra cada 10 ms, unos 5 s con el anillo lleno): así el eje
de tiempo de Teleplot es el de verdad y el periodo de la oscilación se lee directamente en segundos,
sin escalar nada.

> [!TIP] Lo normal es no necesitar la traza: la prueba 18 **mide `Tu` ella sola** y lo enseña en la
> pantalla (`SET 300 TU 0.42`). La traza es para cuando quieras **ver la forma** de la respuesta.

---

### Guionizar el banco desde el PC (`TEST` y `SALIR`)

`TEST <etiqueta>` lanza una prueba del banco **por su nombre del menú**, y `SALIR` sale de ella como
si se hubiera pulsado MENU. Juntas convierten el banco en algo **guionizable desde el PC**: lanzar,
leer la respuesta con marca de tiempo, cambiar una ganancia con `PID KP …` y volver a lanzar. Es lo
que hace posible sintonizar las dos ruedas una detrás de otra.

| Orden | Qué hace | Respuesta |
|:--|:--|:--|
| `TEST SINTONIA` | Lanza la prueba 18 (PID) | `>CMD_TEST:1`, luego `>APP_TEST:1` y `>APP_TEST_REM:1` |
| `TEST MOVIMIENTO` | Lanza la prueba 20 (primitivas) | igual |
| `SALIR` | Sale de la prueba en curso, como el botón MENU | `>CMD_SALIR:1` |

⚠️ **Lanzar una prueba puede arrancar motores**, y desde el PC no se ve el robot ⇒ las pruebas se
hacen **con las ruedas en el aire**.

Tres matices que conviene conocer:

1. **Solo desde el menú.** Con una prueba en curso, `TEST` se rechaza con `>CMD_ERR:3`: primero
   `SALIR`. Saltar de una prueba a otra sin salir dejaría vivos los motores de la anterior.
2. **`SALIR` solo se lee donde se lee la consola.** Las pruebas que no llaman a `CONSOLE_Update()` en
   su bucle (todas menos la 12, la 18, la 20 y la 24) no se enteran hasta que terminan solas.
3. **La etiqueta es la del menú**, no el número de prueba: `TEST SINTONIA`, no `TEST 18`. Se busca en
   todas las tablas de submenú una fila de tipo `LANZAR` con ese nombre.

> [!WARNING] **`SALIR` estuvo sin funcionar desde que se escribió hasta el 2026-10-11**, y merece la
> pena saber por qué: `BUTTONS_SolicitarSalida()` llamaba a `GPIO_MenuBtnIRQ_Handler()`, que despacha
> por `HAL_GPIO_EXTI_IRQHandler()` — y esa función **solo llama al callback si el flag de EXTI está
> puesto**, cosa que en una orden de consola no ocurre porque **no hay flanco**. El contador no subía:
> la orden llegaba, la consola acusaba `>CMD_SALIR:1` y el robot seguía. Ahora existe
> `GPIO_MenuBtnIRQ_Solicitar()`, que llama al callback directamente.
> 📌 **Lección**: la rutina de servicio de una interrupción **atiende** un flanco; no sirve para
> **inventarlo**.

> [!TIP] Por qué esto vive en la consola y no emulando los botones
> El `IDR` de un GPIO en STM32 es de **solo lectura**, así que falsificar OK/UP/DOWN desde el
depurador obligaría a reconfigurar el pin como salida —invasivo, y con el botón muerto hasta
> reiniciar—. Y escribir el estado de la FSM en RAM funciona, pero **las direcciones cambian en cada
> compilación**: medido el 2026-10-11, `tramo_maximo` se movió al añadir código y una lectura con la
> dirección vieja devolvió valores desplazados que parecían buenos. Una orden de consola no depende de
> nada de eso, y además sirve para cualquiera.

> [!NOTE] La vía buena para trabajar es el **cable serie**, no el puente WiFi
> El puente es cómodo para ver telemetría, pero **suele caerse con cada reflasheo**: al resetear el MCU
> el ESP sale del modo transparente y el socket que queda en el PC es un zombi, así que hay que volver
> a entrar en `TELEMETRIA → PUENTE WIFI` con los botones. El cable no: aguanta el reflasheo, así que
> permite iterar (grabar → lanzar → medir → cambiar una ganancia → repetir) sin tocar el robot.
>
> ✅ **Pero no es una ley, y comprobarlo cuesta dos segundos.** El socket TCP vive en el **ESP**, que
> es **otro chip**: si solo se resetea el STM32, el socket **sigue abierto** y la telemetría sigue
> llegando. Medido el 2026-10-11 en varios flasheos: cuando sobrevive se ven las líneas de arranque
> (`>RESET:n`, `>BOOT:1`, `>APP_EST:1`) sin tocar nada. **Antes de rehacer el puente, mira si llega
> telemetría.**

### Medir el retardo del lazo (`LAZO`)

Publica los tiempos medidos **en microsegundos** y reinicia los máximos:

| Etiqueta | Qué mide |
|:--|:--|
| `>LAZO_US` / `>LAZO_MAX` | Una vuelta del lazo: el trabajo **más** la espera hasta la siguiente |
| `>OLED_US` / `>OLED_MAX` | Un `OLED_Update()`: los 512 B del framebuffer por I²C |
| `>UART_US` / `>UART_MAX` | Una línea de telemetría por la UART |

`_US` es el último tramo cerrado; **`_MAX` es el número que importa**, porque esto no es un valor fijo
son **picos**. El cronómetro es el **TIM4** (`hal/timers.c`, 2 µs por cuenta) y los máximos se ponen a
cero **al lanzar una prueba**, así que la lectura habla de la prueba que acabas de hacer.

> [!IMPORTANT] Por qué se pide y no se publica solo
> Medir el lazo **no puede costar telemetría dentro del lazo**: esas seis líneas son ~100 B por la
> UART, o sea ~9 ms de bus, que serían justo el retardo que se está midiendo. Por eso la orden se pide
a mano, y el volcado se hace **antes** de reiniciar los máximos.

Qué esperar, y qué significa cada número:

| Tramo | Valor normal | Si sale mucho más alto |
|:--|:--|:--|
| `LAZO_MAX` | ≈ `MOTION_PASO_MS` + 1-2 ms de trabajo | Hay algo **bloqueando** dentro de la vuelta: mira `OLED_MAX` y `UART_MAX` |
| `OLED_MAX` | ~47 000 µs (≈47 ms) | Es el bus I²C a 100 kHz; con `I2C1_CLOCK_HZ` a 400 000 baja a ~12 ms |
| `UART_MAX` | ~1 500 µs (una línea de 17 B) | Un volcado de la traza manda muchas líneas seguidas |

⚠️ El contador de TIM4 es de **16 bits** y da la vuelta cada **131 ms**, así que un tramo más largo que
eso **no se puede medir** con él (saldría un número pequeño y falso). No es un problema para lo que hay
—el peor caso conocido ronda los 65 ms—, pero conviene saberlo antes de dar por bueno un valor raro.

📌 **Y ojo con la cadencia del banco**: en la prueba 18 la pantalla se repinta cada 200 ms y cuesta
~47 ms, así que el lazo se queda ciego **una cuarta parte del tiempo mientras estás juzgando `Kp`**.
La misión (`app/motion.c`, `app/wall_follower.c`) **no toca la OLED**, así que allí ese agujero no
existe: las ganancias que salgan del banco son **conservadoras** para la misión.

---

## 2. Calibrar la medida de la batería (`BAT CAL`)

La medida pasa por un divisor y por el ADC, así que arrastra la tolerancia de las resistencias (±1 %) y
la de **VREF+ = VDDA** (±2 % del regulador): sin calibrar, `>VBAT` puede desviarse un ±5 % — y con el
umbral crítico justo ahí, esa desviación decide si el robot cree que está bien o mal.

**Procedimiento** (2 minutos):

1. Poner la fuente/batería a una tensión conocida y medirla con el **multímetro**: p. ej. 7,4 V.
2. Lanzar la prueba **12 (BATERIA)** para ver qué dice el firmware: `>VBAT` y `>ADC_B0`.
3. Enviar la tensión real, **en milivoltios**: `BAT CAL 7400`.
4. El firmware recalcula el factor del divisor y publica `>BAT_K` y el `>VBAT` ya corregido.

```
BAT CAL 7400
>BAT_AVISO_PACK:7.000
>BAT_CRIT_PACK:6.600
>BAT_AVISO_CELDA:3.450
>BAT_DESEQ:0.150
>BAT_K:0.3099
>VBAT:7.400
```

### Si `>VBAT` sale muy bajo, mira `>ADC_B0` antes de calibrar

Con la fuente a **7,4 V** y el divisor montado (**22 kΩ arriba / 10 kΩ abajo**), el ADC debería leer unas **2870 cuentas**:

> La cuenta general: el firmware interpreta `V = cuentas × 3,3 / 4096 / k`, con $k$ = 0,3015 (el valor
> calibrado de arranque) ⇒ **≈ 0,00267 V por cuenta**.

| `>ADC_B0` | Tensión en el pin | Qué pasa |
|:---------:|:-----------------:|:---------|
| ≈2870 | 2,31 V | Todo bien: a 7,4 V este es el divisor **22k/10k** (con VDDA real algo menor saldrán ≈2769, y aun así `>VBAT` dará 7,4 V) |
| ≈2560 | 2,06 V | El divisor **no es 22k/10k** (¿otras resistencias?): `BAT CAL` lo corrige |
| ≈1940 | 1,56 V | El divisor **no cuelga de la batería** (¿del riel de 5 V?): revisar el cableado, calibrar no vale |
| 0..50 | ~0 V | Pin sin divisor o soldadura fría |

> [!NOTE] El factor calibrado **no se guarda en Flash**: al reiniciar vuelve al valor de arranque
> (`BAT_K_DIVISOR_DEF` en `sensor/battery.h`, hoy **0,3015**, que es el calibrado con el multímetro el
> 30-sep). Si tu robot necesita otro, pon ahí el que te dio `>BAT_K`.

> [!NOTE] Los umbrales **no se guardan en Flash**: al reiniciar vuelven a los valores por defecto
> (`BAT_V_*_DEF` en `sensor/battery.h`). Si un valor te sirve para siempre, se cambia ahí. El objetivo
> de la consola es **probar** el valor bueno sobre el robot, no sustituir al código.

---

## 3. Ejemplos

Ajustar el umbral crítico del pack a 6,0 V mientras corre la prueba 12:

```
BAT CRIT_PACK 6000
>BAT_AVISO_PACK:7.000
>BAT_CRIT_PACK:6.000
>BAT_AVISO_CELDA:3.450
>BAT_DESEQ:0.150
```

Preguntar los valores que hay ahora:

```
BAT LIST
>BAT_AVISO_PACK:7.000
...
```

Mal escrito o fuera de rango:

```
BAT CRIT_PACK 9
>CMD_ERR:valor rechazado (rango o incoherente)
```

> El `9` de arriba son **9 mV** (sin punto = milivoltios): como el crítico tiene que quedar por debajo
> del de aviso, se rechaza. Para 9 V habría que escribir `9.0`.

---

## 4. Dónde vive y cómo se añade una orden

| Capa | Fichero | Responsabilidad |
|:-----|:--------|:----------------|
| **HAL** | `hal/uart.c/h` | Recibe bytes por interrupción: `UART_RxStart()` arma **solo RXNEIE** y `UART_RxIRQHandler()` (llamada desde `USART3_IRQHandler` en `src/main.c`) guarda el byte en el anillo |
| **APP** | `app/console.c/h` | Monta la línea, la trocea, valida el **texto** y ejecuta la orden |
| **SENSOR** | `sensor/battery.c/h` | `BATTERY_SetUmbral()` (valida el **valor**) y `BATTERY_GetUmbral()` |
| **APP** | `app/application.c/h` | `APP_PID_SetGanancia()` / `APP_PID_SetVelocidad()`: la APP es la dueña del ajuste del lazo |
| **CONTROL** | `control/controller.c/h` | `CTRL_SetGanancias()`: la APP empuja el ajuste, el lazo lo aplica |

> [!WARNING] La recepción NO usa el modo del HAL, y no es un capricho (30-sep)
> La primera versión usaba `HAL_UART_Receive_IT()` y relanzaba desde `HAL_UART_ErrorCallback()`. Eso
> **colgó el robot**: el menú se quedaba pintado en la pantalla y no respondía a ningún botón.
>
> Causa (verificada en `stm32f1xx_hal_uart.c` de `framework-stm32cubef1`, línea 3594):
> `UART_Receive_IT()` empieza con `if (huart->RxState == HAL_UART_STATE_BUSY_RX) { ...leer DR... }`
> y, si no se cumple, **sale sin leer el registro de datos**. Con `RXNEIE` activo, el flag `RXNE` se
> queda puesto ⇒ la interrupción se dispara, sale, vuelve a entrar… **sin fin**: el bucle principal
> nunca avanza (por eso el menú se congela) y **tampoco sale telemetría**.
>
> La ISR propia lee **siempre** el registro de datos (`SR` y luego `DR`), que es lo que limpia
> `RXNE`, `ORE`, `NE` y `FE`. Con eso no hay ningún estado que se pueda desincronizar con los flags
> del periférico y el bucle se repite **es imposible**. De regalo, la interrupción de error (`EIE`)
> no se habilita: el overrun se limpia solo al leer el dato.

Añadir una orden son tres pasos:

1. Si es un valor que vive en una capa, dale su **setter con validación** en esa capa (como
   `BATTERY_SetUmbral()`): la consola no debe decidir si un valor es bueno.
2. Añade el comando en `console_ejecutar()` y su nombre a la ayuda (`CONSOLE_AYUDA`).
3. Documenta la orden en este fichero y en la prueba de banco que la use.

> El nombre de cada umbral está **al lado de su índice** (`BATTERY_UMBRAL_NOMBRES` en `sensor/battery.c`),
> con un `typedef` de tamaño negativo que comprueba que cuadran: si se añade un umbral y se olvida el
> nombre, **no compila**.

---

## 5. Relacionado

- Umbrales y montaje de los divisores: [hardware.md §4.6](./hardware.md#46-alimentación-y-rieles)
- Prueba 12 (batería) y prueba 13 (soldadura del ADC): [pruebas-bringup.md](./pruebas-bringup.md)
- Máquina de estados y FAULT: [maquina-estados-menu.md](./maquina-estados-menu.md)
