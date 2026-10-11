# Bring-up: pruebas de banco

> Doc canónica y registro de resultados: [`Documentacion/pruebas-bringup.md`](../../../../Documentacion/pruebas-bringup.md)

## 1. Filosofía

`app/application.c` **es un banco de pruebas**, no la aplicación final. Se elige la rutina comentando/descomentando una línea en `APP_Run()`.

Reglas:

1. **Una prueba = un periférico.** Si falla, el culpable es único.
2. **Toda prueba debe ser observable**: telemetría por USART3 (formato Teleplot) o LED.
3. **Toda prueba debe ser repetible**: se ejecuta 10 veces antes de darla por buena.
4. **Aísla antes de culpar al módulo**: loopback del bus, continuidad del cableado, lectura de `WHO_AM_I`.
5. **Nada entra en el lazo de control sin pasar su prueba.**

### Freno de banco (trabajar en el hardware)

Para que los motores no giren mientras se toca el hardware: **`while(1){;}` en `main.c` antes de
`APP_Run()`** (preferencia del usuario; más adelante se elegirá el modo con un define).

⚠️ Consecuencia: con `--gc-sections` el enlazador elimina `APP_Run()` y **todas** las `App_Test_*`
del binario. Síntoma: el `.bin` no crece al añadir código y
`strings .pio/build/*/firmware.elf | grep -c BTN` → 0. **Quitar el bucle para probar.**

## 2. Catálogo de pruebas

| # | Rutina | Valida | Salida esperada | Estado |
|:-:|:-------|:-------|:----------------|:------:|
| 1 | `App_Test_ADC()` | ADC1 + sensores IR + telemetría | 5 trazas `>IR0..IR4` entre 0 y 4095, sensibles al acercar la mano (⚠️ `IR4` lee PB0, que según el pinout es **batería**) | ✅ |
| 2 | `App_Test_MotoresEncoders()` | PWM + driver + encoders en bucle cerrado | Avanza exactamente 50 pulsos al 25 % y frena; `>1A/1B/2A/2B` coherentes | ✅ |
| 3 | ~~`App_Test_ADC_PWM()`~~ | ADC → PWM directo | — | ➖ **Retirada** (2026-09-28): el robot no lleva potenciómetros |
| 4 | `App_Test_Encoders()` | Encoders sin motores | Pulsos suben al girar la rueda a mano; reset tras 5 s sin actividad | ✅ |
| 5 | `App_Test_IMU()` | MPU6500 por SPI2 | `>AX/AY/AZ` (g) y `>GX/GY/GZ` (°/s) estables y coherentes al mover la placa | ✅ |
| 6 | `App_Test_SPI2_Loopback()` | TX/RX del MCU (diagnóstico) | Con puente PB15→PB14, RX = `0xA5`; si no, el problema es del MCU/bus | ✅ |
| 7 | `App_Test_CalibracionEncoders()` | PPR del encoder, reducción y µm/cuenta | Modo 1 (a mano, 10 vueltas): debe salir **PPR = 7** (dato del fabricante) · Modo 2 (motor): distancia ≈660 mm | ❌ |
| 8 | `App_Test_Botones()` | Botones MENU/UP/DOWN/OK + antirrebote 30 ms | Una línea `>BTN:n` por pulsación (**1 MENU · 2 UP · 4 DOWN · 8 OK**) + `>BTN_MASK` a 10 Hz | ➖ |
| 9 | `App_Test_PWM_Rampa()` | PWM de **un motor o los dos** en lazo abierto con rampas **medidas en ticks de encoder** (PA8/PA9 izq · PA10/PA11 dcha), frenada en seco | Rampa 0→100 % en **1500 ticks** (500 ticks = 1 s) · freno · 2 s · rampa −100→0 % en 1500 ticks · 2 s, en bucle (`>PWM_FASE`, `>PWM_DUTY`, `>PWM_TICKS`, `>PWM_TPS`, `>ENC_IZQ/DER`, `>PWM_EVT`, `>PWM_AVISO`) | ⚠️ v1 izq ✅ (18-sep) · v2 por ticks ❌ |
| 10 | `App_Test_BotonesOLED()` | Botones + **OLED SSD1306 128×32** por I2C1 | Al pulsar: pantalla con nombre+pin (`MENU` / `PC14`), `>BTN:n` + `>BTN_PIN:PC14`, y al arrancar estado del bus (`>I2C_*`) | ✅ (20-sep) |
| 11 | `App_Test_I2C_Scan()` | **Bus I2C1**: niveles en flotante + escaneo 0x08..0x77 | `>I2C_SCL:1` `>I2C_SDA:1` y `>I2C_FOUND:60` (60 = 0x3C); 0 y 0 ⇒ faltan pull-ups | ✅ (21-sep) · `SCL=1 SDA=1 FOUND=60` ⇒ el módulo ya trae pull-ups |
| 12 | `App_Test_Bateria()` | **Batería 2S** con divisores **22 kΩ/10 kΩ** (montados; k = 0.3125, sin condensadores): pack (PB0), celda 1 (PB1), celda 2 por resta + **aviso** LED/OLED. Canales sin medida: PB1 ≈0 V (banco con fuente) ⇒ `>CELDA1_USADA:0` y solo manda el pack; PB0 ≈0 V (USB/ST-Link) ⇒ `>PACK_PRESENTE:0` y no clasifica nada. La celda **solo monitoriza**: como mucho sube el estado a AVISO. **La medida se calibra con `BAT CAL <mV>`** (`>BAT_K`) | `>VBAT`/`>VCELL1`/`>VCELL2` (±3 % vs multímetro), `>VBAT_ESTADO` (0/1/2, lo decide el pack), `>PACK_PRESENTE`, `>CELDA1_USADA`, `>CELDA_AVISO`, `>BAT_K` y `>ADC_B0`; con la batería baja, LED parpadeando y OLED con `BATERIA`/`CELDA` | ❌ |
| 13 | `App_Test_ADC_Soldadura()` | Chequeo de la soldadura del ADC: 4 receptores IR (PA2–PA5) + 2 divisores de batería (PB0/PB1). Mide **con los 4 emisores apagados y encendidos** ⇒ veredicto automático (no hay que tapar nada) | `>ADC_IRn`, `>ADC_DIFn`, `>ADC_ESTn` (`OK/MUDO/SATURADO/FLOJO`), `>ADC_B0/B1` y `>ADC_EST_B0/B1` (`OK/SIN DIVISOR/MAL DIVISOR/CANALES CRUZADOS/NO USADA`), a 2 Hz | ❌ *implementada 30-sep* |
| 14 | `App_Test_EmisoresIR()` | Los 4 emisores `WALL L1..L4` (PB3/PB4/PB5/PB8) por **BC337**, con JTAG desactivado | `>IR_LED:1..4` uno a uno, **`5` = los cuatro**, `0` = apagados (1 s por paso); visibles con la **cámara del móvil**; ≈1,65 V en cada **22 Ω** ⇒ **75 mA** por emisor (≈300 mA los cuatro) | ❌ *implementada 30-sep* |
| 15 | `App_Test_IR_Receptores()` | **Matriz 4×4** emisor↔receptor y elección de $R_L$: fondo de ambiente vs reflejo | Enciende cada emisor (`>IR_EMISOR:1..4`) y publica `>IRn_OFF`, `>IRn_ON`, `>IRn_DIF` de los cuatro receptores (ciclo de 1 s); anotar la pareja en `ir_pareja_emisor[]` | ❌ *implementada 30-sep* |
| 16 | `App_Test_IR_Modulado()` | Lectura diferencial ON−OFF **por software**: cancela la luz ambiente (8 pares por canal, con la pareja de `ir_pareja_emisor[]`) | `>IRn_DIF` estable ±10 % y **`>IRn_VAR`** (dispersión de la diferencia en %) al encender y apagar la luz de la sala; 4 Hz | ❌ *implementada 30-sep* |
| 17 | `App_Test_CalibracionIR()` | Curva distancia↔ADC a 3/5/8/10/15 cm, guiada por OLED (`UP`/`DOWN` canal, `OK` congela 64 muestras) | `>IR_CAL_CANAL`, `>IR_CAL_CM` e `>IRn_CAL` por punto; al acabar publica la tabla y estima en vivo `>IRn_CM` (error < 1 cm) | ❌ *implementada 30-sep* |
| 18 | `App_Test_PID()` | **Lazo de velocidad por rueda** (capa CONTROL): rampa de consigna + PID → duty | `>PID_SET` frente a `>VEL_IZQ/DER` y `>DUTY_IZQ/DER`; escalón sin sobreimpulso > 20 % y las dos ruedas a la misma consigna. Ganancias ajustables **en caliente** con `PID KP …` | ⚠️ lógica validada en simulación (PC) · falta sintonizar en hardware |
| 19 | `App_Test_Odometria()` | **Odometría diferencial** (capa FILTER): incrementos de arco de las ruedas → pose $(x,y,\theta)$. **El robot se empuja a mano**, no mueve motores | `>ODO_X`, `>ODO_Y` (mm), `>ODO_TH` (°), `>ODO_D` (camino), `>ODO_UM_L/R` (µm por rueda); recto ⇒ X sube y θ ≈ 0. **OK** = origen a cero | ✅ **Ejecutada el 1-oct**: recta de 1 m con **−0,3 %** de error y giro de 90° **clavado (+90° / −90°)** tras calibrar la **trocha efectiva a 77,6 mm** (procedimiento en `pruebas-bringup.md` §15) |

> Todas las pruebas marcadas ✅ están confirmadas en hardware. Pendientes de **ejecutar**: la
> nº 7 (PPR/µm por cuenta), la nº 9 **v2** (rampas por ticks, en las dos ruedas), la nº 12
> (divisores de batería) y las nº **13–17** (bloque IR: ya escritas el 30-sep, sin ejecutar).
> La nº 3 se **retiró** (28-sep) y la nº 8 quedó sustituida por la nº 10.

### Prueba 7 · calibración del encoder

Se elige el modo con `#define CAL_MODO` dentro de `app/application.c`:

| Modo | Qué hace | Para obtener |
|:----:|:---------|:-------------|
| `1` | Cuenta pulsos mientras se gira la rueda a mano 10 vueltas | **PPR** y verificar la reducción |
| `2` | Avanza hasta 2800 pulsos por rueda y frena (cada rueda para por separado) | **µm/cuenta** midiendo con la regla |

Claves de la implementación: se hace una **doble llamada** a `update_encoder_readings()` tras
resetear los contadores (así la primera diferencia es siempre 0 y el total queda limpio), y se
imprime a **10 Hz** porque a 115200 baudios no da para más.

Valores de referencia con Ø21 mm, PPR 7 y 1:10: **2800 pulsos ⇒ 660 mm** ⇒
**≈236 µm/cuenta**. Si el PPR sale **no entero**, la reducción asumida es falsa (lo que se mide
es el producto $n\cdot\text{PPR}$): ajustar `CAL_REDUCCION` hasta que salga entero.

Procedimiento completo: [`Documentacion/pruebas-bringup.md`](../../../../Documentacion/pruebas-bringup.md#7-procedimiento-calibración-del-encoder-paso-a-paso).

Marcas de arranque en `APP_InitAll()`: `>BOOT:init`, `>IMU:init...`, `>IMU:init done`.
Si no llega `init done`, el cuelgue está en `IMU_Init()`.

## 3. Formato de telemetría (Teleplot)

Una línea por variable, prefijo `>` y `:` entre etiqueta y valor:

```
>IR0:1832
>AX:-0.02
>GZ:1.4
>1A:37
```

Implementado en `app/telemetry.c` sobre `UART_SendData()` (USART3, PB10/PB11, 115200 baudios).
Monitor: `platformio device monitor -b 115200`, y pegar la salida en Teleplot.

⚠️ **Todo valor es NUMÉRICO** (decisión del usuario, 30-sep): `>ETIQUETA:numero`, sin textos.
Los estados y los veredictos van como **código entero** (`>APP_EST:1`, `>MENU_EST:2`, `>ADC_EST0:3`),
con la tabla en la documentación del módulo, y **los mensajes de ayuda no se mandan por telemetría**
(viven en `Documentacion/`). Así Teleplot dibuja todas las etiquetas en vez de descartar las que no
son números. Para depurar está la **traza de las FSM**: `>APP_EST`, `>MENU_EST`, `>MENU_PROF`,
`>MENU_IDX`, `>APP_TEST`, `>APP_TEST_N`.

### Entrada: órdenes por el MISMO puerto (30-sep)

El robot recibe líneas de texto y las ejecuta `app/console.c` (recepción por **interrupción** en
`hal/uart.c`, anillo de 64 B: no se pierde nada mientras corre una prueba):

```
BAT LIST                → publica los umbrales de batería en uso
BAT CRIT_PACK 6000      → cambia un umbral (sin punto = mV; con punto = V: 6.0)
PID KP 0.35             → ganancia del lazo de velocidad (prueba 18), en caliente
PID VEL 300             → consigna del lazo en mm/s
PID LIST                → publica ganancias y consigna
HELP                    → lista las órdenes
```

Los valores se validan en su capa (`BATTERY_SetUmbral()` en SENSOR; `APP_PID_SetGanancia()` en APP) y
**no se guardan en Flash**: al reiniciar vuelven a los `#define BAT_*_DEF` / `PID_TEST_*_DEF`.
Protocolo completo: `Documentacion/comandos-serie.md`.

- Se atiende en el menú (1 kHz) y **dentro de las pruebas que se sintonizan en vivo**: la 12 (umbral
  viendo la tensión) y la 18 (ganancias viendo el motor).
- ⚠️ **Una prueba nueva que se ajuste en caliente tiene que llamar a `CONSOLE_Update()` en su bucle**:
  estas pruebas son bloqueantes y, sin esa llamada, las órdenes se quedan en el anillo hasta volver al
  menú, que es justo cuando ya no sirven.
- El reparto de capas se respeta: la consola solo deja el ajuste escrito en su dueño, y es el lazo
  quien lo empuja a CONTROL (`CTRL_SetGanancias()`) en su paso siguiente.

## 4. Añadir una prueba nueva

1. Declara `void App_Test_XXX(void);` en `app/application.h`.
2. Implementa la rutina en `app/application.c`, que **sale con MENU**: `while (!BUTTONS_SalidaSolicitada())`.
3. Añade su fila **al bloque que le toque** de `application.c`, con `{ "ETIQUETA", MENU_TIPO_LANZAR, App_Test_XXX, 0U },`:
   · `menu_bateria[]` · `menu_ir[]` · `menu_motores[]` · `menu_telemetria[]` · `menu_exploracion[]` ·
   `menu_resolucion[]` · `menu_ajustes[]` · `menu_diagnostico[]`.
   ⚠️ Si la tabla es **nueva**, va también en `submenus[]` (una posición más) y el índice que le pases
   a la fila del padre tiene que ser **el suyo**: ese array es un contrato y un índice mal puesto abre
   **la tabla equivocada sin error de compilación**.
   ⚠️ La etiqueta se dibuja con la fuente de `oled.c`: **tiene el alfabeto A–Z completo desde el
   2026-10-11**, pero **ni minúsculas ni acentos**, y tiene que caber en **21 caracteres** con el
   cursor `>` delante.
4. Registra la fila en la tabla de la sección 2 y en `Documentacion/pruebas-bringup.md`.
5. Ejecuta 10 veces y anota resultado (✅/❌) y `>WHO_AM_I`/valores medidos si aplica.

Esqueleto mínimo (estilo: Doxygen al principio, sin comentarios dentro y una función = una tarea):

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

## 5. Calibraciones pendientes/asociadas

| Calibración | Procedimiento | Dónde se guarda |
|---|---|---|
| `micrometers_per_count` | Marcar 1 m, avanzar en recta, `1e6 / pulsos_totales` | `encoder.h` / llamada a `set_micrometers_per_count()` |
| Trocha efectiva $L$ | ✅ **Medida el 1-oct: 77,6 mm** (era 83,6). Girar **5 vueltas** sobre sí mismo y despejar $L = \frac{\Delta s_d - \Delta s_i}{5\cdot 2\pi}$ con la telemetría de la prueba 19 | `ODOM_TROCHA_MM_DEF` en `filter/odometry.h` |
| Curva IR distancia | Medir ADC a 3, 5, 8, 10, 15 cm → tabla + interpolación | `filter/adc_filter.c` |
| Offset de giroscopio | Estático 3 s, promediar GX/GY/GZ y restar | `sensor/imu.c` |
| Batería | `V_bat = ADC_BAT * k` con k de un valor medido con multímetro | `hal/adc.c` |

## 6. Resolución de problemas

| Síntoma | Comprobación primero |
|---|---|
| `IMU:init...` sin `init done` | Cableado SPI (SDA→MOSI, ADO→MISO) y prescaler /64 |
| `WHO_AM_I` = `0xFF` | Cruce de datos o CS mal configurado (PB12 como GPIO out) |
| Encoder no cuenta | Pines PA0/PA1 o PA6/PA7, modo encoder TI12, y que se llame a `ENC_Init()` |
| Encoder cuenta siempre en un sentido | Canal B no llega: gira a mano y mira los pulsos A/B por separado |
| Motor no gira | `PWM_StartAll()`, canal TIM1 correcto, y `DRV8833_Init()` por motor |
| Motores giran al revés | Invertir `channel1`/`channel2` del motor o el sentido en `DRV8833_Set_Speed()` |
| Telemetría vacía | `MX_USART3_UART_Init(115200)`, monitor en `/dev/ttyUSB0`, baudios correctos |
| ADC siempre 0 o 4095 | Canal/pin correcto en `ADC_ReadPin()`, y modo de entrada analógica |
