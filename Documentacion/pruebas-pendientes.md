# 🎯 Pruebas y trabajo pendiente

> Lista viva de lo que falta, ordenada por bloques y con **criterio de aceptación** en cada punto.
> Lo ya hecho está en [historial-pruebas.md](./historial-pruebas.md) y el catálogo de rutinas de
> banco en [pruebas-bringup.md](./pruebas-bringup.md).

> ⚠️ **Aviso (2026-10-02): esta lista está desfasada y algunas de sus pruebas ya no se pueden
> ejecutar.** El recorte de Flash de esa fecha **retiró del menú** las pruebas 2, 3, 4, 5, 6, 7, 9, 10,
11, 12, 13, 14, 15 y 19 (ver el aviso del catálogo en
[pruebas-bringup.md](./pruebas-bringup.md)), así que **los puntos de la §2 sobre el PPR, los µm por
cuenta y la trocha no se pueden repetir tal cual**: sus números ya están medidos y viven en los
ajustes. Antes de dar por pendiente algo de aquí, **comprobar que su prueba sigue en el menú**.

---

## 1. Hardware que falta montar

> ⏳ **En curso (2026-09-28)**: se está **acabando de soldar el bloque analógico** — los divisores de
> batería y los 4 canales de sensores de pared. Al terminar: **prueba 13** (chequeo de la soldadura)
> y después 14 → 17 (procedimiento en [pruebas-bringup.md](./pruebas-bringup.md) §13).
> El diseño del menú que lanzará estas pruebas está en [maquina-estados-menu.md](./maquina-estados-menu.md).

| Qué | Valores | Para qué | Criterio |
|:----|:--------|:---------|:---------|
| **Divisores de batería** | 2× 22 kΩ + 2× 10 kΩ ✅ **montados (30-sep)** | Medir pack y celda 1 (prueba 12) y calibrar con `BAT CAL` | `>VBAT` y `>VCELL1` dentro del ±3 % del multímetro |
| **Emisores IR** | 4× **220 Ω** | Sensores de pared: `WALL L1..L4` = **PB3/PB4/PB5/PB8** | **Prueba 14**: los 4 se ven con la cámara del móvil y el consumo total es ≈**37 mA** |
| **Receptores IR** | 4× **3 kΩ** (**sin condensadores**) | Leer el reflejo en PA2…PA5 | **Pruebas 15–16**: `DIF` > 50 cuentas con pared a 10 cm y estable al cambiar la luz de la sala |
| **Conector de batería e interruptor** | JST XH-2.54 | Encendido/apagado limpio | — |
| ~~Medir el nivel alto de los encoders~~ | ✅ **Hecho (21-sep)**: polímetro en `C1` con el encoder a **5 V** ⇒ **≈3,6 V de pico a pico** | El módulo limita internamente la salida (el pull-up no va directo a `Vcc`) | Nada que hacer: queda dentro del máximo absoluto (4,0 V) de PA0/PA1/PA6/PA7. Si se quiere margen: `VCC` a 3,3 V (§4.2) |
| Colocación física de los **emisores** IR | ✅ **Fijada (30-sep)**: L1 frontal izq · L2 lateral der · L3 lateral izq · L4 frontal der | La mide la prueba 14 | Ver [hardware.md §4.4](./hardware.md#44-sensores-de-pared-ir) |
| Colocación física de los **receptores** IR | ✅ **Fijada (30-sep)**: R0 frontal der · R1 lateral der · R2 lateral izq · R3 frontal izq | Deducida del emparejamiento emisor↔receptor de la prueba 15 | Geometría en «V» alineada a 3–10 cm |

## 2. Calibraciones y medidas

| Qué | Procedimiento | Criterio |
|:----|:--------------|:---------|
| **PPR del encoder y reducción** | Prueba 7 **modo 1**: girar la rueda 10 vueltas a mano → debe salir **PPR = 7** (dato del fabricante) | PPR entero y repetible 10/10 |
| **µm por cuenta** | Prueba 7 **modo 2**: avanzar hasta 2800 pulsos y medir con la regla | Distancia ≈**660 mm** (hoja de cálculo: 235,6 µm/cuenta) · ajustar con `set_micrometers_per_count()` |
| **Trocha efectiva $L$** | Girar el robot 10 vueltas sobre sí mismo (720° con la IMU) → $L = 2\pi d/\theta$ | Insumo para girar 90° con error < 2° |
| **Radio de rodadura $r$** | Avanzar una distancia conocida y despejar de $d = r\theta$ | — |
| **Offset del giroscopio** | Robot quieto 3 s, promediar GX/GY/GZ | Deriva < 1°/min |
| **Pareja emisor↔receptor y $R_L$** | Prueba 15: encender cada emisor y ver qué canal cambia; ajustar $R_L$ hasta que el fondo no sature | Los 4 pares identificados y `DIF` > 50 cuentas a 10 cm |
| **Curva de los sensores IR** | Prueba 17: medir el ADC a 3, 5, 8, 10 y 15 cm de una pared blanca | Tabla + interpolación, error < 1 cm |
| **Calibrado de la batería** | `BAT CAL <mV>` con el multímetro (prueba 12) | Error < 2 % |

## 3. CONTROL — lazo cerrado

1. ~~**PID de velocidad por rueda**~~ ✅ **implementado el 2026-09-28** (`control/pid.c` +
   `setpoint.c` + `controller.c`) y **verificado en simulación**; falta **sintonizarlo sobre el
   robot** con la prueba 18 ([control-pid.md](./control-pid.md) · [pruebas-bringup.md](./pruebas-bringup.md) §14).
   - Sintonía práctica (Ziegler-Nichols sobre el robot, método heredado de las notas de estudio):
     1. Subir `Kp` hasta que la velocidad **oscile de forma sostenida**.
     2. Anotar `Ku` (Kp crítica) y `Tu` (periodo de oscilación).
     3. `Kp = 0.6·Ku` · `Ki = 2·Kp/Tu` · `Kd = Kp·Tu/8`.
   - Visualización: `Setpoint` vs `Real` en Teleplot.
   - **Criterio**: escalón de velocidad sin sobreimpulso > 20 %; ambos motores a la misma
     consigna ⇒ el robot va recto.
2. ~~**Rampas de consigna** en `setpoint.c`~~ ✅ implementadas (28-sep): nunca un escalón directo a
   los motores (protege el DRV8833, la LiPo y evita que patinen las ruedas).
3. **PID lateral** (error = distancia_izquierda − distancia_derecha), más lento que el de velocidad.
4. **Protección de batería**: no arrancar la misión si `BATTERY_Estado` es aviso/crítico (el módulo
   `sensor/battery.c` ya da el estado).

## 4. FILTER — estimación

- Filtro de la señal del ADC (media móvil / mediana).
- `filter/odometry.c`: integración de (x, y, θ) a partir de los encoders.
- Fusión encoder + IMU (`iekf.c`).
- **Criterio**: ángulo y distancia estimados con error < 5 % frente a la medida manual.

## 5. Movimiento

| Primitiva | Criterio |
|:----------|:---------|
| `parar()` | — |
| `avanzar_celda()` (18 cm) | Error < 5 mm |
| `girar_90()` | Error < 2° |
| `centrar_pasillo()` | Se mantiene centrado en un pasillo recto |

## 6. Navegación

1. **Wall follower "mano derecha"** (algoritmo de referencia, de las notas de estudio):
   - Si hay pared al frente → girar a la izquierda.
   - Si no hay pared a la derecha → girar a la derecha.
   - Si hay pared a la derecha y no al frente → avanzar una celda.
   - Limitar los giros consecutivos para evitar bucles.
   - **Criterio**: recorre el laberinto 10/10 sin tocar paredes.
2. **Flood fill** sobre el mapa del laberinto.
3. Construir el **laberinto 4×4** (celda de 18 cm, DM + listones).

## 7. Firmware / plataforma

| Tema | Nota |
|:-----|:-----|
| **Máquina de estados** (`BOOT → MENU → {TEST, CAL, DIAG, EXPLORAR} → STOP/FAULT`) | ✅ **Implementada el 2026-09-28** en `application.c`: `APP_Update()` a 1 kHz (botones + batería + FSM). `CONTROL`/`FILTER` siguen pendientes, y con ellos el estado de misión |
| **Menú con botones + OLED** | ✅ **Implementado el 2026-09-28**: `app/menu.c/h` con tablas `const` (añadir una prueba = **una fila**) + glifos nuevos `F G R X`, `+`, `/`, `>` en `app/oled.c`. El botón MENU va por **interrupción** (EXTI PC14) y sale de cualquier prueba |
| ~~`APP_Pump()` para las pruebas bloqueantes~~ | ❌ **Ya no hace falta**: la salida se comprueba en la condición del bucle (`while (!BUTTONS_SalidaSolicitada())`) y el botón lo cuenta la interrupción, que responde aunque la prueba no llame a nadie |
| ~~Prueba 3 (`App_Test_ADC_PWM`)~~ | ➖ **Retirada el 2026-09-28**: el robot no lleva potenciómetros (era un banco de la primera fase). Función borrada del firmware |
| **Modular los emisores IR por PWM** | Cancela la luz ambiente (restar encendido − apagado). **Sin condensador en el receptor**, no hay límite de frecuencia para el PWM |
| **DMA del ADC** | Lectura continua sin bloquear la CPU |
| **CONTROL y FILTER** | Hay que escribirlos de cero: el *stash* que los anunciaba es un snapshot obsoleto (ver §8) |
| **KiCad / PCB V2** | Aparcado a propósito (decisión del 2026-09-21) |
| **Sustituir el buck MP1584** | ⚠️ El MP1584 está **NRND**: su hoja de datos lleva el sello «NOT RECOMMENDED FOR NEW DESIGNS · **REFER TO MP2338**». Candidato natural: el **MP2338**. El módulo actual funciona y se queda en el prototipo, pero para la **PCB V2** hay que elegir otro: **5 V @ ≥1 A** desde **6–8,4 V**, **Iq de µA** (batería de 450 mAh), encapsulado pequeño y ciclo de vida activo. Valorar un **buck-boost** para no depender del corte a ≈6 V. Criterios en [hardware.md](./hardware.md) §4.6 |
| **Actualizar la batería** | La LiPo 450 mAh es **provisional**: el usuario valorará una más nueva. **Cuando lleguen sus especificaciones** hay que actualizar `hardware.md` §4.6 (capacidad, C, peso, conector), el BOM §7 y revisar el **presupuesto de consumo** y los **divisores** si cambia la tensión |
| **BLE HJ-580** | Aparcado (módulo comprado, sin usar) |

## 8. Nota sobre el *stash* obsoleto

`git stash list` tiene una entrada llamada *«capas pendientes: control, filter, imu, sensor, menu,
oled, spi»* (31 ficheros, 1660 líneas) que **no contiene ningún fichero de `control/` ni de
`filter/`**: es un *snapshot* del firmware **anterior** a la reorganización por capas. Sus versiones
de `application.c`, `hal/*`, `src/main.c`, `encoder.c` y `platformio.ini` están **obsoletas**
respecto al árbol actual.

→ **Decisión (2026-09-21): ignorarlo.** No se integra y no cuenta como trabajo hecho. `CONTROL` y
`FILTER` se escribirán de cero, empezando por el PID de velocidad de §3. (El stash se puede borrar
cuando se quiera: `git stash drop`.)
