# Hardware del Rabosa-MVR

> Doc canónica: [`Documentacion/hardware.md`](../../../../Documentacion/hardware.md) · Pinout: [`Documentacion/Pinout.pdf`](../../../../Documentacion/Pinout.pdf)

## 1. Resumen

| Bloque | Componente | Interfaz |
|---|---|---|
| MCU | STM32F103C8T6 (Blue Pill), 72 MHz, 128 kB | — |
| Motores | 2× **GA12-N20 (B1215)** 6 V 1500 RPM con encoder 2 canales y montura · modelo `GA12-N20-S0310D-E` (12 V de catálogo, rebajable a 6/3 V) | PWM + TIM encoder |
| Reducción · encoder | **1:10** (caja GA12) · **7 PPR** por canal · 100 kHz · salida AB en colector abierto con pull-up de 3,3 kΩ ⇒ **280 cuentas** y **235,6 µm** por vuelta de rueda (`ENCODER_UM_POR_CUENTA`) | — |
| Ruedas | **Ø24 mm con goma** (llanta Ø20, agujero **en D** de Ø3,15), 8 mm de ancho ⇒ $r = 12$ mm · **µm/cuenta medido: 287,0** (el nominal de Ø24 es 269,3) | — |
| Chasis | PLA 106.7 × 84 mm · **trocha efectiva medida: 77,6 mm** ✅ (1-oct, prueba 19); los 83,6 mm son la cota geométrica entre ejes | — |
| Driver | **DRV8833** (confirmado): A = **PA8/PA9** → `motor1` = **izquierda** · B = PA10/PA11 → `motor2` = **derecha** | 4× PWM desde TIM1 |
| IMU | Módulo MPU-9250/6500/9255 (IC marcado `MP65888LB1`) | SPI2 |
| Paredes | 4 emisores **SFH 4550** (860 nm, $\pm$3°) conmutados por **transistor BC337** (base 1 kΩ, LED con **22 Ω** ⇒ **75 mA** por emisor) + 4 fototransistores **ST-1KL3A** (880 nm, $\pm$6°, $R_L$ **3 kΩ**). ⚠️ **Solo uno encendido cada vez** (barrido `L1→L2→L3→L4`) | **PB3/PB4/PB5/PB8 → base** (nivel alto) + ADC1 |
| Batería | LiPo 2S 7.4 V **450 mAh 80C** | ADC: PB0 (total) + PB1 (celda 1) con divisores **22 kΩ/10 kΩ** (**22 kΩ ARRIBA**: al revés el pin se va a 5,09 V y PB0 no es 5 V tolerante) (**sin condensadores**: no hay sitio) (k = 0.3125, 2.625 V a 8.4 V, 0.39 mA) · aviso por LED 1/4 Hz + OLED (`sensor/battery.c`) · **solo el pack protege** (CRÍTICA < 6,6 V ⇒ FAULT, **del que se sale con OK**): la celda es monitorización y como mucho sube a AVISO · umbrales **configurables en caliente** (`BAT CRIT_PACK 6000`, `BATTERY_SetUmbral()`) · canales sin medida: `BATTERY_Celda1Presente()` (PB1 ≈0 V) y `BATTERY_PackPresente()` (PB0 ≈0 V ⇒ no clasifica) |
| Alimentación | **MT3608** boost 9 V + **MP1584** buck 5 V (Blue Pill, **OLED** y **encoders N20**). El **3,3 V lo genera el regulador de la propia Blue Pill** (IMU, emisores y receptores IR). El módulo **AMS1117 externo NO regula**: es el soporte físico del display. Los emisores van **de uno en uno** (75 mA): solo la prueba 14 enciende los 4 a la vez (≈300 mA) | — |
| Botones | **MENU=PC14 · UP=PB9 · DOWN=PC15 · OK=PA12** (pull-up **interno**, activo a bajo, sin resistencias; ✅ probados el 2026-09-20) | `app/buttons.c` |
| Pantalla | **OLED SSD1306 128×32** en **I2C1** (PB6=SCL, PB7=SDA) · dirección **0x3C** (prueba 0x3D) · **alimentada a 5 V** · giro con `OLED_ROTACION_180` | `app/oled.c` (APP, usa `hal/i2c`) |
| LEDs | Solo el integrado de la placa (PC13). Los indicadores extra, solo en la PCB | GPIO |
| Depuración | **ST-Link por SWD** (JTAG desactivado) + conversor TTL↔USB (BLE aparcado) | SWD / USART3 |

**Geometría para la cinemática**: $r = 12$ mm (Ø24 con goma), $L = 77.6$ mm ⇒ $L/r \approx 6.47$,
$\dot{\theta} \approx 0.155\,(\omega_d-\omega_i)$. $v_{max} \approx 1.65$ m/s a 1500 RPM.
✅ **La trocha efectiva ya está medida** (1-oct, prueba 19): con los 83,6 mm entre ejes un giro de
90° a mano se estimaba en 83° ⇒ $L = 83{,}6 \times 83{,}5/90 = 77{,}6$ mm. Es **menor que la
geométrica** (deslizamiento al pivotar): **no se mide con el calibre, se deduce del robot**. La
odometría usa **solo $L$** (el radio va dentro de los µm/cuenta de la prueba 7). Valor en
`ODOM_TROCHA_MM_DEF`; procedimiento en `Documentacion/pruebas-bringup.md` §15.

## 2. Pinout (oficial V1)

> Fuente: `Documentacion/Pinout.pdf` (LibreOffice Calc, 3 sep 2026). Se lee con
> `pdftotext -layout Documentacion/Pinout.pdf -`. Si cambia el ODS, regenera el PDF.

| Pin | Puerto | Función HW | Uso | Etiqueta |
|---|---|---|---|---|
| 10 | PA0 | TIM2_CH1 | Encoder 1A | M1ENCA |
| 11 | PA1 | TIM2_CH2 | Encoder 1B | M1ENCB |
| 12 | PA2 | ADC1_IN2 | Sensor de pared | WALL A1 |
| 13 | PA3 | ADC1_IN3 | Sensor de pared | WALL A2 |
| 14 | PA4 | ADC1_IN4 | Sensor de pared | WALL A3 |
| 15 | PA5 | ADC1_IN5 | Sensor de pared | WALL A4 |
| 16 | PA6 | TIM3_CH1 | Encoder 2A | M2ENCA |
| 17 | PA7 | TIM3_CH2 | Encoder 2B | M2ENCB |
| 29 | PA8 | TIM1_CH1 | PWM Motor 1A | PWM1A |
| 30 | PA9 | TIM1_CH2 | PWM Motor 1B | PWM1B |
| 31 | PA10 | TIM1_CH3 | PWM Motor 2A | PWM2A |
| 32 | PA11 | TIM1_CH4 | PWM Motor 2B | PWM2B |
| 33 | PA12 | GPIO | **Botón MENU** (entrada, pull-up) | BTN_MENU |
| 34 | PA13 | SWDIO | Programación y debug — **no tocar** | SWDIO |
| 37 | PA14 | SWCLK | Programación y debug — **no tocar** | SWCLK |
| 38 | PA15 | GPIO | Libre (era JTDI; JTAG desactivado) | — |
| 18 | PB0 | ADC1_IN8 | Batería: total (VCC−GND) | ADC_BAT 1 |
| 19 | PB1 | ADC1_IN9 | Batería: celda 1 | ADC_BAT 2 |
| 20 | PB2 | BOOT1 | Programación y debug | BOOT1 |
| 39 | PB3 | GPIO | Emisor IR 1 (salida, JTAG desactivado) | WALL L1 |
| 40 | PB4 | GPIO | Emisor IR 2 (salida, JTAG desactivado) | WALL L2 |
| 41 | PB5 | GPIO | Emisor IR 3 (salida) | WALL L3 |
| 42 | PB6 | I2C1_SCL | I2C | SCL |
| 43 | PB7 | I2C1_SDA | I2C | SDA |
| 45 | PB8 | GPIO | Emisor IR 4 (salida) | WALL L4 |
| 46 | PB9 | GPIO | **Botón DOWN** (entrada, pull-up) | BTN_DOWN |
| 21 | PB10 | USART3_TX | Telemetría / programación | TX |
| 22 | PB11 | USART3_RX | Telemetría / programación | RX |
| 25 | PB12 | SPI2_CS | SPI (IMU) | CS |
| 26 | PB13 | SPI2_SCK | SPI (IMU) | SCK |
| 27 | PB14 | SPI2_MISO | SPI (IMU) | MISO |
| 28 | PB15 | SPI2_MOSI | SPI (IMU) | MOSI |
| 2 | PC13 | GPIO | **LED onboard** (salida). PC13–PC15: máx 3 mA, solo en salida | — |
| 3 | PC14 | GPIO | **Botón OK** (entrada, pull-up, `OSC32_IN`) | BTN_OK |
| 4 | PC15 | GPIO | **Botón UP** (entrada, pull-up, `OSC32_OUT`) | BTN_UP |

> La numeración «LED 1…6» del pinout original (PA12, PA15, PB9, PC13, PC14, PC15) es **histórica**:
> cinco de esos pines se reasignaron a botones y emisores IR. Solo PC13 sigue siendo un LED.

Asignación analógica (`hal/adc.h`): A2→ch2 · A3→3 · A4→4 · A5→5 · B0→8 · B1→9.

**PWM de motores**: `MX_TIM1_Init()` (`hal/timers.c`) pone **PA8–PA11** (TIM1 CH1–CH4) como
salida alterna, así que `gpio.c` no toca esos pines. `PWM_PIN_MASK` (`include/pinout.h`) es una
máscara **de GPIOA**: aplicarla a GPIOB chocaría con el emisor IR `WALL L4` (PB8) y el botón
DOWN (PB9). ✅ **Izquierda/derecha identificados en banco (2026-09-19)**: `motor1` (PA8/PA9) =
izquierda, `motor2` (PA10/PA11) = derecha.

**Encoders** (modo ya programado en `sensor/encoder.c`: `TIM_ENCODERMODE_TI12` ×4, pull-up, 16 bits):
**izquierda PA0=A/PA1=B (TIM2)** · **derecha PA6=A/PA7=B (TIM3)** — cableado confirmado por el usuario
el 2026-09-19, sin cambios de configuración. ⚠️ `MX_GPIO_Init()` deja PA4–PA7 analógicos: los devuelve
a entrada `ENC_Init()`, así que **el orden de `APP_InitAll()` importa**. Si un encoder cuenta al revés,
intercambiar sus hilos A/B.

⚠️ **Los encoders van a 5 V (usuario, 2026-09-21) y PA0/PA1/PA6/PA7 NO son tolerantes a 5 V**: son
pines `TTa` (llevan ADC: `ADC12_IN0/1/6/7`) y en el datasheet salen como `I/O` **sin** la marca `FT`
(leyenda: «FT = 5 V tolerant»; PA8 sí lo es). Máximo absoluto de «any other pin» = **4.0 V**, y con
5 V se inyecta corriente en `VDD` por el diodo de protección. Opciones: (A) alimentar el encoder a
**3,3 V**; (B) **divisor 10 kΩ serie + 20 kΩ a GND** por canal ⇒ 3,33 V, **desactivando el pull-up
interno** (`GPIO_NOPULL`, si no el nivel alto cae a ≈2,9 V); (C) mover TIM2/TIM3 a pines `FT`
(PA15/PB3, PB4/PB5) → **descartada, choca con los emisores IR**.

## 3. Cableado crítico (causa habitual de fallo)

**IMU por SPI — cruce de datos.** En el módulo MPU:

- `SDA/SDI` es **entrada** de datos → va a **MOSI (PB15)**.
- `ADO/SDO` es **salida** de datos → va a **MISO (PB14)**.

Si se cruzan, el `WHO_AM_I` devuelve `0xFF` y no hay respuesta. Configuración validada:
SPI2 modo 0, prescaler /64 (≈562 kHz; el MPU-9250 admite ~1 MHz máx.),
`WHO_AM_I` = `0x70` (MPU-6500) o `0x71` (MPU-9250).

**JTAG desactivado, SWD únicamente (gotcha resuelto, fácil de romper otra vez).**
La placa se programa solo por **SWD**: SWDIO=**PA13**, SWCLK=**PA14**, más GND y 3.3 V.
`gpio.c` llama a `__HAL_AFIO_REMAP_SWJ_NOJTAG()` al principio de `MX_GPIO_Init()`, que deja
SWD activo y libera **PA15 (JTDI)**, **PB3 (JTDO)** y **PB4 (NJTRST)** como GPIO.

- Es **imprescindible** para los **emisores IR L1/L2 (PB3/PB4)**.
- ⚠️ `__HAL_AFIO_REMAP_SWJ_DISABLE()` liberaría también PA13/PA14, pero **pierdes el ST-Link**.
- ⚠️ `__HAL_AFIO_REMAP_SWJ_ENABLE()` volvería a ocupar PB3/PB4 y rompería los emisores IR.
- `gpio.c` tampoco debe reconfigurar **PA13/PA14**: son SWDIO/SWCLK, y dejarlos como GPIO corta
  la sesión de depuración del programa en ejecución.

**Botones** (asignación confirmada): MENU=**PA12** · UP=**PC15** · DOWN=**PB9** · OK=**PC14**.
Botón entre pin y GND con **pull-up interno** ⇒ activo a **nivel bajo**. Antirrebote por
software de **30 ms** en `app/buttons.c`, que exige llamar a `BUTTONS_Update()` a **1 kHz**.
API: `BUTTONS_Init()` · `BUTTONS_Update()` · `BUTTONS_IsPressed()` · `BUTTONS_WasPressed()` ·
`BUTTONS_GetMask()` · `BUTTONS_GetEvent()` (potencias de dos: 1 MENU · 2 UP · 4 DOWN · 8 OK ·
0 ninguno, los mismos bits de la máscara) + `TELEM_SendButton(n)` ⇒ `>BTN:n`.
Cableado: botón entre el pin y **GND** + **pull-up interno** (~30–50 kΩ, ~80 µA al pulsar) ⇒
**sin resistencias ni condensador externos**. Activo a nivel bajo; antirrebote software de 30 ms ⇒
`BUTTONS_Update()` a **1 kHz**.
⚠️ **I2C1 y pull-ups**: en el STM32F1 el pull-up interno **no funciona en `AF_OD`** (el HAL solo
usa `Pull` en modo entrada) ⇒ el bus I2C **necesita pull-ups del módulo o externos (2× 4.7 kΩ a
3.3 V)**. Diagnóstico: prueba 11 (niveles en flotante + escaneo de direcciones); si los pull-ups
son flojos, bajar `I2C1_CLOCK_HZ` a 50 kHz.

**Datasheets en el repo**: `Datasheets/{sfh4550,st-1kl3a,drv8833,stm32f103c8}.pdf`. Dato que
condiciona el montaje de los IR: el ST-1KL3A da **5 mA a solo 200 lx** ⇒ en continua la **luz
ambiente satura** el receptor; hay que medir y ajustar $R_L$, y a futuro **modular los emisores**
(PWM + resta encendido/apagado en firmware). Ambos son de haz estrecho (±3° y ±6°).(consume el evento) · `BUTTONS_GetMask()` (bit0 MENU … bit3 OK).
PC13 sigue siendo el LED onboard; PA15 queda libre.

⚠️ **PC14/PC15 son `OSC32_IN/OUT`** (posible cristal de 32.768 kHz en la placa) y, junto con
PC13, están limitados a **3 mA** — pero **solo en modo salida**. Como entradas no hay problema.
No poner LEDs indicadores en PC13–PC15.

## 4. Alimentación

```
LiPo 2S 7.4V ──┬─► MT3608 boost 9V ──► DRV8833 ──► Motores
 450mAh 80C    ├─► MP1584 buck 5V ──┬─► Blue Pill (5V) ──┬─► IMU (3.3V)
               │                     │   [regulador 3.3V] ├─► emisores IR (3.3V)
               │                     │                    └─► receptores IR (3.3V)
               │                     ├─► OLED SSD1306 (5V, lleva su propio LDO)
               │                     └─► encoders N20 (5V) ✅ probado en banco (medir nivel alto de C1)
               └─► ADC: PB0 (total) y PB1 (celda 1) — monitor de batería
```

⚠️ El **MP1584 es un buck**, no un buck-boost: el riel de 5 V se cae por debajo de ≈6 V de
batería (PB0/PB1 lo detectan). El módulo **AMS1117 externo no regula**: es el soporte del display.
⚠️ **Encoders a 5 V (medido 2026-09-21)**: el módulo admite 3,3–5 V y su salida es de colector
abierto con pull-up interno, pero **da ≈3,6 V de pico a pico** (limita internamente, no saca los
5 V del riel). PA0/PA1/PA6/PA7 son pines `TTa` (con ADC) y **no** toleran 5 V (máximo absoluto
4,0 V) ⇒ 3,6 V entra, aunque justo en el límite de `VDD + 0,3 V`. No hay que cambiar nada; si se
quiere margen para la PCB: `VCC` a 3,3 V (también válido) o divisor 10 kΩ/20 kΩ con `GPIO_NOPULL`.

## 4b. Emisores IR: límites del GPIO

Los 4 emisores SFH 4550 van por **GPIO directo** a 3.3 V, sin transistor. Ojo:

- $R = (3.3 - V_F)/I_F$ ⇒ **220 Ω para ≈9.3 mA** (4 emisores = 37 mA; con 100 Ω serían 20 mA cada uno).
- Pin del STM32F103: ≈20 mA recomendados. Suma de todos los pines ≈150 mA ⇒ 4×20 mA = 80 mA
  está al límite.
- Sin regulación: si baja la batería, baja la corriente del emisor y **cambian las lecturas**
  del receptor. Si hace falta más alcance → transistor o PWM (que permite filtrar luz ambiente).

## 5. Dar de alta hardware nuevo

Al recibir hardware nuevo del usuario, registra **siempre** estos campos:

```markdown
### <Componente>
- **Modelo / referencia:**
- **Interfaz:** (SPI2 / I2C1 / ADC1_INx / TIMx_CHy / GPIO)
- **Pines asignados:** (tabla pinout)
- **Alimentación:** (V y de qué riel)
- **Driver / capa:** (`hal/xxx.c` + `sensor/xxx.c`)
- **Prueba de banco asociada:** (`App_Test_XXX`)
- **Notas de cableado y gotchas:**
- **Estado:** ✅ probado / ⚠️ pendiente / ❌ no funciona
```

Y además:
1. Añade la fila al pinout de este fichero y al de `Documentacion/hardware.md`.
2. Comprueba el **conflicto de pines**: un pin no puede tener dos funciones.
3. Actualiza el diagrama de bloques del `README.md` si cambia la cadena de alimentación.
4. Añade la prueba de banco en `references/bringup.md` y `Documentacion/pruebas-bringup.md`.

## 6. Cabos sueltos

| Tema | Estado |
|---|---|
| Driver de motor | ✅ **Resuelto**: **DRV8833** |
| Sensores IR | ✅ **Resuelto**: 4 canales `WALL A1–A4` + 4 emisores `WALL L1–L4` |
| Reducción del N20 | ✅ **1:10** (tabla del fabricante: filas de 3000 RPM a 12 V / 1500 RPM a 6 V) |
| **PPR del encoder** | ✅ **7** por canal y vuelta de motor ⇒ **280 cuentas** por vuelta de rueda (×4, reductora 10) ⇒ **235,6 µm/cuenta** |
| **Trocha efectiva** | ⚠️ **Inconsistente** (82.8 vs 83.6 vs ≈74.8 mm) — crítico para los giros |
| Radio de rueda | ⚠️ Ø21 mm con goma, a confirmar al rediseñar |
| Resistencias de emisores IR | ✅ **Calculadas: 220 Ω** (≈9.3 mA por emisor, 37 mA los cuatro) |
| Niveles de los encoders | ✅ **Medido: ≈3,6 V de pico a pico** con el encoder a 5 V ⇒ dentro del máximo absoluto (4,0 V) de PA0/PA1/PA6/PA7 (`TTa`, sin `FT`). Se queda como está |
| Divisores de batería PB0/PB1 | ✅ **Soldados (30-sep)**; en el banco la celda 1 no se usa (fuente de alimentación) |
| Posición física de los canales IR | ⚠️ Sin decidir |
| Identidad de la IMU | ⚠️ `MP65888LB1` no es InvenSense conocido; decidir por `WHO_AM_I` |
| IMU SPI2 vs I2C | ⚠️ El código usa SPI2; `planificacion.md` dice I2C |
| `App_Test_ADC()` | ⚠️ Lee PB0 como si fuese un 5.º sensor de pared |
| **JTAG activo** | ✅ Corregido con `SWJ_NOJTAG()` en `MX_GPIO_Init()` |
| **Emisores IR L1–L3** | ✅ Corregido a `OUTPUT_PP`; verificar en hardware |
| **Botones** | ✅ Asignados e implementados; pendiente probar (prueba 8) |

> Los cabos sueltos se resuelven **midiendo**, no eligiendo una fuente por preferencia.

## 7. Lista de materiales (BOM)

Tabla completa en [`Documentacion/hardware.md`](../../../../Documentacion/hardware.md#7-lista-de-materiales-bom)
(la BOM con enlaces de compra está en `Documentacion/hardware.md` §7).

MCU STM32F103C8T6 · 2× **GA12-N20** (B1215) 6 V 1500 RPM con encoder (1:10, 7 PPR) · driver **DRV8833** · módulo
MPU-9250/6500/9255 · 4× **SFH 4550** + 4× **ST-1KL3A** · LiPo 2S 7.4 V 450 mAh 80C +
cargador · **MT3608** (9 V) y **MP1584** (5 V), con los **3,3 V del regulador de la Blue Pill** · 4 pulsadores
(MENU/UP/DOWN/OK) · LEDs indicadores solo en la PCB ·
ruedas Ø21 mm a rediseñar · chasis PLA 106.7×84 mm · ST-Link v2 (solo SWD) · TTL↔USB · laberinto 4×4.
