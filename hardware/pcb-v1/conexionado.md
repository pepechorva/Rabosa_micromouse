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

> ⚠️ **Quedan dos GPIO: `PA15` y `PB2`.** Todo lo demás está ocupado. Si la V2 quiere succión, o se
> usa uno de esos dos, o se libera un pin con un decodificador, o se cambia de encapsulado. Ver
> [#9](https://github.com/pepechorva/Rabosa_micromouse/issues/9).

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

#### Entrada de batería y protecciones

El prototipo **no tiene ninguna**: la LiPo va directa al boost y al buck. La V1 lo arregla.

**Tres amenazas distintas, y se tapan con cosas distintas:**

| Amenaza | Qué pasa | Qué la tapa |
|:--|:--|:--|
| **Corto** en la placa o el cableado | El pack puede dar **≈36 A** teóricos (450 mAh × 80C): la pista hace de fusible y el PLA del chasis se funde | **PTC** en serie con `VBAT` |
| **Polaridad inversa** | Se destruyen el boost, el buck y el driver a la vez | ✅ **Resuelto sin componentes**: el **JST XH va con llave** y no admite la conexión al revés. **No se pone P-MOSFET** (2026-11-12) |
| **Descarga profunda** | La celda baja de 3,0 V y se degrada | Aquí **sí hay protección**: `BAT_CRIT_PACK` en el firmware frena y avisa |

| Elemento | Valor propuesto | Coste en tensión / calor | Veredicto |
|:--|:--|:--|:--|
| **PTC** (polifusible rearmable) | Retención **2,6 A**, disparo 5,2 A, **1812**, ≈40 mΩ | 0,08 V y 160 mW a 2 A | ⏳ **En estudio** (2026-11-12). Ventaja: rearmable y tolera el *inrush* de los condensadores |
| ~~P-MOSFET en serie~~ | — | — | ❌ **Descartado**: el JST XH con llave ya impide la inversión. Para el caso aislado de un cable mal crimpado, no compensa 240 mW de pérdida |
| Fusible rápido | 2 A | 0 | ❌ Se funde a mitad de competición, y el pico de *inrush* de los condensadores puede dispararlo sin avería |

**Orden recomendado en la entrada:**

```
JST XH-2 ──► [ PTC, si se pone ] ──► [ interruptor ] ──► VBAT ──┬── boost ──► 9 V
                                                                 └── buck  ──► 5 V
```

⚠️ **El PTC va antes del interruptor**, para que también proteja el interruptor y el cableado interno.

⚠️ **Cuarta vía de entrada que no hay que olvidar: el conector de balance.** La celda 1 se mide del
hilo central del balance, así que hay **dos conectores** de batería en la placa. El de balance no
lleva protección y sus pines quedan al aire: si se cruza con el de potencia, hay corto directo.
Separarlos físicamente en el layout, y que el de potencia sea el que esté junto al PTC.

**Descarga profunda**: se controla **por firmware** (decisión 2026-11-12). Ver el apartado siguiente.

#### Batería baja: qué apaga el firmware, y hasta dónde puede

Política decidida: con la batería demasiado baja **no funcionan ni los motores ni los LEDs, solo la
pantalla OLED**. Lo que ya hace el firmware hoy: `BAT_V_AVISO_PACK` = **6,60 V** avisa, y
`BAT_V_CRIT_PACK` = **6,00 V** lleva a `FAULT` (frena, parpadea el LED y avisa por pantalla), que se
abandona con **OK** y no se reintenta hasta que la medida se recupere.

⚠️ **La ventana «solo OLED» es estrecha, y no por el firmware: por el hardware.**

El MCU **cuelga del riel de 5 V**, que sale del buck desde `VBAT`. Y un buck **no puede dar 5 V con
5 V de entrada**: el MP2338 necesita del orden de **4,5–5,5 V** en la entrada para regular. Así que:

| `VBAT` | Qué sigue vivo |
|:--|:--|
| > 6,6 V | todo (`OK`) |
| 6,0–6,6 V | todo, con aviso (`AVISO`) |
| **6,0 V → ~5,5 V** | **motores cortados, OLED encendida** ← la ventana que se quiere |
| < ~5,5 V | **se cae el riel de 5 V: se apaga también la OLED** (la pantalla la maneja el MCU, que es el que necesita 3,3 V de ese riel) |

⇒ El umbral de **6,00 V no es arbitrario: está justo donde el buck empieza a fallar**, o sea que la
pantalla se apaga en cuanto el pack baja un poco más. Sirve para decir «recarga», no para quedarse
mirando. Si algún día hiciera falta una ventana larga habría que **buck-boost** en vez de buck; hoy
no compensa.

**Qué LEDs se pueden apagar de verdad:**

| LED | ¿Lo apaga el firmware? |
|:--|:--|
| Onboard `PC13` | ✅ sí (GPIO) |
| **Verde del módulo** (`R2` + `LED1` sobre `VCC` = 9 V) | ❌ **no**: está cableado al riel de motores. Luce mientras el boost esté vivo |
| **Rojo de `nFAULT`** | ✅ **solo**: al dormir el driver (`nSLEEP` = 0) `nFAULT` queda en alta impedancia y el LED se apaga |

⇒ **El verde del módulo se quedará encendido** aunque el firmware corte lo demás: es el único LED que
contradice la política, y no hay forma de evitarlo sin cortar el boost.

⚠️ **Cortar por debajo de 6,0 V con los motores en marcha puede dar un falso positivo**: el pack
cede 0,3–0,5 V en un arranque. La medida ya es media de 16 muestras, pero el umbral conviene
aplicarlo sobre el valor **en reposo** (o con una confirmación de unos cientos de ms), o el robot se
parará solo en mitad de una aceleración con la batería sana.

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
| `P1` | 1 | `EEP` = **`nSLEEP`** | **`PA15`** en la V1 (el prototipo lo tiene a 5 V; ver la sección de `nSLEEP`) |
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

En la **V1 se cambia `nSLEEP` a un GPIO** (`PA15`): el prototipo se queda como está, pero la placa
nueva gana el apagado del driver por software y el paro automático si el MCU se cuelga. Ver la
sección de `nSLEEP` más abajo.

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

| Puerto | Ocupados | Estado |
|:--|:--|:--|
| **PA** | PA0–PA1 (encoder izq.) · PA2–PA5 (ADC pared) · PA6–PA7 (encoder der.) · PA8–PA11 (PWM) · PA12 (botón OK) · PA13–PA14 (SWD) · **PA15 (`nSLEEP`)** | **completo** (PA15 era JTDI; lo libera el JTAG desactivado) |
| **PB** | PB0–PB1 (ADC batería) · PB3–PB5, PB8 (emisores IR) · PB6–PB7 (I2C) · PB9 (botón UP) · PB10–PB11 (USART3) · PB12–PB15 (SPI2) | **`PB2` libre**: es `BOOT1`, y con `BOOT0 = 0` su valor es **indiferente** (AN2586, tabla 2) |
| **PC** | PC13 (LED) · PC14, PC15 (botones MENU y DOWN) | ninguno |

⇒ **Un GPIO libre: `PB2`.** `PA15` queda ocupado por `nSLEEP` (ver más abajo). Y no hay más sin quitar algo:

| Para liberar… | Se recuperan | Coste |
|:--|:-:|:--|
| Decodificador **2→4** para los emisores IR (74AHC139) | **+2** | 1 integrado, y se pierde la posibilidad de encender dos a la vez (hoy nunca se hace) |
| Un botón menos (4 → 3) | +1 | Cambia la UI |
| El LED de `PC13` | +1 | Solo 3 mA, y el LED integrado de la placa no se puede quitar |

⚠️ **`PC13`–`PC15` no son «pines libres disfrazados»**: el LED y dos botones van ahí porque eran los
que quedaban, no porque sean buenos pines (PC13 aguanta 3 mA como salida).

---

## `nSLEEP` y `nFAULT`: cómo usarlos

### Lo que hace el chip ante una avería (datasheet, tabla 3 y §7.3.2)

- **La protección es de reintento automático**, no un pestillo: ante sobrecorriente, el puente afectado
  se desactiva, `nFAULT` baja, y **se reactiva solo** pasado `t_OCP` = **1,35 ms**. Si la avería sigue,
  el ciclo se repite indefinidamente (3,3 A de disparo, 4 µs de *deglichting*).
- `nFAULT` es **salida *open-drain***: en reposo está en alta impedancia y hay que subirla.
- Solo se desactiva **el puente** donde se detecta la avería; el otro sigue funcionando.

### Por qué una AND directa **no** es la solución

La idea —`nSLEEP` = `MCU_habilitar` **AND** `nFAULT`— tiene un problema de realimentación, porque
`nFAULT` es una **salida del propio chip**:

```
avería → nFAULT baja → nSLEEP baja → el driver duerme → sin corriente → la avería desaparece
       → nFAULT sube → nSLEEP sube → el driver despierta → avería otra vez → …
```

Con la AND a secas eso **oscila**, y el resultado práctico es el mismo reintento automático que ya
hace el chip, con el sueño/despertar de propina. Para que corte de verdad hace falta **memorizar** la
avería: un **biestable** (74AHC1G74) o dos transistores, que mantengan `nSLEEP` a 0 hasta que el MCU
lo suelte explícitamente. Eso son 1 integrado, 2 pines y una señal más que depurar.

### Y no protege del fallo que sí da miedo

Una AND con `nFAULT` **no evita que un firmware colgado deje el PWM al 100 %**: en ese caso no hay
avería del driver hasta que el robot choca, y el choque suele llegar tarde. Para eso el STM32F103
trae gratis el **`IWDG`** (perro guardián independiente, con su propio oscilador interno): si el lazo
deja de refrescarlo, el MCU **se reinicia** y las entradas `xIN` caen a su pull-down ⇒ el puente queda
en *coast*. Cuesta **0 pines y 0 componentes**.

### `nFAULT` como indicador visual (sin pin)

Idea: no leerlo con el MCU, pero **verlo** con un LED. Es buena para un robot que hace intentos de
~15 s con alguien mirando.

#### Dos LEDs en antiparalelo **no** funcionan

`nFAULT` es **salida *open-drain***: solo puede **hundir** a 0 V, **nunca empujar** a 3,3 V. Con dos
LEDs en antiparalelo entre `nFAULT` y `3V3`:

| LED | Camino | ¿Puede lucir? |
|:--|:--|:--|
| Rojo (ánodo en `3V3`) | `3V3` → LED → `nFAULT` (a 0 V) | ✅ **cuando hay avería** |
| Verde (ánodo en `nFAULT`) | necesitaría `nFAULT` > `3V3` + V_F | ❌ **nunca**: no existe esa tensión |

⇒ El par antiparalelo se comporta como **un solo LED rojo**: lucen en avería, apagado el resto. No da
verde.

#### El verde ya está puesto

El módulo trae **`R2` (4k7) + `LED1` entre `VCC` y GND**, y `VCC` es el riel de 9 V del boost ⇒ **el
LED verde del módulo ya significa «boost vivo»**. Solo falta el rojo.

#### Circuito (2 componentes)

```
3V3 ──┬── [10 kΩ] ──────────┬──── nFAULT (open-drain del DRV8833)
      │                     │
      └── [1 kΩ] ── LED rojo ─┘      ánodo hacia 3V3 · cátodo hacia nFAULT
```

| Elemento | Valor | Cuenta |
|:--|:--|:--|
| `R` del LED | **1 kΩ** | (3,3 − 1,9) / 1 k ≈ **1,4 mA**. El pin aguanta **5 mA** (`V_OL` = 0,5 V a 5 mA) |
| *Pull-up* | **10 kΩ** | Deja a `nFAULT` un nivel definido (3,3 V) y apaga el LED con seguridad cuando no hay avería |

#### Por qué se ve fijo y no parpadeando

La protección del DRV8833 **reintenta cada `t_OCP` = 1,35 ms** y en cada ciclo deja `nFAULT` a 0 V:

| Estado | Duración |
|:--|--:|
| `nFAULT` bajo (puente desactivado) | **1,35 ms** |
| `nFAULT` alto (reintento) | ~4 µs (`t_DEG` del OCP) |

⇒ **99,7 % del tiempo encendido** ≈ 740 Hz, invisible al ojo. **Rojo fijo = avería persistente.**

⚠️ `nFAULT` también baja por **subtensión de `VM`** y por **sobretemperatura**, no solo por
sobrecorriente. Con la LiPo cerca del final, un rojo puede significar «el riel se hunde», que es
información igual de útil.

> 💡 Si algún día se quiere **además** que el MCU lo sepa, basta llevar `nFAULT` a **`PB2`** como
> entrada con pull-up interno (así el `10 kΩ` externo sobra). Cuesta el último pin libre; hoy no hace
> falta porque el LED ya avisa a quien mira.

### Decidido (2026-11-12): `nSLEEP` a un GPIO, `nFAULT` sin usar

`nFAULT` **no se cablea** (no hace falta enterarse de la avería). El que se usa es **`nSLEEP`**, y no
tanto por el consumo como por lo que pasa cuando el firmware falla.

#### Lo que se ahorra de verdad (números del datasheet)

| Concepto | Valor | Fuente |
|:--|--:|:--|
| Consumo del DRV8833 **despierto** (`I_VM`) | **1,7–3 mA** | `drv8833.pdf`, eléctricas |
| Consumo del DRV8833 **en *sleep*** | µA (figura 2) | `drv8833.pdf` |
| **MT3608 en reposo** (PFM, sin conmutar) | **100–200 µA** | `mt3608.pdf`, eléctricas |
| **MT3608 apagado** (`V_EN` = 0) | **0,1–1 µA** | `mt3608.pdf`, eléctricas |
| Tiempo de arranque del driver (`t_WAKE`) | **1 ms** | `drv8833.pdf` |

⚠️ **Dormir el driver no apaga el boost**, porque **el módulo MT3608 no tiene `EN` accesible** (el
integrado sí lo tiene, pin 4, pero los módulos comerciales lo atan a `VIN`). `nSLEEP` ahorra el
consumo del **DRV8833**, no el del convertidor.

#### ¿Merece la pena? Los dos números, sobre una batería de 450 mAh

| Qué se apaga | Consumo | Al día | Veredicto |
|:--|--:|--:|:--|
| **DRV8833** (`nSLEEP`) | 1,7–3 mA | **≈16 %** | ✅ **Sí**: es el 16 % de la batería por día |
| **MT3608** (solo con `EN` = 0) | 100–200 µA | ≈0,8 % | ❌ No compensa buscarlo en el módulo |
| MT3608, riel de 9 V sin carga (no hay forma de apagarlo aquí) | 100–200 µA | ≈0,8 % | — |

⇒ **Dormir el DRV8833 sí; perseguir el `EN` del boost, no.** El convertidor se queda como está y su
consumo en reposo es un 5 % del del driver.

> 💡 Si en la **V1** el boost no va como módulo sino con el **MT3608 desnudo** (SOT23-6), el pin `EN`
> queda disponible y se podría apagar con el mismo GPIO. Ya puestos, sale gratis; pero la ganancia
> es de 0,8 % al día, así que no debe ser un criterio para elegir.

#### El argumento que sí pesa: qué pasa si el firmware se cuelga

| `nSLEEP` atado a… | Si el MCU se cuelga o se resetea |
|:--|:--|
| **5 V** (como está hoy) | El pin sigue a 5 V, **TIM1 sigue generando PWM** con la última comparación ⇒ **los motores siguen girando** |
| **Un GPIO del MCU** | El pin queda en alta impedancia ⇒ el **pull-down interno de 500 kΩ** lo lleva a 0 ⇒ **el driver duerme y los motores se paran** |

Es decir: mover `nSLEEP` al MCU convierte un fallo de firmware en un robot parado en vez de un robot
que se va recto contra la pared. Y `t_WAKE` = **1 ms** es despreciable frente a una celda de 180 mm
(≈600 ms a 300 mm/s), así que se puede dormir el driver entre movimientos sin coste.

⚠️ Ojo con el arranque: al resetear, el GPIO está en alta impedancia, así que **el driver arranca
dormido**. Es lo deseable (no hay tirones antes de que el firmware configure `TIM1`), pero el firmware
tiene que **activarlo explícitamente** antes del primer movimiento.

#### Reparto final de los pines

| Pin | Función | Nota |
|:--|:--|:--|
| **`PA15`** | **Salida `nSLEEP`** | Push-pull. En reposo a 0. No necesita nada externo |
| **`PB2`** | **Libre** (reserva) | `BOOT1`, indiferente con `BOOT0` = 0. Útil para la V2 |

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

- **D1**: si el microcontrolador va desnudo o como módulo. ⚠️ **Argumento nuevo (2026-11-12)**: el
  F103C8 **no tiene pines para la V2** (solo queda `PB2` libre; la succión no cabe). Como el MCU se va
  a **cambiar de todas formas**, no tiene sentido invertir en un LQFP48 desnudo en la V1: el módulo (o
  un zócalo) se tira en cuanto llegue la V2, y el esfuerzo va a la placa. Sigue pendiente de cerrar
  junto con la decisión de una o dos alturas.
- **La forma de la placa**: el contorno sale del **FreeCAD del chasis**, que se va a rehacer a mano
  (el `chasis-v1.py` y su DXF no se usan).
- **El interruptor de encendido**: ✅ **decidido que se pone** (2026-11-12). Falta el tipo, la
  posición (accesible sin desmontar) y si corta solo `VBAT` o también el conector de balance.
- **El modelo concreto de boost** (≥2 A de salida) y la disponibilidad del MP2338.
- **Conectores de motores**, pads de SWD/UART y test points.
- **Los conectores**: JST XH para batería y motores es el candidato (hay hoja de datos en
  `Datasheets/jst-xh.pdf`), pero no está decidido.
- **La posición de los sensores**: las cotas del montaje en «V» dependen del chasis nuevo.
- **El valor final de $R_L$**: se cierra con las pruebas 1 y 15, no en el esquemático.
