# 🔗 Conexionado de la V1 — de bloques a netlist

Este documento es **la especificación eléctrica de partida** para dibujar el esquemático de KiCad.
No repite el pinout (está en [`../../Documentacion/hardware.md`](../../Documentacion/hardware.md) §3)
ni las cuentas de diseño (§4): ordena **qué terminal de qué componente va a qué red**, bloque por
bloque, con los valores que ya están medidos.

> 📅 **2026-11-12.** Sale de `firmware/include/pinout.h`, de `hardware.md` §2–§4 y del código de
> `hal/gpio.c`. Donde la documentación se contradice, se dice aquí.

**Cómo usarlo**: cada bloque es una hoja del esquemático. La columna **Net** es el nombre que
conviene ponerle a la etiqueta en KiCad, para que la red sobreviva de una hoja a otra.

---

## 0. Lo que hay que decidir antes de dibujar

| # | Decisión | Opciones | Consecuencia |
|:-:|:--|:--|:--|
| **D1** | **¿Blue Pill como módulo, o STM32F103C8T6 desnudo?** | Módulo (zócalo o soldado) **vs** LQFP48 desnudo | ⏳ **Abierta a propósito**: depende de si el robot va a **una o dos alturas**, y eso lo decide el chasis nuevo. El desnudo obliga a añadir **cristal de 8 MHz + 2×22 pF**, **regulador 3,3 V propio**, **BOOT0/BOOT1 con resistencias** y desacoplo por pin; el módulo trae todo eso pero ocupa **1 192 mm²** (19 % de la placa) y sube ~11 mm con zócalo |
| **D2** | **De dónde sale `VM` (motores)** | **Boost a 9 V** ✅ | Se mantiene como en el prototipo, para no perder velocidad punta. ⚠️ Añadir **≥100 µF** junto al driver: el boost no absorbe energía y solo hay **2,8 V** de margen hasta el máximo absoluto del DRV8833 (11,8 V). ✅ Y a cambio **protege a la LiPo**, porque su diodo bloquea el camino inverso. Ver el bloque **A** |
| **D3** | **Riel de los encoders** | **3,3 V** ✅ | A 5 V el nivel alto medido era **3,6 V** y el máximo absoluto de PA0/PA1/PA6/PA7 es **4,0 V** (pines `TTa`, sin `FT`): funcionaba, pero **al límite**. A 3,3 V queda holgado y el módulo lo admite (3,3–5 V) |
| **D4** | **Riel de la OLED** | **5 V** ✅ | El módulo lleva su propio regulador: a 5 V regula bien. Los pull-ups del I2C van al **3,3 V** en cualquier caso |
| **D5** | **¿El buck es MP2338?** | **Sí** ✅ | El **MP1584 está NRND** y su hoja de datos remite al MP2338 ([#13](https://github.com/pepechorva/Rabosa_micromouse/issues/13)). Se queda el riel de 5 V (**D3+D4**), así que el buck es de **5 V** |

> ⚠️ **No queda ni un pin libre.** Con el reparto actual están ocupados **PA0–PA14, PB0–PB15 y
> PC13–PC15**: el único GPIO disponible es **PA15** (y es JTDI, necesita el JTAG desactivado). Si la
> V2 quiere succión, o se libera un pin o se cambia de encapsulado. Ver [#9](https://github.com/pepechorva/Rabosa_micromouse/issues/9).

---

## A. Rieles

| Net | De | A | Nota |
|:--|:--|:--|:--|
| `VBAT` | JST de la LiPo 2S | Entrada del buck de 5 V · entrada del **boost** | 6,0–8,4 V · **450 mAh 80C** |
| `9V` (= `VM` del driver) | **Boost** desde `VBAT` (**D2**) | `VM` del DRV8833 | 2,7–10,8 V · máx. absoluto **11,8 V**. Ver el aviso de frenado, debajo |
| `5V` | Salida del buck (**MP2338**) | Blue Pill (`5V`) · OLED `VCC` (**D4**) | ≥1 A · carga real **< 0,1 A** |
| `3V3` | Regulador de la Blue Pill (o LDO propio si **D1** = desnudo) | **encoders `VCC`** (**D3**) · IMU `VCC` · emisores IR · receptores IR · pull-ups I2C | ≈1 A disponibles · peor caso **0,3 A** |
| `GND` | — | Todo | Plano bajo el MCU y la señal; **la corriente de los motores vuelve por su propia pista hasta el negativo de la batería**, no por debajo del microcontrolador |

> [!WARNING] El boost y el frenado: el condensador no es opcional
> Un boost **solo empuja corriente hacia su salida: no la absorbe**. Cuando el DRV8833 frena
> (cortocircuita el motor) o la rueda se para de golpe, el motor hace de generador y esa energía
> vuelve al riel de 9 V, donde **no tiene camino de vuelta a la batería**: la tensión sube. Y con el
> riel a 9 V quedan **menos de 2 V** hasta el máximo absoluto del DRV8833 (11,8 V).
>
> Con **D2 = boost**, el único que puede tragarse ese pico es el condensador de entrada del driver, así
> que conviene **≥100 µF** junto al driver. Si aparece un reinicio con el motor
> forzado, `>RESET:n` dice si fue tensión (1) o el pin `NRST` (2): si sale **1**, falta capacidad.
>
> ⚠️ Y el boost tiene que entregar **1,4 A a 9 V** (≈12,6 W, unos 1,9 A de entrada a 7,4 V): los
> módulos comerciales se anuncian con 1–2 A, así que está **en el límite**. Elegir uno de **≥2 A** de
> salida, o replantear **D2** si aparece recalentamiento o caída de tensión con los dos motores en
> *stall*.

**Desacoplo obligatorio** (de `hardware.md` §4.6, tras los reinicios del 30-sep):

| Condensador | Dónde | Para qué |
|:--|:--|:--|
| **≥100 µF** electrolítico (220–470 si cabe) + **100 nF** cerámico | Entre `VM` y `GND`, **junto al DRV8833** | Absorber el rizado inductivo y el caso de `Coast` (en `Brake` la energía se queda en el motor). Con D2 = boost, el boost no puede tragarla |
| **100 nF** | **`NRST` → GND**, junto al pin | Filtrar el ruido que dispara reset |
| **100 nF** | En los **bornes** de cada motor | Ruido de escobillas |

---

## B. Núcleo

| Net | Componente | Pin | Dirección | Nota |
|:--|:--|:-:|:-:|:--|
| `SWDIO` | ST-Link `SWDIO` | **PA13** | — | **No reconfigurar** |
| `SWCLK` | ST-Link `SWCLK` | **PA14** | — | No reconfigurar |
| `NRST` | ST-Link `RST` | NRST | — | + **100 nF** a GND |
| `3V3` / `GND` | ST-Link | — | — | Solo para referencia, no para alimentar |
| `BOOT0` | Resistencia **10 kΩ a GND** | BOOT0 | — | Arranque desde Flash. Pulsador a `3V3` opcional para el bootloader |
| — | Resistencia **10 kΩ a GND** | **PB2** (`BOOT1`) | — | Deja `BOOT1` a 0 sin ocupar un GPIO útil |

⚠️ **JTAG desactivado por software** (`__HAL_AFIO_REMAP_SWJ_NOJTAG()`), y de eso depende que
**PB3, PB4 y PA15** sean GPIO. Es una decisión permanente: si alguien lo revierte, los emisores
`WALL L1` y `WALL L2` dejan de funcionar.

---

## C. Puente en H y motores

| Net | DRV8833 | MCU | Rueda | Estructura |
|:--|:--|:-:|:--|:--|
| `PWM1A` | `AIN1` | **PA8** (TIM1_CH1) | — | `motor1` |
| `PWM1B` | `AIN2` | **PA9** (TIM1_CH2) | **izquierda** | `motor1` |
| `PWM2A` | `BIN1` | PA10 (TIM1_CH3) | — | `motor2` |
| `PWM2B` | `BIN2` | PA11 (TIM1_CH4) | **derecha** | `motor2` |
| `M1+` / `M1−` | `AOUT1` / `AOUT2` | — | Borne del motor izquierdo | — |
| `M2+` / `M2−` | `BOUT1` / `BOUT2` | — | Borne del motor derecho | — |
| `VM` | `VM` | — | — | Alimentación **de potencia**: de aquí sale la corriente de los motores. 2,7–10,8 V · máx. absoluto 11,8 V · consumo propio del chip **1,7–3 mA** |
| `GND` | `GND`, `xISEN` | — | — | `xISEN` a GND (sin sensado de corriente) |
| `nSLEEP` | — | — | — | **A `3V3`** directamente. ⚠️ **Nunca a `VM`:** el pin lleva un zener que lo recorta a 6,5 V y `VM` son 7,4 V ⇒ si se quiere atar al riel de motores, **con resistencia** (es lo que dice el fabricante) |
| `VCP` | — | — | — | **10 nF / 16 V cerámico entre `VCP` y `VM`**, pegado a los pines (bomba de carga) |
| `VINT` | — | — | — | **2,2 µF / 6,3 V cerámico a GND**, pegado al pin |

⚠️ **Sentido de giro**: el motor 1 va montado al revés que el 2. Se corrige **en firmware**
(`motor1.channel1 ↔ channel2` en `APP_InitAll()`), no cruzando el cableado en la placa. Si se
prefiere arreglarlo en hardware, hay que quitar ese cruce del firmware a la vez.

⚠️ **El DRV8833 no tiene alimentación de lógica** (no existe pin `VCC` en el integrado). Las
entradas `xIN` son *logic-level* —`V_IH` ≥ **2 V**, `V_IL` ≤ 0,7 V— con **pull-down interno de
150 kΩ**, así que los 3,3 V del MCU las atacan directamente. Algunos módulos *breakout* sacan un pin
llamado `VCC`: alimenta sus pull-ups y su LED, **no** el chip.

#### Conectores del módulo (para cablear el prototipo o copiar la hoja)

| Conector | Pin | Nombre | Va a |
|:--|:-:|:--|:--|
| `P2` | 1 | `IN4` | **PA11** (`PWM2B`, → `BIN2`) |
| `P2` | 2 | `IN3` | **PA10** (`PWM2A`, → `BIN1`) |
| `P2` | 3 | `GND` | `GND` |
| `P2` | 4 | `VCC` = **`VM` del chip** | **Salida del boost (9 V)**. Es potencia, no lógica |
| `P2` | 5 | `IN2` | **PA9** (`PWM1B`, → `AIN2`) |
| `P2` | 6 | `IN1` | **PA8** (`PWM1A`, → `AIN1`) |
| `P1` | 1 | `EEP` = **`nSLEEP`** | **Cableado al riel de 5 V** (step-down). Ver el aviso sobre el zener, abajo |
| `P1` | 2 | `OUT1` | Motor **izquierdo**, borne + (`AOUT1`) |
| `P1` | 3 | `OUT2` | Motor **izquierdo**, borne − (`AOUT2`) |
| `P1` | 4 | `OUT3` | Motor **derecho**, borne + (`BOUT1`) |
| `P1` | 5 | `OUT4` | Motor **derecho**, borne − (`BOUT2`) |
| `P1` | 6 | `ULT` = **`nFAULT`** | Sin conectar. Si se quiere leer, **10 kΩ a `3V3`** y al **PA15** (el único GPIO libre del MCU) |

⚠️ El módulo **saca la potencia de motores por el pad `VCC`**, no por un borne aparte: en esta placa
`VCC` = `VM` = salida del boost. Las salidas a los motores son `OUT1`–`OUT4`.

#### Lo que lleva el módulo actual (esquema del *breakout*, 2026-11-12)

El módulo es una aplicación de referencia del chip: sirve tal cual para copiar los pasivos si al
final el DRV8833 va **desnudo** en la placa.

| Componente | Valor | Veredicto |
|:--|:--|:--|
| `C2` entre **`VCP` y `VM`** | **10 nF** | ✅ el datasheet pide 0,01 µF / 16 V X7R |
| `C1` en **`VINT`** a GND | **2,2 µF** | ✅ el datasheet pide 2,2 µF / 6,3 V cerámico |
| `C3` en **`VM`** a GND | **10 µF** | ⚠️ poco para el caso de `Coast`: añadir **≥100 µF** externos, ver abajo |
| `R1` de `VCC` a **`nSLEEP`** | **47 kΩ** | ✅ En el módulo original es el pull-up de *enable*. ⚠️ En nuestro montaje `nSLEEP` va al riel de 5 V, así que `R1` queda entre **9 V y 5 V**: una fuga de **(9−5)/47 k ≈ 85 µA** hacia el riel de 5 V (850 µA si `R1` fuese de 4,7 k). Inofensiva, pero el buck **no puede absorberla**: si algún día el riel de 5 V aparece alto **sin carga**, es esto |
| `AISEN` / `BISEN` | a GND | ✅ sin sensado de corriente |
| `nFAULT` | **sin pull-up** | ⚠️ expuesto en el conector y **flotando** |
| LED de encendido | 4k7 + LED desde `VCC` | — |

> [!NOTE] Cuánta capacidad hace falta en `VM`, con el número delante
> La primera versión de este documento pedía **470 µF** «porque el boost no absorbe el frenado». Eso
> era **alarmista**, y el propio datasheet del DRV8833 lo desmiente:
>
> | Modo | Entradas | Salidas | Adónde va la energía del motor |
> |:--|:-:|:-:|:--|
> | **Brake** (lo que hace `DRV8833_Brake()`) | `xIN1 = xIN2 = 1` | `L`–`L` | **Se queda en el motor**: los dos *low-side* lo cortocircuitan y la corriente circula por el bobinado. **No entra en `VM`** |
> | **Coast** | `0`–`0` | `Z`–`Z` | Sale por los **diodos de cuerpo** hacia `VM` |
> | PWM normal | conmutando | — | Rizado inductivo en cada flanco, de orden **mJ** |
>
> Con un `Brake` (que es lo que usa el firmware al parar), el motor **no** inyecta energía en el riel.
> El caso que sí lo hace es `Coast` con el robot empujado, o soltar el motor en marcha.
>
> | Energía a absorber | En 22 µF | En 100 µF | En 470 µF |
> |:--|--:|--:|--:|
> | 0,15 mJ (un flanco de PWM) | +0,7 V | +0,15 V | +0,03 V |
> | 4,5 mJ (robot de 100 g a 0,3 m/s, todo al riel) | +13 V | +2,8 V | +0,6 V |
>
> ⇒ **100 µF basta** para el caso realista; **220–470 µF** son seguro barato si caben. Lo que **no**
> hay que hacer es fiarse solo de los **10 µF** del módulo en el caso de `Coast`.
>
> ✅ **Y el boost protege a la LiPo por construcción**: el diodo Schottky de salida bloquea el camino
> inverso, así que **ningún pico del riel de 9 V llega a la batería**. Es un argumento a favor de
> **D2 = boost** que no estaba en la tabla.

⚠️ **`nFAULT`**: si se quiere leer, **10 kΩ a `3V3`** (es salida de avería en *open-drain*; el
módulo lo saca al conector **sin** pull-up). Si no, dejar sin conectar. El firmware no lo lee hoy.

✅ **Resuelto en hardware (2026-11-12), y probado en el robot**: el `VCC` del módulo va **directo
a la salida del boost (9 V)** y ese es el `VM` del chip; el `nSLEEP` (`EEP`) va **soldado al riel de
5 V** del step-down. Son dos nets distintas a propósito: **potencia a 9 V, lógica a 5 V**. Funciona.

> [!NOTE] Por qué el zener de `nSLEEP` no es un problema aquí
> `nSLEEP` lleva un zener interno que recorta a **6,5 V** y no admite más de **250 µA**. Con el pin a
> **5 V** el zener **no llega a conducir**: la corriente se queda en `5 V / 500 kΩ` ≈ **10 µA**, y el
> umbral de entrada es `V_IH` ≥ **2,5 V** ⇒ margen de 2,5 V. Correcto.
>
> De ahí sale la regla para la **V1**: `nSLEEP` puede ir a **`3V3`** (3,3 V > 2,5 V, con 800 mV de
> margen) o quedarse en el riel de 5 V. Lo que **no** debe hacerse es atarlo al `VM` de 9 V sin una
> resistencia que limite: ahí el zener sí conduce y hay que quedarse por debajo de 250 µA.

#### Lo que lleva el módulo actual (esquema del *breakout*, 2026-11-12)

El módulo es una aplicación de referencia del chip: sirve tal cual para copiar los pasivos si al
final el DRV8833 va **desnudo** en la placa.

| Componente | Valor | Veredicto |
|:--|:--|:--|
| `C2` entre **`VCP` y `VM`** | **10 nF** | ✅ el datasheet pide 0,01 µF / 16 V X7R |
| `C1` en **`VINT`** a GND | **2,2 µF** | ✅ el datasheet pide 2,2 µF / 6,3 V cerámico |
| `C3` en **`VM`** a GND | **10 µF** | ⚠️ poco para el caso de `Coast`: añadir **≥100 µF** externos, ver abajo |
| `R1` de `VCC` a **`nSLEEP`** | **47 kΩ** | ✅ En el módulo original es el pull-up de *enable*. ⚠️ En nuestro montaje `nSLEEP` va al riel de 5 V, así que `R1` queda entre **9 V y 5 V**: una fuga de **(9−5)/47 k ≈ 85 µA** hacia el riel de 5 V (850 µA si `R1` fuese de 4,7 k). Inofensiva, pero el buck **no puede absorberla**: si algún día el riel de 5 V aparece alto **sin carga**, es esto |
| `AISEN` / `BISEN` | a GND | ✅ sin sensado de corriente |
| `nFAULT` | **sin pull-up** | ⚠️ expuesto en el conector y **flotando** |
| LED de encendido | 4k7 + LED desde `VCC` | — |

> [!NOTE] Cuánta capacidad hace falta en `VM`, con el número delante
> La primera versión de este documento pedía **470 µF** «porque el boost no absorbe el frenado». Eso
> era **alarmista**, y el propio datasheet del DRV8833 lo desmiente:
>
> | Modo | Entradas | Salidas | Adónde va la energía del motor |
> |:--|:-:|:-:|:--|
> | **Brake** (lo que hace `DRV8833_Brake()`) | `xIN1 = xIN2 = 1` | `L`–`L` | **Se queda en el motor**: los dos *low-side* lo cortocircuitan y la corriente circula por el bobinado. **No entra en `VM`** |
> | **Coast** | `0`–`0` | `Z`–`Z` | Sale por los **diodos de cuerpo** hacia `VM` |
> | PWM normal | conmutando | — | Rizado inductivo en cada flanco, de orden **mJ** |
>
> Con un `Brake` (que es lo que usa el firmware al parar), el motor **no** inyecta energía en el riel.
> El caso que sí lo hace es `Coast` con el robot empujado, o soltar el motor en marcha.
>
> | Energía a absorber | En 22 µF | En 100 µF | En 470 µF |
> |:--|--:|--:|--:|
> | 0,15 mJ (un flanco de PWM) | +0,7 V | +0,15 V | +0,03 V |
> | 4,5 mJ (robot de 100 g a 0,3 m/s, todo al riel) | +13 V | +2,8 V | +0,6 V |
>
> ⇒ **100 µF basta** para el caso realista; **220–470 µF** son seguro barato si caben. Lo que **no**
> hay que hacer es fiarse solo de los **10 µF** del módulo en el caso de `Coast`.
>
> ✅ **Y el boost protege a la LiPo por construcción**: el diodo Schottky de salida bloquea el camino
> inverso, así que **ningún pico del riel de 9 V llega a la batería**. Es un argumento a favor de
> **D2 = boost** que no estaba en la tabla.

⚠️ **`nFAULT`**: si se quiere leer, **10 kΩ a `3V3`** (es salida de avería en *open-drain*; el
módulo lo saca al conector **sin** pull-up). Si no, dejar sin conectar. El firmware no lo lee hoy.

✅ **Resuelto (2026-11-12): hay una sola red `VCC` en el módulo, y es la salida del boost.**
La confusión venía del esquema de referencia, que rotula «VCC» en dos sitios distintos del dibujo
(el pad de `VM` y el pin 4 del conector de entradas). En la placa real **solo existe el `VCC` de la
fila de entradas**, y de ahí sale también el `VM` del chip. No hay que alimentar nada a 3,3 V en el
módulo, y `nSLEEP` no queda flotante: lo sube `R1` y lo fuerza el *jumper* `en/sleep`.

> [!WARNING] Comprobar el valor de `R1` si el `VCC` va a 9 V
> El *jumper* `en/sleep` une `nSLEEP` con `VCC` a través de `R1`. El datasheet avisa: `nSLEEP` lleva
> un **zener interno que lo recorta a 6,5 V** y no admite más de **250 µA**.
>
> | `R1` | Corriente a `VCC` = 9 V | Veredicto |
> |--:|--:|:--|
> | **47 kΩ** | **53 µA** | ✅ correcto |
> | **4,7 kΩ** | **532 µA** | ⚠️ **2× por encima del límite**: comprobar el valor real |
>
> En la foto de la placa se lee `472` (= 4,7 kΩ) donde el esquema de referencia pone 47 kΩ. **Medir
> `R1` con el polímetro** antes de dar el módulo por bueno a 9 V; si es de 4,7 kΩ, sustituirlo o
> alimentar `VCC` a 5 V.

---

## D. Encoders

| Net | Motor | Pin MCU | Timer | Nota |
|:--|:--|:-:|:--|:--|
| `M1ENCA` | izquierdo, canal A | **PA0** | TIM2_CH1 | — |
| `M1ENCB` | izquierdo, canal B | **PA1** | TIM2_CH2 | — |
| `M2ENCA` | derecho, canal A | **PA6** | TIM3_CH1 | — |
| `M2ENCB` | derecho, canal B | **PA7** | TIM3_CH2 | — |
| `VCC` | ambos | — | — | **3,3 V** (**D3**): el módulo admite 3,3–5 V y así el nivel alto queda lejos del máximo absoluto |
| `GND` | ambos | — | — | — |

Salida de **colector abierto con pull-up de 3,3 kΩ en el módulo** ⇒ el nivel alto acompaña al riel.
En el prototipo, con `VCC` = 5 V, se midieron **3,6 V** de pico (21-sep): dentro del máximo absoluto de
esos pines (**4,0 V**, son `TTa` sin `FT`) pero **justo en el límite**. En la V1 van a 3,3 V.

---

## E. Emisores IR — 4 × (BC337 + 22 Ω)

Un bloque idéntico por emisor; solo cambia el GPIO.

```
3V3 ──[ LED SFH 4550 ]──[ 22 Ω ½ W ]──┬── C (pin 1)
                                      │
GPIO ──[ 1 kΩ ]── B (pin 2)   BC337 (TO-92)
                                      │
                                    E (pin 3) ── GND
```

| Emisor | GPIO | Net base | Posición física | Etiqueta |
|:-:|:-:|:--|:--|:--|
| 1 | **PB3** | `IR_LED_B1` | **frontal izquierdo** | `WALL L1` |
| 2 | **PB4** | `IR_LED_B2` | **lateral derecho** | `WALL L2` |
| 3 | **PB5** | `IR_LED_B3` | **lateral izquierdo** | `WALL L3` |
| 4 | **PB8** | `IR_LED_B4` | **frontal derecho** | `WALL L4` |

| Magnitud | Valor | Límite |
|:--|--:|:--|
| $I_F$ por emisor | **≈75 mA** | 100 mA continuos del SFH 4550 |
| Corriente de base | **2,5 mA** | ≈20 mA por pin |
| Disipación de la 22 Ω | **124 mW** | ⇒ usar **½ W**, no ¼ W |
| Los 4 a la vez | ≈300 mA | solo en la prueba 14 |

⚠️ **El orden `L1…L4` no es geométrico**, y la **1 kΩ de base es obligatoria** (sin ella el pin da
30–50 mA, por encima de su límite).

---

## F. Receptores IR — 4 × ST-1KL3A

Colector a **3,3 V** (nunca 5 V), emisor → $R_L$ → GND, y **el emisor a la entrada del ADC**.

```
3V3 ──── C  ST-1KL3A  E ────┬──── ADC (PA2…PA5)
                            │
                        [ 3 kΩ ]  ← RL, sin condensador
                            │
                           GND
```

| Canal | Pin | Net ADC | Posición física | Etiqueta |
|:-:|:-:|:--|:--|:--|
| `R0` | **PA2** | `WALL_A1` | **frontal derecho** | `WALL A1` |
| `R1` | **PA3** | `WALL_A2` | **lateral derecho** | `WALL A2` |
| `R2` | **PA4** | `WALL_A3` | **lateral izquierdo** | `WALL A3` |
| `R3` | **PA5** | `WALL_A4` | **frontal izquierdo** | `WALL A4` |

⚠️ **La numeración `R0…R3` no es un recorrido por el robot.** Y `R_L` = **3 kΩ** es el valor de
partida **medido a ojo**: es la pieza que la campaña de sensores va a mover (`hardware.md` §4.4).
En la PCB conviene dejar el sitio para **2 resistencias en paralelo** o un *footprint* que admita
2,2 k – 10 k.

---

## G. Sensado de batería

| Net | Pin | Divisor | $k$ | Mide |
|:--|:-:|:--|--:|:--|
| `BAT_PACK` | **PB0** (ADC_IN8) | **22 kΩ** arriba · **10 kΩ** abajo | **0,3125** | Tensión total del pack |
| `BAT_CEL1` | **PB1** (ADC_IN9) | **3k9** arriba · **10 kΩ** abajo | **0,7194** | Celda 1 (la pegada a GND) |

**El orden importa: la resistencia grande va arriba.** La celda 2 sale por resta
($V_2 = V_{pack} - V_1$).

⚠️ Los dos divisores **no son iguales a propósito**: con una sola $k$, la celda 2 daba **−1,29 V** y
disparaba avisos falsos. En la V1 se pueden **igualar** (las dos con 22 k/10 k), pero entonces hay
que cambiar la $k$ del firmware **en el mismo commit** y volver a medir.

---

## H. IMU — SPI2

| Net | Módulo IMU | Pin MCU | Nota |
|:--|:--|:-:|:--|
| `IMU_CS` | `CS` | **PB12** | GPIO de salida (el HAL lo reconvierte tras `MX_SPI2_Init()`) |
| `IMU_SCK` | `SCL/SCK` | **PB13** | SPI2_SCK |
| `IMU_MISO` | **`ADO/SDO`** | **PB14** | Salida del módulo ⇒ **MISO** |
| `IMU_MOSI` | **`SDA/SDI`** | **PB15** | Entrada del módulo ⇒ **MOSI** |
| `3V3` / `GND` | `VCC` / `GND` | — | — |

⚠️ **El cruce `SDA/SDI` ↔ `ADO/SDO` es el fallo clásico**: si se intercambian, `WHO_AM_I` devuelve
**0xFF** y no hay comunicación. La placa lo hace bien o no funciona, sin término medio.

⚠️ PB12 esta precargado como AF por `gpio.c` y **`MX_SPI2_Init()` lo reconfigura como GPIO de
salida (CS)**: es intencionado, no un descuido.

---

## I. Pantalla — I2C1

| Net | OLED | Pin MCU | Nota |
|:--|:--|:-:|:--|
| `I2C_SCL` | `SCL` | **PB6** | AF *open-drain* |
| `I2C_SDA` | `SDA` | **PB7** | AF *open-drain* |
| `5V` | `VCC` | — | **D4**: 5 V, porque el módulo lleva su propio regulador |
| `GND` | `GND` | — | — |

⚠️ **El F1 no puede activar el pull-up interno en modo AF_OD** ⇒ el bus lo tiene que subir alguien:
los pull-ups del módulo (que van al **3,3 V**) o **2 × 4,7 kΩ de PB6 y PB7 a `3V3`**. Conviene
dejar **el footprint** en la placa aunque el módulo traiga los suyos.

Dirección **0x3C** (el driver también prueba 0x3D). Velocidad 100 kHz.

---

## J. Botones

| Botón | Pin | Net | Evento |
|:--|:-:|:--|:-:|
| MENU | **PC14** | `BTN_MENU` | (máscara; va por interrupción) |
| UP | **PB9** | `BTN_UP` | 2 |
| DOWN | **PC15** | `BTN_DOWN` | 4 |
| OK | **PA12** | `BTN_OK` | 8 |

Cada pulsador va **entre el pin y GND**, con el **pull-up interno** activado ⇒ activo a nivel bajo.
**Sin resistencias ni condensadores**: el antirrebote es por software (30 ms). Verificado en banco.

⚠️ **PC14 y PC15 son `OSC32_IN` / `OSC32_OUT`**: si algún día se monta el cristal de 32,768 kHz (RTC),
estos dos botones hay que moverlos. Como entradas no les afecta el límite de corriente de PC13–PC15.

---

## K. Consola y telemetría

| Net | Componente | Pin MCU | Nota |
|:--|:--|:-:|:--|
| `UART_TX` | `RXD` del conversor TTL↔USB | **PB10** (USART3_TX) | 115200 8N1 |
| `UART_RX` | `TXD` del conversor | **PB11** (USART3_RX) | Recepción **por interrupción** |
| `GND` | Conversor | — | **Imprescindible** |

Se puede llevar a un **conector de 4 pines** (TX, RX, GND, +5 V) para un único cable al PC.
⚠️ **El módulo BLE (DA14580) está aparcado**: si se reserva su footprint, no cablearlo.

---

## L. LED indicador

| Net | Componente | Pin | Nota |
|:--|:--|:-:|:--|
| `LED_STATUS` | LED + resistencia a GND | **PC13** | ⚠️ PC13 aguanta **≈3 mA** ⇒ resistencia de **1 kΩ** o más |

Si se quieren **LEDs de estado** de verdad, van en pines dedicados con su resistencia: **no** en
PC13–PC15, que es justo donde no se puede pedir corriente.

---

## Resumen: pines ocupados y libres

| Puerto | Ocupados | Libre |
|:--|:--|:--|
| **PA** | PA0–PA14 (encoder, ADC, PWM, SWD, botón) | **PA15** (JTDI) |
| **PB** | PB0–PB15 (ADC, IR, I2C, UART, SPI, botón, BOOT1) | **ninguno** |
| **PC** | PC13 (LED), PC14, PC15 (botones) | **ninguno** |

⇒ **Un solo GPIO libre en todo el microcontrolador.** Es la restricción que condiciona la V2 (y el
motivo por el que la succión no cabe sin liberar un pin o cambiar de encapsulado).

---

## Redes: convención de nombres

Para que las etiquetas sobrevivan entre hojas del esquemático:

| Prefijo | Uso | Ejemplo |
|:--|:--|:--|
| `PWM` | Salidas de TIM1 al puente H | `PWM1A` … `PWM2B` |
| `M1`/`M2` | Encoders y bornes de motor | `M1ENCA`, `M2−` |
| `WALL_` | Sensores de pared | `WALL_A1` (ADC), `WALL_L1` (emisor) |
| `BAT_` | Divisores de batería | `BAT_PACK`, `BAT_CEL1` |
| `IR_LED_` | Base y colector de cada emisor | `IR_LED_B1`, `IR_LED_C1` |
| `BTN_` | Pulsadores | `BTN_OK` |
| `IMU_` / `I2C_` / `UART_` | Buses | `IMU_CS`, `I2C_SCL`, `UART_TX` |
| `VBAT` / `VM` / `5V` / `3V3` / `GND` | Rieles | — |

---

## Lo que este documento **no** decide

- **D1**: si el microcontrolador va desnudo o como módulo. Depende de si el robot acaba siendo de
  **una o dos alturas**, y eso lo fija el chasis nuevo.
- **La forma de la placa**: el contorno sale del FreeCAD del chasis, que se está rehaciendo.
- **Los conectores**: JST XH para batería y motores es el candidato (hay hoja de datos en
  `Datasheets/jst-xh.pdf`), pero no está decidido.
- **La posición de los sensores**: las cotas del montaje en «V» dependen del chasis nuevo.
- **El valor final de $R_L$**: se cierra con las pruebas 1 y 15, no en el esquemático.
