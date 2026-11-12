#include "sensor/encoder.h"

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

/* ==================================================================
 * Variables de estado estilo mmlib
 * ================================================================== */

/* Diferencia de pulsos respecto a la última lectura */
static volatile int32_t left_diff_count;
static volatile int32_t right_diff_count;

/* Total acumulado de pulsos */
static volatile int32_t left_total_count;
static volatile int32_t right_total_count;

/* Distancia recorrida en micrómetros */
static volatile int32_t left_micrometers;
static volatile int32_t right_micrometers;

/* Velocidad en metros por segundo */
static volatile float left_speed;
static volatile float right_speed;

/**
 * @brief Muestras de la media móvil que suaviza la velocidad.
 *
 * La velocidad de **un solo periodo** está cuantizada: a 1 kHz y 287 µm por cuenta, cada milisegundo
 * solo se pueden contar 0, 1 o 2 cuentas, así que la medida cruda salta entre 0, 287 y 574 mm/s. Con
 * esa señal el PID no puede trabajar: no es que esté mal sintonizado, es que **la realimentación es
 * ruido** y el motor va a bandazos.
 *
 * Promediando 20 muestras la resolución baja a **≈14 mm/s** (una cuenta en 20 ms), que sí es una
 * medida de velocidad. El retardo que añade es de ~10 ms, aceptable para la dinámica de estos motores.
 *
 * @note Se promedian **cuentas y tiempo**, no velocidades ya calculadas: así el número es correcto
 *       aunque quien llama lo haga cada 1 ms (el lazo) o cada 10 ms (las primitivas de movimiento).
 *
 * 🔬 **Por qué 20 y no otro valor (medido el 2026-10-11, forzando el límite con `Kp = 0,20`)**: el
 * tamaño de la ventana mueve **a la vez** el retardo (~N/2 muestras) y la cuantización (una cuenta
 * partida por la ventana), **en sentidos opuestos**. Así que tiene un óptimo y no se puede jugar con
 * él a gusto:
 *
 * | Muestras | Retardo | Cuantización | σ(duty) con `Kp = 0,20` |
 * |--:|--:|--:|--:|
 * | 10 | ~10 ms | 28,7 mm/s | 8 % |
 * | **20** | **~15 ms** | **14,3 mm/s** | **28 %** |
 * | 40 | ~25 ms | 7,2 mm/s | 51 % (y el lazo se iba a 428 mm/s pidiendo 300) |
 *
 * ⇒ **El que manda es el retardo, no la resolución**: con la ventana de 40 la medida es 4× más fina y
 * el lazo va mucho peor. Pero acortar tampoco sirve, porque la cuantización se come lo que se gana. Y
 * con `Kp = 0,10` (la buena) la σ de la velocidad **medida en cuentas** sale la misma (0,7) con
 * ventana de 10 y de 20 ⇒ el residuo es cuantización pura y el lazo está en el suelo de la medida.
 * **Ni acortar ni alargar sube el límite de `Kp`**: el acoplamiento es real y este valor lo tiene
 * cerca del óptimo. Romperlo exigiría otro estimador (método M/T), pero el premio esperable en `Kp`
 * es de un factor ~1,5-2, no un cambio de escala.
 */
#define ENCODER_VEL_VENTANA   20U

static int32_t velocity_buf_izq[ENCODER_VEL_VENTANA];
static int32_t velocity_buf_der[ENCODER_VEL_VENTANA];
static float   velocity_buf_dt[ENCODER_VEL_VENTANA];
static uint8_t velocity_idx;
static int32_t velocity_suma_izq;
static int32_t velocity_suma_der;
static float   velocity_suma_dt;

/* Factor de calibración: micrómetros por cuenta del encoder.
 * Por defecto se usa el valor **medido** (`ENCODER_UM_POR_CUENTA_DEF` en encoder.h): empujando el robot
 * 1 000 mm con las ruedas nuevas salieron 287,0 µm por cuenta, frente a los **269,3** que predice el
 * nominal (Ø24 con goma y 280 cuentas por vuelta). El 6,6 % apunta a un diámetro real mayor de 24 mm.
 * Se puede ajustar en caliente con set_micrometers_per_count(). */
static float micrometers_per_count = ENCODER_UM_POR_CUENTA_DEF;

/* ================================================================== */

static void MX_TIM2_Encoder_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 0;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 0xFFFF;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.RepetitionCounter = 0;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    TIM_Encoder_InitTypeDef sConfig = {0};
    sConfig.EncoderMode  = TIM_ENCODERMODE_TI12;
    sConfig.IC1Polarity  = TIM_ICPOLARITY_RISING;
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC1Filter    = 0;
    sConfig.IC2Polarity  = TIM_ICPOLARITY_RISING;
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC2Filter    = 0;

    HAL_TIM_Encoder_Init(&htim2, &sConfig);
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
}

/* ================================================================== */

static void MX_TIM3_Encoder_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 0;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 0xFFFF;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.RepetitionCounter = 0;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    TIM_Encoder_InitTypeDef sConfig = {0};
    sConfig.EncoderMode  = TIM_ENCODERMODE_TI12;
    sConfig.IC1Polarity  = TIM_ICPOLARITY_RISING;
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC1Filter    = 0;
    sConfig.IC2Polarity  = TIM_ICPOLARITY_RISING;
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC2Filter    = 0;

    HAL_TIM_Encoder_Init(&htim3, &sConfig);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
}

/* ================================================================== */

void ENC_Init(void)
{
    MX_TIM2_Encoder_Init();
    MX_TIM3_Encoder_Init();
}

int32_t ENC_GetCount1(void) { return (int32_t)__HAL_TIM_GET_COUNTER(&htim2); }
int32_t ENC_GetCount2(void) { return (int32_t)__HAL_TIM_GET_COUNTER(&htim3); }

void ENC_ResetCount1(void) { __HAL_TIM_SET_COUNTER(&htim2, 0); }
void ENC_ResetCount2(void) { __HAL_TIM_SET_COUNTER(&htim3, 0); }

/* ================================================================
 * Conteo por canal individual (software polling) – compatibilidad
 * ================================================================ */
static volatile uint32_t pulse_1a = 0;
static volatile uint32_t pulse_1b = 0;
static volatile uint32_t pulse_2a = 0;
static volatile uint32_t pulse_2b = 0;

static uint8_t last_1a = 0;
static uint8_t last_1b = 0;
static uint8_t last_2a = 0;
static uint8_t last_2b = 0;

void ENC_UpdatePulses(void)
{
    uint8_t cur_1a = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t cur_1b = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t cur_2a = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t cur_2b = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7) == GPIO_PIN_SET) ? 1 : 0;

    if (cur_1a && !last_1a) pulse_1a++;
    if (cur_1b && !last_1b) pulse_1b++;
    if (cur_2a && !last_2a) pulse_2a++;
    if (cur_2b && !last_2b) pulse_2b++;

    last_1a = cur_1a;  last_1b = cur_1b;
    last_2a = cur_2a;  last_2b = cur_2b;
}

uint32_t ENC_GetPulse1A(void) { return pulse_1a; }
uint32_t ENC_GetPulse1B(void) { return pulse_1b; }
uint32_t ENC_GetPulse2A(void) { return pulse_2a; }
uint32_t ENC_GetPulse2B(void) { return pulse_2b; }

void ENC_ResetPulses(void)
{
    pulse_1a = 0; pulse_1b = 0;
    pulse_2a = 0; pulse_2b = 0;
    last_1a  = 0; last_1b  = 0;
    last_2a  = 0; last_2b  = 0;
}

/* ================================================================
 * API estilo mmlib – para motores N20
 * ================================================================ */

void set_micrometers_per_count(float value)
{
    micrometers_per_count = value;
}

float get_micrometers_per_count(void)
{
    return micrometers_per_count;
}

/**
 * @brief Determina la diferencia más probable entre dos lecturas de un
 *        contador uint16_t, manejando correctamente el overflow.
 *
 * Cuando se lee un contador que puede incrementar o decrementar hay que:
 * - Considerar el overflow del contador.
 * - Considerar la dirección (incremento o decremento).
 *
 * Se asume que las lecturas son lo suficientemente rápidas para que la
 * dirección venga dada por la diferencia mínima entre las dos lecturas.
 *
 * @param[in] now   Lectura actual del contador.
 * @param[in] before Lectura anterior del contador.
 *
 * @return Diferencia con signo (positivo = avance, negativo = retroceso).
 */
int32_t max_likelihood_counter_diff(uint16_t now, uint16_t before)
{
    uint16_t forward_diff;
    uint16_t backward_diff;

    forward_diff = (uint16_t)(now - before);
    backward_diff = (uint16_t)(before - now);
    if (forward_diff > backward_diff)
        return -(int32_t)backward_diff;
    return (int32_t)forward_diff;
}

/**
 * @brief Actualiza todas las variables del encoder.
 *
 * - Lee los contadores hardware (TIM encoder mode).
 * - Calcula la diferencia respecto a la lectura anterior usando
 *   max_likelihood_counter_diff() para manejar overflow.
 * - Acumula el total de pulsos.
 * - Calcula la distancia recorrida en micrómetros.
 * - Calcula la velocidad en metros por segundo.
 *
 * Debe llamarse a la frecuencia definida por SYSTICK_FREQUENCY_HZ
 * (típicamente 1000 Hz = cada 1 ms).
 */
void update_encoder_readings(float dt)
{
    static uint16_t last_left_count;
    static uint16_t last_right_count;

    uint16_t left_count;
    uint16_t right_count;

    float mic_per_count;

    left_count  = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);
    right_count = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);

    /* ⚠️ El `dt` **entra por parámetro** desde el 2026-10-11, y no es un capricho de estilo: la
       ventana de la media móvil es la **suma de los `dt`**, así que su duración depende del ritmo al
       que se llame a esto. Midiéndolo por dentro, la misma función daba una ventana de ~38 ms en la
       prueba 18 (bucle de 1,9 ms) y de **~200 ms en las primitivas de movimiento** (bucle de 10 ms)
       ⇒ ~95 ms de retraso al regular, y sin que nada lo delatara. Ahora lo llena la interrupción de
       TIM1 a 1 kHz fijo (`app/control_loop.c`) y la ventana son 20 ms siempre. */
    if (dt <= 0.0f) {
        dt = 1.0f / (float)SYSTICK_FREQUENCY_HZ;
    }

    mic_per_count = get_micrometers_per_count();
    if (mic_per_count <= 0.0f)
        mic_per_count = 1.0f;   /* evitar división por cero si no se ha calibrado */

    left_diff_count =
        max_likelihood_counter_diff(left_count, last_left_count);

    /* ⚠️ El encoder de la RUEDA DERECHA (TIM3, PA6/PA7) está cableado al revés que el de la izquierda:
       girando las dos ruedas **hacia adelante a mano**, la izquierda sumaba y la derecha restaba
       (comprobado el 30-sep con la prueba `PPR A MANO`). Se invierte el signo aquí para que en todo el
       firmware **positivo = hacia adelante** en las dos ruedas, que es lo que esperan la odometría y el
       PID. Si algún día se cruzan los hilos A y B en el conector de ese encoder, hay que **quitar este
       menos**.
       ⚠️ Y ojo con lo que provocaba: en `UM MOTORES` el frenado por pulsos (`pulsos_r >= objetivo`) no se
       cumplía nunca con el signo al revés, así que la rueda derecha no paraba, el robot se torcía y
       acababa chocando. */
    right_diff_count = -max_likelihood_counter_diff(right_count, last_right_count);

    left_total_count  += left_diff_count;
    right_total_count += right_diff_count;

    left_micrometers  = (int32_t)(left_total_count * mic_per_count);
    right_micrometers = (int32_t)(right_total_count * mic_per_count);

    /* Media móvil de cuentas y de tiempo: sale la muestra más vieja, entra la nueva. */
    velocity_suma_izq -= velocity_buf_izq[velocity_idx];
    velocity_suma_der -= velocity_buf_der[velocity_idx];
    velocity_suma_dt  -= velocity_buf_dt[velocity_idx];

    velocity_buf_izq[velocity_idx] = left_diff_count;
    velocity_buf_der[velocity_idx] = right_diff_count;
    velocity_buf_dt[velocity_idx]  = dt;

    velocity_suma_izq += left_diff_count;
    velocity_suma_der += right_diff_count;
    velocity_suma_dt  += dt;

    velocity_idx = (uint8_t)((velocity_idx + 1U) % ENCODER_VEL_VENTANA);

    left_speed = 0.0f;
    right_speed = 0.0f;

    if (velocity_suma_dt > 0.0f) {
        left_speed = (float)velocity_suma_izq * (mic_per_count / MICROMETERS_PER_METER) /
                     velocity_suma_dt;
        right_speed = (float)velocity_suma_der * (mic_per_count / MICROMETERS_PER_METER) /
                      velocity_suma_dt;
    }

    last_left_count  = left_count;
    last_right_count = right_count;
}

/* ------------------------------------------------------------------
 * Getters
 * ------------------------------------------------------------------ */

int32_t get_encoder_left_diff_count(void)
{
    return left_diff_count;
}

int32_t get_encoder_right_diff_count(void)
{
    return right_diff_count;
}

int32_t get_encoder_left_total_count(void)
{
    return left_total_count;
}

int32_t get_encoder_right_total_count(void)
{
    return right_total_count;
}

int32_t get_encoder_left_micrometers(void)
{
    return left_micrometers;
}

int32_t get_encoder_right_micrometers(void)
{
    return right_micrometers;
}

int32_t get_encoder_average_micrometers(void)
{
    return (left_micrometers + right_micrometers) / 2;
}

float get_encoder_left_speed(void)
{
    return left_speed;
}

float get_encoder_right_speed(void)
{
    return right_speed;
}
