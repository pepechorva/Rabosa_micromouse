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
| `PID KP\|KI\|KD <valor>` | Cambia una ganancia **con el lazo corriendo** |
| `PID VEL <mm/s>` | Cambia la consigna de velocidad de la prueba 18 |
| `MOV OK\|UP\|DOWN [n]` | Lanza `n` movimientos en la **prueba 20** (primitivas de `app/motion.c`): celda de 180 mm o giros de 90° |
| `TRACE` | Vuelca la **traza en RAM** del lazo por telemetría (`>TR_SET`, `>TR_IZQ`, `>TR_DER`, `>TR_FIN`) |
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

Los valores por defecto son los **medidos en el robot el 2026-10-01** (ver
[control-pid.md](./control-pid.md) §4): dan un **0,5 % de error de régimen**.

Funciona **de las dos maneras**: se puede dejar puesta la ganancia antes de entrar en la prueba (se
aplica al arrancar) o cambiarla **mientras corre** (se aplica en el paso siguiente del lazo, a
100 Hz). Sin esto, cada tanteo de sintonía son un recompilar, un flashear y un reinicio que borra la
observación en curso.

> ⚠️ **`PID KP 0` solo deja la P si el integrador se resetea.** El integrador guarda un **término de
> salida en %**, no un acumulador del error: con `KI = 0` conserva su último valor y **ese valor sigue
> sumándose a la salida**, así que las ruedas siguen girando. `PID_SetGanancias()` llama a
> `PID_Reset()`, y aun así **conviene comprobarlo**: si al poner las tres ganancias a cero el duty no
> se va a 0, algo se ha quedado cargado (pasó el 2026-10-01).

Con cada cambio se publican `>PID_KP`, `>PID_KI`, `>PID_KD` y `>PID_VEL` —las mismas etiquetas que
manda la prueba al arrancar— para que el registro de Teleplot diga con qué ganancias se hizo cada
tramo.

> ⚠️ **No se guarda en Flash**: al reiniciar vuelven los valores de `PID_TEST_*_DEF`
> (`app/application.c`).

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
