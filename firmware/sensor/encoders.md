# Encoders de cuadratura — Motores N20

## Arquitectura

```
TIM2 (PA0=CH1, PA1=CH2)  ← encoder motor 1
TIM3 (PA6=CH1, PA7=CH2)  ← encoder motor 2
       │
       ▼  (modo TI12: 4× resolución)
__HAL_TIM_GET_COUNTER → uint16_t (0–65535)
       │
       ▼
max_likelihood_counter_diff(now, before)
       │
       ├─ Detecta dirección de giro (±)
       ├─ left/right_diff_count   (pulsos en este periodo)
       ├─ left/right_total_count  (acumulado)
       ├─ left/right_micrometers  (distancia)
       └─ left/right_speed        (velocidad en m/s)
```

## Método de lectura

### Anterior (reemplazado)
- Dos sistemas paralelos incompatibles:
  - **TIM encoder mode**: contador hardware crudo sin procesar.
  - **Software polling**: lectura GPIO por separado, sin detectar dirección (solo suma de flancos de subida, 1× resolución).
- Sin manejo de overflow del contador.
- Sin cálculo de distancia ni velocidad.

### Nuevo (mmlib)
- **Un solo método**: TIM encoder mode TI12 (4× resolución).
- **`max_likelihood_counter_diff()`**: resuelve dirección y overflow del contador de 16 bits mediante comparación de diferencias directa e inversa — la menor es la dirección real.
- **Lectura periódica**: se llama `update_encoder_readings()` a intervalo fijo (típicamente 1 kHz).
- **Salidas**: pulsos diferenciales (con signo), total acumulado, distancia en µm, velocidad en m/s.

#### Visual de `max_likelihood_counter_diff`

```
Caso avance:     before=65000, now=1000
  forward_diff  = (uint16_t)(1000-65000) = 1576   ← pequeño → es avance
  backward_diff = (uint16_t)(65000-1000) = 64000
  → resultado = +1576

Caso retroceso:  before=1000, now=65000
  forward_diff  = (uint16_t)(65000-1000) = 64000
  backward_diff = (uint16_t)(1000-65000) = 1576   ← pequeño → es retroceso
  → resultado = -1576
```

## API

### Inicialización

| Función | Descripción |
|---|---|
| `ENC_Init()` | Inicializa TIM2 y TIM3 en modo encoder (TI12). Configura pines GPIO (entrada **con pull-up**) y siembra `micrometers_per_count` con el valor de especificación. |
| `set_micrometers_per_count(float value)` | Establece el factor de calibración (µm/pulso). Permite afinarlo tras medir con la regla (prueba 7). |
| `get_micrometers_per_count()` | Devuelve el factor actual. |

### Constantes del motor + encoder (`encoder.h`)

El fabricante del GA12-N20 publica los datos, así que el factor ya viene puesto de fábrica:

| Constante | Valor | De dónde sale |
|---|---|---|
| `ENCODER_PPR_MOTOR` | **7.0f** | PPR por canal y vuelta del eje del motor (fabricante) |
| `ENCODER_REDUCCION` | **10.0f** | Reductora GA12 1:10 (fabricante) |
| `ENCODER_CUENTAS_POR_VUELTA` | **280** | `4 × PPR × reducción` (modo TI12 ×4) |
| `RUEDA_DIAMETRO_MM` | **24.0f** | Rueda Ø20 con goma: Ø24 en reposo |
| `ENCODER_UM_POR_CUENTA` | **≈235.6 µm** | `π × Ø × 1000 / cuentas por vuelta` |

> [!NOTE] Alimentación y niveles (2026-09-21)
> El encoder admite **3,3–5 V** y está alimentado al riel de **5 V** (así se hicieron las pruebas de
> banco de motores y encoders). **Medido**: la salida da **≈3,6 V de pico a pico**, o sea que el
> módulo limita internamente el nivel alto (el pull-up de 3,3 kΩ no va directo a `Vcc`).
> Con eso, PA0/PA1/PA6/PA7 (pines `TTa` con ADC, **no** tolerantes a 5 V, máximo absoluto 4.0 V)
> quedan dentro de especificación y muy por encima del umbral de lectura ⇒ no hay que cambiar nada.
> Detalle en `Documentacion/hardware.md` §4.2.

### Lectura periódica (llamar a 1 kHz)

| Función | Descripción |
|---|---|
| `update_encoder_readings()` | Lee contadores hardware, calcula diferencias, acumula totales, actualiza distancia y velocidad. |

> [!WARNING] La velocidad va **promediada**, y no es un adorno (arreglado el 2026-10-01)
> La versión anterior calculaba la velocidad con **las cuentas del último milisegundo**:
> `v = cuentas × 287 µm × 1000 Hz` ⇒ **cada cuenta valía 287 mm/s**. A 300 mm/s el robot hace ≈1,05
> cuentas por ms, así que la medida **saltaba entre 0, 287 y 574 mm/s**. Con esa realimentación ningún
> lazo podía funcionar: el PID no estaba mal sintonizado, **es que la medida era ruido**, y los motores
> iban a bandazos.
>
> Ahora `update_encoder_readings()` mantiene una **media móvil de 20 muestras** (≈14 mm/s de
> resolución, ~10 ms de retardo) y promedia **cuentas y tiempo medido**, no velocidades ya calculadas.
>
> | Ventana | Resolución de la velocidad |
> |:-------:|:--------------------------:|
> | 1 ms | **287 mm/s** ❌ |
> | 20 ms | **14 mm/s** ✅ |
>
> La regla general: la resolución vale `µm_por_cuenta / T_ventana`.
>
> ⚠️ Y el **`dt` se mide** con el reloj, no se supone: esta función también se llama cada 10 ms desde
> `app/motion.c`, y con el periodo escrito a fuego la velocidad salía **10 veces menor** allí.

### Getters

| Función | Devuelve | Unidad |
|---|---|---|
| `get_encoder_left_diff_count()` | Pulsos diferenciales (último período) | pulsos (±) |
| `get_encoder_right_diff_count()` | Ídem motor derecho | pulsos (±) |
| `get_encoder_left_total_count()` | Total acumulado de pulsos | pulsos |
| `get_encoder_right_total_count()` | Ídem motor derecho | pulsos |
| `get_encoder_left_micrometers()` | Distancia recorrida | µm |
| `get_encoder_right_micrometers()` | Ídem motor derecho | µm |
| `get_encoder_average_micrometers()` | Distancia promedio (izq+der)/2 | µm |
| `get_encoder_left_speed()` | Velocidad **promediada** (media móvil de 20 muestras) | m/s |
| `get_encoder_right_speed()` | Ídem motor derecho | m/s |

### Compatibilidad (funciones legacy)

| Función | Descripción |
|---|---|
| `ENC_GetCount1()` / `ENC_GetCount2()` | Lectura raw del contador TIM |
| `ENC_ResetCount1()` / `ENC_ResetCount2()` | Reset del contador TIM |
| `ENC_UpdatePulses()` | Software polling por canal (obsoleto) |
| `ENC_GetPulse1A/B()` / `ENC_GetPulse2A/B()` | Pulso por canal (obsoleto) |
| `ENC_ResetPulses()` | Reset pulsos (obsoleto) |

## Ejemplos de uso

### 1. Uso básico — odometría

```c
#include "sensor/encoder.h"

void setup(void)
{
    ENC_Init();

    /* Calibración: calcular según rueda y reducción.
     *
     * Fórmula: µm/pulso = (π × Ø_rueda_mm × 1000) / (PPR × 4 × reducción)
     *
     * Ejemplo: rueda 32 mm, encoder N20 16 PPR, reducción 1:30, TI12 (×4):
     *   µm/pulso = (π × 32 × 1000) / (16 × 4 × 30) ≈ 52.36
     */
    set_micrometers_per_count(52.36f);
}

void loop_1kHz(void)
{
    /* Llamar cada 1 ms (ej. desde systick o timer) */
    update_encoder_readings();

    int32_t distancia = get_encoder_average_micrometers();
    float velocidad = get_encoder_left_speed();

    /* Hacer algo con distancia y velocidad... */
}
```

### 2. Control proporcional de velocidad

```c
float target_speed = 0.5f;  /* 0.5 m/s */

void control_loop(void)
{
    update_encoder_readings();

    float current = get_encoder_left_speed();
    float error = target_speed - current;
    float pwm_output = error * KP_SPEED;  /* KP_SPEED a calibrar */

    DRV8833_Set_Speed(&motor1, (int32_t)pwm_output);
}
```

### 3. Avanzar una distancia exacta

```c
void move_millimeters(int32_t target_mm)
{
    int32_t target_um = target_mm * 1000;
    int32_t start = get_encoder_average_micrometers();

    ENC_ResetCount1();
    ENC_ResetCount2();

    while (get_encoder_average_micrometers() - start < target_um) {
        update_encoder_readings();
        DRV8833_Set_Speed(&motor1, 30);
        DRV8833_Set_Speed(&motor2, 30);
        HAL_Delay(1);
    }

    DRV8833_Brake(&motor1);
    DRV8833_Brake(&motor2);
}
```

### 4. Logging de datos para calibración

```c
void log_encoder_data(void)
{
    update_encoder_readings();

    printf("%ld,%ld,%.3f,%.3f,%ld,%ld,%ld\n",
           get_encoder_left_diff_count(),
           get_encoder_right_diff_count(),
           get_encoder_left_speed(),
           get_encoder_right_speed(),
           get_encoder_left_micrometers(),
           get_encoder_right_micrometers(),
           get_encoder_average_micrometers());
}
```

### 5. Calibración del factor `micrometers_per_count`

Para obtener el valor exacto de `micrometers_per_count`:

1. Marca una distancia conocida en el suelo (ej. 1 m = 1000 mm).
2. Haz que el robot avance esa distancia en línea recta.
3. Lee `get_encoder_left_total_count()` y `get_encoder_right_total_count()`.
4. Calcula:

```
µm/pulso = (distancia_recorrida_µm) / (total_pulsos)

Ejemplo: 1000 mm → 1 000 000 µm, 32000 pulsos
µm/pulso = 1000000 / 32000 ≈ 31.25
```

## Constantes definidas en `encoder.h`

| Constante | Valor | Descripción |
|---|---|---|
| `MICROMETERS_PER_METER` | `1000000L` | µm por metro |
| `SYSTICK_FREQUENCY_HZ` | `1000` | Frecuencia de llamada de `update_encoder_readings()` |

Ajusta `SYSTICK_FREQUENCY_HZ` si llamas a `update_encoder_readings()` a otra frecuencia (ej. 500 Hz si la llamas cada 2 ms).
