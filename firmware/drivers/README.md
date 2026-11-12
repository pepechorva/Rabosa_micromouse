# `drivers/` — capa de dispositivo

Aquí vive **el único código que sabe cómo es este robot por dentro**: qué motor, qué
encoder, qué sensor, en qué pin.

Es la otra mitad de la frontera que define [`../core/core_io.h`](../core/core_io.h):

| Capa | Qué es | ¿Sabe del STM32? |
|:--|:--|:--|
| `core/` | La **aplicación**: perfiles, movimiento, mapa, navegación | **No** |
| **`drivers/`** | Los **dispositivos**: motores, encoders, paredes, reloj | **Sí** |
| `hal/` | Los periféricos del MCU: ADC, PWM, timers, UART, I2C, SPI | Sí |
| `hw/` | Registros y pines | Sí |

## Por qué está separada de `hal/`

`hal/` responde a «cómo se enciende el TIM1»; `drivers/` responde a «cuánto hay que
pedirle al motor para ir a 300 mm/s». Son dos preguntas distintas y cambian por motivos
distintos: `hal/` cambia si **cambia el microcontrolador**; `drivers/` cambia si **cambia
el robot** — otro motor, otra reductora, otro encoder, otro sensor.

Y es aquí donde van los **ajustes finos**: la calibración de µm por cuenta, las ganancias
del lazo de velocidad, el *feedforward* de fricción, los umbrales de pared. Todo eso es de
este robot concreto, y **ninguno de esos números debe aparecer en `core/`**.

## Estado

⚠️ **Todavía no hay código.** La carpeta existe con este README porque **el contrato se
escribió antes que la implementación**, a propósito: la interfaz de `core/core_io.h` sale
de lo que el movimiento va a pedir, no de lo que hoy es cómodo de implementar.

Se llenará cuando `core/motion` empiece a usar el contrato. Entonces:

1. Añadir `"drivers"` a la lista de `../extra_script.py`.
2. Implementar las funciones de `core/core_io.h` sobre `hal/` y `sensor/`.
3. Y **mover aquí** lo que sea de dispositivo y hoy esté mal colocado:
   `hal/drv8833.c` (es un chip de fuera, no un periférico del MCU) y los módulos de
   `sensor/` que dan magnitud física.

## Lo que NO va aquí

- Lógica de movimiento, perfiles, mapas: eso es `core/`.
- Registros del MCU: eso es `hal/` y `hw/`.
- El menú, la telemetría o el banco de pruebas: eso es `app/`.
