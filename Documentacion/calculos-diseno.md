# 🧮 Cálculos de diseño y lista de la compra

> Volcado de la sesión del **2026-09-21** para tener a mano los números de los sensores IR y de la
> batería, más la lista de lo que falta por comprar.
>
> Las fuentes **canónicas** siguen siendo [hardware.md](./hardware.md) §4.4 (sensores IR) y §4.6
> (alimentación y batería); este documento es el resumen de cálculos. Si cambia un valor, se cambia
> allí y se actualiza aquí.

---

## 1. Datasheets en el repositorio

| Fichero | Componente | Fabricante |
|:--------|:-----------|:-----------|
| `Datasheets/sfh4550.pdf` | Emisor IR **SFH 4550** | ams-OSRAM |
| `Datasheets/bc337.pdf` | Transistor **NPN** de conmutación (800 mA) | onsemi |
| `Datasheets/bc546-bc550.pdf` | Familia **NPN** de señal (BC546…BC550, 100 mA) | onsemi |
| `Datasheets/bc517.pdf` | **Darlington** NPN (BC517) | onsemi |
| `Datasheets/bc327.pdf` | **PNP** de conmutación (BC327/BC328, 800 mA) | onsemi |
| `Datasheets/bc556-bc558.pdf` | Familia **PNP** de señal (BC556/557/558, 100 mA) | onsemi |
| `Datasheets/st-1kl3a.pdf` | Fototransistor **ST-1KL3A** | Kodenshi |
| `Datasheets/drv8833.pdf` | Driver de motores **DRV8833** | Texas Instruments |
| `Datasheets/stm32f103c8.pdf` | **STM32F103C8** | ST |
| `Datasheets/um1850-…pdf` | Manual HAL/LL de la familia F1 | ST |
| `Datasheets/12MM Small Dc Gear Motor_PDF00.pdf` | Motorreductor **N20** (parcial) | — |

> [!TIP] Cómo se encontraron los dos primeros
> El centro de descargas de ams-OSRAM es una web con JavaScript y no expone la URL del PDF, pero se
> puede localizar la copia archivada con la API **CDX del archivo de Internet**:
> ```
> https://web.archive.org/cdx/search/cdx?url=<dominio>&matchType=domain&filter=original:.*PARTE.*&fl=original
> ```
> El del ST-1KL3A lo publica Kodenshi directamente en su web (`kodenshi.co.jp`).

---

## 2. Emisores IR — resistencias en serie calculadas

**Datos del SFH 4550** (datasheet del fabricante):

| Parámetro | Valor |
|:----------|:------|
| Tensión directa $V_F$ | **1.5 V típ. / 1.7 V máx.** (medida a $I_F$ = 100 mA) |
| Corriente directa $I_F$ | **100 mA** continua · 1 A pulsada ($t_p$ ≤ 400 µs, $D$ ≤ 0.005) |
| Potencia total | 180 mW |
| Longitud de onda pico | **860 nm** (centroide 850, FWHM 30 nm) |
| Intensidad radiante | 710…2800 mW/sr a 100 mA |
| **Ángulo semiapertura** | **±3°** ← haz muy estrecho |
| $V_R$ inversa | 5 V |

$$R = \frac{V_{CC} - V_F}{I_F} = \frac{3{,}3 - V_F}{I_F}$$

| $I_F$ objetivo | $V_F$ a esa corriente | $R$ calculada | **Valor E24** | $I_F$ real | Potencia en R | Los 4 juntos |
|:--------------:|:---------------------:|:-------------:|:-------------:|:----------:|:-------------:|:------------:|
| 5 mA | ≈1.2 V | 420 Ω | **470 Ω** | 4.5 mA | 9 mW | 18 mA |
| **10 mA** | ≈1.25 V | 205 Ω | **220 Ω** | **9.3 mA** | 19 mW | **37 mA** |
| 20 mA | ≈1.35 V | 98 Ω | **100 Ω** | 19.5 mA | 38 mW | 78 mA |
| 30 mA | ≈1.4 V | 63 Ω | 68 Ω | 28 mA | 53 mW | 112 mA ⚠️ |

> [!WARNING] La tabla de arriba vale para **GPIO directo** (sin transistor).
> En el robot hay montada una **22 Ω**, que pide ≈75 mA: con GPIO directo eso va **directo al pin**,
> que admite ≈20 mA. Con 22 Ω el transistor **no es opcional**.

### 2.1 Montaje elegido: 4 × BC337 + 22 Ω

**Transistor recomendado: BC337** (NPN, TO-92, [`Datasheets/bc337.pdf`](../Datasheets/bc337.pdf)).
Patillaje **1 = colector · 2 = base · 3 = emisor** (cara plana hacia ti, patas hacia abajo).

```
3,3 V ── LED SFH 4550 ── 22 Ω ──┬── C (1)
                               │
   PB3/PB4/PB5/PB8 ── 1 kΩ ── B (2)    BC337
                               │
                             E (3) ── GND
```

| Cuenta | Valor | Comentario |
|:-------|:------|:-----------|
| $I_F$ por emisor | $\dfrac{3{,}3 - 1{,}45 - 0{,}2}{22} \approx$ **75 mA** | $V_F$ ≈1.45 V a esa corriente, $V_{CE(sat)}$ ≈0.2 V |
| **Consumo del robot** | **≈75 mA** | **solo hay un emisor encendido cada vez** (decisión del usuario) |
| Los 4 a la vez | ≈300 mA | solo como prueba de esfuerzo del riel (prueba 14) |
| Corriente de base | $\dfrac{3{,}3 - 0{,}8}{1000} =$ **2.5 mA** | los 4 pines suman 10 mA (cabe de sobra) |
| $h_{FE}$ forzada | 75 / 2.5 = **30** | el BC337 tiene $h_{FE}$ ≥ 100 a 100 mA ⇒ satura de sobra |
| Potencia en la 22 Ω | $I^2R$ = **124 mW** | ¼ W aguanta justo ⇒ mejor **½ W** |
| Potencia en el LED | 109 mW | límite 180 mW ✅ |
| Potencia en el transistor | ≈15 mW | límite 625 mW a 25 °C ✅ (nada) |

Datos del BC337 que hacen falta (del datasheet): $V_{CEO}$ 45 V · $I_C$ **800 mA** continua ·
$P_D$ 625 mW a 25 °C (deriva 5 mW/°C) · $h_{FE}$ 100–630 a $I_C$ = 100 mA ·
$V_{CE(sat)}$ 0.7 V máx a 500 mA · $R_{\theta JA}$ 200 °C/W.

> [!IMPORTANT] Por qué se eligen **75 mA** y no los 100 mA del LED
> El SFH 4550 da 100 mA **a 25 °C de ambiente**, pero su curva de derating ($R_{\theta JA}$ = 450 K/W,
> $T_j$ máx = 100 °C) deja el límite en **≈74 mA a 50 °C** y ≈59 mA a 60 °C. Con 22 Ω estamos justo
> ahí: 75 mA es el punto sensato para un robot que se calienta. Y el premio por arriesgar a 100 mA
> sería pequeño: +33 % de luz ⇒ **+15 % de alcance**.
> Para más alcance, **pulsar**: 1 A con $t_p$ ≤ 400 µs y $D$ ≤ 0.005 (10× la intensidad radiante
> instantánea, 10× menos calor medio). Detalle en [`hardware.md` §4.4](./hardware.md#44-sensores-de-pared-ir).

> La base se lleva con **1 kΩ** a propósito: satura el transistor (β forzada 30, muy por debajo de la
> $h_{FE}$ mínima) y deja la corriente por pin en 2.5 mA. **Es obligatoria**: sin ella el pin daría
> ≈50 mA (límite absoluto 25 mA). Los pull internos (30–50 kΩ, y solo en modo entrada) no valen como
> sustituto. Justificación y alternativas si no hay sitio (SMD, array de 4, MOSFET de nivel lógico):
> [`hardware.md` §4.4](./hardware.md#44-sensores-de-pared-ir).
>
> El **10 kΩ de base a GND** es opcional pero recomendable: durante el arranque el pin del STM32 queda
> flotante y el transistor podría encenderse con el ruido (PB3/PB4, además, son pines de JTAG y llevan
> pull-up por defecto al reset).

> [!NOTE] Solo un emisor encendido cada vez
> Los cuatro LEDs **nunca lucen a la vez**: el firmware los barre (`L1 → L2 → L3 → L4`). El riel de
> 3,3 V solo tiene que dar **75 mA** (≈0.13 W extra en el AMS1117 de la Blue Pill) y cada receptor
> ve **de qué emisor** le llega la luz, así que se puede levantar la matriz 4×4 emisor↔receptor y
> elegir la mejor pareja por canal (pruebas 15 y 16).
>
> Si en alguna prueba se encienden los cuatro a la vez (la 14 lo hace un segundo) el riel pide
> ≈300 mA: mirar que no baje de ≈50 mV, porque el ADC usa $V_{DDA}$ como referencia.

### 2.2 Por qué no los otros transistores de la caja

| Parte | Tipo | $I_C$ máx | Veredicto |
|:------|:-----|:---------:|:----------|
| **BC337** | NPN | **800 mA** | ✅ **elegido**: margen grande de corriente, satura a 0.1–0.2 V a 75 mA |
| BC327 | PNP (800 mA) | 800 mA | ❌ es el complementario PNP del BC337: para conmutar por el lado de GND hace falta NPN |
| BC517 | NPN Darlington | 1.2 A | ❌ tienta porque la base casi no consume ($h_{FE}$ 30 000), pero su $V_{CE(sat)}$ de **1.0 V** (a 100 mA) deja la corriente en ≈40 mA con la misma 22 Ω |
| BC547 / 548 / 549 / 550 | NPN de señal | **100 mA** | ⚠️ funcionaría a 75 mA, pero sin margen y con menos $h_{FE}$: mejor para señal |
| BC556 / 557 / 558 | PNP de señal | 100 mA (200 de pico) | ❌ PNP, y encima de señal |

> La cuenta del Darlington, para verlo: $I_F = \dfrac{3{,}3 - 1{,}45 - 1{,}0}{22} \approx$ **38 mA**
> (hasta ~45 mA, porque a esa corriente $V_F$ baja). No está mal, pero es la mitad de alcance, disipa
> ≈70 mW por transistor en vez de 15 mW y no aporta nada aquí (la base del BC337 ya la mueve un GPIO
> sin esfuerzo).

---

## 3. Receptores IR — resistencia de carga

**Datos del ST-1KL3A** (datasheet del fabricante):

| Parámetro | Valor |
|:----------|:------|
| $V_{CEO}$ · $I_C$ máx · $P_C$ | 40 V · 50 mA · 150 mW |
| **Corriente de oscuridad** | **1 nA típ.** / 200 nA máx ($V_{CE}$ = 10 V) → despreciable |
| **Corriente de luz** $I_L$ | **1.5 mA mín / 5 mA típ** ($V_{CE}$ = 10 V, $E_v$ = **200 lx**) |
| $V_{CE(sat)}$ | 0.2 V típ. (a $I_C$ = 5 mA) |
| $t_r$ / $t_f$ | 3.2 / 4.8 µs (con $R_L$ = 100 Ω) |
| Sensibilidad espectral | 500–1050 nm · **pico 880 nm** |
| **Ángulo semiapertura** | **±6°** |

**Montaje**: colector a 3,3 V, emisor → $R_L$ → GND, **salida = el emisor** a PA2…PA5.
Así $V_{out} = I_C \cdot R_L$ **sube con la luz** (satura hacia 3,2 V).

| $R_L$ | 50 µA | 100 µA | 200 µA | 300 µA | 500 µA | $I_L$ = 5 mA (200 lx) |
|:-----:|:-----:|:------:|:------:|:------:|:------:|:---------------------:|
| 1 kΩ | 0.05 V | 0.10 V | 0.20 V | 0.30 V | 0.50 V | saturado |
| 2.2 kΩ | 0.11 | 0.22 | 0.44 | 0.66 | 1.10 | saturado |
| **3 kΩ** | **0.15** | **0.30** | **0.60** | **0.90** | **1.50** | saturado |
| 4.7 kΩ | 0.24 | 0.47 | 0.94 | 1.41 | 2.35 | saturado |
| 10 kΩ | 0.50 | 1.00 | 2.00 | 3.00 | —— | saturado |
| 22 kΩ | 1.10 | 2.20 | 3.2 (sat) | —— | —— | saturado |

**Elegido: 3 kΩ** (el usuario tiene 3 kΩ, 3,6 kΩ y 3,9 kΩ). Se monta **el más bajo** porque el margen
de ambiente es lo único que puede invalidar la medida —si se satura, la señal desaparece del todo—
mientras que la señal se recupera después pulsando más fuerte. Pierde un **36 % de señal** respecto a
4,7 kΩ y gana un **≈57 % de margen** (aguanta ≈1,07 mA de fondo antes de saturar, en vez de 0,68 mA).
Los otros dos valen como alternativa si, una vez medidas, el fondo queda cómodo y falta señal:
**3,6 kΩ** (+31 % de margen, −23 % de señal) y **3,9 kΩ** (+21 %, −17 %).
**Sin condensador** (no hay sitio en el chasis) y **a ajustar midiendo** con las pruebas 1 y 15.

### 3.1 Cómo se elige $R_L$ de verdad: dos medidas

$R_L$ es un **compromiso**, no un número fijo: con la resistencia grande se gana señal (el IR que
vuelve de la pared) pero se satura antes con la luz de la sala.

| $R_L$ más grande | $R_L$ más pequeña |
|:-----------------|:------------------|
| Más señal útil | Más margen contra la luz ambiente |
| Más fácil de saturar con la luz de la sala | Más cerca del ruido del ADC |

1. **Emisor APAGADO**, en la sala donde va a correr el robot y con el receptor **ya apantallado**:
   el fondo ambiente debe quedar en **≈1/3 del fondo de escala (≈1200–1400 cuentas, en torno a 1 V)**.
   Si sale pegado a 4095, ningún valor de $R_L$ lo arregla: el problema es el **apantallado**, no la
   resistencia.
2. **Emisor ENCENDIDO** a la distancia de trabajo (3–10 cm) y restar (ON − OFF): si la diferencia se
   queda por debajo de ~20–30 cuentas, **subir** $R_L$ (10 kΩ, 22 kΩ) siempre que el paso 1 siga
   dejando margen.

> El **ADC no es el que limita**: con el muestreo de 239.5 ciclos (21 µs) admite fuentes de hasta
> ~256 kΩ, así que 3,3–22 kΩ le da exactamente igual. Y la velocidad tampoco: $\tau = R_L \cdot C$
> ≈ 660 ns con 22 kΩ ($C$ ≈ 30 pF), frente a pulsos de 100 µs.

> [!WARNING] Apantallar manda más que el valor de $R_L$
> Con 200 lx el fototransistor puede dar **5 mA**; a 3 kΩ eso pediría 15 V, así que **se satura**:
> la salida se queda clavada en ≈3.1 V ($V_{CE}$ ≈ 0.2 V) y la señal desaparece. Es exactamente lo
> que pasa cuando el receptor ve el techo o la luz de la sala. La señal útil es el IR que vuelve de
> la **pared**, así que el receptor tiene que ver la pared y nada más: tubo negro y, si se puede,
> filtro IR. Con el receptor apantallado el fondo baja a décimas de mA y la 3 kΩ deja el margen
> justo para la señal.

> [!TIP] Montaje de prueba
> Si hay sitio, deja la $R_L$ **accesible** (o en un zócalo) durante la puesta a punto: es la pieza
> que más se cambia. Con **3 kΩ** montada, si sobra margen de ambiente y falta señal se sube a
> 3,6 kΩ / 3,9 kΩ (que ya tienes); si el fondo se satura, se baja a 1,5 kΩ (2× 3 kΩ en paralelo).

> [!WARNING] La luz ambiente manda
> El datasheet da **5 mA a solo 200 lx**: con $R_L$ = 4,7 kΩ serían 23 V, así que en una habitación
> iluminada la salida **se satura**. La señal útil (el IR reflejado en la pared) es una **diferencia**
> sobre ese fondo.
>
> - Si el ADC se queda en 4095 a todas las distancias → bajar $R_L$ (2,2 kΩ, 1 kΩ) y/o **apantallar**
>   el receptor (tubo negro, filtro IR) para que no vea el techo.
> - Si los valores son muy bajos (< 200 cuentas) → subir $R_L$ (10 kΩ, 22 kΩ).
> - Solución buena a futuro: **modular los emisores** con PWM (1–10 kHz) y restar en firmware
>   (encendido − apagado). La ambiente se cancela y se puede subir $R_L$ para ganar señal. El ADC ya
>   está preparado (12 MHz, 21 µs por conversión).

> [!IMPORTANT] Los dos son de haz estrecho (±3° y ±6°)
> A 10 cm, el emisor ilumina un punto de ≈1 cm. El montaje clásico en "V" (los dos apuntando a la
> pared, con los haces cruzándose a 3–10 cm) funciona bien, pero **exige alineación cuidadosa**.

---

## 4. Batería 2S — divisores de tensión

> [!IMPORTANT] Montaje real y **definitivo** (30-sep): **22 kΩ / 10 kΩ en el pack** y **3k9 / 10 kΩ en la celda 1**
> El cálculo de 100 kΩ/56 kΩ se sustituyó por los valores que había a mano. En el pack el pin queda al
> **80 % del fondo de escala** y la **impedancia de fuente baja a 6,9 kΩ**; en la celda 1 el 3k9 sube el
> pin al **92 % del fondo** con **2,8 kΩ** de impedancia. El consumo pasa de 81 µA a **0,56 mA**:
> irrelevante frente a los 44 mA del robot. ⚠️ **El orden importa**: al revés el pin del pack se iría a
> 5,09 V y PB0 **no** es tolerante a 5 V (máximo absoluto 4,0 V). ⚠️ Y ojo: **los dos divisores no son
> iguales**, así que el firmware lleva **una $k$ por canal** (con una sola, la celda 2 —que sale por
> resta— daba −1,29 V). Detalle y comprobación: [hardware.md §4.6](./hardware.md#46-alimentación-y-rieles).

```
Pack    : VBAT+ ──[ 22 kΩ ]──┬── PB0
                             │
                         [ 10 kΩ ]
                             │
                            GND

Celda 1 : CEL1  ──[ 3k9 ]───┬── PB1
                             │
                         [ 10 kΩ ]
                             │
                            GND
```

$$k_{pack} = \frac{10}{32} = 0{,}3125 \qquad k_{celda} = \frac{10}{13{,}9} = 0{,}7194$$

| Magnitud | Valor |
|:---------|:------|
| $V_{ref}$ | 3,3 V (`VREF+` = `VDDA`, regulador de la Blue Pill) |
| Atenuación a 8,4 V (pack lleno) | **2,63 V** en el pin (80 % del fondo) |
| Atenuación a 4,2 V (celda llena) | **3,02 V** en el pin (92 % del fondo) |
| Fondo de escala del pack | **10,56 V** |
| Fondo de escala de la celda 1 | **4,59 V** |
| Corriente del canal del pack | **≈ 260 µA** |
| Corriente del canal de la celda 1 | **≈ 300 µA** |
| **Consumo total** | **≈ 0,56 mA** (≈ 800 h con la batería de 450 mAh) |
| Impedancia de fuente | **6,9 kΩ** (pack) · **2,8 kΩ** (celda) |
| Resolución | **2,67 mV/cuenta** en el pack · **1,12 mV/cuenta** en la celda |

**Cuentas esperadas del ADC** (comprobación sin multímetro):

| Tensión | ADC | Tensión | ADC |
|:-------:|:---:|:-------:|:---:|
| 8,40 V (pack lleno) | ≈ 3258 | 4,20 V (celda llena) | ≈ 3750 |
| 7,40 V (nominal) | ≈ 2870 | 3,70 V (celda nominal) | ≈ 3304 |
| 6,60 V (3,3 V/celda) | ≈ 2560 | 3,00 V (celda vacía) | ≈ 2679 |
| 6,00 V (3,0 V/celda, corte) | ≈ 2327 | | |

> ⚠️ Las cuentas de la **celda** son con **su** divisor (3k9/10k). Con el del pack (22k/10k) saldrían
> ≈1629 / ≈1435 / ≈1164, que es justo lo que daba el firmware antes de separar las dos $k$.

**Cableado**: el canal del pack, del **positivo del pack** a R1; el de la celda, del **punto medio
de las dos celdas** (hilo central del conector de balance) a R1. Todos los GND al negativo del pack.
La **celda 2 se calcula por resta**: $V_2 = V_{pack} - V_1$.

### Lista de componentes del divisor

| Canal | Pin | R1 (batería → pin) | R2 (pin → GND) |
|:------|:---:|:------------------:|:--------------:|
| Pack completo | **PB0** | **22 kΩ** | **10 kΩ** |
| Celda 1 | **PB1** | **22 kΩ** | **10 kΩ** |
| Celda 2 | — | *por resta* | — |

> **Sin condensadores** (decisión del 2026-09-21: no hay sitio en el chasis). El muestreo del ADC es
> de 239,5 ciclos (21 µs) y el condensador interno del *sample-and-hold* es de ~8 pF ⇒ la carga del
> nodo (36 kΩ × 8 pF ≈ 0,3 µs) es 70 veces más rápida que el muestreo: **la conversión sale correcta
> sin condensador**. El ruido se combate con la **media de 16 lecturas** de `sensor/battery.c`.

> Con resistencias corrientes (5 %) y `VDDA` con otro ±1-2 %, el divisor se desvía varios puntos. Se
> corrige todo de golpe **calibrando con el multímetro**: `BAT CAL <mV>` (ver
> [comandos-serie.md §2](./comandos-serie.md#2-calibrar-la-medida-de-la-batería-bat-cal)), que
> recalcula el factor del divisor en `sensor/battery.c`.

> [!NOTE] Si algún día se cambia el divisor
> **27 kΩ / 15 kΩ** ($k$ = 0,357 → 3,00 V a 8,4 V, fuente de 9,6 kΩ) también vale: sube el consumo a
> 300 µA y deja más margen de tensión en el pin. Lo único prohibido es poner **más** resistencia
> arriba que abajo (el pin se sale del rango de 4,0 V).

---

## 5. Aviso de batería baja — cómo funciona

| Capa | Módulo | Responsabilidad |
|:-----|:-------|:----------------|
| **SENSOR** | `firmware/sensor/battery.c` | Mide (media de 16 lecturas) y **clasifica** |
| **APP** | `firmware/app/application.c` → `app_aviso_bateria()` | **Presenta**: LED onboard + pantalla OLED |

| Estado | Umbral | LED (PC13) | Pantalla |
|:-------|:-------|:-----------|:---------|
| `BATTERY_OK` | pack ≥ 6,60 V · celdas ≥ 3,30 V · desequilibrio < 0,40 V | apagado | — |
| `BATTERY_AVISO` | pack < 6,60 V (3,30 V/celda) o celda < 3,30 V o desequilibrio ≥ 0,40 V | parpadea **1 Hz** | estado + las tres tensiones (prueba 12) |
| `BATTERY_CRITICA` | pack < 6,00 V (3,00 V/celda) | parpadea **4 Hz** | FAULT: `MOTORES PARADOS` |

- `BATTERY_Update()` + `app_aviso_bateria()` se llaman **a 1 kHz** en el bucle de la prueba 12.
- La pantalla solo se **redibuja al cambiar de estado** (y luego 1 vez por segundo), porque un
  `OLED_Update()` son ~50 ms de bus I2C.
- Cuando exista la máquina de estados, **basta añadir esas dos llamadas a su bucle**.
  La protección de verdad (no arrancar con la batería baja) irá en CONTROL/APP.
- Telemetría: `>VBAT_ESTADO` (0 = OK · 1 = aviso · 2 = crítica) + `>VBAT`, `>VCELL1`, `>VCELL2`,
  `>VBAT_DESEQ`.

---

## 6. Lista de la compra

| Para qué | Valor | Cantidad |
|:---------|:------|:--------:|
| Divisores de batería: R1 (arriba) | **22 kΩ** ¼ W | 2 |
| Divisores de batería: R2 (abajo) | **10 kΩ** ¼ W | 2 |
| Emisores IR: en serie | **22 Ω** (½ W) \* | 4 |
| Emisores IR: base del transistor | **1 kΩ** | 4 |
| Emisores IR: transistor | **BC337** (NPN, TO-92) | 4 |
| Receptores IR: carga | **3 kΩ** | 4 |

> \* El **220 Ω** de la tabla del §2 era para GPIO directo. Con la 22 Ω montada y el BC337 se
> cumple el límite del SFH 4550 (75 mA de 100 mA) y el del pin (2.5 mA de 20 mA). La $R_L$ de 3 kΩ
> la tiene el usuario en la caja (junto con 3,6 kΩ y 3,9 kΩ, las alternativas).

> **Ningún condensador**: no hay sitio en el chasis (decisión del 2026-09-21).

---

## 7. Estado del repositorio tras esta sesión

- **Firmware**: compila (RAM 8 %, Flash 36,3 % con la prueba 10 activa).
- **Datasheets**: los 6 PDF de la tabla del punto 1, versionados en `Datasheets/`.
- **Pruebas pendientes de ejecutar**: 3, 7, 12 (batería, falta montar los divisores).
- **Ajustes del ADC ya aplicados** en `firmware/hal/adc.c`:
  1. Prescaler a **12 MHz** (`RCC_ADCPCLK2_DIV6`) — antes quedaba en 36 MHz, fuera de especificación.
  2. Tiempo de muestreo a **239.5 ciclos** → admite fuentes de hasta ~256 kΩ (divisores y
     fototransistores).
