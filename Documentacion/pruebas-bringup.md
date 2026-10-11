# 🧪 Pruebas de bring-up — Rabosa-MVR

Protocolo, catálogo y registro de las pruebas de banco del firmware.
`firmware/app/application.c` **es un banco de pruebas**: las rutinas se lanzan desde el **menú de la
pantalla OLED** (sin recompilar) y se sale con el botón **MENU**.

---

## 1. Protocolo

| Regla | Motivo |
|---|---|
| **Un periférico por prueba** | Si falla, el culpable es único |
| **Toda prueba es observable** | O imprime por telemetría, o cambia un LED |
| **Toda prueba es repetible** | Se ejecuta 10 veces antes de darla por buena |
| **Aislar antes de culpar al módulo** | Loopback del bus, continuidad, `WHO_AM_I` |
| **Nada entra en el lazo de control sin prueba** | Evita depurar dos cosas a la vez |

> [!NOTE] El banco de pruebas es **temporal** (decisión del usuario, 28-sep)
> Ocupa una parte importante del binario (Flash al 61 % con las 19 pruebas y el menú). Cuando el robot
> ya tenga sus datos básicos y validada la conexión de cada periférico, las pruebas se recortarán para
> liberar Flash: **no se van a optimizar antes**. Si hiciera falta espacio, primero se quitan pruebas,
> no se cambia el código de las capas.

> [!WARNING] Banco con fuente de alimentación: la **celda 1 no se usa** (30-sep)
> Mientras las pruebas se hacen con una **fuente de alimentación** en vez de la LiPo 2S, el canal de la
> celda 1 (PB1) no tiene de dónde medir: lee ≈0 V. El firmware lo detecta
> (`BATTERY_Celda1Presente()`, capa SENSOR) y **no lo toma por una celda agotada**: clasifica la
> batería **solo con el pack**, no avisa de desequilibrio y la prueba 13 marca ese canal como
> `NO USADA` en lugar de `SIN DIVISOR`. Sin esto, el 0 de la celda 1 daba `BATTERY_CRITICA` y el robot
> se quedaba clavado en **FAULT** sin poder ni entrar al menú.
>
> Lo mismo vale para el **pack**: si la placa se alimenta por **USB o por el ST-Link** (sin batería) y
> PB0 lee ≈0 V, `BATTERY_PackPresente()` devuelve 0, se publica `>PACK_PRESENTE:0` y el módulo **no
> clasifica nada**: un canal sin medida no bloquea el robot. El FAULT solo se dispara cuando hay
> tensión de verdad por debajo del umbral crítico del pack, y **se sale con `OK`** (el robot queda
> frenado y avisando, pero se puede seguir trabajando). Lo único que dispara el FAULT es el pack: la
> celda solo **monitoriza** (puede subir el estado a `BATTERY_AVISO` y cambiar el rótulo de la
> pantalla, pero nunca a CRÍTICA).
>
> Los cuatro umbrales se **cambian en caliente** por el puerto serie, sin recompilar:
> `BAT AVISO_PACK 6.6` · `BAT CRIT_PACK 6.0` · `BAT AVISO_CELDA 3.30` · `BAT DESEQ 0.40`
> ([comandos-serie.md](./comandos-serie.md)). En la prueba 12 se atienden las órdenes dentro del bucle,
> así que se puede ajustar el umbral **viendo la tensión** en Teleplot.

### Cómo arrancar una prueba

1. Compilar y flashear (`platformio run -t upload`).
2. Encender el robot: arranca en el **menú** de la pantalla.
3. Navegar con UP/DOWN y entrar con OK: en **TEST** están las pruebas y en **CAL** los asistentes de
   calibración.
4. **MENU sale de la prueba** en cualquier momento (va por interrupción, por eso responde incluso
   dentro del bucle de una prueba).
5. Abrir el monitor serie a 115200 y volcar la salida en Teleplot.
6. Para **cambiar valores en caliente** (umbrales de batería) se escriben órdenes por el mismo puerto:
   `BAT CRIT_PACK 6000`, `BAT LIST`, `HELP` ([comandos-serie.md](./comandos-serie.md)).

> [!IMPORTANT] Toda la telemetría es **numérica** (decisión del 30-sep)
> El formato es `>ETIQUETA:numero`, sin textos: `>BOOT:1`, `>IMU:1`, `>APP_EST:1`. Los estados y los
> veredictos van como **código entero** (aquí tienes las tablas) y los mensajes de ayuda no se mandan
> por telemetría: están en este documento. Así Teleplot dibuja **todas** las etiquetas en vez de
> descartar las que no son números.
>
> Con la traza de las FSM (`>APP_EST`, `>MENU_EST`) se reconstruye la sesión entera sin mirar la
> pantalla: [maquina-estados-menu.md §10](./maquina-estados-menu.md#10-máquinas-de-estados-implementadas-2026-09-30).

### Marcas de arranque

`APP_InitAll()` emite: `>BOOT:init`, `>IMU:init...`, `>IMU:init done`.

> Si aparece `IMU:init...` pero nunca `IMU:init done`, el firmware se ha colgado
> dentro de `IMU_Init()` (típicamente por la IMU o el bus SPI).

> [!WARNING] `while(1){;}` en `main.c` = freno de banco, pero el banco de pruebas no existe
> Se pone para que los motores **no giren mientras se toca el hardware**. El enlazador va con
> `--gc-sections`: si `APP_Run()` queda inalcanzable, **todas** las `App_Test_*` y sus ayudantes se
> descartan del binario. Síntomas: la prueba no imprime nada, el LED no parpadea, `APP_InitAll()` sí
> funciona (salen `>BOOT:init` y `>IMU:init done`) y el `.bin` **no cambia de tamaño** aunque añadas
> código nuevo. Comprobación: `strings .pio/build/*/firmware.elf | grep -c BTN` → 0 significa "no
> está enlazado". **Quita el bucle para probar** (más adelante, un define para elegir el modo).

---

## 2. Catálogo de pruebas

| # | Rutina | Qué valida | Salida esperada | Estado |
|:-:|:-------|:-----------|:----------------|:------:|
| 1 | `App_Test_ADC()` | ADC1 + sensores IR + telemetría | 4 trazas `>IR0..IR3` (PA2–PA5) entre 0 y 4095, sensibles al acercar la mano (**PB0 sale de aquí: es la batería → prueba 12**) | ✅ Validada en su versión de 5 canales · **reescrita a 4 canales (30-sep)**, pendiente de revalidar |
| 2 | `App_Test_MotoresEncoders()` | PWM + driver + encoders en bucle cerrado | Avanza exactamente 50 pulsos al 25 % y frena; `>1A/1B/2A/2B` coherentes | ✅ Validada |
| 3 | ~~`App_Test_ADC_PWM()`~~ | ADC → PWM directo | — | ➖ **RETIRADA** (2026-09-28): el robot no lleva potenciómetros |
| 4 | `App_Test_Encoders()` | Encoders sin motores | Los pulsos suben al girar la rueda a mano; se resetean tras 5 s sin actividad | ✅ Validada |
| 5 | `App_Test_IMU()` | MPU6500 por SPI2 | `>AX/AY/AZ` en g y `>GX/GY/GZ` en °/s, estables y coherentes al mover la placa | ✅ Validada |
| 6 | `App_Test_SPI2_Loopback()` | TX/RX del MCU (diagnóstico) | Con puente PB15→PB14, RX = `0xA5` | ✅ Validada |
| 7 | `App_Test_PPR()` y `App_Test_UmCuenta()` | PPR del encoder, reducción y µm/cuenta | `PPR A MANO`: **2707 pulsos en 10 vueltas** (repetible) · µm/cuenta empujando el robot 1 000 mm: **270,7** | ✅ PPR y µm/cuenta (coherentes con Ø24 al 0,5 %) ⏳ **repetir con las ruedas nuevas** |
| 8 | `App_Test_Botones()` | Botones MENU/UP/DOWN/OK + antirrebote | Una línea `>BTN:n` por pulsación (**1=MENU · 2=UP · 4=DOWN · 8=OK**) y `>BTN_MASK`/`>BTN_*` con el estado mantenido | ❌ Pendiente |
| 9 | `App_Test_PWM_Rampa()` | PWM de un motor (o los dos) en lazo abierto, con rampas **medidas en ticks de encoder**, frenada en seco y sentido contrario | Rampas de 1500 ticks (≈3 s) de 0→100 % y de −100→0 %, con freno y pausa de 2 s entre ellas, en bucle; `>PWM_DUTY` sigue la rampa y `>PWM_TICKS` marca la distancia | ⚠️ Izquierda ✅ · derecha pendiente |
| 10 | `App_Test_BotonesOLED()` | Botones + **pantalla OLED SSD1306 128×32** por I2C1 | Una línea `>BTN:n` + `>BTN_PIN:PC14` por pulsación, la pantalla muestra nombre y pin, y al arrancar el estado del bus (`>I2C_SCL`, `>I2C_SDA`, `>I2C_N`, `>I2C_FOUND`) | ✅ Validada (2026-09-20) |
| 11 | `App_Test_I2C_Scan()` | **Bus I2C1**: niveles en reposo y escaneo de direcciones | `>I2C_SCL:1` `>I2C_SDA:1` (bus subido) y `>I2C_FOUND:60` con la pantalla presente (60 = 0x3C) | ✅ Validada (2026-09-21) |
| 12 | `App_Test_Bateria()` | **Batería 2S**: pack (PB0, **22 kΩ/10 kΩ**), celda 1 (PB1, **3k9/10 kΩ** — cada canal con su $k$) y celda 2 por resta, + **aviso** por LED y pantalla | `>VBAT`, `>VCELL1`, `>VCELL2` coherentes con el multímetro (±3 %) y `>VBAT_ESTADO`; la OLED muestra **el estado y las tres tensiones** (`BATERIA OK` / `TOTAL` / `CEL1` / `CEL2`); incluye el **calibrado del divisor del pack** contra el multímetro (`BAT CAL <mV>`) | ✅ Pack (7,77 V) y las dos celdas (3,80 / 3,97 V) |
| 13 | `App_Test_ADC_Soldadura()` | **Chequeo de la soldadura del ADC**: los 4 receptores IR (PA2–PA5) y los 2 divisores de batería (PB0/PB1) | Por canal `>ADC_IRn` y `>ADC_DIFn` (los 4 emisores encendidos menos apagados) + veredicto `>ADC_ESTn` (`OK/MUDO/SATURADO/FLOJO/NO USADA`); `>ADC_B0`/`>ADC_B1` con `>ADC_EST_B0`/`>ADC_EST_B1`. Se repite a 2 Hz | ✅ 4 receptores y PB0 `OK` (falta luz controlada) |
| 14 | `App_Test_EmisoresIR()` | Los 4 emisores IR por GPIO (**PB3/PB4/PB5/PB8**) con el JTAG desactivado, conmutados por **BC337** | `>IR_LED:1..4` uno a uno, **`5` = los cuatro** y `0` = apagados (1 s por paso); cada LED se ve con la **cámara del móvil** y caen ≈1,65 V en sus **22 Ω** (≈75 mA; los cuatro ≈300 mA) | ❌ Pendiente (implementada 2026-09-30) |
| 15 | `App_Test_IR_Receptores()` | **Pareja emisor↔receptor** (matriz 4×4) y elección de $R_L$: fondo de luz ambiente frente a reflejo | Enciende cada emisor en su turno (`>IR0`..`>IR3`, 1 = encendido) y publica `>R0`..`>R3` con las **cuentas crudas** del ADC de los cuatro receptores; cada emisor dura 2 s | ✅ Pareja medida: **R0←IR3 · R1←IR1 · R2←IR2 · R3←IR0** (falta el 10/10) |
| 16 | `App_Test_IR_Modulado()` | **Lectura diferencial ON−OFF por software**: cancela la luz ambiente | `>R0_OFF`/`>R0_ON`/`>R0_DIF` (encendido − apagado) y `>R0_VAR` (dispersión de la diferencia, en %): el `DIF` debe aguantar ±10 % al encender y apagar la luz de la sala. 8 pares por canal, los cuatro canales cada 250 ms, con la pareja de `ir_pareja_emisor[]` | ❌ Pendiente (implementada 2026-09-30) |
| 17 | `App_Test_CalibracionIR()` | **Curva distancia↔ADC** (3/5/8/10/15 cm) y umbral de detección de pared | Guiada por OLED: `UP`/`DOWN` eligen canal, `OK` congela 64 muestras → `>IR_CAL_CANAL`, `>IR_CAL_CM` y `>R0_CAL`; al completar los 4 canales publica la tabla y estima en vivo `>R0_CM`. Enciende el emisor del canal en curso (`ir_cal_emisor()`). ⚠️ **Hasta el 2-oct medía con los emisores apagados** (fondo de sala) ⇒ las curvas hechas antes no valen | ❌ Pendiente (implementada 2026-09-30) |
| 18 | `App_Test_PID()` | **Lazo de velocidad por rueda** (capa CONTROL): rampa de consigna + PID → duty | `>PID_SET` frente a `>VEL_IZQ`/`>VEL_DER` y `>DUTY_IZQ`/`>DUTY_DER`; escalón **sin sobreimpulso > 20 %** y las dos ruedas a la misma consigna. Las ganancias se ajustan **en caliente** con la orden `PID` | ⚠️ **Lógica validada en simulación (PC)** · pendiente de sintonizar en hardware |
| 19 | `App_Test_Odometria()` | **Odometría diferencial** (capa FILTER): incrementos de arco de cada rueda → pose $(x, y, \theta)$ | `>ODO_X`, `>ODO_Y` (mm), `>ODO_TH` (°), `>ODO_D` (camino), `>ODO_UM_L/R` (µm de cada rueda) y `>ODO_TROCHA_MM`; empujando el robot recto, X sube y θ no se mueve. **OK** pone el origen a cero | ✅ **Ejecutada el 2026-10-01**: recta de 1 m con −0,3 % de error y giro de 90° corregido con $L$ = 77,6 mm (era 83,6) |
| 20 | `App_Test_Movimiento()` | **Primitivas de movimiento** (`app/motion.c`, capa APP): avanzar una distancia y girar un ángulo con lazo cerrado sobre la odometría; el rumbo al avanzar lo corrige el **giroscopio** | `>MOV_D` (camino), `>MOV_X`/`>MOV_Y` (posición), `>MOV_ANG`, `>MOV_GYR` (ángulo del giroscopio) con `>MOV_GYROK`, y los errores `>MOV_E_D`/`>MOV_E_ANG`; **OK** avanza 180 mm, **UP/DOWN** giran ±90° y por consola hay `MOV OK\|UP\|DOWN\|BACK [n]`. Criterios: celda < 5 mm y giro < 2° | ✅ **Medida el 2026-10-01**: celdas de **177,7–181,1 mm** (error ≤ 2,3 mm ✓), giros de **83,3° / 90,7°** (⚠️ criterio < 2°) y la deriva de rumbo al avanzar en **+0,2°** por celda |
| 21 | `App_Test_Paredes()` | **Detección de paredes** (`app/walls.c`): medidas de los receptores → «hay pared / no hay» con umbral ajustable | `>WALL_n`, `>WALL_n_UMB` y `>WALL_n_HAY`; la medida es `encendido − apagado`, así que **no depende de la luz de la sala**. **OK** cambia de pared y **UP/DOWN** mueven su umbral | ✅ **Medida en el laberinto el 2026-10-02**: frontal **149** con pared y **0** sin ella ⇒ umbral **75**. Los laterales dan 105 y −3 **con los lados al aire**: no ven paredes laterales |
| 22 | `App_Test_WallFollower()` | **Algoritmo de la mano** (`app/wall_follower.c`, capa APP): decisión por celda con las paredes | `>WF_CELDAS`, `>WF_DEC`, `>WF_FRENTE`/`>WF_IZQ`/`>WF_DER`, `>WF_X`/`>WF_Y`/`>WF_ANG`, **`>WF_GYR`**/`>WF_GYROK` y `>WF_ATASCO`; **OK** arranca o para · **UP/DOWN** eligen la mano · **MENU** sale. Se para sola por MENU, por tope de celdas o por **atasco** | ✅ **Corrida el 2026-10-02** en el laberinto: 3 celdas rectas seguidas (179, 177 y 174 mm) con el corrector de rumbo por giroscopio (1,5 → 3,4 → 11,4° frente a 30-41° del corrector viejo) y giro en el sitio al chocar |
| 23 | `App_Test_ESP8266()` | **Módulo ESP8266 por AT** (`hal/esp8266.c`, capa HAL): `AT`, `AT+CWMODE=1` y `AT+CWLAP` | ⚠️ **Sin telemetría a propósito** (el módulo ocupa la USART3): el resultado sale **solo en la pantalla**, como un veredicto **`CONECTADO` / `SIN ESP`**. **OK** lanza la secuencia · **MENU** sale | ⚠️ Escrita el 2026-10-01, pendiente de probar con el módulo cableado |
| 24 | `App_Test_Puente()` | **Puente WiFi** (`hal/esp8266.c` + `app/console.c`): la **USART3 entera por TCP**, la telemetría hacia fuera y las órdenes hacia dentro | ⚠️ Mientras está arriba **no hay telemetría por el cable** (el módulo ocupa la USART3): sale por el socket. Latido `>PUENTE_T` a **1 Hz** para saber que el enlace vive, y la pantalla pone `PUENTE` / `SIN CABLE` / `MENU:SALIR` | ✅ **Ejecutada el 2026-10-02**: telemetría y órdenes en los **dos sentidos** (umbrales de `BAT LIST` y `>VBAT:7.394` leídos por WiFi) con el robot en **192.168.2.198** |

> ⏹️ **Recorte del 2026-10-02 — once pruebas retiradas del menú.** Para bajar la Flash del **88,3 %**
> al **74,2 %** se desconectaron las pruebas **ya validadas y sin uso previsto**: **2, 3, 4, 5, 6,
> 7, 9, 10, 11, 12, 13, 14, 15 y 19**. Sus funciones **siguen en `app/application.c`**, pero su
> **fila del menú está comentada**, así que **ya no se pueden lanzar** y el enlazador las deja fuera
> del binario. Aquí se conservan sus filas como historial. **Siguen en pie la 1, 16, 17, 18, 20, 21,
> 22, 23 y 24.**
>
> **Para recuperar una prueba**: descomentar su fila en el menú que le toque (está marcada con el
> número de prueba). **No hay que tocar la función.**
>
> **Por qué quitar la fila basta**: se compila con `-ffunction-sections` y el enlazador lleva
> `--gc-sections`, así que **una función que nadie referencia se queda fuera del binario**. Envolver
> además el cuerpo en `#if 0` no ahorraría ni un byte más, y dejaría sin usar los `static` que solo
> esas funciones llaman.
>
> ⚠️ **Y la prueba 8 (botones) además nunca fue ejecutable**: `App_Test_Botones()` estaba
> declarada y definida, pero **no figuraba en ningún menú**, así que el enlazador la tiraba desde el
> principio. Figuraba como *pendiente* cuando en realidad **no se podía lanzar**. (La que sí está en
> el menú es `App_Test_BotonesOLED`, la 10.)

---

## 3. Registro de resultados

> Rellena una fila por ejecución relevante. `10/10` significa que la prueba se ejecutó
> diez veces seguidas con resultado correcto.

| # | Prueba | Fecha | Repeticiones | Resultado | Observaciones |
|:-:|:-------|:------|:------------:|:---------:|:--------------|
| 1 | ADC + IR | ≤ 2026-09-10 ⚠️ | — | ✅ | Validada antes del 10-sep; falta anotar repeticiones y valores de referencia |
| 2 | Motores + encoders | 2026-07-17 | — | ✅ | Primeras pruebas de motores, rampa 0→100 % y conteo de pulsos |
| 3 | ADC → PWM | — | — | ➖ | **Retirada** (2026-09-28): era un banco de la primera fase (Blue Pill sola, con potenciómetros). Función borrada de `application.c` |
| 4 | Encoders | 2026-07-17 | — | ✅ | Pulsos `1A/1B/2A/2B` por teleplot |
| 5 | IMU por SPI2 | 2026-09-04 | — | ✅ | Incluye la identificación del cruce `SDA/ADO` como causa de fallo |
| 6 | Loopback SPI2 | ≤ 2026-09-10 ⚠️ | — | ✅ | Usada como diagnóstico del bus |
| 7 | Calibración de encoders (PPR / µm por cuenta) | 2026-09-30 | 3 | ⏳ | `PPR A MANO`: **2707 pulsos en 10 vueltas** (repetible) y **2993 en 11 vueltas** ⇒ ratio 1,106 ≈ 11/10, así que el contador es **lineal y no pierde pulsos** ✅. Pero salen **≈271 cuentas por vuelta de rueda** frente a las 280 nominales (−3,1 %): el `PPR` implícito sale 6,77/6,80, que **no es entero** (o la reducción no es exactamente 1:10, o el PPR no es 7). Falta `UM MOTORES` + regla para sacar el µm/cuenta real, que es lo que usa la odometría |
| 8 | Botones MENU/UP/DOWN/OK | — | — | ➖ | Sustituida por la prueba 10 (botones + pantalla) |
| 9 | Rampa de PWM por ticks de encoder (izq. PA8/PA9 · dcha. PA10/PA11) | 2026-09-18 | 1 | ⚠️ | v1 (temporal) validada con la **rueda izquierda**; v2 por ticks pendiente |
| 10 | Botones → OLED por I2C1 | 2026-09-20 | — | ✅ | Funcionando; se giró la pantalla 180° (`OLED_ROTACION_180`) |
| 11 | Diagnóstico del bus I2C1 (niveles + escaneo) | 2026-09-21 | — | ✅ | `SCL=1`, `SDA=1`, `I2C_FOUND:60` → el módulo lleva pull-ups y responde en 0x3C |
| 12 | Batería 2S (pack + celdas) | 2026-09-30 | 3 | ⏳ | **Pack calibrado y validado**: con la fuente a ~7,39 V, `>VBAT` cae dentro del ±0,3 % (oscila 7,37–7,42 V) gracias a `BAT_K_DIVISOR_PACK_DEF` = **0,3015** (las resistencias de 22 kΩ/10 kΩ apartan un 3,5 % del teórico: tolerancia del 5 %). **Celda 1 validada** con divisor **3k9/10 kΩ**: `>VCELL1` = **3,80 V** frente a **3,79 V** del multímetro. Por el camino se destapó que el firmware usaba una sola $k$ para los dos canales (la celda 2 daba −1,29 V) — ahora hay una $k$ por canal. ⏳ Falta la pasada con la **batería 2S real** para ver las dos celdas y el desequilibrio |
| 12b | Batería 2S — **las dos celdas** y los umbrales | 2026-09-30 | 1 | ✅ | **Pack 7,77 V · celda 1 3,80 V · celda 2 3,97 V** (por resta). La celda 1 cuadra con el multímetro (3,80 frente a 3,79). Por el camino: el firmware usaba **una sola $k$** para los dos canales ⇒ la celda 2 daba −1,29 V (arreglado), y el **desequilibrio** de 0,17 V encendía el aviso estando al filo del umbral de 0,15 V (parpadeo OK/AVISO). **Umbrales nuevos**: aviso a **3,30 V/celda** (6,60 V de pack), crítico a **3,00 V/celda** (6,00 V) y desequilibrio a **0,40 V** | La pantalla ahora enseña el estado y las tres tensiones, así que se ve el pack y las dos celdas a la vez |
| 13 | Chequeo de soldadura del ADC (IR + batería) | 2026-09-30 | 1 | ✅ | Los 4 receptores `OK` y PB0 `OK` (los emisores iluminan y los receptores lo ven: `DIF` > 0); PB1 `N/USA`, esperado con fuente (la celda 1 no tiene de dónde medir). Medida **con sol directo** (el fondo puede estar alto: repetir con luz controlada). Falta el 10/10 y anotar `>ADC_Rn`/`>ADC_Rn_DIF` |
| 14 | Emisores IR (uno a uno y los cuatro) | — | — | ❌ | Implementada el 30-sep; se ve con la cámara del móvil y se mide la caída en las **22 Ω** |
| 15 | Receptores IR: matriz 4×4 (ambiente vs reflejo) | 2026-09-30 | 1 | ✅ | Pareja medida: **R0←IR3 · R1←IR1 · R2←IR2 · R3←IR0** (anotada en `ir_pareja_emisor[]`) ⇒ los receptores quedan **situados**: R0 frontal der, R1 lateral der, R2 lateral izq, R3 frontal izq. Falta el 10/10 y anotar los valores de fondo |
| 16 | IR modulado (ON−OFF por software) | — | — | ❌ | Implementada el 30-sep; es la base del futuro `filter/adc_filter.c` |
| 17 | Curva distancia↔ADC de los IR | — | — | ❌ | Implementada el 30-sep; 5 puntos por canal; insumo del wall follower |
| 18 | PID de velocidad por rueda | 2026-09-28 | — | ⚠️ | **Verificado en el PC** (simulación): sobreimpulso −0.2 % y error 0.2 % con planta de primer orden; falta la sintonía sobre el robot y el 10/10 |
| 19 | Odometría diferencial (pose x, y, θ) | 2026-10-01 | 1 | ⚠️ | **Medida empujando a mano**, con las ruedas nuevas: recta de 1 m ⇒ `>ODO_D` = **997 y 995 mm** (−0,3 % y −0,5 %: el factor de **287,0 µm/cuenta** es bueno y la mesa **no** inflaba la medida) · giro de 90° a mano ⇒ `>ODO_TH` = **+83°** (izquierda, X 2 mm) y **−84°** (derecha, X 7 mm): un 7 % corto y con el **mismo sesgo en los dos sentidos** ⇒ el culpable es la trocha, no un encoder · **trocha efectiva medida: 77,6 mm** (`ODOM_TROCHA_MM_DEF`, era 83,6) y con ella el giro sale **clavado: +90° y −90°**. La pantalla enseña `D`/`ANG`/`CIE` y **UP** congela la lectura, así que se puede usar **sin Teleplot**. ⏳ Falta el cuadrado de 4×300 mm (§15) y la pasada 10/10 |

> **Cómo leer la columna de estado**: ✅ validada en hardware · ⚠️ validada **con reservas**
> (falta la fecha exacta, la repetibilidad, o quedó a medias) · ➖ **retirada o sustituida** por otra
> prueba · ❌ pendiente de ejecutar.
>
> **Puerta de calidad**: ninguna prueba se da por cerrada hasta cumplir su criterio **10/10**
> veces seguidas y con la fila de §3 rellena (fecha y repeticiones).

> **Fechas reconstruidas** del historial de sesiones del proyecto (no había registro a mano).
> Más detalle en [historial-pruebas.md](./historial-pruebas.md) y lo que queda en
> [pruebas-pendientes.md](./pruebas-pendientes.md).

> **Pendiente de completar:** fecha y número de repeticiones de cada prueba (la meta es 10/10).
> A medida que se repitan, sustituye los `—` por los datos reales.

---

## 4. Telemetría

Implementada en `firmware/app/telemetry.c` y enviada por **USART3** (PB10/PB11, 115200 baudios).
Formato **Teleplot**: una línea por variable, `>` + etiqueta + `:` + valor.

```
>IR0:1832
>AX:-0.02
>GY:0.3
>1A:37
```

Monitor serie: `platformio device monitor -b 115200`.
Pegar la salida en Teleplot para ver las trazas en el tiempo.

---

## 5. Añadir una prueba nueva

1. Declarar `void App_Test_XXX(void);` en `app/application.h`.
2. Implementar la rutina en `app/application.c`, que **sale con MENU** (`while (!BUTTONS_SalidaSolicitada())`).
3. Añadir su fila a `menu_test[]` en `app/application.c`:
   `{ "ETIQUETA", MENU_TIPO_LANZAR, App_Test_XXX, 0U },`.
4. Añadir la fila a la tabla de la sección 2 y al registro de la sección 3.
5. Documentar el hardware asociado en [hardware.md](./hardware.md).

Esqueleto (estilo de código: Doxygen al principio, sin comentarios dentro):

```c
/**
 * @brief <qué valida, en una frase> (prueba N).
 *
 * @note Se sale con el botón MENU, como el resto del banco.
 */
void App_Test_XXX(void)
{
    XXX_Init();

    while (!BUTTONS_SalidaSolicitada()) {
        TELEM_SendFloat("XXX", XXX_Read());
        HAL_Delay(10);
    }
}
```

---

## 6. Calibraciones

| Calibración | Procedimiento | Resultado se guarda en |
|:------------|:--------------|:-----------------------|
| **PPR del encoder** | Girar la rueda a mano **10 vueltas** exactas y dividir los pulsos entre 40 (decodificación ×4) | `encoder.h` |
| **Trocha efectiva $L$** | **✅ Medida el 2026-10-01: 77,6 mm.** Girar el robot **5 vueltas exactas** sobre sí mismo y despejar $L = \frac{\Delta s_d - \Delta s_i}{5 \cdot 2\pi}$ con la telemetría de la prueba 19 (§15) | `ODOM_TROCHA_MM_DEF` en `filter/odometry.h` |
| **Radio de rodadura $r$** | No hace falta calcularlo aparte: los µm por cuenta (prueba 7) ya lo incluyen | — |
| `micrometers_per_count` | Marcar 1 m, avanzar en recta, `1e6 / pulsos_totales` | Llamada a `set_micrometers_per_count()` |
| **Pareja emisor↔receptor y $R_L$** | Prueba 15: encender un emisor y ver qué canal cambia; ajustar $R_L$ hasta que el fondo con la luz de la sala no sature | `hardware.md` §4.4 |
| **Curva distancia IR** | Prueba 17: medir el ADC a 3, 5, 8, 10 y 15 cm de una pared blanca → tabla + interpolación | `filter/adc_filter.c` |
| Offset del giroscopio | Robot estático: lo promedia solo `MOTION_Init()` (50 muestras) al entrar en las pruebas 20 y 21 | `app/motion.c` → `motion_gyro_calibrar()` |
| **Divisores de batería** | Prueba 12 + multímetro: comparar `>VBAT`/`>VCELL1` con el multímetro y ajustar con **`BAT CAL <mV>`** si hay diferencia | `sensor/battery.c` |

---

## 7. Procedimiento: calibración del encoder paso a paso

La prueba 7 tiene **dos modos**, y cada uno es una entrada del menú (**TEST → MOTORES**), así que
**no hay que recompilar** para cambiar de uno a otro:

| Entrada del menú | Función | Qué hace | Pantalla |
|:-----------------|:--------|:---------|:---------|
| **`PPR A MANO`** | `App_Test_PPR()` | Girar la rueda 10 vueltas a mano y contar pulsos | `PPR A MANO` · `PULSOS nnnn` · `PPR x.xx` (en vivo) |
| **`UM MOTORES`** | `App_Test_UmCuenta()` | Avanzar 2800 pulsos con los motores y medir con la regla | `UM POR CUENTA` · `P1 n P2 n` · `MIDE CON REGLA` |

### Parámetros ajustables

| Define | Valor por defecto | Significado |
|:-------|:-----------------:|:------------|
| `CAL_N_VUELTAS` | `10.0f` | Vueltas que se van a girar a mano |
| `CAL_REDUCCION` | `10.0f` | Reducción asumida de la caja de engranajes |
| `CAL_DIAMETRO_RUEDA_MM` | `21.0f` | Diámetro de la rueda con goma |
| `CAL_DUTY_MOTOR` | `35` | PWM (%) en `UM MOTORES`. **Medido (30-sep): el mínimo de arranque son 25-29 %** |
| `CAL_ARRANQUE_MS` | `2000U` | Margen para soltar el robot antes de que arranquen los motores |
| `CAL_RAMPA_DUTY_INI` | `20` | Duty (%) con el que empieza la rampa de arranque (por debajo del mínimo) |
| `CAL_RAMPA_PASOS` · `CAL_RAMPA_PASO_MS` | `10U` · `50U` | Escalones de la rampa: 10 × 50 ms = **500 ms** de 8 % a 20 % |
| `CAL_PULSOS_OBJETIVO` | `2800` | Pulsos por rueda a recorrer en `UM MOTORES` |
| `CAL_TIMEOUT_MS` | `15000U` | Parada de seguridad de `UM MOTORES` |
| `CAL_DISTANCIA_MEDIDA_MM` | `0.0f` | Se rellena tras medir con la regla |

### Paso 1 — `PPR A MANO`: obtener el PPR y verificar la reducción

1. Entrar en **TEST → MOTORES → `PPR A MANO`**.
2. Hacer una **marca en la rueda** y otra en el chasis, alineadas.
3. Girar la rueda **exactamente 10 vueltas**, despacio y siempre en el mismo sentido. La pantalla enseña
   los pulsos de **las dos ruedas** (`P1 … P2 …`) y el PPR **en vivo**, así que se puede girar y leer a
   la vez. Por telemetría salen `>CAL_P1`, `>CAL_P2` y `>CAL_PPR1`.
4. Aprovechar para **comprobar el sentido de conteo** de cada encoder: girando su rueda **hacia adelante**
   a mano, su número tiene que **subir**. Si baja, ese encoder cuenta al revés.
5. Al soltar la rueda, apuntar el número que se queda. La prueba ya calcula
   $\text{PPR} = \dfrac{\text{pulsos}}{4 \times 10 \times 10}$.

Interpretación:

| `CAL_P1` leído | PPR implícito | Lectura |
|:--------------:|:-------------:|:--------|
| ≈2800 | **7** | Coincide con la hipótesis |
| ≈4400 | 11 | Otra resolución típica de estos encoders |
| ≈5200 | 13 | Otra resolución típica |
| No sube / 0 | — | Encoder mal conectado o pines equivocados |
| ≈1400 | 3.5 | **La reducción no es 1:10** → ver abajo |

> Si el PPR resultante **no es un número entero**, la reducción asumida es incorrecta:
> lo que se mide realmente es el producto $n \cdot \text{PPR}$. Ajustar `CAL_REDUCCION`
> hasta que el PPR salga entero (la reducción es siempre un número entero pequeño).

### Paso 2 — `UM MOTORES`: obtener µm/cuenta

1. Entrar en **TEST → MOTORES → `UM MOTORES`**. La pantalla hace una **cuenta atrás de 2 s**
   (`SUELTA EL ROBOT` / `ARRANCA EN 2` / `ARRANCA EN 1`): son para soltar el robot antes de que los
   motores tiren. Durante la espera se puede **abortar con MENU**. Después el duty sube **en rampa del
   20 % al 35 % con los dos motores a la vez**, para que el robot arranque recto.
   > ⚠️ La rampa fue **escalonada** (primero M1 y luego M2) mientras se creía que el reinicio venía de un
   > pico de corriente; el 30-sep se comprobó que la fuente de 5 A no cambiaba nada y el reinicio era
   > otra cosa (`>RESET`, abajo), así que volvió a ser **simultánea**. Con las tandas el robot arrancaba
   > torcido durante medio segundo, que es peor que el problema que evitaban.
2. Marcar en el suelo la posición de partida (una línea junto a la rueda).
3. El robot avanza hasta que **cada rueda** cuenta 2800 pulsos y frena (cada rueda se detiene por
   separado, así tiende a ir recto aunque los motores no sean idénticos). Entonces la pantalla se
   queda con `P1` y `P2` y el aviso `MIDE CON REGLA` (o `TIEMPO AGOTADO` si no llegó a tiempo).
4. Medir con la regla la distancia recorrida desde la marca.
5. Poner ese valor en mm en `CAL_DISTANCIA_MEDIDA_MM` (`app/application.c`), reflashear y repetir
   para que imprima `>CAL_UM_CUENTA`.
6. Ese valor es el que se pasa a `set_micrometers_per_count()`.

> ⏱️ La pantalla se pinta **al terminar**, no durante el avance: un `OLED_Update()` son ~50 ms de bus
> I2C y con el lazo parado ese rato el frenado por encoder se pasaría de largo (a 30 % de duty, unas
> 200 cuentas ≈ 5 cm).

> 🛑 **Corte por subtensión** (`CAL_VBAT_MIN` = 6 V): durante el avance la prueba mide el pack (publica
> `>VBAT` a 10 Hz) y **frena sola** si baja del umbral, dejando `VBAT SE CAE` en pantalla y
> `>CAL_AVISO:3`. Es para no perder el diagnóstico: cuando un motor se fuerza y la tensión se cae, la
> placa se reinicia y no queda ni el rastro.

> 🔎 **`>RESET`: por qué se ha reiniciado la placa.** El firmware **no tiene watchdog ni llama a
> `NVIC_SystemReset()`**, y el `HardFault_Handler` **no reinicia** (parpadea el LED onboard a 3 Hz y se
> queda ahí). Por tanto un reinicio espontáneo **solo puede ser eléctrico**, y el STM32 deja dicho de
> dónde vino en `RCC->CSR`. Al arrancar, `APP_InitAll()` lo lee con `RESET_LeerCausa()` y publica
> `>RESET:n` junto a `>BOOT:1`:
>
> | `>RESET` | `RCC_CSR` | Qué ha pasado | Dónde mirar |
> |:--:|:--|:--|:--|
> | 0 | ninguna bandera | Ya se había leído, o el reinicio no dejó bandera | — |
> | 1 | `PORRSTF` | Se ha dado tensión **o el riel se ha caído** (brownout) | Alimentación: cables, conectores, condensadores, batería |
> | 2 | `PINRSTF` a solas | Pin **NRST**: botón de reset, ruido o un roce en el cableado | El botón de reset y todo lo que pueda tocar ese pin |
> | 3 | `SFTRSTF` | Reinicio pedido por software | `NVIC_SystemReset()` |
> | 4 | `IWDGRSTF`/`WWDGRSTF` | Watchdog | (no se usa todavía) |
> | 5 | `LPWRRSTF` | Salida de bajo consumo | (no se usa todavía) |
>
> Si están puestas **`PORRSTF` y `PINRSTF` a la vez** se informa **1** (tensión): cuando el riel cae
> despacio, NRST se dispara antes que el detector de tensión y deja las dos banderas, pero el problema
> está en la alimentación. Un botón pulsado con la alimentación estable deja `PINRSTF` a solas.
>
> **Cómo distinguirlo de un cuelgue:** mira el **LED onboard**. Fijo encendido = arrancó bien (lo enciende
> `main()`). Parpadeando a ~3 Hz = `HardFault`, y entonces no es un problema eléctrico; el LED deja de
> parpadear al pulsar NRST.
>
> **La misma causa se ve en la pantalla**: las esperas de `UM MOTORES` y `COMPARA M` ponen `RST n` en la
> última línea junto a `MENU:ABORTA`. Si la placa se ha reiniciado sola durante una prueba, al volver a
> entrar se lee de dónde vino sin depender de Teleplot.
>
> **Qué hacer según el código:**
>
> | `>RESET` | Qué tocar |
> |:--:|:--|
> | **1** (tensión) | Alimentación, **no** el firmware: cableado del pack (grosor y conectores), y **condensadores de desacoplo en VM** — un electrolítico de 100–470 µF + 100 nF cerámico junto al DRV8833. El pico de la energía inductiva del motor al bloquearse no lo absorbe nada si no están |
> | **2** (NRST) | Ruido en NRST: **100 nF de NRST a GND** junto al pin, condensadores de **100 nF en los bornes de cada motor**, y **desconectar el ST-Link** durante las pruebas (su cable hace de antena y NRST cuelga de él) |
| **3** (software) | Alguna llamada a `NVIC_SystemReset()` — hoy no hay ninguna |
>
> 🔬 **La corriente no puede ser la causa con estos números** (30-sep): forzando las ruedas hasta
> bloquearlas, la fuente marca un máximo de **243 mA** y no llega más. Con 7,4 V eso son ~1,8 W: no hay
> forma de tumbar una fuente de 5 A, y el pack tampoco se cae (si se cayera, el `>VBAT` a 10 Hz lo
> grabaría y la prueba habría frenado sola con `>CAL_AVISO:3`). Un transitorio que reinicia la placa
> **sin que el ADC de la batería se entere** es un pico rápido de la energía inductiva del motor
> (→ condensador en VM) o ruido en NRST (→ filtro y apantallar).
>
> ✅ **RESUELTO (30-sep): era la fuente de laboratorio.** Repitiendo `UM MOTORES` **con la batería LiPo**,
> forzando y parando un motor a mano, **la placa ya no se reinicia** (se para la prueba por el corte de
> subtensión y sigue). El transitorio no era del robot: el lazo de control de una fuente de banco es lento
> y su salida se hunde ante un escalón de corriente, cosa que una LiPo de 80C no hace.
>
> ➡️ **Regla del banco: los motores se prueban con la batería.** La fuente se queda para medidas de
> tensión estables (calibrar el divisor, prueba 12) y para pruebas sin motores.

### Paso 2 bis — µm/cuenta **sin motores** (empujando el robot)

Camino recomendado mientras el robot **no vaya recto** o el banco no sea de fiar: mide lo mismo sin
arrancar nada. Y sale **mejor que con motores**: empujando no está el deslizamiento del arranque, y la
distancia incluye el **aplastamiento del neumático** sobre la superficie donde rueda, que es lo que
cuenta de verdad.

Se hace con **cinta métrica en el suelo**, no contando vueltas de rueda: contar vueltas a mano tiene un
error de ±0,2 vueltas (±2 %), y una distancia larga medida con cinta no.

1. Marcar en el suelo **dos líneas separadas una distancia redonda**, cuanto más larga mejor (1 000 mm
   va bien), y comprobar la separación con la cinta.
2. Poner esa distancia en mm en `CAL_DISTANCIA_MEDIDA_MM` y reflashear.
3. Entrar en **TEST → MOTORES → `PPR A MANO`**.
4. **Empujar el robot a mano**, recto, desde la primera línea hasta pasar la segunda.
5. Leer `UM/C` en la pantalla (o `>CAL_UM_CUENTA`) **justo al cruzar la marca**: mientras se empuja el
   valor va bajando y se estabiliza al llegar. Repetir la pasada para comprobar que se repite.
6. Ese valor es el que se pasa a `set_micrometers_per_count()`.

> ℹ️ En esta variante el **PPR de la pantalla no vale** (se calcula suponiendo `CAL_N_VUELTAS` vueltas
> exactas): lo que manda es `UM/C`. El PPR ya se mide aparte, con la rueda marcada y contando vueltas.

> [!WARNING] El µm/cuenta **depende de la superficie**, y sobre una mesa lisa sale inflado
> Empujando a mano **sí hay algo de deslizamiento** cuando la superficie es lisa: la rueda gira más de lo
> que el robot avanza, así que se cuentan **menos** cuentas por metro y el µm/cuenta sale **mayor**.
>
> Con las ruedas nuevas, empujando 1 000 mm **sobre la mesa**, el medido fue **287,0** frente a los
> **269,3** que predice el nominal de Ø24. Ese **6,6 %** marca la horquilla en la que puede estar el
> deslizamiento (o que el Ø real sea mayor de 24 mm, porque el calibre comprime la goma).
>
> ⇒ **Re-medirlo sobre la superficie del laberinto** y con una pasada **corta** (5 vueltas de rueda,
> ≈390 mm) para que el deslizamiento acumulado sea menor. La pasada larga y la corta deberían parecerse;
> si la corta sale bastante más baja, lo que sobra en la larga es deslizamiento.
>
> ℹ️ Con esto se zanja la duda entre las 271 cuentas/vuelta medidas y las **280 nominales**: lo que vale
> es lo que avanza el robot sobre la mesa, no lo que diga la reducción del fabricante.

### Paso 2 ter — **trocha y odometría** (prueba 19), también empujando

Ya que el robot está en el suelo y se empuja a mano, es el momento de la capa FILTER.

1. Entrar en **TEST → MOTORES → `ODOMETRIA`** (es la última de la lista: hay que bajar **8 veces**
   con `DOWN`; la cabecera lleva el contador `TEST MOTORES i/11`). La pantalla enseña `D` (camino en
   mm), `ANG` (grados) y `X`; la telemetría publica `>ODO_X`, `>ODO_Y`, `>ODO_TH`, `>ODO_D`,
   `>ODO_UM_L`, `>ODO_UM_R` y la trocha (`>ODO_TROCHA_MM`).
2. **Empujar recto** ~1 m: `>ODO_UM_L` y `>ODO_UM_R` deben parecerse y `>ODO_TH` quedarse cerca de 0.
   Si θ se dispara, un encoder está contando al revés.
3. **Girar el robot 90° a mano** sobre su centro y mirar `>ODO_TH`: debe acercarse a 90.
4. El botón **OK** borra el origen sin salir, para encadenar maniobras.

> Puerta de salida: error < 5 % en la distancia y < 2° en un giro de 90°.

### Paso 3 — `MIN M1` / `MIN M2` / `MIN AMBOS`: duty mínimo de arranque

Por debajo de cierto duty el motor **solo zumba**: el PWM no vence la fricción estática y el encoder no
cuenta ni una vuelta. Estas tres entradas del menú averiguan ese mínimo subiendo el duty poco a poco.

1. `MIN M1` mueve solo el motor 1, `MIN M2` solo el 2 y `MIN AMBOS` los dos con el mismo duty (que es
   como los pide la rampa de `UM MOTORES`).
2. Tras la cuenta atrás para soltar el robot, el duty arranca en `ARR_DUTY_INI` (5 %) y sube
   `ARR_DUTY_PASO` (2 %) cada `ARR_PASO_MS` (400 ms).
3. En cada paso se mira si el **encoder** de ese motor ha contado al menos `ARR_CUENTAS_MIN` (3)
   cuentas: cuando lo hace, ese es el mínimo y queda anotado.
4. La pantalla enseña el duty que se está probando y los mínimos ya averiguados (`M1 24  M2 26`); por
   telemetría salen `>ARR_DUTY`, `>ARR_M1` y `>ARR_M2`.
5. Los motores se frenan solos al terminar (o al llegar al 100 %).

Ese mínimo es el que hay que poner en **`CAL_RAMPA_DUTY_INI`** para que la rampa de `UM MOTORES` empiece
donde el motor de verdad se mueve, y no en un 8 % que solo hace zumbar.

**Medido el 30-sep** (fuente de laboratorio, robot **apoyado en el suelo**). Seis pasadas de `MIN M2`,
ya con el motor 1 invertido:

| Prueba | Motor 1 | Motor 2 |
|:-------|:-------:|:-------:|
| `MIN M1` | **21 %** | — |
| `MIN M2` | — | **25 · 21 · 19 · 21 · 19 %** ⇒ peor caso **25 %** |
| `MIN AMBOS` | 19 % | 17 % |

Una pasada anterior (con el motor 1 todavía al revés) daba 29 % en M1 y 25 % en M2.

> 🧪 **Metodología (importante)**: la fricción estática depende del **reparto de peso**, así que una mesa
> **inclinada** o los **cables** del programador y de la alimentación tirando del robot cambian la medida
> (una pasada dio 11 %, «cuesta abajo», y hubo que descartarla). Antes de medir: robot **nivelado** y
> cables recogidos sin tensar. Y para el µm/cuenta la mesa tiene que estar nivelada, o la distancia
> saldrá sesgada.

De ahí los valores de `UM MOTORES`: rampa desde **20 %** hasta **35 %**, diez puntos por encima del peor
caso medido.

### Paso 4 — `COMPARA M`: ¿tira igual un motor que el otro?

Con los dos motores al **mismo duty** (rampa de arranque y luego `COMP_MS` = 2 s), cada rueda recorre lo
que su motor da. La diferencia entre las cuentas de una y otra es justo la que hace que el robot se vaya
de lado:

| Etiqueta | Qué es |
|:---------|:-------|
| `>COMP_P1` | Cuentas de la rueda **izquierda** |
| `>COMP_P2` | Cuentas de la rueda **derecha** |
| `>COMP_DESV` | Diferencia en % sobre la izquierda (positivo = la derecha ha ido más) |

**Antes** de esta prueba se comprueba a mano que las dos ruedas giran **suaves** con el robot apagado
(✅ hecho el 30-sep: giran suaves las dos, así que la mecánica queda descartada).

**Criterio**: con el mismo duty, la desviación debe ser **pequeña (< 10 %)**. Si un motor tira claramente
menos, o tiene un problema (motor, conexión o canal del DRV8833) o hay que compensar el duty por rueda.
Cuando entre el PID (prueba 18) el lazo lo corrige solo, así que la compensación a lazo abierto es
provisional.

**Medido el 30-sep** (robot en el suelo, con M1 ya invertido):

```
>COMP_P1:1467
>COMP_P2:-849
>COMP_DESV:-157
```

✅ **Resuelto el 30-sep**: girando cada rueda a mano hacía adelante, `P1` **subía** y `P2` **bajaba** ⇒ el
invertido era el **encoder derecho**, no el motor (por eso **no** se cruzaron los canales del DRV8833).
Arreglado **en el firmware** (`sensor/encoder.c`: se invierte `right_diff_count`), que es más cómodo que
recablear el conector del encoder.

⚠️ **Y este era el origen de todo el lío de la tarde**: con el signo al revés, el frenado por pulsos de
`UM MOTORES` (`pulsos_r >= objetivo`) **no se cumplía nunca**, así que la rueda derecha no paraba, el
robot se torcía y acababa chocando — y de ahí el reinicio que parecía de corriente. También afectaba a la
odometría (creería ir hacia atrás) y habría dejado el PID inestable.

> ⚠️ Si quien está invertido es el **encoder**, afecta a todo lo que use la velocidad o la posición de esa
> rueda: la odometría creería que va hacia atrás y el PID tendría la realimentación con el signo cambiado
> (inestable). Se arregla antes de seguir con las pruebas de movimiento.

**Verificación cruzada esperada:** con Ø21 mm, PPR 7 y reducción 1:10, 2800 pulsos
corresponden a 10 vueltas de rueda:

$$
d = 10 \times \pi \times 21\ \text{mm} \approx 660\ \text{mm}
\qquad\Rightarrow\qquad
\text{µm/cuenta} = \frac{660000}{2800} \approx 236\ \text{µm}
$$

> Si la distancia medida es muy inferior a 660 mm hay **deslizamiento** o el radio de
> rodadura real es menor que el geométrico. Si es muy superior, se ha producido un
> *overshoot* por inercia al frenar.

> [!TIP] Primera pasada en el aire
> Antes de dejarlo correr por el suelo, sujetar el robot con las ruedas **en el aire** para
> comprobar que ambas giran en el sentido correcto y que los pulsos suben.

---

## 8. Resolución de problemas

| Síntoma | Comprobar primero |
|:--------|:------------------|
| `IMU:init...` sin `init done` | Cableado SPI (`SDA`→MOSI, `ADO`→MISO) y prescaler /64 |
| `WHO_AM_I` = `0xFF` | Cruce de datos o CS mal configurado (PB12 como GPIO de salida) |
| El encoder no cuenta | Pines PA0/PA1 o PA6/PA7, modo encoder TI12 y llamada a `ENC_Init()` |
| El encoder cuenta siempre en el mismo sentido | Canal B no llega: girar a mano y mirar los pulsos A/B por separado |
| El motor no gira | `PWM_StartAll()`, canal TIM1 correcto y `DRV8833_Init()` por motor |
| Los motores giran al revés | Invertir `channel1`/`channel2` del motor o el signo en `DRV8833_Set_Speed()` |
| La telemetría no llega | `MX_USART3_UART_Init(115200)`, puerto `/dev/ttyUSB0` y baudios correctos |
| El ADC da siempre 0 o 4095 | Canal/pin correcto en `ADC_ReadPin()` y modo de entrada analógica |
| Un botón no responde | Que esté en el pin correcto del pinout y con `GPIO_PULLUP` |
| PA15 no funciona como GPIO | Es **JTDI**: necesita `SWJ_NOJTAG()` (ya aplicado para los emisores IR) |
| Un botón da pulsaciones dobles | `BUTTONS_Update()` no se llama a 1 kHz (el antirrebote de 30 muestras equivale a 30 ms reales) |
| Un botón está siempre pulsado | Falta el pull-up o el botón no está conectado a GND |
| El ST-Link no engancha tras un flasheo | Conectar **SWDIO (PA13), SWCLK (PA14), GND y 3.3 V**; si aún así no responde, usar *connect under reset* (sujetar RESET al iniciar) |
| La pantalla OLED no aparece | **Prueba 11**: `>I2C_SCL` y `>I2C_SDA` deben ser 1. Si los dos son 0, faltan **pull-ups externos** (2× 4.7 kΩ a 3.3 V): en el F1 el pull-up interno no funciona en I2C. Probar también `I2C1_CLOCK_HZ` a 50 kHz |
| `>I2C_FOUND:60` pero no se ve nada | La pantalla responde (60 = 0x3C) pero no pinta: contraste, `charge pump` o que sea un SSD1309/SSH1106 (comandos distintos) |
| Los emisores IR L1/L2 no encienden | Alguien volvió a habilitar el JTAG: `MX_GPIO_Init()` debe llamar a `__HAL_AFIO_REMAP_SWJ_NOJTAG()` |

---

## 9. Procedimiento: rampa de PWM en lazo abierto (prueba 9)

La prueba `App_Test_PWM_Rampa()` mueve **un motor o los dos** —cada par de pines se corresponde con
un canal del DRV8833— y **mide el avance con los encoders**: el reloj de la secuencia ya no son los
segundos, son los **ticks del encoder** (500 ticks = 1 s del algoritmo temporal). Sirve para validar
de golpe el pinout de PWM, la frenada en seco, el sentido de giro y el cableado de los encoders. El
motor se elige con `PWM_RAMPA_MOTOR` (ver más abajo).

**Estado (2026-09-19)**: rueda **izquierda** (`motor1`, PA8/PA9) validada ✅ en versión temporal.
Versión por ticks y rueda **derecha** (`motor2`, PA10/PA11) pendientes de ejecutar.

### Secuencia (en bucle infinito)

| Fase | Acción | Duración |
|:----:|:-------|:--------:|
| 1 | Rampa de **0 % → +100 %** (sentido A), en 100 pasos del 1 % | **1500 ticks** (≈3 s) |
| — | **Frenada en seco** (`DRV8833_Brake`) | instantánea |
| — | Parada | 2 s (en ms, ver nota) |
| 2 | Rampa de **−100 % → 0 %** (sentido contrario) | **1500 ticks** (≈3 s) |
| — | **Frenada en seco** | instantánea |
| — | Parada | 2 s (en ms) |

La rampa avanza por **distancia**: cada paso de duty (1 %) espera a que la rueda gire los ticks que
le tocan (1500 / 100 = **15 ticks**). Así la pasada es reproducible aunque cambien la batería, el
rozamiento o el peso: marca el ritmo el encoder, no el reloj.

> **¿Por qué la pausa sigue en milisegundos?** Con el motor frenado no se generan ticks, así que una
> pausa medida en ticks no terminaría nunca (el robot se quedaría parado esperando). Por eso las dos
> paradas usan `PWMRAMPA_PAUSA_MS`, y las rampas usan ticks.

> La FASE 2 empieza con un **escalón** de 0 % a −100 % (la rampa recorre de 100 % a 0 %, no al
> revés). Es la secuencia pedida; implica un pico de corriente. Para arrancar suave, cambiar la
> llamada por `pwm_rampa(activos, n_activos, 0, -100, 2)`.

### Seguridad si la rueda no avanza

Con la rampa medida en ticks, un encoder que no cuenta o una rueda atascada dejarían el duty subiendo
sin avanzar. Hay dos amortiguadores:

| Define | Valor | Qué hace |
|:-------|:-----:|:---------|
| `PWMRAMPA_TICKS_TIMEOUT_MS` | `300U` | Si un paso no consigue sus 15 ticks en ese tiempo, **se pasa al siguiente duty** y se avisa: `>PWM_AVISO:paso sin ticks` |
| `PWMRAMPA_FASE_TIMEOUT_MS` | `20000U` | Si la rampa entera pasa de 20 s, se **aborta** y se frena: `>PWM_AVISO:fase abortada` |

> Al arrancar, el motor no se mueve hasta vencer el rozamiento estático (típicamente 15–25 % de duty),
> así que los primeros pasos se los come el primer tiempo de espera y se verán varios
> `>PWM_AVISO:paso sin ticks`. Es normal; si molesta, bajar `PWMRAMPA_TICKS_TIMEOUT_MS`.

### Encoders usados

El **modo encoder ya está programado** en `sensor/encoder.c` (no hubo que cambiar nada):
`TIM_ENCODERMODE_TI12` (decodificación **×4**), entradas con pull-up y contador de 16 bits.

| Rueda | Encoder | Pines | Timer |
|:------|:--------|:------|:------|
| izquierda | encoder 1 | **PA0 = A · PA1 = B** | TIM2 |
| derecha | encoder 2 | **PA6 = A · PA7 = B** | TIM3 |

⚠️ `MX_GPIO_Init()` pone PA4–PA7 como analógicos; es `ENC_Init()` quien reconfigura PA6/PA7 como
entradas. Por eso **el orden de `APP_InitAll()` importa**: `MX_GPIO_Init()` → … → `ENC_Init()`.

La prueba usa los **contadores crudos** del timer (16 bits, giran en ambos sentidos) y toma el valor
absoluto del avance, así que funciona en las dos fases. Si un encoder cuenta al revés, se intercambian
sus hilos **A y B** físicamente.

### Pines y motores

| Pin | Timer | Driver | Motor | Rueda |
|:---:|:------|:-------|:------|:------|
| **PA8** | TIM1_CH1 (`PWM1A`) | DRV8833 `AIN1` | `motor1` | izquierda |
| **PA9** | TIM1_CH2 (`PWM1B`) | DRV8833 `AIN2` | `motor1` | izquierda |
| **PA10** | TIM1_CH3 (`PWM2A`) | DRV8833 `BIN1` | `motor2` | derecha |
| **PA11** | TIM1_CH4 (`PWM2B`) | DRV8833 `BIN2` | `motor2` | derecha |

`MX_TIM1_Init()` configura **PA8–PA11** (TIM1 CH1…CH4) como salida alterna: no hay que tocar
`gpio.c`. `PWM_PIN_MASK` (`pinout.h`) es una máscara **de GPIOA**; aplicarla a GPIOB chocaría
con el emisor IR `WALL L4` (PB8) y el botón DOWN (PB9).

✅ **Qué rueda es cada canal**: identificado en banco el 2026-09-19. Ver el reparto en
[hardware.md](./hardware.md#41-motores-transmisión-y-chasis).

### Parámetros ajustables

| Define | Valor | Significado |
|:-------|:-----:|:------------|
| `PWM_RAMPA_MOTOR` | `0` | Motor que se mueve: `1` = **izquierdo** (PA8/PA9) · `2` = **derecho** (PA10/PA11) · `0` = **los dos** a la vez |
| `PWMRAMPA_TICKS` | `1500U` | Ticks por rampa (500 ticks = 1 s ⇒ 1500 ≈ 3 s) |
| `PWMRAMPA_PASOS` | `100U` | Pasos de duty de la rampa (100 ⇒ 1 % por paso, 15 ticks por paso) |
| `PWMRAMPA_PAUSA_MS` | `2000U` | Parada entre rampas (en ms) |
| `PWMRAMPA_TICKS_TIMEOUT_MS` | `300U` | Máximo por paso sin ticks (ver seguridad) |
| `PWMRAMPA_FASE_TIMEOUT_MS` | `20000U` | Máximo por rampa completa (ver seguridad) |

> Con `PWM_RAMPA_MOTOR` a `1` o `2`, la rueda **no elegida se deja frenada** para que el robot no
> ruede por el banco. Con `0` (valor por defecto) el robot avanza en la fase 1 y retrocede en la
> fase 2 → **ruedas en el aire o espacio de sobra**, y ojo: **si en vez de ir recto gira sobre sí
> mismo**, un motor está montado en espejo respecto al otro (hay que invertir un canal).

### Interpretación de la telemetría

| Observación | Significado |
|:------------|:------------|
| `>PWM_MOTOR` y `>PWM_TICKS_RAMPA` al arrancar | Confirman qué rueda se mueve (`IZQUIERDO`/`DERECHO`/`ambos`) y cuántos ticks dura la rampa |
| `>ENC_IZQ` y `>ENC_DER` suben al moverse la rueda | Los dos encoders cuentan. Si uno se queda clavado, su cableado (A/B) o su pull-up |
| `>ENC_DER` sube **hacia arriba** (positivo) y `>ENC_IZQ` también | Los dos cuentan en el mismo sentido. Si uno baja, sus hilos A y B están invertidos |
| `>PWM_TICKS` llega a ~1500 al final de la rampa | La rampa ha consumido el presupuesto de ticks correcto |
| `>PWM_TPS` crece con el duty | Es la aceleración del motor: ticks/s medidos. Debe subir de forma monótona, sin mesetas largas |
| `>PWM_DUTY` sube de 0 a 100 y luego de −100 a 0, sin saltos intermedios | La rampa avanza en pasos del 1 % en el orden correcto |
| El motor **no gira nunca** | `PWM_StartAll()`, `DRV8833_Init()`, alimentación del `VM` del DRV8833 o cableado del par de pines del motor elegido |
| Gira en las dos fases pero **siempre en el mismo sentido** | Las dos entradas del canal están cortocircuitadas o el driver está en modo *coast* en una de ellas |
| Gira **solo en una fase** | Falta una de las dos señales (`IN1`/`IN2` del canal elegido): revisar continuidad del par de pines |
| Aparecen muchos `>PWM_AVISO:paso sin ticks` | La rueda no arranca con ese duty (rozamiento) o el encoder no cuenta. Se ve en `>ENC_IZQ`/`>ENC_DER`: si no suben, es el encoder |
| `>PWM_AVISO:fase abortada` | La rampa no terminó en 20 s: el encoder no cuenta, la rueda está bloqueada o la alimentación del driver no llega |
| La frenada no es instantánea | `DRV8833_Brake()` pone **ambas** entradas del canal a 1 (*short brake*); si el motor sigue rodando, revisar `IN1`/`IN2` o la inercia del conjunto |
| El LED onboard no alterna | Debe estar **encendido durante las rampas** y apagado en las pausas (PC13, activo a bajo) |

---

## 10. Procedimiento: botones → pantalla OLED (prueba 10)

Valida de una vez tres cosas: el bus **I2C1**, la **pantalla OLED** y el **cableado de los
botones**. Al pulsar un botón, la pantalla (128×32) muestra su nombre y el pin, a escala ×2:

```
MENU
PA12
```

1. Elegir **TEST → BOTONES OLED** en el menú.
2. Compilar, flashear y abrir el monitor (o Teleplot).
3. Comprobar la marca de arranque **`>OLED:OK I2C1`** y el estado del bus
   (`>I2C_SCL:1`, `>I2C_SDA:1`, `>I2C_N:1` y `>I2C_FOUND:60`). Si sale `>OLED:no detectada`,
   ejecutar la **prueba 11** y ver [hardware.md §4.9](./hardware.md#49-pantalla-oled-ssd1306-12832).
4. Pulsar cada botón y mirar la pantalla: debe aparecer el nombre arriba y el pin abajo.

| Botón | Pin que debe salir | Evento por telemetría |
|:------|:------------------:|:---------------------:|
| MENU | PC14 | `>BTN:1` + `>BTN_PIN:PC14` |
| UP | PB9 | `>BTN:2` + `>BTN_PIN:PB9` |
| DOWN | PC15 | `>BTN:4` + `>BTN_PIN:PC15` |
| OK | PA12 | `>BTN:8` + `>BTN_PIN:PA12` |

Diagnóstico cruzado, que es lo bueno de tener las dos salidas:

| Telemetría | Pantalla | Conclusión |
|:-----------|:---------|:-----------|
| `>BTN:n` correcto | Cambia bien | Todo OK ✅ |
| `>BTN:n` correcto | No cambia | El fallo está **solo en el OLED**: bus, dirección o alimentación |
| `>BTN:n` de otro pin | Muestra ese otro pin | Cableado: ese botón está en otro pin (la pantalla solo pinta `BUTTONS_GetPinName()`) |
| Nada | No cambia | Botón sin contacto, pull-up o `BUTTONS_Update()` no llamada a 1 kHz |

> La fuente del driver solo tiene los glifos de
> [hardware.md §4.9](./hardware.md#49-pantalla-oled-ssd1306-12832); el texto de los botones
> (`MENU`, `UP`, `DOWN`, `OK` y los pines) está cubierto.

---

## 11. Procedimiento: diagnóstico del bus I2C1 (prueba 11)

Para cuando la pantalla no aparece. `App_Test_I2C_Scan()` repite cada 2 s dos medidas, para poder ir
tocando el cableado y ver el efecto en directo:

1. **Niveles en reposo** — `I2C_BusLevels()` pone PB6/PB7 como entradas **flotantes** y los lee.
2. **Escaneo** de 0x08 a 0x77 — `I2C_Scan()` prueba cada dirección con `HAL_I2C_IsDeviceReady()`.

| `>I2C_SCL` | `>I2C_SDA` | `>I2C_N` | Diagnóstico | Qué hacer |
|:----------:|:----------:|:--------:|:------------|:----------|
| 1 | 1 | 0 | El bus está subido pero **nadie contesta** | Cableado SDA/SCL (¿cruzados?), alimentación del módulo, dirección rara |
| 1 | 1 | ≥1 | Hay dispositivos en el bus | Comparar `>I2C_FOUND` con **60** (0x3C) o **61** (0x3D) |
| 0 | 0 | 0 | **Nadie sube el bus**: faltan pull-ups | Poner **2× 4.7 kΩ** de PB6 y PB7 a 3.3 V |
| 0 | 1 | 0 | SCL tirado a GND | Cortocircuito o SCL mal cableado |
| 1 | 0 | 0 | SDA tirado a GND | Cortocircuito o SDA mal cableado |

### Pull-ups: el detalle que muerde en el F1

En el STM32F1 el pull-up interno **no se puede activar en modo alterno open-drain**. No es una
opinión: en `stm32f1xx_hal_gpio.c`, `GPIO_Init->Pull` solo se usa en la rama `GPIO_MODE_INPUT`;
para `GPIO_MODE_AF_OD` se escribe `config = speed + AF_OUTPUT_OD` y el campo `Pull` **se ignora**.
Como el periférico I2C exige los pines en `AF_OD`, no hay pull-up interno posible: el bus lo tiene
que subir el módulo o dos resistencias externas.

| Opción | Cuándo |
|:-------|:-------|
| Dejarlo como está | El módulo lleva pull-ups propios (muchos SSD1306 sí) |
| **2× 4.7 kΩ** de PB6/PB7 a 3.3 V | El módulo no los lleva → es la solución buena |
| Bajar `I2C1_CLOCK_HZ` a `50000U` en `hal/i2c.h` | Hay pull-ups pero flojos (≥10 kΩ) y falla a 100 kHz |

---

## 12. Procedimiento: medida de la batería 2S (prueba 12)

Antes de ejecutarla hay que **montar los divisores** de `hardware.md §4.6`: en cada canal,
`R1 = 22 kΩ` de la tensión a medir al pin del ADC y `R2 = 10 kΩ` del pin a GND. **Sin condensador**
(no hay sitio en el chasis): el ADC va sobrado con 239,5 ciclos de muestreo y la media de 16 lecturas.

| Canal | De dónde sale R1 | Pin |
|:------|:-----------------|:---:|
| Pack (total) | Positivo del pack | **PB0** |
| Celda 1 | Punto medio entre las dos celdas (hilo central del conector de balance) | **PB1** |

La celda 2 se calcula restando: $V_2 = V_{pack} - V_1$. Todos los GND al negativo del pack.

1. Elegir **TEST → BATERIA** en el menú.
2. Compilar, flashear y abrir el monitor.
3. **Antes de montar los divisores**: el pack no da medida, así que el estado se queda en
   `>VBAT_ESTADO:0` y sale `>PACK_PRESENTE:0`. Un canal sin medida **no** es una batería agotada y
   no bloquea nada; la pantalla enseña `0.00V`.
4. Con los divisores puestos, comparar `>VBAT` y `>VCELL1` con el multímetro. Lo primero que hay que
   mirar es **`>BAT_K`**: dice con qué factor está convirtiendo el ADC el firmware que está corriendo.
5. Si hay diferencia, calibrar **desde la consola**: con el pack a una tensión conocida (medida con
   el multímetro en la entrada del divisor) se ejecuta `BAT CAL <mV>`, por ejemplo `BAT CAL 7400`.
   Corrige a la vez la tolerancia de las resistencias y la de `VREF+`.
6. Para ver el **aviso** sin descargar la batería: bajar el umbral en caliente, `BAT AVISO_PACK 8000`.

La pantalla de la prueba enseña la medida (`PACK`, `CEL1` y `K`, el factor del divisor en uso) y en la
última línea `MENU: SALIR`. Si la batería entra en aviso, el aviso manda sobre esa pantalla (dos líneas
a escala 2 y el LED parpadeando). El detalle fino —celda 2, desequilibrio, estado y presencia de cada
canal— va por telemetría.

**Comprobar el canal de la celda 1 sin batería 2S** (con fuente no hay punto medio que medir) —
**opcional**, no es crítico para nada: puentear la **entrada del divisor de PB1** —el lado de la
22 kΩ, **nunca el pin**— a los 3,3 V del riel. Entonces:

| Con `K` = | `>VCELL1` debe marcar |
|:---------:|:---------------------:|
| 0,3125 (teórico del divisor) | ≈ **3,20 V** |
| ≈ 0,3015 (valor de arranque) | ≈ **3,30 V** |

Sale 3,30 V con `K` calibrado porque el patrón es el propio VDDA: es una **comprobación cruzada** de
que `K` coincide con el divisor real. ⚠️ Está **justo en el umbral de aviso** (3,30 V), así que con el
ruido puede parpadear el LED, y el desequilibrio calculado sale enorme, porque el firmware cree que hay
dos celdas de verdad. Se silencia mientras se prueba con `BAT AVISO_CELDA 3000` y `BAT DESEQ 2000`
([comandos-serie.md](./comandos-serie.md)).

La medida la hace **`sensor/battery.c`** (capa SENSOR) y la presentación
**`app_aviso_bateria()`** (capa APP, en `application.c`). Umbrales en `sensor/battery.h`:
`AVISO` a **3,30 V/celda** (6,60 V de pack) y `CRITICA` a **3,00 V/celda** (6,00 V), más desequilibrio
entre celdas > 0,40 V. El LED parpadea a **1 Hz** en aviso y a **4 Hz** en crítica.

Comprobación rápida sin multímetro, con `>ADC_B0` (cuentas del pack) y `>ADC_B1` (celda 1):
8.4 V → ≈ **3258** cuentas · 7.4 V → ≈ **2870** · una celda a 3.7 V → ≈ **3304** ⚠️ las cuentas de la
celda son con **su** divisor (3k9/10k), no con el del pack: si salen ≈1435 se está aplicando la $k$
equivocada. El pack tiene 10,6 V de fondo de escala y la celda 4,59 V, así que ninguno satura nunca.

Con la fuente a 7.4 V el multímetro debe medir ≈ **2.24 V** en el pin del pack (PB0), y el firmware
con `>BAT_K` = **0,3015** marca ≈ **7.40 V** (es el `k` calibrado contra el multímetro; lo que quede
lo cierra `BAT CAL`).

| Lectura | Significado |
|:--------|:------------|
| `>BAT_K` = **0,3015** | Es el firmware actual, con el `k` ya calibrado contra el multímetro ✅ |
| `>BAT_K` = **0,3125** | Binario **anterior** a la calibración del 30-sep: mide un **3,5 % alto** (7,14 en vez de 7,40 V) |
| `>BAT_K` = **0,35897** | Binario **anterior** al cambio del divisor: volver a flashear (marcaría 6.2 V con 7.4 V reales) |
| `>VBAT` ≈ 8.4 V y `>VBAT_DESEQ` ≈ 0 | Todo bien ✅ |
| `>VBAT` ≈ 0 y `>ADC_B0` ≈ 0 | Falta R1, el pin no llega a la batería o el divisor no está montado (`>PACK_PRESENTE:0`) |
| `>VBAT` ≈ 3.7 V (lo mismo que una celda) | El canal del pack está midiendo el punto medio: R1 conectada al sitio equivocado |
| `>VBAT` ≈ 11 V (saturado, `>ADC_B0` ≈ 4095) | Divisor mal: R2 demasiado grande o R1 puenteada |
| `>VBAT_DESEQ` > 0.40 V | Las celdas van muy desequilibradas: balancear el pack |
| `>VBAT_ESTADO` = 1 | Pack por debajo de 6.60 V (3,30 V/celda): aviso (LED a 1 Hz) |
| `>VBAT_ESTADO` = 2 | Pack por debajo de 6.00 V (3,00 V/celda): crítica (LED a 4 Hz) |
| `>CELDA_AVISO` = 1 | El aviso lo dispara una celda por debajo de 3.30 V (informativo: no bloquea) |

---

## 13. Procedimiento: sensores de pared IR (pruebas 13–17)

El ADC1 tiene **6 canales** y todos son de impedancia alta, así que este bloque se hace **de una
vez**: los 4 receptores de pared (PA2–PA5) y los 2 divisores de batería (PB0/PB1, prueba 12).
Se ejecuta **al terminar de soldar**, en el orden de la tabla: primero el chequeo, y solo si sale
bien se sigue.

| Orden | Prueba | Para qué |
|:-----:|:-------|:---------|
| 1º | **13** `App_Test_ADC_Soldadura()` | ¿Están bien los 6 canales? (soldadura, cortos, saturación) |
| 2º | **14** `App_Test_EmisoresIR()` | ¿Encienden los 4 LED y dónde está cada uno? |
| 3º | **15** `App_Test_IR_Receptores()` | ¿Qué receptor ve a qué emisor? ¿Aguanta la $R_L$? |
| 4º | **16** `App_Test_IR_Modulado()` | ¿Se cancela la luz ambiente restando ON−OFF? |
| 5º | **17** `App_Test_CalibracionIR()` | Curva distancia↔ADC y umbral de pared |

> 🔎 **Para mirar un solo canal** están los submenús **`VER R0`..`VER R3`** (añadidos el 30-sep a
> petición del usuario): son la prueba 15 vista por un receptor — el mismo barrido de emisores
> (`IR 1 0 0 0`) pero con el valor de ese receptor a pantalla completa. Es la forma cómoda de emparejar
> emisor con receptor y de ver moverse un canal al acercar la pared.

> 🖥️ **Las cinco pruebas del bloque pintan en la OLED**, así que se pueden seguir sin tener el
> Teleplot delante: la **13** el veredicto de cada canal en palabra corta; la **14**, la **15** y la
> **16** un cartel con el paso en curso y el contador `LATIDO` (avanza a 2 Hz: es la **señal de
> vida**, para que un cartel fijo no parezca una prueba colgada); y la **17** la calibración guiada,
> punto a punto. El detalle numérico sigue estando en Teleplot, y la pantalla lo recuerda con
> `VER TELEPLOT`.

> 🔤 **Convención de nombres** (fijada el 30-sep a petición del usuario, que se liaba con las
> etiquetas): los **emisores** son `IR0`..`IR3` (`WALL L1`..`L4`, en PB3/PB4/PB5/PB8) y los
> **receptores** son `R0`..`R3` (`WALL A1`..`A4`, en PA2..PA5). Antes los receptores salían como `IRn`
> en la telemetría, que es justo lo que hacía difícil saber cuál era cuál.
> Ya está aplicada en las cinco pruebas del bloque: `>ADC_Rn*` (13), `>Rn` (15), `>Rn_OFF/ON/DIF/VAR`
> (16) y `>Rn_CAL`/`>Rn_CM` (17).

> ⚠️ **Emisor y receptor no son pareja eléctrica**: que el emisor esté en PB3 no significa que su
> reflexión la lea PA2. La correspondencia la **mide** la prueba 15 y se anota en
> [hardware.md §4.4](./hardware.md#44-sensores-de-pared-ir), donde los **emisores ya tienen posición
> física** (L1 frontal izq · L2 lateral der · L3 lateral izq · L4 frontal der) y los receptores
> siguen «sin decidir».
>
> ⚠️ **El orden `L1`..`L4` no es geométrico**: L1 y L4 son los frontales y L2/L3 los laterales.

### 13.1 Prueba 13 — chequeo de la soldadura

**Qué valida**: que los 6 canales del ADC lleguen a donde tienen que llegar. Es la prueba más
rápida para saber si hay que repasar una soldadura antes de perder tiempo con lecturas raras.

Lee los 6 canales con media de 16 muestras, imprime a 2 Hz y añade un **veredicto** por canal:

```
>ADC_R0:1832      >ADC_R1:1770      >ADC_R2:1908      >ADC_R3:1815
>ADC_R0_DIF:140   >ADC_R1_DIF:95    >ADC_R2_DIF:180   >ADC_R3_DIF:110
>ADC_R0_EST:0     >ADC_R1_EST:0     >ADC_R2_EST:0     >ADC_R3_EST:0
>ADC_B0:3258      >ADC_B1:1629
>ADC_EST_B0:0     >ADC_EST_B1:0
```

**En la pantalla** sale el mismo veredicto en palabra corta —`ADC SOLDADURA` / `R0 OK R1 MUDO` /
`B0 OK B1 N/USA` / `VER TELEPLOT` / `MENU: SALIR`— para poder repasar la soldadura sin Teleplot. Los
nombres van abreviados porque **la fuente de la OLED no tiene `H`, `J`, `Q`, `Y` ni `Z`** (ver
`app/oled.h`): `SAT` es *saturado*, `DEBIL` es *flojo*, `IGUAL` es *canales cruzados* y `N/USA` es
*no usada*.

Los receptores se miden **con los emisores apagados y con los cuatro encendidos**, así que el
veredicto sale solo (no hay que tapar nada con la mano): la lectura dice si el canal está muerto,
saturado o flojo, y `>ADC_DIFn` (encendido − apagado) confirma que **ve luz**.

El veredicto de la **celda 1** depende de que ese divisor esté en uso: con fuente de alimentación el
canal no tiene de dónde medir y sale `NO USADA` (no `SIN DIVISOR`), que no es una avería.

| Lectura | Código `>ADC_Rn_EST` | Veredicto | Causa probable |
|:--------|:------------------:|:---------:|:---------------|
| Clavada en **0** | 1 | `MUDO` | Pin no conectado, soldadura fría, pista cortada |
| Clavada en **4095** | 2 | `SATURADO` | Luz ambiente, $R_L$ demasiado alta, colector/emisor invertidos |
| Baja de 200 cuentas | 3 | `FLOJO` | Poca señal: subir $R_L$ o apantallar |
| Entre 200 y 4090 y `DIF` > 0 | **0** | `OK` | Canal listo |
| **PB0** ≈ 0 | 4 | `SIN DIVISOR` | R1 sin soldar, divisor sin montar |
| **PB1** ≈ 0 y el divisor de la celda 1 no se usa (fuente de alimentación) | 7 | `NO USADA` | No es un fallo: el canal no tiene de dónde medir |
| PB0 o PB1 > 3200 cuentas | 5 | `MAL DIVISOR` | R2 demasiado grande o R1 puenteada |
| PB0 ≤ PB1 | 6 | `CANALES CRUZADOS` | El canal del pack está leyendo el punto medio del balance |

**Criterio**: los 6 canales con veredicto `OK` (o el fallo explicado), repetible 10/10.
**Antes** de esta prueba: comprobar con el multímetro que los 3.3 V del regulador de la Blue Pill
están bien (de ahí cuelgan emisores, receptores e IMU).

### 13.2 Prueba 14 — emisores IR por transistor

**Qué valida**: los 4 emisores `WALL L1..L4` (**PB3, PB4, PB5, PB8**) encienden de uno en uno, cada
uno con su **transistor BC337** (base por **1 kΩ**, LED con **22 Ω** en serie; ver
[hardware.md §4.4](./hardware.md#44-sensores-de-pared-ir)). Posiciones físicas (medidas el 30-sep):
**L1 frontal izquierdo · L2 lateral derecho · L3 lateral izquierdo · L4 frontal derecho**. Es la
prueba que confirma que el **JTAG está desactivado**: PB3 y PB4 son `JTDO`/`NJTRST`, y si alguien
vuelve a habilitar el JTAG **dejan de funcionar**.

1. Enciende un emisor cada segundo, en bucle: `L1 → L2 → L3 → L4 → los 4 → apagados`, y saca
   `>IR_LED:1`, `>IR_LED:2`… (**`5` = los cuatro juntos**, para la prueba de esfuerzo del riel, y
   `0` = los cuatro apagados).
2. **Comprobación visual**: mirar el LED **a través de la cámara de un móvil** (un LED de 860 nm
   es casi invisible al ojo pero sale como un punto claro en el sensor).
3. **Comprobación eléctrica**, con el multímetro:
   - Caída en la **22 Ω** de un emisor encendido: ≈**1.65 V** ⇒ $I_F$ ≈**75 mA** (V = I·R); el
     consumo del riel, con **un** emisor encendido, ≈**75 mA**.
   - $V_{CE}$ del transistor encendido: **≤0.3 V** (si sale ≈1 V, está poco saturado: revisar la
     resistencia de base o el patillaje).
   - **Prueba de esfuerzo** (el paso «los 4» de la secuencia): ≈**300 mA** y el riel de 3,3 V sin
     bajar más de **50 mV**. En el uso normal solo luce uno, pero si el riel se hundiera al encender
     los cuatro, el ADC cambiaría de escala (su referencia es $V_{DDA}$).

| Síntoma | Causa probable |
|:--------|:---------------|
| `L1` o `L2` no encienden y los demás sí | JTAG activo: falta `__HAL_AFIO_REMAP_SWJ_NOJTAG()` en `gpio.c` |
| Ninguno enciende | Riel de 3.3 V caído, transistores sin soldar o base al aire |
| Un emisor no enciende y su base sí llega a 0.8 V | Transistor al revés (¡el BC337 va **C-B-E**, no E-B-C!) o emisor/colector cruzados |
| Corriente muy baja (< 20 mA) | Patillaje mal, 22 Ω de otro valor o LED invertido |
| El riel de 3,3 V se hunde > 100 mV | El regulador de la Blue Pill no puede con 300 mA: modular o subir la R en serie |
| El transistor se calienta | Está conduciendo en zona activa (base sin corriente suficiente) |

**Mientras corre esta prueba**, apuntar qué emisor es cada uno y anotar en `hardware.md §4.4` su
**posición física** definitiva. La de los emisores ya está fijada (30-sep): **L1 frontal izquierdo ·
L2 lateral derecho · L3 lateral izquierdo · L4 frontal derecho**; lo que queda por identificar es
**con qué receptor** está emparejado cada uno (eso lo mide la prueba 15).

**Criterio**: los 4 se identifican individualmente, cada uno consume ≈75 mA y, en el paso de los
cuatro a la vez, el riel de 3,3 V no baja más de 50 mV.

> [!NOTE] En el robot solo hay **un emisor encendido cada vez** (decisión del usuario, 28-sep): el
> barrido `L1→L2→L3→L4` permite además saber **de qué emisor** viene la luz que ve cada receptor
> (pruebas 15 y 16). El paso de «los 4» de esta prueba queda como comprobación de que el riel
> aguanta, no como modo de trabajo.

### 13.3 Prueba 15 — receptores: fondo y reflejo

**Qué valida**: que cada receptor responda y **con qué emisor está emparejado**, y de paso elige la
$R_L$ definitiva (el valor de partida, **3 kΩ**, se eligió con el datasheet y el criterio de
`calculos-diseno.md` §3.1).

La prueba recorre la **matriz 4×4**: enciende cada emisor y lee los cuatro receptores. Publica **solo
8 etiquetas y sin pausas**, de forma continua, para poder dejarla corriendo mientras se mueve la
pared o el robot:

| Etiqueta | Qué es |
|:---------|:-------|
| `>IR0`..`>IR3` | Qué emisor está encendido (`1` = encendido, `0` = apagado) |
| `>R0`..`>R3` | **Cuentas crudas del ADC** de cada receptor |

```
>IR0:1  >IR1:0  >IR2:0  >IR3:0
>R0:3960  >R1:3810  >R2:4105  >R3:3815
```

Cada emisor luce `IR_PASO_MS` (**2 s**), así que el ciclo completo dura 8 s: tiempo de sobra para leer
los ocho valores en la pantalla o en Teleplot (si molesta, se cambia en esa constante). El receptor
cuyo valor **sube** al encenderse un emisor concreto es el que lo ve: en el ejemplo, el emisor 0 lo ve
el receptor 0 (3960 frente a los ≈3810 de fondo de los demás).

La pantalla enseña lo mismo (`IR 1 0 0 0` y las cuatro lecturas), así que se puede emparejar sin
Teleplot. Si con los cuatro números a la vez no se ve claro, los submenús **`VER R0`..`VER R3`** del
bloque IR hacen este mismo barrido mostrando solo la lectura del receptor elegido:

```
IR 1 0 0 0
R1 3960
MENU: SALIR
```

**Pareja medida el 30-sep** (con pared a 10 cm, mirando los submenús `VER Rn`): **R0←IR3, R1←IR1,
R2←IR2 y R3←IR0**, ya anotada en `ir_pareja_emisor[]` (`app/application.c`), que es la que usan las
pruebas 16 y 17. Medida **con sol directo**, que no afecta al emparejamiento (lo que cambia es el
fondo). No es la pareja «natural»: el soldado no siguió el orden geométrico. Y como cada
receptor ve al emisor que apunta hacia el mismo lado, esto **sitúa los receptores**
([hardware.md §4.4](./hardware.md#44-sensores-de-pared-ir)).

| Síntoma | Qué hacer |
|:--------|:----------|
| Un receptor clavado en 4095 con la luz encendida | El fondo satura: **bajar $R_L$ a 2.2 kΩ o 1 kΩ** y/o apantallar el receptor (tubo negro) |
| Un receptor por debajo de 200 cuentas | Poca luz sobre el receptor: **subir $R_L$ a 10 kΩ** |
| El valor sube menos de 20 cuentas apuntando a la pared a 5 cm | Señal muy pequeña: revisar alineación (el haz del emisor es de **±3°**), apantallado, o subir $R_L$ |
| Un canal **no cambia** ni al taparlo con la mano | Receptor no conectado o soldadura fría |
| La lectura **casi no se mueve** al iluminar (mV, no décimas de V) | **Colector y emisor invertidos**: el fototransistor ve la luz pero sin ganancia |
| Saturado a 4095 **incluso tapando** el receptor | El colector está a **5 V** en vez de 3,3 V (el ADC no admite más de 3,6 V) |
| Un canal solo sube con un emisor concreto | **Pareja emisor↔receptor identificada**: apuntarla en `ir_pareja_emisor[]` |

**Criterio**: con una pared blanca a 10 cm y la luz de la sala encendida, el valor **sube más de 50
cuentas** en los 4 canales. Si hay que cambiar $R_L$, se cambia en hardware y se actualiza
[hardware.md §4.4](./hardware.md#44-sensores-de-pared-ir).

### 13.4 Prueba 16 — modulación ON−OFF (cancelación de la luz ambiente)

**Qué valida**: el truco que hace prescindible el condensador en el receptor. En vez de filtrar en
continua, se **conmuta el emisor y se resta** (encendido − apagado): la luz ambiente cae en los dos
términos y se cancela.

1. Por cada canal: **8 pares** de medidas (emisor ON, emisor OFF) y media → `>R0_OFF`, `>R0_ON`,
   `>R0_DIF` y **`>R0_VAR`** (dispersión de la diferencia, en % de su valor medio) a 4 Hz.
2. **Prueba de fuego**: encender y apagar la luz de la habitación con el robot quieto apuntando a
   la pared. El `DIF` debe aguantar **±10 %** (y `VAR` quedarse por debajo de 10).

**En la pantalla** salen los cuatro `DIF` **de dos en dos** (`R0 1250  R1 1240` / `R2 1350  R3 1600`) y el
aviso de salida, sin contador de latidos. El **`VAR` no se pinta**: para seguir el `DIF` estorba, y
como indicador sigue publicándose por telemetría.

> ⚠️ **Dos canales por fila, y no uno.** En 128×32 solo hay **4 filas (0..3)**: con una fila por canal
> el `R3` caía en la fila 4 y `OLED_Print()` **sale sin dibujar nada, en silencio**. Así estuvo hasta el
> 2-oct: en la pantalla solo aparecían R0, R1 y R2.

> ☀️ **Se hace con luz controlada** (sala encendida y apagada, pared en sombra). Con **sol directo**
> sobre la pared el fondo sube y el `DIF` se queda corto: daría un suspenso falso.

**Presupuesto de tiempo** (polling, 21 µs por conversión): un par ON/OFF ≈ 42 µs; 8 pares ≈ 0.4 ms;
los 4 canales ≈ 1.5 ms ⇒ sobra para refrescar a 100 Hz. Cuando el robot corra de verdad, esto
migrará a un **PWM por timer + DMA** (fase posterior), sin límite de frecuencia.

**Criterio**: `DIF` estable con la luz cambiando. Es la base de `filter/adc_filter.c`.

### 13.5 Prueba 17 — curva distancia↔ADC

**Qué valida**: la tabla con la que el robot decide «hay pared a X cm».

Procedimiento **guiado con botones** (es el primer sitio donde se aprovecha el menú del futuro
[maquina-estados-menu.md](./maquina-estados-menu.md)):

1. Elegir el canal con `UP`/`DOWN` (la OLED pide el canal, el punto y la distancia).
2. Poner el robot a la primera distancia (3 cm) de una **pared blanca**, medir con regla.
3. `OK` congela la media de 64 muestras y envía `>IR_CAL_CANAL:0`, `>IR_CAL_CM:3` e `>R0_CAL:2740`;
   pasa sola a la siguiente distancia (y al siguiente canal al completar los 5 puntos).
4. Repetir para **5, 8, 10 y 15 cm**.
5. Al terminar los 4 canales, publica la tabla completa y pasa a modo vivo: `>Rn_CM` con la
   distancia estimada de cada receptor (interpolación lineal entre los 5 puntos).

> 💡 **El emisor del canal que se está calibrando está encendido.** Lo enciende `ir_cal_emisor(canal)`
> (el emisor que empareja `ir_pareja_emisor[]`): al entrar en la prueba, al cambiar de canal con
> `UP`/`DOWN`, **antes de congelar cada punto** y, en el modo vivo, conmutando por canal antes de cada
> lectura. Como solo puede lucir **uno a la vez**, el modo vivo hace los cuatro seguidos. Al salir con
> `MENU` se apagan los cuatro.
>
> ⚠️ **Hasta el 2-oct la prueba no encendía ningún emisor** y medía el **fondo de la sala**: sus
> `>Rn_CAL` no dependían de la distancia, así que **las curvas hechas con ella no valen y hay que
> repetirlas**. Ver la fila del 2026-10-02 en [estado-del-proyecto.md](./estado-del-proyecto.md).

| Distancia | Qué se espera |
|:---------:|:--------------|
| 3 cm | Valor alto y **sin saturar** (< 4095) |
| 15 cm | `DIF` al menos **3× el ruido** del fondo |
| Menos de 1 cm | Que no sature tampoco (útil al centrar pasillo) |

**Criterio**: error < 1 cm entre 3 y 15 cm, con la luz de la sala encendida. La tabla se guarda en
`filter/adc_filter.c` cuando exista FILTER; mientras tanto, como `#define` en el sensor.

> ☀️ Igual que la prueba 16: **con sol directo** los cinco puntos se desplazan (el fondo sube),
> así que se calibra con luz controlada.

### 13.6 Nota sobre las pruebas 1 y 3

- **Prueba 1** (`App_Test_ADC()`): prueba de humo del ADC. ✅ **Reescrita el 2026-09-30 para leer solo
  PA2–PA5** (4 canales): el 5.º valor que antes llamaba `IR4` era **PB0, la batería**, y eso ya lo
  cubre la prueba 12. Queda pendiente **revalidarla** en el robot (la validación de §3 es de la
  versión de 5 canales).
- **Prueba 3** (`App_Test_ADC_PWM()`): ➖ **RETIRADA el 2026-09-28** (decisión del usuario). Era
  «ADC → PWM con potenciómetro», un banco de la primera fase con la Blue Pill sola: el robot no
  lleva potenciómetros. La función se ha borrado de `application.c` y su declaración de
  `application.h`.

---

## 14. Procedimiento: PID de velocidad por rueda (prueba 18)

Es la primera prueba que usa la capa **CONTROL** y la **puerta de salida** de esa fase. El diseño y
la teoría están en [control-pid.md](./control-pid.md).

**Qué hace**: cierra el lazo de velocidad de cada rueda por separado — consigna con rampa → PID →
duty — midiendo la velocidad con su encoder. Cada rueda tiene su PID: es lo que hace que el robot
vaya recto aunque los motores no sean idénticos.

### Modos (`PID_TEST_MODO`)

| Modo | Qué hace | Para qué |
|:----:|:---------|:---------|
| **1** | Escalón de consigna 0 ↔ `PID_TEST_VEL_MM_S` cada 2 s | Sintonía a mano. ⚠️ Al alternar cada 2 s **corta la oscilación** justo cuando hay que ver si es sostenida |
| **2** | Consigna con rampa (`PID_TEST_RAMPA_MM_S2`) | Funcionamiento normal; es el que se deja puesto |
| **3** | Escalón que **se queda puesto** + **barrido automático de `KP`** | **El modo de sintonía recomendado**: busca `Ku` solo |

### Modo 3: barrido automático de `Kp`

⚠️ **No se busca que el robot se mueva suave.** Se busca el punto en que el lazo **oscila sin
parar**: ese `Kp` es `Ku`. La suavidad llega después, con las ganancias calculadas a partir de `Ku`.

Eso que se ve —el robot avanzando y retrocediendo— **es la oscilación que buscas**: cuando el lazo
oscila, el duty cambia de signo y el motor va hacia atrás.

El modo 3 lo hace solo:

1. Pide la consigna **una vez** y la deja puesta (sin alternar, para que la oscilación no se corte).
2. Arranca en `KP = 0,05` y lo sube `0,05` cada **3 s**.
3. Mide la **amplitud pico a pico** de la velocidad en la **parte final** de cada tramo.
4. Cuando la amplitud supera el **40 % de la consigna** en **dos ventanas seguidas**, para: ese `KP` es
   `Ku`, lo publica como `>PID_KU` (con `>PID_TU`) y deja puesta la `Kp = 0,6·Ku` que pide
   Ziegler-Nichols, para empezar a afinar sin teclear nada.

> [!WARNING] Dos detalles que parecen menores y no lo son
> **No se mide desde el primer instante del tramo**: la consigna se pide de golpe y la velocidad salta
> de 0 a su valor, así que ese pico **no es oscilación** pero cuenta como amplitud enorme. Sin
> descartar el transitorio (`PID_AUTO_ASENTAR_MS`, 1,5 s), el barrido se declara terminado en el primer
> tramo con un `Ku = 0,05` falso.
>
> **Y hacen falta dos ventanas seguidas** (`PID_AUTO_CONFIRMACIONES`): un pico suelto no es oscilación
> sostenida. Mientras la amplitud sea alta pero no se confirme, el barrido **no sube** `Kp`: espera.

**Pantalla:**

```text
AUTO KP 0.30      ← el Kp que está probando ahora
IZQ 148 DER 150
AMP 45  SET 150   ← amplitud pico a pico de esta ventana
OK:PAUSA MENU:SALIR
```

> **OK pausa el barrido y para los motores.** Con el `Kp` subiendo solo y los motores temblando no hay
> quien lea la pantalla: en pausa se ve `PAUSA KP 0.30` con el último `AMP` congelado. Otra pulsación
> reanuda y da un **tramo completo** antes de volver a medir.

Al terminar, la primera fila cambia a `KU 0.45 TU 0.21` (o `SIN OSCILAR` si llegó a `PID_AUTO_KP_MAX`
= 2,0 sin verla).

> ⚠️ **Las ruedas tienen que girar.** Sujetar el robot de forma que la rueda no gire deja al lazo **sin
> medida** (el encoder no cuenta) y el PID se ciega. Vale tenerlo **en el aire con las ruedas libres**
> o suelto en el suelo.

> Los números del barrido están en `app/application.c`: `PID_AUTO_KP_INI`, `PID_AUTO_KP_PASO`,
> `PID_AUTO_KP_MAX`, `PID_AUTO_PASO_MS` y `PID_AUTO_AMP_FRAC`. Si `Ku` sale muy alto o muy bajo,
> se ajustan ahí.

### Parámetros

| Define | Valor | Significado |
|:-------|:-----:|:------------|
| `PID_TEST_VEL_MM_S_DEF` | `300` | Consigna de velocidad de partida (mm/s ≈ 0,3 m/s) · **en caliente: `PID VEL`** |
| `PID_TEST_RAMPA_MM_S2` | `600` | Pendiente de la rampa (0 → 300 mm/s en 0,5 s) |
| `PID_TEST_KP_DEF` / `KI_DEF` / `KD_DEF` | `0.10 / 0.50 / 0.00` | Ganancias de partida **medidas** · **en caliente: `PID KP` / `KI` / `KD`** |
| `PID_TEST_DUTY_MAX` | `60` | Tope de duty **durante la prueba** (no llevarse un susto) |
| `PID_TEST_LAZO_MS` | `10` | Periodo del lazo: **100 Hz** (los encoders se leen a 1 kHz) |

Los que acaban en `_DEF` son solo el **valor de arranque**: las ganancias y la consigna se cambian con
la orden `PID` de la consola ([comandos-serie.md](./comandos-serie.md)) **sin recompilar**.

### Botones de la prueba

| Botón | Qué hace |
|:-----:|:---------|
| **OK** | Cambia de parámetro: `KP` → `KI` → `KD` → `VEL` |
| **UP / DOWN** | Sube o baja el parámetro marcado con `>` |
| **MENU** | Sale |

| Parámetro | Paso por pulsación |
|:----------|:------------------:|
| `KP` | ±0,05 |
| `KI` | ±0,05 |
| `KD` | ±0,05 |
| `VEL` | ±25 mm/s |

Pantalla durante la sintonía:

```text
PID >KP 0.25      ← el parámetro que mueven los botones (marcado con >)
KI 0.00 KD 0.00   ← las otras dos ganancias, SIEMPRE visibles
IZQ 148 DER 150
SET 150 D 42 43   ← consigna y duty de cada rueda
```

> **Las tres ganancias se enseñan a la vez**, no solo la seleccionada. Es por las órdenes de consola:
> al mandar `PID KI 0` el valor cambia, pero si la pantalla solo enseñara el parámetro marcado **no
> habría forma de darse cuenta** y parecería que la orden no ha entrado.

> Los botones están porque para que la sintonía valga el robot tiene que estar **en el suelo**, no sobre
> la mesa, y ahí el cable estorba. **Con el cable puesto manda la consola**, que además deja meter
> valores exactos (`PID KP 0.37`); los botones van a saltos de 0,05. Las dos vías hacen lo mismo.

### Sintonía por respuesta al escalón (2026-10-01)

> ⚠️ **Ziegler-Nichols NO sirve aquí, y está comprobado.** La velocidad de una rueda es un sistema de
> **primer orden**, y un lazo proporcional sobre un sistema de primer orden **no oscila nunca**, suba lo
> que suba `Kp`. El modo 3 (barrido automático buscando `Ku`) llegó a `Kp = 2,0` sin encontrarla. El
> método de la ganancia crítica necesita **fase**, y aquí la única que hay son los ~10 ms de la media
> móvil del encoder. El barrido automático se queda en el código como herramienta de diagnóstico, pero
> **no es el camino**: se sintoniza **por respuesta al escalón**.

Con el robot **en el aire y las ruedas libres**:

1. Poner `PID_TEST_MODO 1` (el modo sí es `#define`). En la prueba, dejar **solo la P**:
   `PID KP 0.10` · `PID KI 0` · `PID KD 0`. Comprobar que las ruedas **paran de verdad**: si siguen
   girando, el integrador se ha quedado con un valor viejo (pasó el 2026-10-01; ver
   [control-pid.md](./control-pid.md) §4).
2. **Subir `Kp`** (`PID KP 0.15`, `0.20`…) hasta que la velocidad deje de asentarse y **oscile de
   forma sostenida**. Ese valor ya es demasiado: el límite está en `Kp ≈ 0,2`.
3. Vuelto al `Kp` seguro (`0,10`), **subir `Ki`** desde 0 hasta que el error de régimen desaparezca
   sin que aparezca oscilación.
4. `Kd` se deja en **0** a propósito: la planta es de primer orden y el derivativo solo amplificaría
   el ruido de cuantificación del encoder (~7 mm/s).
5. **Grabar las ganancias en `PID_TEST_KP_DEF` / `KI_DEF` / `KD_DEF`** y recompilar. Las de la consola
   viven en **RAM y se pierden al apagar**. La prueba 20 y todo el lazo de `app/motion.c` toman las
   mismas (`APP_PID_GetGanancia()`), así que se sintoniza una vez y vale para todo.

**Ganancias medidas el 2026-10-01: `Kp = 0.10` · `Ki = 0.50` · `Kd = 0.00`**

| Medida | Valor |
|:-------|:------|
| Planta (duty → velocidad), ruedas libres | `K ≈ 15 mm/s` por 1 % de duty · `tau ≈ 45 ms` |
| Error de régimen | **0,5 %** (298,5 mm/s de mediana pidiendo 300) |
| Asentamiento tras un escalón | 0,6 s |
| Límite de `Kp` | `≈ 0,2` · con `0,3` el lazo **satura y oscila** (±60 % de duty, las dos ruedas desincronizadas) |
| Pico del escalón (~490 mm/s) | **Es el escalón, no el lazo**: `P + I` piden 40 % de duty de golpe. En uso real la rampa de 600 mm/s² lo evita |

> **Lo que limita `Kp` no es la planta, es el retardo del lazo** (~25 ms: la media móvil de 20 muestras
> del encoder más los 10 ms del lazo). Con `tau ≈ 45 ms` eso da `T/tau ≈ 0,6` y el margen se agota en
> `K·Kp ≈ 3`. Y como `Kp` no puede subir, **el error de régimen lo tiene que quitar el integrador**: con
> solo la P se quedaba en 180 mm/s pidiendo 300 (un 40 % de error).

> **Todo el barrido se hace con un solo flasheo y sin cable**: los botones cambian la ganancia con el
> lazo corriendo y el cambio se aplica en el paso siguiente (100 Hz). Cada cambio publica `>PID_KP`,
> `>PID_KI`, `>PID_KD` y `>PID_VEL`, así que el registro de Teleplot dice con qué valores se hizo cada
> tramo.

### Interpretación de la telemetría

| Observación | Significado |
|:------------|:------------|
| `>VEL_IZQ` pegado a `>PID_SET` con `>DUTY_IZQ` estable | Lazo cerrado funcionando ✅ |
| La velocidad oscila sin parar | `KP` demasiado alta (o `KD` amplificando ruido): bajar |
| Se queda corta y no llega nunca | Falta `KI`, o el duty está topado por `PID_TEST_DUTY_MAX` |
| `>VEL_*` negativa con `>DUTY_*` positiva | **Encoder invertido** respecto al motor: intercambiar A/B de ese encoder o invertir el signo del duty en la prueba |
| El robot se va de lado | Las ruedas no siguen la misma consigna: comparar `>VEL_IZQ` con `>VEL_DER` |
| `>DUTY_*` a tope y la velocidad no sube | El motor no puede con esa consigna (o patina): bajar `PID_TEST_VEL_MM_S` |

**Criterio (puerta de CONTROL)**: escalón de velocidad **sin sobreimpulso > 20 %**, error en
régimen permanente < 2 %, y con la misma consigna en las dos ruedas **el robot va recto**.

> ⚠️ La sintonía es de cada robot y de cada batería: si la tensión cambia mucho, las ganancias
> también. Se puede repetir cuando haga falta: el menú la tiene en **MOTORES → SINTONIA**.
---

## 15. Procedimiento: odometría (prueba 19) y calibración de la trocha

Primera prueba de la capa **FILTER**. El algoritmo y su justificación están en
[odometria.md](./odometria.md); la lógica ya está verificada en el PC con
`firmware/test/odometry_sim.c`.

**Qué hace**: integra los incrementos de distancia de las dos ruedas (µm → m) y publica la pose de la
odometría diferencial. **No mueve los motores: el robot se empuja a mano.**

### Telemetría

| Etiqueta | Unidad | Qué es |
|:---------|:------:|:-------|
| `>ODO_X` / `>ODO_Y` | mm | Posición respecto al origen (donde arrancó la prueba) |
| `>ODO_TH` | ° | Orientación (0 = el frente del robot al arrancar; positivo = giro a la izquierda) |
| `>ODO_D` | mm | Camino recorrido acumulado |
| `>ODO_UM_L` / `>ODO_UM_R` | µm | Total de cada rueda: **el diagnóstico**. Si se separan al empujar recto, hay un encoder invertido o perdiendo pulsos |
| `>ODO_TROCHA_MM` | mm | Trocha que está usando la odometría |
| `>ODO_HOLD` | 0/1 | 1 = pantalla congelada |

**Botones**: **OK** pone el origen a cero sin salir (para encadenar maniobras) · **UP** congela la
pantalla para apuntar la lectura · **MENU** sale.

A diferencia de casi todas las pruebas de banco, esta **sí repinta la pantalla** (a 5 Hz) y se puede
usar **sin Teleplot** —importante, porque con el robot en el suelo el cable estorba—:

| Fila | Qué enseña |
|:----:|:-----------|
| 0 | `ODOMETRIA` (o `CONGELADO` con el *hold* puesto) |
| 1 | `D` — camino recorrido en mm |
| 2 | `ANG` — ángulo en grados · `X` — posición en mm |
| 3 | `CIE` — **cierre**: distancia al origen en mm |

> La `Y` no se puede escribir en la OLED (la fuente de `app/oled.c` no tiene ese glifo), pero tampoco
> hace falta: lo que se mira al cerrar una maniobra es el **cierre**, que con X e Y sale de un solo
> número.

### Paso 5 — el cuadrado de 4 × 300 mm

Es la prueba que dice si los errores se **cancelan** o se **acumulan**: cuatro rectas y cuatro giros
encadenados. Se hace a mano (las primitivas de movimiento con motor todavía no existen).

1. Marca en el suelo un cuadrado de **300 mm de lado** con sus cuatro esquinas.
2. Robot en una esquina, alineado con el primer lado → **OK** (origen a cero).
3. Recorre los cuatro lados empujando recto y girando 90° a mano en cada esquina.
4. Al volver a la esquina de salida, **UP** para congelar y apunta la pantalla:

   | Dato | Esperado | Tolerancia |
   |:-----|:--------:|:----------:|
   | `D` | 1200 mm (4 × 300) | < 1 % |
   | `ANG` | 0 | < 5° |
   | `CIE` | 0 | **< 20 mm** |

> ⚠️ **Sé escéptico con el cierre a mano**: un error de alineación de 2–3° en cada esquina da 20–40 mm
> de cierre sin que la odometría tenga ninguna culpa. Un cierre **moderado y aleatorio** es el empujón;
> lo que delata un problema es un cierre que **crece en el mismo sentido** en cada vuelta.

> Es la **11.ª y última** entrada de `TEST → MOTORES`, y la lista enseña de 3 en 3: hay que bajar
> **8 veces** con `DOWN`. En la cabecera de la OLED se ve la posición (`TEST MOTORES i/11`).

### Paso 1 — comprobar que los encoders van en el mismo sentido

1. Robot en el suelo, elegir **TEST → MOTORES → `ODOMETRIA`** y anotar `>ODO_TROCHA_MM` (77,6 mm,
   el valor ya medido).
2. Empujar **en línea recta** 50 cm, despacio.
3. Mirar la telemetría:
   - `>ODO_UM_L` y `>ODO_UM_R` deben crecer **los dos** y parecerse (diferencia < 5 %).
   - `>ODO_TH` debe quedarse cerca de 0 (menos de 3°).
4. Si uno sube y el otro baja → **ese encoder está invertido**: intercambiar sus hilos A/B
   (ver [encoders.md](../firmware/sensor/encoders.md)).

### Paso 2 — distancia (µm por cuenta)

1. Marcar 1 m en el suelo, poner el origen a cero con **OK** y empujar hasta la marca.
2. `>ODO_D` debe dar 1000 mm ± 10 mm. Con más error, repetir antes la **prueba 7** (µm/cuenta);
   no tocar la trocha para arreglar un error de distancia.

> ✅ **Resultado (2026-10-01): 997 y 995 mm** (−0,3 % y −0,5 %). Confirma que el factor de
> **287,0 µm/cuenta** es el bueno y que la mesa **no** inflaba la medida: la duda queda cerrada.

### Paso 3 — giros

| Maniobra | Esperado | Tolerancia |
|:---------|:---------|:-----------|
| Giro de 90° a mano sobre el centro | `>ODO_TH` ≈ ±90 | < 2° |
| Giro de 360° sobre sí mismo (marca en el suelo) | `>ODO_TH` vuelve a 0 | < 5° |
| Cuadrado de 4 × 300 mm | vuelve al origen y `>ODO_TH` a 0 | cierre < 20 mm y < 5° |

> ✅ **Resultado antes de calibrar (2026-10-01)**: 90° a la izquierda → **+83°** (X 2 mm); 90° a la
> derecha → **−84°** (X 7 mm). Un **7 % corto y con el mismo sesgo en los dos sentidos** ⇒ el culpable
> era la trocha, no un encoder (un encoder invertido daría un signo distinto en cada sentido).
> La X pequeña confirma además que el giro fue casi puro, sobre el centro.
>
> ✅ **Resultado con la trocha nueva (77,6 mm)**: **+90° y −90°**, los dos clavados. El criterio de la
> prueba (< 2° de error en 90°) se cumple en los dos sentidos.

### Paso 4 — calibrar la trocha efectiva $L$

Si el robot **va recto** pero **gira de más o de menos**, el culpable es $L$. Con $\Delta\theta$ corto,
$L$ **baja**:

$$L_{nueva} = L_{usada} \cdot \frac{\theta_{medido}}{\theta_{real}} = 83{,}6 \times \frac{83{,}5}{90} = 77{,}6\ \text{mm}$$

Para afinarla con más precisión que un solo giro de 90°:

1. Marcar una referencia de orientación en el chasis y en el suelo.
2. Poner el origen a cero (**OK**) y girar el robot **5 vueltas exactas sobre sí mismo**, volviendo a
   la marca. A mano va bien: no hace falta rapidez.
3. Anotar los µm de cada rueda (`>ODO_UM_L` / `>ODO_UM_R`) y despejar:

   $$L = \frac{\Delta s_d - \Delta s_i}{5 \cdot 2\pi}$$

4. Escribir el valor en `ODOM_TROCHA_MM_DEF` (`firmware/filter/odometry.h`) y repetir el paso 3.

> ✅ **Hecho y confirmado el 2026-10-01** (en una sola iteración): `ODOM_TROCHA_MM_DEF` = **77,6 mm**
> (era 83,6) y con ese valor el giro de 90° a mano sale **clavado** en los dos sentidos. Ya no hace
> falta el método de las 5 vueltas para este robot; se conserva como procedimiento por si cambian las
> ruedas o el chasis.
>
> Con 5 vueltas, un error de alineación de 3° se queda en el 0,2 %: sobra precisión.
> La alternativa "girar 720° y leer el *yaw* de la IMU" hoy no sirve: `sensor/imu.c` solo da el
> giroscopio en crudo, sin ángulo fusionado.

### Resolución de problemas

| Síntoma | Causa probable |
|:--------|:---------------|
| `>ODO_X` no se mueve aunque el robot se mueva | La APP no está leyendo los encoders, o los encoders no cuentan (probar la **prueba 4**) |
| `>ODO_TH` se dispara empujando recto | Un encoder invertido o un cable suelto: comparar `>ODO_UM_L` con `>ODO_UM_R` |
| La recta sale bien y los giros no | Trocha mal medida: paso 4 |
| Todo se va al doble o a la mitad | µm por cuenta mal calibrados: **prueba 7** |

---

## 16. Procedimiento: primitivas de movimiento — celda y giro (prueba 20)

Es el banco de `app/motion.c`: las dos primitivas que van a usar el *wall follower* y el *flood fill*.
**Cada pulsación lanza un movimiento** y deja el resultado en pantalla, así que encadenando pulsaciones
se prueba lo que de verdad importa: que los errores **no se acumulen**.

### Botones

| Botón | Qué hace |
|:-----:|:---------|
| **OK** | Avanza `MOV_TEST_MM` (**180 mm**, una celda) |
| **UP** | Gira `MOV_TEST_GRADOS` (**+90°**, a la izquierda) |
| **DOWN** | Gira lo mismo a la derecha (−90°) |
| **MENU** | Sale |

### Pantalla

| Fila | Qué enseña |
|:----:|:-----------|
| 0 | `MOVIMIENTO` |
| 1 | `D` — distancia recorrida · `E` — **error acumulado** respecto a lo que debería llevar |
| 2 | `ANG` — ángulo girado · `E` — error acumulado |
| 3 | `OK:180 UP:90 DN:90` — el mapa de botones |

Telemetría: `>MOV_D`, `>MOV_ANG`, `>MOV_E_D`, `>MOV_E_ANG`, `>MOV_GYR`, `>MOV_GYROK` y `>MOV_OK`
(1 si el último llegó a tolerancia).

> ⚠️ **Los cinco valores se publican SOLO al terminar el movimiento**, no durante. Mientras el robot
> avanza (~2,2 s) la pantalla y la telemetría se quedan **congeladas** en el resultado anterior: es el
> precio de que las primitivas sean bloqueantes.

### El rumbo al avanzar lo corrige el **giroscopio** (2026-10-02)

Hasta el 2026-10-02 la corrección de rumbo usaba la **odometría**: la diferencia de arcos de las dos
ruedas. El problema no era la medida, sino que **el error se heredaba**: cada celda arrancaba con la
referencia puesta en el rumbo que el robot tuviera en ese instante, así que lo que un tramo no
corregía lo arrastraba el siguiente y el desvío **crecía** en vez de apagarse.

Medido el 2026-10-02 en el pasillo, con `>WF_DEC` = avanzar en todas las celdas:

| `>WF_CELDAS` | 0 | 1 | 2 | 3 | 3 (parado) |
|:-------------|:--|:--|:--|:--|:--|
| `>WF_ANG` | 44,3 | 30,3 | 26,3 | 29,9 | **41,1** |

Ahora la referencia la da el **giroscopio** (`>MOV_GYR`), que mide el giro **de verdad** —no lo deduce
de las ruedas— y se pone a **cero al empezar cada avance**. El convenio de signo es el mismo que el de
$\theta$ (positivo = izquierda), así que el resto del lazo no cambia.

| | Odometría (respaldo) | Giroscopio |
|:--|:--|:--|
| De dónde sale el giro | Diferencia de arcos de las ruedas | Sensor inercial |
| Constante de tiempo del lazo | $\tau = L/(2K_p)$ = **12,9 s** | **3,2 s** |
| Lo que corrige en una celda de 1 s | 7 % del desvío | el **27 %** |
| Empieza a saturar a partir de | — | **5°** de error |
| `MOTION_RUMBO_KP` | 3 | — |
| `MOTION_GYRO_KP` | — | 40 |

#### Lo medido el 2026-10-02 (robot en el suelo, `>MOV_GYR` como testigo)

| Movimiento | Odometría | Giroscopio | Veredicto |
|:-----------|:---------:|:----------:|:----------|
| **Avance** de 180 mm | **+2,3°** | **−0,1°** | ✅ **va recto** (los dos coinciden) |
| **Giro** de 90° | **+125,4°** | **+87,1°** | ⚠️ la odometría **sobreestima un 44 %** |

De ahí salen las dos conclusiones que gobiernan el diseño del lazo:

1. **En el avance la odometría es buena**, y el giroscopio la confirma. La deriva de **+2,3° en 180 mm**
   (1,3 %) está cerca del **+0,2°** medido el 2026-10-01 con el corrector viejo ⇒ **el zigzag del pasillo
   no era el robot**: era el corrector persiguiendo una medida que no era la del giro.
2. **En el pivote la odometría sobreestima un 44 %.** Girar sobre el centro obliga a las ruedas a
   **deslizar lateralmente** para pivotar, así que cuentan más arco del que el robot gira de verdad. Con
   la odometría como referencia, `MOTION_Girar(90)` corta cuando marca 90° y el robot se queda en
   **62°**: un **30 % menos** en cada esquina, y eso el *wall follower* lo acumula.

> ⛔ **La tentación de arreglarlo tocando la trocha no vale**: el factor 1,44 es del **pivote**, y en el
> avance la odometría es correcta. Un solo parámetro no puede servir para los dos. La referencia tiene
> que ser el sensor que **no depende de las ruedas**.
>
> ⇒ Desde el 2026-10-02 **el ángulo del giro también se cierra con el giroscopio** (`MOTION_Girar()`),
> no solo el rumbo del avance. Si la IMU no responde, el giro vuelve al respaldo por odometría.

#### El zigzag: por qué la ganancia se bajó de 40 a 12

Con $K_p = 40$ y tope de 60 mm/s, la corrección **saturaba con solo 1,5° de error**:

$$e_{sat} = \frac{60\ \text{mm/s}}{40\ \text{mm/s por grado}} = 1{,}5°$$

Y un lazo saturado no es proporcional: es un **lazo de relé**, y un relé con retardo **oscila
siempre**. El retardo lo pone la inercia del robot (tarda unos 0,2 s en cambiar de velocidad angular),
que da una amplitud de oscilación de $\dot\theta_{max} \times T \approx 88°/\text{s} \times 0{,}25\ \text{s}$
≈ **20°** — mucho más que la propia corrección que se quiere hacer.

Medido en el laberinto el 2026-10-02: el robot **hacía zigzag** (lo vio el usuario) y el desvío **crecía
celda a celda** (1,5 → 3,4 → 11,4°), que es divergencia y no un desvío constante.

> ✅ **El criterio no es «que corrija rápido», es «que no sature»**: $e_{sat} = \text{tope}/K_p$ tiene
> que quedar **por encima del desvío que se ve dentro de una celda**. Con $K_p = 12$ satura a 5° y el
> lazo trabaja en su tramo lineal, que es el único que amortigua.
>
> ⚠️ **Subir el tope no es la solución**: agranda el tramo saturado y la amplitud del zigzag. Si hay
> que corregir desvíos mayores, se **baja la ganancia**.

#### El sesgo del giroscopio: el fallo del 2026-10-02

El **sesgo** es la lectura del giroscopio con el robot quieto, que debería ser cero y no lo es. Se mide
solo, promediando 50 muestras al **entrar** en las pruebas 20 y 22. **Si en ese medio segundo están
moviendo el robot, el sesgo sale falseado** en decenas de °/s, y el lazo integra ese error **todo el
rato**:

- El integrador crece solo ⇒ el lazo cree que el robot se desvía ⇒ **corrige a tope (±60 mm/s) sin parar**.
- Medido: el robot **chocó contra la pared antes de cruzar la primera celda** y el giroscopio acumuló
  **272° y luego 257° más** mientras el robot estaba **parado, empujando** (lo vio el usuario).

⇒ Dos protecciones, desde el 2026-10-02:

| | Qué hace |
|:--|:--|
| **Tanda con dispersión** | La calibración mira la **varianza** de las 50 muestras: si supera 2 °/s es que están moviendo el robot ⇒ se tira la tanda y se repite (hasta 3 veces) |
| **Refinamiento en el asentamiento** | Al final de cada movimiento el robot está **frenado**, y ese es el único momento en que se sabe que no gira: se aprovecha el bucle que ya existe para volver a medir el sesgo y corregirlo con un filtro lento |

> 📌 **Y por eso el arreglo se autocura**: aunque la calibración inicial salga mal, el sesgo converge
> en los primeros movimientos y el robot se endereza solo.
>
> ⚠️ **Al entrar en la prueba 20 o 22, deja el robot quieto dos segundos.** Si lo estás colocando, la
> calibración lo detecta y reintenta; si los tres intentos salen sucios, el primer movimiento irá
> torcido y el segundo ya no.

> ⚠️ **Y el refinamiento tiene su propia trampa, aprendida el mismo día**: las muestras hay que tomarlas
> en la **segunda mitad** del asentamiento. Al frenar de golpe el robot **todavía gira por inercia**
> unos milisegundos, y una muestra tomada ahí se lleva el giro del movimiento al sesgo. Medido: con las
> muestras desde el primer milisegundo el sesgo quedó en **16 °/s** y el giroscopio **dejó de ver la
> media vuelta** (marcó 3,5° donde el robot giró 146°) ⇒ como el giro se cierra con el giroscopio, el
> lazo siguió pidiendo girar hasta agotar los 8 s y el robot acabó **parado empujando contra la pared**
> (`>WF_ATASCO:1`). De ahí, además, el **tope de paso** de cada refinamiento: 5 °/s, para que una
> medida suelta no pueda darle la vuelta al sesgo.
>
> ✅ **Y ahora se puede VER**: `>MOV_SESGO` y `>WF_SESGO` publican el sesgo en uso. **Por encima de
> 1 °/s con el robot quieto es un fallo**, no una curiosidad.
>
> 📌 **La firma que no engaña**: si el robot está **parado empujando** contra una pared y el giroscopio
> **acumula grados**, el sesgo está mal. Un giroscopio sano con el robot quieto integra **cero**.

#### El sesgo tiene **deriva térmica** (medido el 2026-10-02)

| Momento | `>WF_SESGO` |
|:--------|:-----------:|
| Robot recién encendido (chip frío) | **−6,5 °/s** |
| Dos movimientos después | −2,4 °/s |
| Ya caliente | **≈−1 °/s** |

⇒ **Calibrar una sola vez al entrar no puede funcionar**: el sesgo **cambia con la temperatura**. De ahí
las dos decisiones de esa noche:

1. **El refinamiento no filtra**: toma la muestra del asentamiento (robot frenado, y solo la segunda
   mitad, cuando ya no queda inercia) **tal cual**. Un filtro lento iría **persiguiendo** un blanco que
   se mueve.
2. **Con el sesgo fuera de rango, el lazo NO usa el giroscopio.** `MOTION_GYRO_SESGO_MAX_USABLE` = **3
   °/s**: por encima manda el **respaldo por odometría** —giros cortos, pero el robot **no se atasca**— y
   en cuanto el sesgo se sanea, el avance y los giros pasan al giroscopio solos, sin intervención.

> ⚠️ **Un sesgo de 6,5 °/s son 13° de desvío falso por celda** (2 s), y el lazo los convierte en
> corrección **real**: el robot se va de lado hasta chocar. **Un giroscopio mal calibrado es peor que no
> tenerlo**, y por eso el umbral de uso no es un lujo.
>
> 📌 **Y es un fallo que solo se ve en el número**: `>WF_SESGO` lo publica. Sin esa etiqueta, esto se
> habría diagnosticado (otra vez) como «el robot se va torcido», que es lo que se le achaca al
> rozamiento.
>
> ✅ **Comprobación rápida**: en un **avance recto** el giroscopio tiene que quedarse **cerca de 0**
> (`>MOV_GYR` ≈ `>MOV_ANG` ≈ 0). Si acumula decenas o cientos de grados con el robot yendo recto, el
> sesgo está mal y hay que **volver a entrar en la prueba con el robot quieto**.

#### ✅ La medida que zanjó el asunto: cuatro giros de 90 (2026-10-02)

Maniobra **`MOV UP 4`** (cuatro giros de 90° hacia el mismo lado) sobre la mesa, con el **giroscopio**
cerrando el lazo y el **corrector de centro desactivado**:

| Giro | `>MOV_GYR` (acumulado) | **Incremento** | `>MOV_ANG` (odometría) | Arrastre |
|:----:|:----------------------:|:--------------:|:----------------------:|:--------:|
| 1 | 91,3° | **+91,3°** | +94,5° | 21 mm |
| 2 | 181,0° | **+89,8°** | −132,6° | 27 mm |
| 3 | 269,7° | **+88,6°** | −30,5° | 43 mm |
| 4 | **359,0°** | **+89,3°** | +86,7° | 52 mm |

**Cuatro giros de ~90° y el robot vuelve a mirar donde miraba: 359° de los 360 pedidos.** Y el **camino
parásito total cae a 52 mm**, cuando con el corrector de centro a 8 eran **143 mm por giro**.

> ✅ **De aquí salen las tres decisiones finales**: **(1)** el **giroscopio** es la referencia del giro;
> **(2)** el **corrector de centro se desactiva** —se realimentaba con la odometría del pivote, que
> miente, y **arrastraba al robot** en vez de centrarlo—; **(3)** el **umbral de sesgo sube a 10 °/s**,
> porque con ±7 el giroscopio seguía acertando (ver la tabla `+90 −90` de abajo: reprodujo un giro a la
> derecha que la odometría daba como izquierda).

> ⚠️ **La tabla de abajo está SUPERADA por esta medida**, y se conserva por el **error de método** que
> contiene: sus conclusiones salen de usar **el giroscopio como testigo** para juzgar al giroscopio.
> Fue el usuario, mirando el robot, quien dijo «ese giro no es de 90» y quien destapó que de los dos
> sensores **el que miente es la odometría**, no el gyro. 📌 **Un sensor no se juzga con otro sensor del
> mismo robot: se juzga con una marca en el suelo.**

#### ✅ El error de **escala** del giroscopio (2026-10-02)

Con la escala del datasheet (16,4 LSB por °/s, rango de 2000 °/s) el integrador se quedaba **corto un
25 %**. Medido con **cuatro giros de 90° hacia el mismo lado** (`MOV UP 4` = 360 pedidos):

| Escala que llevaba | Giro real (4 giros) | Escala que sale |
|:------------------:|:-------------------:|:---------------:|
| 1,00 | ~450° | 1,25 |
| 1,25 | ~405° («se ha pasado unos 45») | **1,40** |

Se toma **1,40**, que es la medida hecha con más atención. Y queda **verificado** con los mismos cuatro
giros:

| Giro | Incremento medido | Arrastre |
|:----:|:-----------------:|:--------:|
| 1 | +90,83° | 15 mm |
| 2 | +89,06° | 19 mm |
| 3 | +89,33° | 41 mm |
| 4 | +91,24° | 54 mm |
| **Total** | **360,46°** (pedidos 360) | **54 mm** |

⇒ **0,5° de error en una vuelta completa**, y el **sesgo se queda en 0,1 °/s** (venía de ±12: al no
arrastrarse el robot durante el giro, el asentamiento mide el sesgo de verdad).

> ⚠️ **Es un error de ESCALA, no de sesgo, y el módulo es el culpable**: el IC va marcado
> `MP65888LB1`, que no corresponde a ningún InvenSense — es un **clon**, y su sensibilidad real no tiene
> por qué ser la del datasheet. Por eso la constante está medida y no copiada.
>
> 📌 **Y la lección de método**: un error de escala **no se ve en un giro suelto** — 90° se quedan en 65
> y parece «un giro que se ha quedado corto», que es exactamente lo que se achaca al rozamiento. Lo
> delata **pedir varias vueltas**: el error se multiplica y sale a la vista. Para calibrar una escala hay
> que medir **mucho**, no fino.
>
> 📌 **Y la otra**: la escala se estimó con **dos pasadas** (1,25 y 1,40) porque la primera medida salió
> de una apreciación a ojo. Con el segundo giro, hecho prestando atención, el número cuadró al 0,1 %.

#### ⚠️ Superada: la comparación que se hizo con el testigo equivocado (2026-10-02)

Maniobra **`+90° −90°`** sobre la mesa, que es la que hay que usar para juzgar un giro: **duplica el
error** y tiene que devolver el robot **al mismo sitio y mirando a lo mismo**. Se repite con cada
referencia **sin reflashear**, con la orden de consola `GYRO ODOM|GIRO|FUSION`.

Los giros «reales» los mide el **giroscopio**, que aquí se tomó por testigo independiente — **y eso fue
el error**: se estaba usando el sensor sospechoso para juzgar a los demás.

| Referencia | Giro +90 (real) | Giro −90 (real) | **Cierre orientación** | Cierre posición | Camino parásito | Duración |
|:-----------|:---------------:|:---------------:|:----------------------:|:---------------:|:---------------:|:--------:|
| Odometría | **+89,5°** | **−93,9°** | **−4,4°** | **11,5 mm** | **53 mm** | **0,72 s** |
| Giroscopio | +86,7° | −90,9° | −4,2° | 13,3 mm | 216 mm | 1,80 s |
| Fusión | +89,4° | −104,1° | **−14,7°** | 17,3 mm | 247 mm | 1,55 s |

Lo único que **sí** quedó claro en esta tanda, y sigue valiendo: **la fusión es la peor** (promediar un
error con otro no reparte el fallo), y **el corrector de centro arrastra al robot** de forma
proporcional a lo que dura el giro. El resto de conclusiones (que «ganaba la odometría») **eran
falsas**.

#### El giro en el laberinto: el corrector de centro necesitaba tope (2026-10-02)

Con el sesgo ya arreglado, el robot **cruzó tres celdas rectas seguidas** —**179,3 · 180,2 · 177,4 mm**
con `>WF_GYR` en **−2,4 · −0,9 · −0,6°**— y **detectó la pared** de la celda 4. El atasco se había
mudado al **giro**: en la celda sin salida el robot se quedaba empujando contra la pared
(`>WF_ATASCO:1`). Dos causas, las dos del giro:

| | Qué pasaba | Arreglo |
|:--|:--|:--|
| **Corrector de centro sin tope** | `centro_mm_s = -MOTION_GIRO_CENTRO_KP × desplazamiento_mm` **no tenía recorte**. Es un término **común** a las dos ruedas, así que no toca la diferencia —que es lo que gira— pero **mueve el robot**. Y girando **rozando las paredes** la odometría cuenta desplazamiento que no es real: con 100 mm de cuenta falsa y $K_p = 8$ pide **800 mm/s a las dos ruedas** ⇒ el robot **sale disparado contra la pared** en vez de pivotar | Tope **`MOTION_GIRO_CENTRO_MAX_MM_S` = 60 mm/s** (24 % del crucero): el corrector sigue quitando el desplazamiento de verdad, que son milímetros |
| **Cinco giros por decisión** | El robot comprobaba la izquierda **girando**: girar a la mano, deshacer, girar al otro lado, medir y volver ⇒ hasta **cinco** giros de 90° en una celda sin salida, y en el laberinto **cada giro roza** (el chasis barre ~147 mm en una celda de 180) | Orden nuevo: **frente → mano → atrás**, que resuelve un fondo de saco con **dos** giros |

> ⚠️ **El precio del segundo cambio**: un cruce en «T» hacia la izquierda se pasa de largo, porque el
> robot ya no mira a ese lado. Se cambia **explorar más** por **atascarse menos**, que es lo que hace
> falta para el objetivo de «moverse sin chocar».

#### Verificar el giroscopio (1 minuto, **desde la prueba 20**)

`MOTION_GYRO_SIGNO_Z` **no se puede deducir del código**: depende de cómo esté montado el módulo. En
este robot **ya está comprobado** el 2026-10-02 —`MOV UP` gira a la izquierda y `>MOV_GYR` **sube**, así
que el valor bueno es `1.0f`—. Si algún día se remonta la IMU, se vuelve a medir así:

> ⚠️ **No se mide con la prueba 5**: esa prueba está **fuera del menú** desde el recorte de Flash del
> 2026-10-02. Se mide con la **20**, que sí está y ya publica el ángulo del giroscopio.

1. **TEST → MOTORES → MOVIMIENTO** (`>MOV_GYROK` tiene que salir **1**; si sale **0**, la IMU no
   responde y el giro va con la odometría).
2. **UP** ⇒ el robot gira **90° a la izquierda** y publica el grupo `MOV_*`.
3. Comparar los dos ángulos:

| Giro a la **izquierda** | `>MOV_ANG` (odo) | `>MOV_GYR` (giro) | Veredicto |
|:------------------------|:----------------:|:-----------------:|:----------|
| | sube | **sube** | ✅ signo correcto (`1.0f`) |
| | sube | **baja** | ⚠️ invertido ⇒ `MOTION_GYRO_SIGNO_Z = -1.0f` |
| | sube | ≈ **0** o disparatado | El módulo mide otro eje, o la IMU no responde: mirar `>MOV_GYROK` |

> ✅ **Este es el orden seguro: girar antes de avanzar.** Una medida equivocada no estropea el giro (el
> giroscopio solo se mira al principio); lo que estropea es el **avance siguiente**, donde el lazo se
> realimenta y el robot se va en curva clara. Con el signo al revés, `correccion` se queda pegada al tope
> y el desvío **crece** en vez de apagarse.

### ⚠️ Por qué `ANG` da 90 → 180 → −90 → 0

La odometría normaliza $\theta$ al rango $(-180, 180]$, así que girando **siempre a la izquierda** el
ángulo no llega a 360°: pasa por 90, 180 y luego **salta a −90**. Es correcto, no es un fallo.

El **error** `E`, en cambio, se calcula como una *diferencia* de ángulos y **también hay que
normalizarlo** (arreglado el 2026-10-01): antes, al tercer giro, `MOV_E_ANG` salía **−360** aunque el
robot estuviera perfecto, y parecía un suspenso. Si el error sale cerca de ±360, mira `ANG` antes de
creerte el número: el giro en sí no se ve afectado (`MOTION_Girar()` ya normaliza su objetivo).

### Cuánto tarda, y cómo se aborta

| | |
|:--|:--|
| Una celda de 180 mm | ≈ **2,2 s** · el pico de velocidad es de ~210 mm/s, **no** los 250 de `MOTION_VEL_MM_S`: como la velocidad se pide proporcional a lo que falta, acelera y frena dentro del mismo movimiento |
| Un giro de 90° | ≈ **1,2 s** |
| Tiempo máximo de un movimiento | **8 s** (`MOTION_TIMEOUT_MS`); al agotarse para y devuelve `>MOV_OK:0` |
| Abortar | **MENU** en cualquier momento (se consulta en cada paso del lazo) |

> Va **despacio a propósito**: de estas dos primitivas no se pide velocidad, sino **repetibilidad**. Si
> el robot tiene que correr más en el laberinto, se suben `MOTION_VEL_MM_S` y `MOTION_AVANCE_KP`.

### También se puede mandar por consola

Los tres botones tienen su orden equivalente, con un contador para encadenar movimientos **sin tener
que estar delante**:

```
MOV OK       una celda de 180 mm
MOV OK 4     cuatro celdas seguidas
MOV UP 4     360° a la izquierda
MOV DOWN     90° a la derecha
```

> ⚠️ **Solo dentro de la prueba 20** (es la única que lee la consola): en otro sitio responde
> `>CMD_ERR:3`. Y se atiende **entre** movimientos, así que conviene esperar al resultado antes de
> mandar la siguiente orden.

> **Las dos vías llaman al mismo `mov_test_ejecutar()`**, así que miden exactamente lo mismo. Eso no es
> un detalle de estilo: con dos caminos separados, lo que se mide por consola no sería comparable con
> lo que se mide con el dedo, y la sintonía dejaría de servir para las dos.

### Cómo se hace

1. Robot en el suelo, con **180 mm** marcados por delante y una referencia de orientación.
2. **OK** → el robot avanza una celda. Anota `E`.
3. Otra vez **OK**, hasta cuatro veces. El error acumulado debe quedarse **por debajo de 10 mm**; lo que
   **no** vale es que crezca 5 mm en cada celda: eso es un sesgo del µm por cuenta, no del lazo.
4. **UP** cuatro veces: `ANG` debe llegar a 360° con el error acumulado **por debajo de 5°**.
5. **UP** y **DOWN** alternos: el error debe volver cerca de 0, no derivar siempre en el mismo sentido.
6. **Mira `D` durante un giro: no debe subir.** `>MOV_D` acumula $|ds_{centro}|$, y en un giro sobre el
   centro los dos arcos son **iguales y opuestos** ($ds_{centro} = 0$). Si `D` crece mientras gira, el
   giro **no es simétrico**: una rueda recorre más que la otra. Es el síntoma que apareció al empujar
   el cuadrado a mano (+31, +67, +36 y +45 mm por giro), y aquí se puede medir con motores.

**Criterios** (los de [pruebas-pendientes.md](./pruebas-pendientes.md)): celda de 180 mm con error
**< 5 mm** y giro de 90° con error **< 2°**.

> ⚠️ El robot se mueve: **en el suelo**, con espacio, y **con la batería** (no con la fuente de
> laboratorio, cuyo lazo de control se hunde en los escalones de corriente).

### Ajuste de las constantes

Los números que se tocan en banco están todos en `app/motion.h`:

| Constante | Por defecto | Qué cambia |
|:----------|:-----------:|:-----------|
| `MOTION_VEL_MM_S` | 250 | Velocidad de crucero [mm/s] |
| `MOTION_TOL_MM` / `MOTION_TOL_GRADOS` | 3 / 1,5 | Cuándo se da por terminado el movimiento |
| `MOTION_AVANCE_KP` | 1,5 | Freno proporcional al avanzar |
| `MOTION_GIRO_KP` | 4 | Rapidez del lazo de ángulo |
| `MOTION_GIRO_CENTRO_KP` | 8 | Cuánto corrige el **desplazamiento** del giro [mm/s por mm] |
| `MOTION_GIRO_CENTRO_MAX_MM_S` | 60 | **Tope** de esa corrección. ⚠️ Sin él el giro se pierde: girando rozando las paredes la odometría cuenta desplazamiento falso y el corrector pide cientos de mm/s a las dos ruedas |
| `MOTION_GIRO_VEL_MIN_MM_S` | 40 | **Mínimo para vencer la fricción estática**: si el robot se queda a medio giro, es esto |
| `MOTION_RUMBO_KP` | 3 | Cuánto corrige el desvío al avanzar recto **con la odometría** (solo si no hay IMU) |
| `MOTION_GYRO_KP` | 12 | Cuánto corrige el desvío al avanzar recto **con el giroscopio**. ⚠️ El valor está puesto para que el lazo **no sature** (ver «El zigzag»), no por rapidez |
| `MOTION_GYRO_CORR_MAX_MM_S` | 60 | Tope de esa corrección [mm/s]: al 24 % de la velocidad de crucero. Con 12, satura a partir de **5°** |
| `MOTION_GYRO_SIGNO_Z` | 1 | Signo del eje Z — **medido**, ver «Verificar el giroscopio» |
| `MOTION_GYRO_REFINAR_MAX_PASO` | 5 | Cuánto puede mover el sesgo un solo refinamiento [°/s]: es el tope que impide que una medida contaminada le dé la vuelta |

Las **ganancias del lazo de velocidad** no están ahí: se cogen de la prueba 18 (`APP_PID_GetGanancia()`),
que se sintoniza en caliente por consola. Se sintoniza una vez y vale para todo.

### Resolución de problemas

| Síntoma | Causa probable |
|:--------|:---------------|
| El robot se queda a medio giro, parado, sin llegar | `MOTION_GIRO_VEL_MIN_MM_S` demasiado bajo: el duty no vence la fricción. Subirlo |
| El giro oscila alrededor del objetivo | `MOTION_GIRO_KP` alto, o velocidad de crucero alta: bajar alguno de los dos |
| Se pasa siempre en el mismo sentido al frenar | Es la inercia: `MOTION_TOL_GRADOS` demasiado estrecha para esa velocidad |
| Al avanzar se va de lado | Si `>MOV_GYROK` es **1**, el problema **no** es el lazo: mira si el robot roza, si una rueda arrastra o si los encoders no son simétricos (prueba 7). Si es **0**, la IMU no responde (prueba 5) y manda el respaldo por odometría, que **arrastra** el error (subir `MOTION_RUMBO_KP` no lo arregla) |
| `>MOV_GYR` se separa de `>MOV_ANG` | Los dos miden el mismo giro y **en el avance coinciden** (02-oct: +2,3° frente a −0,1°). Si se separan mucho **en el avance**, la odometría está mal (µm por cuenta, prueba 7; trocha, prueba 19). En un **giro** se separan a propósito: la odometría sobreestima el pivote ~44 % y ahora manda el giroscopio |
| El robot gira **menos** de 90° | Si `>MOV_GYROK` es **1** ya no puede ser el lazo: mira si el robot **derrapa** al pivotar, y comprueba el **dato real** con una marca en el suelo. Con **0**, manda la odometría, que sobreestima el pivote un 44 % (medido el 02-oct) |
| El robot se va en curva **clara** al avanzar | `MOTION_GYRO_SIGNO_Z` al revés (ver «Verificar el giroscopio») |
| `>MOV_GYR`/`>WF_GYR` acumula **cientos de grados** y el robot **no gira** | **El sesgo del giroscopio está mal medido** (se calibró con el robot moviéndose): el lazo integra ese error y corrige a tope sin parar ⇒ el robot choca de lado. Volver a entrar en la prueba **con el robot quieto** |
| El robot hace **zigzag** (va recto pero temblando) | **El lazo está saturado**: con $K_p$ alto la corrección llega al tope con un desvío mínimo y el lazo se comporta como un **relé**, que oscila siempre por el retardo. **Bajar `MOTION_GYRO_KP`** (o mirar `e_sat = tope/K_p`: tiene que quedar por encima del desvío que se ve en una celda) |
| El error de distancia **crece** en cada celda | No es el lazo: es el **µm por cuenta** (prueba 7) |

---

## 17. Procedimiento: detección de paredes y ajuste del umbral (prueba 21)

Es la prueba que se hace **en el laberinto, no en casa**. `app/walls.c` convierte la medida de los
receptores en «hay pared / no hay pared», y su umbral depende de cómo reflejen **esas** paredes.

### Botones y pantalla

| Botón | Qué hace |
|:-----:|:---------|
| **OK** | Cambia de pared (FRENTE → DERECHA → IZQUIERDA) |
| **UP / DOWN** | Sube o baja el umbral de la pared marcada con `>` (±20) |
| **MENU** | Sale |

```text
PAREDES U 200      ← umbral de la pared seleccionada
>FRENTE 1200 1     ← medida y veredicto (1 = hay pared)
 DERECHA 98 0
 IZQUIERDA 15 0
```

Telemetría: `>WALL_n` (medida), `>WALL_n_UMB` (umbral) y `>WALL_n_HAY` (0/1), con `n` = 0 **frente**,
1 **derecha** y 2 **izquierda**.

### La medida no depende de la luz de la sala

Lo que se mide es **`encendido − apagado`**: se lee el receptor con su emisor apagado y encendido, y se
resta. La luz ambiente cae en los dos términos y se va en la resta. Por eso el mismo umbral vale de día
y de noche.

### Valores medidos en el laberinto (2026-10-02)

Robot en el pasillo, prueba 21, con la pared delantera puesta y **sin nada a los lados**:

| Canal | Con pared delante | Sin pared (fondo) | Umbral puesto |
|:------|:-----------------:|:-----------------:|:-------------:|
| **FRENTE** (R0) | **149** | **0** | **75** |
| DERECHA (R1) | — | 105 | 2050 |
| IZQUIERDA (R2) | — | −3 | 900 |

El frontal queda a mitad de camino, con **2× de margen por cada lado**, y la histéresis de 60 cabe
justa (enciende por encima de 105, apaga por debajo de 45).

> ⚠️ **El `−3` de la izquierda es la clave de lectura de la tabla**: la medida es una **resta**
> (`encendido − apagado`), y salir negativa quiere decir que ahí **no hay señal**, solo ruido. Los dos
> laterales miden eso **cuando no hay nada al lado**: no es que estén mal, es que **apuntan hacia
> delante** y no ven las paredes laterales. Por eso el *wall follower* decide **solo con el frontal**.

### Cómo se ajusta (2 minutos)

1. Robot **quieto**, en el sitio donde va a decidir, con las paredes del laberinto puestas.
2. Mira `FRENTE` **con pared delante** y **sin pared delante**. Apunta los dos números.
3. Pon el umbral **en medio**, más cerca del valor «sin pared».
4. Repite con `DERECHA` y `IZQUIERDA`, que son los que mantienen el robot pegado.
5. Comprueba el **parpadeo**: si el veredicto baila entre 0 y 1, aleja el umbral del valor de reposo.

> El umbral va con **histéresis** (`WALLS_HISTERESIS`, 60 cuentas): una pared ya encendida aguanta
> hasta 60 cuentas por debajo del umbral, y una apagada necesita superarlo por 60. Sin eso, una medida
> que oscila alrededor del corte enciende y apaga la pared en pasos consecutivos, y la FSM da tirones.

> ⚠️ **Los umbrales no se guardan en Flash**: si un ajuste te vale para siempre, hay que poner el valor
> en su `#define` de `app/walls.h` — **uno por pared**: `WALLS_UMBRAL_FRENTE_DEF`,
> `WALLS_UMBRAL_DERECHA_DEF` y `WALLS_UMBRAL_IZQUIERDA_DEF`. **No hay un umbral común, y no es un
> capricho**: el fondo de cada canal es muy distinto, y con uno solo bajo las tres paredes leerían
> «hay» siempre.
>
> ⛔ **Y con los umbrales de fábrica altos el robot es CIEGO**: medido el 2026-10-02, el frontal daba
> **149** con pared y **0** sin ella, y su umbral de arranque era **500** ⇒ `WALLS_Hay()` devolvía 0
> **siempre** y el *wall follower* avanzó derecho **hasta chocar** (16: `>WF_FRENTE:0` en todas las
> muestras). Si el robot no ve nada, **mirar primero el umbral**, no el sensor.

### Criterios

| Comprobación | Esperado |
|:-------------|:---------|
| Con pared delante y el umbral ajustado | `FRENTE` = 1 |
| Sin pared delante (más de 30 cm) | `FRENTE` = 0 |
| ⚠️ Robot centrado en un pasillo de 18 cm | **Los laterales dan 0**: este robot no ve paredes laterales. **No es un fallo que hay que arreglar aquí**, es la geometría de los receptores (apuntan hacia delante) |
| Robot quieto 30 s | el veredicto **no parpadea** |

---

## 18. Procedimiento: módulo ESP8266 por AT (prueba 23)

Comprueba que el módulo está **vivo y bien cableado**, y de paso barre las redes WiFi de alrededor. Es
el primer paso para **quitar el cable**: la telemetría y la consola van por la **USART3 (PB10/PB11)**, y
el ESP se pone **en su lugar**, así que mientras está puesto no hay cable al PC.

> ⚠️ **Es la única prueba que NO publica telemetría**, y es a propósito: si publicara, el ESP recibiría
> los `>ETIQUETA:valor` como si fueran comandos y la consola se comería las respuestas del módulo. El
> resultado sale **en la pantalla**.

### Cableado

| ESP8266 | STM32 | Nota |
|:--------|:------|:-----|
| TX | **PB11** (RX del STM32) | ⚠️ **cruzado**: es el fallo número uno |
| RX | **PB10** (TX del STM32) | |
| GND | GND | **imprescindible**, y el primero que se olvida |
| VCC | 3,3 V | el ESP8266 **no** es de 5 V, y pide picos de ~300 mA |

Los dos son de 3,3 V, así que van directos. Y el **cable USB-serie hay que quitarlo**: si se queda
puesto, el adaptador y el ESP escriben los dos en la misma línea del STM32.

### Qué hace

1. `AT` — lo mínimo: que conteste.
2. `AT+CWMODE=1` — modo **estación**, que es lo que hace falta para poder barrer redes. En modo punto
   de acceso el `AT+CWLAP` no devuelve nada.
3. `AT+CWLAP` — barre y enseña las redes con su potencia.

### Pantalla

| Fila | Qué enseña |
|:----:|:-----------|
| 0-1 | El **veredicto** a doble tamaño: **`CONECTADO`** o **`SIN ESP`** |
| 0-1 | Mientras trabaja: `ESP8266` y en qué punto va (`PULSA OK`, `MANDO AT`, `MODO ESTACION`, `BUSCANDO REDES`) |
| 2 | Con el módulo vivo, **cuántas redes** ha visto (`REDES 12`) · si no contesta, **lo que haya llegado** |
| 3 | `OK:OTRA VEZ` |

> **Se pide un sí o un no, no un listado.** Lo que se viene a saber es si el módulo contesta, y para eso
> un veredicto claro vale más que una lista de redes. De los SSID solo se guarda **cuántos hay**: dice
> que la radio funciona, y ahorra 800 B de RAM que la fuente de la OLED no podría enseñar de todas
> formas (no tiene minúsculas ni `H`, `J`, `Q`, `Y`, `Z`). El nombre y la contraseña de la red se
> teclean a mano cuando toque.

### Si no contesta

| Síntoma | Qué mirar |
|:--------|:----------|
| **Los botones dejan de responder y la pantalla se queda congelada** | ⚠️ Es un **cuelgue**, no unos botones rotos: mientras el módulo hable sin parar, el firmware se queda vaciando el anillo de recepción y no vuelve a mirar los botones. **Arreglado el 2026-10-01** acotando ese vaciado (`ESP_DRENA_MS`). Si vuelve a pasar, **reinicia la placa** |
| `SIN ESP` y en la fila 2 pone `NADA` | No ha llegado **ni un byte**: TX/RX sin cruzar, GND sin conectar, o el módulo sin alimentar |
| `SIN ESP` y en la fila 2 pone **`RUIDO`** | El módulo habla **sin parar** y no hay ningún `OK` dentro: casi siempre **bucle de arranque por alimentación** —el ESP8266 pide picos de ~300 mA y se cae si la fuente es floja—. Mira también que TX/RX estén cruzados y no al revés |
| `SIN ESP` y en la fila 2 sale **basura corta** | Llega algo, pero no `OK`: **baudios** (el AT de fábrica va a 115200) o el módulo todavía arrancando |
| `AT` va pero `AT+CWLAP` no devuelve redes | El módulo está en modo punto de acceso: hay que pasar por `AT+CWMODE=1` |
| `ERROR` en `AT+CWMODE=1` | Firmware AT antiguo: en algunas versiones el modo se pone con `AT+CWMODE_CUR=1` |

> Si el `AT` contesta pero el barrido no, **ya se sabe lo que importa**: el cableado está bien. Lo demás
> son comandos.

---

## 19. Procedimiento: wall follower — la mano (prueba 22)

Recorre el laberinto **pegado a una pared**, sin conocer el mapa. Está en **EXPLORE → WALL FOLLOWER**,
no en TEST: es navegación, no banco.

> **Por qué la mano y no un *flood fill***: el laberinto del sábado es de la organización y han quitado
> las islas, así que es un **árbol** —todos los pasillos se recorren sin dejar nada por explorar— y la
> regla de la mano lo recorre entero. Un *flood fill* haría falta para **resolverlo**, y el objetivo es
> **moverse sin chocar**.

### Antes de empezar

**Los umbrales de pared tienen que estar ajustados** (prueba 21) **contra el laberinto de verdad**. Si
se ajustan en casa y el laberinto es otro, las paredes que reflejen distinto mienten. Y **no se pueden
ajustar desde esta prueba**: hay que ir a TEST → IR → PAREDES, ajustar y volver.

### Botones

| Botón | Qué hace |
|:-----:|:---------|
| **OK** | Arranca o para. Cada arranque **pone el contador a cero y reinicia la traza** |
| **UP** | Mano **izquierda** |
| **DOWN** | Mano **derecha** |
| **MENU** | Sale (y para) |

### Pantalla

| Fila | Qué enseña |
|:----:|:-----------|
| 0 | `WALL FOLLOWER >` — el `>` marca que está andando |
| 1 | `MANO DER C 12` — mano y celdas recorridas |
| 2 | `F1 I0 D1 AVANZA` — las tres paredes y lo que decidió |
| 3 | **Por qué está parado** (ver abajo) · andando: `OK:PARA MENU:SALIR` |

> ⚠️ **La fila 3 es lo que hace la prueba utilizable sin PC.** En el laberinto el robot va **sin cable**
> y **no se puede usar telemetría**, así que `>WF_ATASCO` no le sirve a nadie que esté mirando: sin esa
> línea, un **atasco**, el **tope de celdas** y una **parada a mano** se ven **exactamente igual** —el
> robot quieto— y no se sabe si tocar el umbral, reiniciar o dejarlo.

### Regla de decisión

1. Si el lado de la **mano** está abierto → gira hacia él.
2. Si no, si el **frente** está abierto → sigue.
3. Si no, gira al **otro** lado.
4. Y si tampoco → **media vuelta**.

Se mira el lado de la mano **antes** que el frente, y no es un detalle: si se mirara el frente primero,
el robot seguiría de largo y volvería a pasar por el mismo cruce.

> **Se decide en el centro de la celda**, no al ver el hueco. Los sensores van 12 cm por delante del
> eje, así que a esa altura ya «ven» el cruce que viene; girar en cuanto se ve el hueco dejaría al robot
> **desalineado** justo donde el pasillo nuevo no tiene pared que lo corrija.

### Se para sola por tres motivos

| Motivo | Qué pone la **fila 3** | Qué significa |
|:-------|:-----------------------|:--------------|
| **Atasco** | `PARADO: ATASCO` | Un avance agotó sus 8 s sin llegar a la celda: el robot está **empujando contra algo** |
| Tope de `WF_CELDAS_MAX` (250) | `PARADO: FIN 250` | Recorrió el tope. En un árbol (sin islas) eso **no debería pasar** |
| **OK** (pausa a mano) | `OK:ANDA UP:IZ DN:DE` | Lo paraste tú y puedes reanudar |
| **MENU** | — (sales al menú) | Lo pulsaste tú |

> El atasco es la protección que más vale: sin ella, un umbral mal puesto manda al robot a «recorrer» un
> laberinto que no existe, sin avanzar, hasta que se acaba la batería. **Y es lo que más probable es que
> veas en el laberinto ajeno**: una pared que no detecta hace girar al robot donde no hay hueco.

### Sin cable: la traza

El *wall follower* va **sin cable**, así que para depurarlo lo que vale es la **traza en RAM**: guarda
**una muestra por celda** con las tres medidas de pared y la decisión tomada. Se descarga después con la
orden `TRACE`, ya con el cable puesto:

| Etiqueta | Qué es |
|:---------|:-------|
| `>TR_SET` | Medida de la pared **de frente** |
| `>TR_IZQ` | Medida de la pared **izquierda** |
| `>TR_DER` | Medida de la pared **derecha** |
| `>TR_EXTRA` | **Decisión**: 1 avanzar · 2 girar izquierda · 3 girar derecha · 4 media vuelta |

---

## 20. Procedimiento: puente WiFi — quitar el cable (prueba 24)

Lleva la **USART3 entera por TCP**: la telemetría sale y las órdenes entran **sin cable**. Está en
**TEST → VARIOS → WIFI AL PC**, *antes* del test del ESP: la prueba 23 averigua si el módulo habla, y
esta lo pone a trabajar.

> **Qué es un puente *transparente***: el módulo se configura con `AT+CIPMODE=1`, un modo en el que
> **deja de interpretar comandos AT y pasa los bytes tal cual**. Visto desde el STM32, la USART3 se
> comporta como si el cable siguiera puesto, pero los bytes viajan por la red.

### La cadena

| Tramo | Quién |
|:------|:------|
| PC | un **servidor TCP** de banco escuchando en un puerto |
| Red | el módulo se conecta **como cliente** a la IP del PC (**no** al revés) |
| ESP8266 | tras `AT+CIPSEND` entra en modo transparente |
| STM32 | la USART3: `app/telemetry.c` hacia fuera, `app/console.c` hacia dentro |

### Preparación

1. `firmware/include/wifi_secrets.h` con el **SSID**, la clave, la **IP del PC** y el puerto. Ese
   fichero está en `.gitignore` —hay una plantilla `wifi_secrets.h.example` versionada— porque lleva
   la contraseña de la red.
2. Arrancar el **servidor TCP** en el PC y dejarlo escuchando **antes** de pulsar OK.
3. **Quitar el cable USB-serie**: si se queda puesto, el adaptador y el ESP escriben en la misma línea.
4. **TEST → VARIOS → WIFI AL PC** y **OK**. La pantalla va enseñando por qué paso va.

### Pasos que da por sí sola

| Paso | Comando |
|:-----|:--------|
| 1 | `AT` |
| 2 | `ATE0` — quita el **eco** |
| 3 | `AT+CWMODE=1` — modo estación |
| 4 | `AT+CWJAP="SSID","clave"` — se une a la red |
| 5 | `AT+CIPMUX=0` — **una sola** conexión |
| 6 | `AT+CIPMODE=1` — modo transparente |
| 7 | `AT+CIPSTART="TCP","<IP del PC>",<puerto>` |
| 8 | `AT+CIPSEND` — a partir de aquí, todo es transparente |

Si alguno falla, la pantalla pone **`FALLO EN <paso>`** y debajo **lo que contestó el módulo**, en
crudo: es lo que permitió cazar los dos fallos de la tabla de abajo.

### Cómo se sabe que está arriba

| Señal | Qué significa |
|:------|:--------------|
| Pantalla `PUENTE` / `SIN CABLE` / `MENU:SALIR` | Arriba y escuchando |
| `>PUENTE_T` a **1 Hz** | El enlace vive. **Es el latido que más vale**: con el puente arriba no hay cable, así que si el latido llega, la cadena entera funciona |
| La orden de consola contesta con su `>ETIQUETA` | El camino **PC → robot** también funciona |
| `LED 1` / `LED 0` | Confirmación **a ojo**, sin mirar la pantalla del PC |

> ⚠️ **El LED está encendido desde el arranque**, así que un `LED 1` no demuestra nada: no se ve
> cambiar. Para que se vea hay que **alternar** (`LED 1` / `LED 0`) o usar `LED TOGGLE`, que no
> necesita saber en qué estado estaba.

> Y las órdenes del robot **no se pueden mandar de una en una desde fuera con pausas de reloj**: un LED
> que cambia cada medio minuto no es un latido, es un cambio de estado. El **parpadeo se hace en el
> script del PC**, que manda los `LED 1` / `LED 0` con sus pausas de 250 ms.

### Fallos que costaron una tarde

| Síntoma | Causa | Arreglo |
|:--------|:------|:--------|
| **Bucle de eco**: llegan `>CMD_ERR:4` rebotando, la pantalla marca `REDES 0` y los botones dejan de responder | El módulo **viene con `ATE1`** y **repite todo lo que recibe**. Como la telemetría va por la **misma línea**, el STM32 oía su propia salida y la volvía a interpretar como orden | Por los **dos lados**: `ATE0` en `ESP_Preparar()` **y** la consola **ignora las líneas que empiezan por `>`** (la telemetría no es una orden) |
| **`FALLO EN ATE0` aunque el módulo contesta `OK`** | `esp_linea[]` no llevaba **terminador en cada byte**, así que al cerrar la línea quedaba el **rabo de la anterior**: `OK` + `E0` se leía como `OKE0`. Con `AT` funcionaba **por casualidad**, porque las dos cadenas miden lo mismo | `esp_linea[esp_linea_n] = '\0'` **a cada byte**, no solo al cerrar la línea |
| **Los botones se congelan** y la pantalla se queda quieta | La consola vaciaba el anillo de recepción **sin tope** (`while ((byte = UART_RxGet()) >= 0)`): con un módulo que habla sin parar, el firmware no volvía a mirar los botones. Se veía también **fuera** del test | `CONSOLE_BYTES_MAX`, y **ningún** bucle de recepción sin tope en todo el firmware |

### Notas de banco

- El **servidor del PC tiene que ser callado**: si imprime cada línea recibida, el terminal **nunca se
  queda quieto**, acaba **interrumpiendo el proceso** y además cada orden tarda **medio minuto** en
  volver. Lo que hay que hacer es **escribir la telemetría a un fichero** y leerla de ahí.
- **Si se reinicia el servidor, el módulo se reconecta solo**: no hace falta volver a pulsar OK. Viene
  bien saberlo porque reiniciar el servidor es justo lo que hay que hacer para cambiar el script.
- `AT+CIPMUX=0` es **una sola conexión**: no vale abrir una segunda desde el PC mientras el robot está
  dentro.

> **Lo que esto cambia para el sábado**: el robot ya se puede **soltar en el laberinto y seguir
> mirándolo**. Sin cable no hay nada que se enganche, y la traza en RAM se descarga al final con
> `TRACE` sin haber tocado el robot.

Con eso se reconstruye el recorrido entero y se ve **si la decisión era la correcta** para lo que midió.