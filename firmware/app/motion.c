/**
 * @file motion.c
 * @brief Primitivas de movimiento (capa APP): avanzar una distancia y girar un ángulo.
 *
 * El lazo es siempre el mismo, y es el único sitio que toca el hardware:
 *
 *   leer encoders -> mover la pose (FILTER) -> calcular el duty (CONTROL) -> escribirlo en el driver
 *
 * Lo que cambia entre avanzar y girar es **qué se pide** a cada rueda: la misma velocidad en las dos
 * (con el rumbo corregido) o la misma velocidad **opuesta** (para girar sobre el centro).
 *
 * El rumbo que se corrige al avanzar lo mide el **giroscopio** (prueba 20), y la diferencia con medirlo
 * por odometría no es un detalle: la odometría saca el giro de la **diferencia de arcos de las dos
 * ruedas**, así que un error se arrastra de un movimiento al siguiente, mientras que el giroscopio mide
 * el giro de verdad y su referencia se pone a cero al empezar cada avance. **Los giros también se
 * cierran con el giroscopio**, y por la misma razón: al pivotar, las ruedas deslizan y la odometría
 * sobreestima el giro (medido el 2026-10-02: dice 125° donde el robot gira 87°).
 *
 * Las dos primitivas van **despacio a propósito**. Para lo que se usan —cruzar una celda de 180 mm y
 * doblar una esquina— no hacen falta 600 mm/s, y la precisión de 180 mm y de 90° sale mucho más
 * barata en milisegundos de movimiento que en código de corrección.
 */

#include "app/motion.h"
#include "app/buttons.h"
#include "hal/drv8833.h"
#include "hal/systemclock.h"
#include "sensor/encoder.h"
#include "sensor/imu.h"

#include <math.h>

/** @brief π, para no repetir la constante. */
#define MOTION_PI             3.14159265f

/** @brief Conversión radianes → grados. */
#define MOTION_RAD_A_GRADOS   (180.0f / MOTION_PI)

/** @brief Periodo del lazo de las primitivas [ms]: 100 Hz, el mismo que CONTROL. */
#define MOTION_PASO_MS        10U

/**
 * @brief Referencia con la que se cierra el **giro**.
 *
 * Es una variable y no un `#define` para poder **comparar las tres referencias sin reflashear**: la
 * orden `GYRO` de la consola la cambia en caliente, y así la misma maniobra se puede repetir con cada
 * una y comparar el error de cierre.
 *
 * ✅ **Por defecto, GIROSCOPIO.** Medido el 2026-10-02 con la maniobra `+90° ×4` (360° pedidos) en la
 * mesa, y **comparado con lo que el usuario vio a ojo**: el robot hizo un giro a la derecha y tres a
 * la izquierda y quedó **girado unos 45°**.
 *
 * | Referencia | Giro 1 | Giro 2 | Giro 3 | Giro 4 | **Neto** |
 * |:-----------|:------:|:------:|:------:|:------:|:--------:|
 * | Lo que se vio | derecha | izquierda | izquierda | derecha | **+45°** |
 * | **Giroscopio** | **−197,6°** | +87,5° | +90,7° | +78,9° | **+59,5°** ✓ |
 * | Odometría | +52,9° | +77,1° | +126,7° | +87,7° | **+344,5°** ✗ |
 *
 * ⚠️ **La odometría no solo se equivoca de magnitud: se equivoca de SIGNO.** Al pivotar con las ruedas
 * deslizando, los arcos que cuentan los encoders no guardan relación con el giro real, y el lazo corta
 * cuando su cuenta llega a 90° — o sea, **con el robot girado 10°**. El giroscopio es el único de los
 * dos que refleja lo que pasa de verdad.
 */
static Motion_GiroModo_t motion_modo_giro = MOTION_GIRO_GIROSCOPIO;

/**
 * @brief Elige la referencia del lazo de giro.
 */
void MOTION_SetModoGiro(Motion_GiroModo_t modo)
{
    if ((uint8_t)modo < (uint8_t)MOTION_GIRO_MODO_N) {
        motion_modo_giro = modo;
    }
}

/**
 * @brief Referencia que está usando el lazo de giro.
 */
Motion_GiroModo_t MOTION_GetModoGiro(void)
{
    return motion_modo_giro;
}

/**
 * @brief Ganancias del lazo de giro, **en variables y no en `#define`**.
 *
 * Las constantes de `motion.h` son los valores **de arranque**; estas son las que usa el lazo, y la
 * orden `GYRO` de la consola las cambia en caliente. Se ahorra así un flasheo —y un reinicio, que
 * borra la calibración del giroscopio— por cada prueba de sintonía.
 */
static float motion_giro_kp     = MOTION_GIRO_KP;
static float motion_giro_ki     = 0.0f;
static float motion_giro_kd     = MOTION_GIRO_KD;
static float motion_giro_escala = MOTION_GYRO_ESCALA;
static float motion_giro_vmin   = MOTION_GIRO_VEL_MIN_RODANDO_MM_S;
static float motion_giro_varr   = MOTION_GIRO_VEL_MIN_MM_S;
static float motion_giro_vdps   = MOTION_GIRO_RODANDO_DPS;

/**
 * @brief Cambia una ganancia del lazo de giro.
 */
uint8_t MOTION_SetGiroGanancia(Motion_GiroGanancia_t cual, float valor)
{
    switch (cual) {
    case MOTION_GIRO_GANANCIA_KP:
        if ((valor < 0.0f) || (valor > 100.0f)) {
            return 0U;
        }
        motion_giro_kp = valor;
        break;
    case MOTION_GIRO_GANANCIA_KI:
        if ((valor < 0.0f) || (valor > 20.0f)) {
            return 0U;
        }
        motion_giro_ki = valor;
        break;
    case MOTION_GIRO_GANANCIA_KD:
        if ((valor < 0.0f) || (valor > 5.0f)) {
            return 0U;
        }
        motion_giro_kd = valor;
        break;
    case MOTION_GIRO_GANANCIA_ESCALA:
        if ((valor < 0.5f) || (valor > 2.0f)) {
            return 0U;
        }
        motion_giro_escala = valor;
        break;
    case MOTION_GIRO_GANANCIA_VMIN:
        if ((valor < 0.0f) || (valor > 200.0f)) {
            return 0U;
        }
        motion_giro_vmin = valor;
        break;
    case MOTION_GIRO_GANANCIA_VARR:
        if ((valor < 0.0f) || (valor > 200.0f)) {
            return 0U;
        }
        motion_giro_varr = valor;
        break;
    case MOTION_GIRO_GANANCIA_VDPS:
        if ((valor < 0.0f) || (valor > 100.0f)) {
            return 0U;
        }
        motion_giro_vdps = valor;
        break;
    default:
        return 0U;
    }

    return 1U;
}

/**
 * @brief Lee una ganancia del lazo de giro.
 */
float MOTION_GetGiroGanancia(Motion_GiroGanancia_t cual)
{
    switch (cual) {
    case MOTION_GIRO_GANANCIA_KP:
        return motion_giro_kp;
    case MOTION_GIRO_GANANCIA_KI:
        return motion_giro_ki;
    case MOTION_GIRO_GANANCIA_KD:
        return motion_giro_kd;
    case MOTION_GIRO_GANANCIA_ESCALA:
        return motion_giro_escala;
    case MOTION_GIRO_GANANCIA_VMIN:
        return motion_giro_vmin;
    case MOTION_GIRO_GANANCIA_VARR:
        return motion_giro_varr;
    case MOTION_GIRO_GANANCIA_VDPS:
        return motion_giro_vdps;
    default:
        return 0.0f;
    }
}

/** @brief Tiempo que se espera a que el robot se detenga antes de dar un movimiento por terminado [ms]. */
#define MOTION_ASENTAR_MS     250U

/** @brief Ganancia de posición al avanzar [mm/s por mm que falta].

 * Con 1,5 el robot va a tope si le faltan más de 170 mm y entra en el tramo final frenando solo. */
#define MOTION_AVANCE_KP      1.5f

/** @brief Velocidad mínima al avanzar [mm/s]: por debajo, el motor no vence la fricción estática. */
#define MOTION_AVANCE_VEL_MIN_MM_S 50.0f

/* Las funciones del giroscopio se declaran aquí porque `motion_asentar()` —que está más arriba— ya
   las usa para refinar el sesgo aprovechando que el robot está frenado. */
static uint8_t motion_gyro_muestrear(float *media);
static void    motion_gyro_calibrar(Motion_t *m);
static void    motion_gyro_refinar(Motion_t *m, float suma, uint8_t n);

/**
 * @brief Un paso de las primitivas: mide, mueve la pose, calcula el duty y lo aplica.
 *
 * La odometría se alimenta con los incrementos de µm **de cada rueda** desde la lectura anterior, que
 * es lo que espera `ODOM_Update()` (arcos, no velocidades).
 *
 * @param m Estructura de las primitivas
 * @param consigna_izq Velocidad pedida a la rueda izquierda [mm/s]
 * @param consigna_der Velocidad pedida a la rueda derecha [mm/s]
 * @param dt Segundos desde el paso anterior
 */
static void motion_paso(Motion_t *m, float consigna_izq, float consigna_der, float dt)
{
    int32_t um_izq;
    int32_t um_der;

    update_encoder_readings();

    um_izq = get_encoder_left_micrometers();
    um_der = get_encoder_right_micrometers();

    ODOM_Update(&m->odo,
                (float)(um_izq - m->um_izq_previo) / 1000000.0f,
                (float)(um_der - m->um_der_previo) / 1000000.0f);

    m->um_izq_previo = um_izq;
    m->um_der_previo = um_der;

    CTRL_SetConsigna(&m->izq, consigna_izq);
    CTRL_SetConsigna(&m->der, consigna_der);

    DRV8833_Set_Speed(m->motor_izq,
                      (int16_t)CTRL_Update(&m->izq, get_encoder_left_speed() * 1000.0f, dt));
    DRV8833_Set_Speed(m->motor_der,
                      (int16_t)CTRL_Update(&m->der, get_encoder_right_speed() * 1000.0f, dt));
}

/**
 * @brief Deja de pedir velocidad y espera a que el robot se detenga del todo.
 *
 * Se llama al final de cada movimiento, y no es un adorno: si se frena en seco con el robot lanzado,
 * los últimos milímetros (o grados) se recorren **después** de haber mirado la medida, y el movimiento
 * siempre se pasa en el mismo sentido.
 *
 * @param m Estructura de las primitivas
 */
static void motion_asentar(Motion_t *m)
{
    uint32_t t_inicio = HAL_GetTick();
    uint32_t t_ultimo = t_inicio;
    float    suma_gyro     = 0.0f;
    uint8_t  muestras_gyro = 0U;

    /* Se **suelta el pedal de golpe**, y no es un detalle. El integrador del PID venía guardando el
       duty que hacía falta para avanzar (~13 %), y se descarga a `Ki · error` ≈ 25 %/s, así que
       seguía **empujando** unos 0,4 s despues de haberse dado el movimiento por terminado. Como la
       odometría sigue contando mientras el robot rueda, ese rato se sumaba al recorrido: medido el
       2026-10-01, **19,6 mm de más** en una celda de 180 y **~19° de más** en un giro de 90°.

       Reseteando el PID el empuje se apaga de golpe y, con la consigna a cero, es la **P** la que
       frena (el error es negativo).

       ⚠️ Y no bastaba con **descontar** esos milímetros del objetivo: la frenada real varió entre 2 y
       44 mm de una celda a otra, porque depende de cuánto le quedara al integrador. Lo que hay que
       quitar es el rodaje, no calibrarlo. */
    CTRL_Parar(&m->izq);
    CTRL_Parar(&m->der);

    /* El robot está **frenado**, y ese es el único momento de todo un movimiento en el que se sabe que
       no está girando. Es, por tanto, donde el sesgo del giroscopio se puede medir **sin que el propio
       movimiento lo contamine**, y se aprovecha el bucle que ya existe: no cuesta tiempo de más. */
    while ((HAL_GetTick() - t_inicio) < MOTION_ASENTAR_MS) {
        uint32_t ahora = HAL_GetTick();
        float    dt    = (float)(ahora - t_ultimo) / 1000.0f;
        IMU_Data_t imu;

        t_ultimo = ahora;

        motion_paso(m, 0.0f, 0.0f, dt);

        /* ⚠️ **Las muestras se toman solo en la SEGUNDA MITAD del asentamiento**, y no es un detalle:
           al frenar de golpe el robot **todavía gira por inercia** unos milisegundos, y una muestra
           tomada ahí se lleva el giro del movimiento al sesgo. Medido el 2026-10-02 en el laberinto:
           con las muestras desde el primer milisegundo el sesgo quedó falseado y el giroscopio
           **dejó de ver la media vuelta** (dijo 8,5° donde la odometría y el desplazamiento del robot
           decían 165°). */
        if ((ahora - t_inicio) >= (MOTION_ASENTAR_MS / 2U)) {
            if (IMU_Read(&imu)) {
                suma_gyro += imu.gz;
                muestras_gyro++;
            }
        }

        /* Además de la P, **freno dinámico**: el DRV8833 cortocircuita el motor y la rueda para en
           decenas de milisegundos. Sin esto el robot rueda libre y sigue sumando recorrido. */
        DRV8833_Brake(m->motor_izq);
        DRV8833_Brake(m->motor_der);

        HAL_Delay(MOTION_PASO_MS);
    }

    DRV8833_Brake(m->motor_izq);
    DRV8833_Brake(m->motor_der);

    motion_gyro_refinar(m, suma_gyro, muestras_gyro);
}

/**
 * @brief Lleva un ángulo al rango $(-\pi, \pi]$.
 *
 * Hace falta para que un giro que cruza el ±180° no pida dar la vuelta entera: sin esto, girar 10° a
 * la izquierda desde 175° pediría recorrer 350° en lugar de 10°.
 *
 * @param rad Ángulo en radianes
 * @return Ángulo equivalente dentro del rango
 */
static float motion_error_angulo(float rad)
{
    while (rad > MOTION_PI)  { rad -= 2.0f * MOTION_PI; }
    while (rad <= -MOTION_PI) { rad += 2.0f * MOTION_PI; }

    return rad;
}

/**
 * @brief Toma una tanda de muestras del eje Z y dice si son de fiar, con su media [°/s].
 *
 * **La dispersión es el dato, no la media.** Un robot parado sobre el suelo tiene poco más que ruido
 * en el giroscopio (décimas de °/s), así que una dispersión grande solo puede venir de una cosa: **el
 * robot se está moviendo**. Medido el 2026-10-02: calibrando con el robot en la mano, el sesgo quedó
 * falseado en decenas de °/s y, como el lazo integra ese error, el robot se fue de lado y chocó contra
 * la pared **antes de cruzar la primera celda** (`>WF_GYR:272°` con el robot sin girar).
 *
 * @param media Media de las muestras tomadas [°/s]
 * @return 1 si la tanda vale (hay muestras y el robot estaba quieto), 0 si no
 */
static uint8_t motion_gyro_muestrear(float *media)
{
    IMU_Data_t imu;
    float      suma    = 0.0f;
    float      suma2   = 0.0f;
    uint8_t    validas = 0U;

    for (uint8_t i = 0U; i < MOTION_GYRO_SESGO_N; i++) {
        if (IMU_Read(&imu)) {
            suma  += imu.gz;
            suma2 += imu.gz * imu.gz;
            validas++;
        }

        HAL_Delay(MOTION_PASO_MS);
    }

    if (validas < (MOTION_GYRO_SESGO_N / 2U)) {
        return 0U;
    }

    *media = suma / (float)validas;

    /* Varianza: con el robot quieto esto es ruido; si lo están moviendo, se dispara. */
    if (((suma2 / (float)validas) - ((*media) * (*media))) >
        (MOTION_GYRO_SESGO_DISP_MAX * MOTION_GYRO_SESGO_DISP_MAX)) {
        return 0U;
    }

    return 1U;
}

/**
 * @brief Mide el sesgo del giroscopio, **insistiendo hasta que el robot esté quieto**.
 *
 * Se llama desde `MOTION_Init()`, que es el momento en que el robot *debería* estar parado. Pero
 * «debería» no basta: al entrar en una prueba se está colocando el robot en el laberinto, y si la
 * medida cae mientras lo sostienen, el sesgo sale mal y **el lazo arrastra ese error todo el rato**.
 *
 * De ahí los reintentos: una tanda con dispersión alta se tira y se vuelve a empezar. Si ninguna sale
 * limpia se usa la última media, y el **refinamiento del asentamiento** (`motion_asentar()`) la
 * corrige en los primeros movimientos: el robot se endereza solo.
 *
 * @param m Estructura de las primitivas
 */
static void motion_gyro_calibrar(Motion_t *m)
{
    float media = 0.0f;

    m->gyro_ok    = 0U;
    m->gyro_rumbo = 0.0f;
    m->gyro_total = 0.0f;
    m->gyro_sesgo = 0.0f;

    if (!IMU_Detected()) {
        return;
    }

    for (uint8_t intento = 0U; intento < MOTION_GYRO_SESGO_INTENTOS; intento++) {
        if (motion_gyro_muestrear(&media) != 0U) {
            break;
        }
    }

    m->gyro_sesgo = media;
    m->gyro_ok    = 1U;
}

/**
 * @brief Corrige el sesgo con lo medido durante el asentamiento, cuando se sabe que no gira.
 *
 * @param m Estructura de las primitivas
 * @param suma Suma de las muestras tomadas con el robot frenado [°/s]
 * @param n Número de muestras
 */
static void motion_gyro_refinar(Motion_t *m, float suma, uint8_t n)
{
    float media;
    float paso;

    if ((m->gyro_ok == 0U) || (n < MOTION_GYRO_REFINAR_MIN)) {
        return;
    }

    media = suma / (float)n;

    /* El filtro es lento **y el paso está topado**: una medida suelta —o una que se haya colado con el
       robot aún moviéndose— no puede dar la vuelta al sesgo de golpe. Es la diferencia entre
       corregirlo y estropearlo. */
    paso = MOTION_GYRO_REFINAR_ALFA * (media - m->gyro_sesgo);

    if (paso > MOTION_GYRO_REFINAR_MAX_PASO) {
        paso = MOTION_GYRO_REFINAR_MAX_PASO;
    } else if (paso < -MOTION_GYRO_REFINAR_MAX_PASO) {
        paso = -MOTION_GYRO_REFINAR_MAX_PASO;
    }

    m->gyro_sesgo += paso;
}

/**
 * @brief Lee el giroscopio e integra el ángulo girado [°].
 *
 * Actualiza **dos** acumuladores con la misma muestra:
 *   - `gyro_rumbo`, que se pone a cero al empezar cada avance y es la referencia del corrector;
 *   - `gyro_total`, que no se toca nunca y sirve para comparar con `MOTION_GetAngulo()`.
 *
 * @param m Estructura de las primitivas
 * @param dt Segundos desde la lectura anterior
 * @return 1 si hay medida nueva, 0 si el giroscopio no está disponible
 */
static uint8_t motion_gyro_leer(Motion_t *m, float dt)
{
    IMU_Data_t imu;
    float      giro;

    if (m->gyro_ok == 0U) {
        return 0U;
    }

    if (!IMU_Read(&imu)) {
        /* Un fallo del bus a mitad de movimiento no se puede tapar leyendo un cero: se da el
           giroscopio por caído y el avance sigue con el respaldo por odometría. */
        m->gyro_ok = 0U;
        return 0U;
    }

    giro = (imu.gz - m->gyro_sesgo) * MOTION_GYRO_SIGNO_Z * motion_giro_escala;

    m->gyro_velocidad = giro;
    m->gyro_rumbo += giro * dt;
    m->gyro_total += giro * dt;

    return 1U;
}

/**
 * @brief Prepara las primitivas: arranca los dos lazos y pone la pose en el origen.
 */
void MOTION_Init(Motion_t *m,
                 DRV8833_Motor_t *motor_izq,
                 DRV8833_Motor_t *motor_der,
                 float kp, float ki, float kd)
{
    CTRL_Init(&m->izq, kp, ki, kd, MOTION_DUTY_MAX);
    CTRL_Init(&m->der, kp, ki, kd, MOTION_DUTY_MAX);
    CTRL_SetRampa(&m->izq, MOTION_RAMPA_MM_S2);
    CTRL_SetRampa(&m->der, MOTION_RAMPA_MM_S2);

    m->motor_izq = motor_izq;
    m->motor_der = motor_der;

    ODOM_Init(&m->odo, 0.0f, 0.0f, 0.0f);

    update_encoder_readings();
    m->um_izq_previo = get_encoder_left_micrometers();
    m->um_der_previo = get_encoder_right_micrometers();

    motion_gyro_calibrar(m);
}

/**
 * @brief Ángulo que llevas girado desde el último MOTION_Init() [°].
 */
float MOTION_GetAngulo(const Motion_t *m)
{
    return m->odo.theta * MOTION_RAD_A_GRADOS;
}

/**
 * @brief Ángulo girado según el **giroscopio**, acumulado desde el último `MOTION_Init()` [°].
 */
float MOTION_GetGyroAngulo(const Motion_t *m)
{
    return m->gyro_total;
}

/**
 * @brief Dice si el giroscopio se puede usar **como referencia** del lazo.
 *
 * No basta con que responda: un sesgo de varios °/s hace que el lazo persiga un giro que no existe y
 * el robot acabe **empujando contra la pared** (medido el 2026-10-02 con 6,5 °/s). Mientras el sesgo
 * esté fuera de rango se usa el respaldo por odometría.
 *
 * @param m Estructura de las primitivas
 * @return 1 si el giroscopio responde y su sesgo es de fiar
 */
static uint8_t motion_gyro_fiable(const Motion_t *m)
{
    return (uint8_t)((m->gyro_ok != 0U) &&
                     (fabsf(m->gyro_sesgo) <= MOTION_GYRO_SESGO_MAX_USABLE));
}

/**
 * @brief Sesgo del giroscopio que está usando el lazo [°/s].
 */
float MOTION_GetGyroSesgo(const Motion_t *m)
{
    return m->gyro_sesgo;
}

/**
 * @brief Dice si el corrector de rumbo está usando el giroscopio o el respaldo por odometría.
 */
uint8_t MOTION_GyroDisponible(const Motion_t *m)
{
    return m->gyro_ok;
}

/**
 * @brief Frena los dos motores y deja los lazos parados.
 */
void MOTION_Parar(Motion_t *m)
{
    CTRL_Parar(&m->izq);
    CTRL_Parar(&m->der);

    DRV8833_Brake(m->motor_izq);
    DRV8833_Brake(m->motor_der);
}

/**
 * @brief Avanza en línea recta la distancia pedida.
 *
 * La velocidad se pide **proporcional a lo que falta**, así que el robot ya va frenando solo en el
 * tramo final y no hay que adivinar dónde cortar. El rumbo se corrige con el **giroscopio**: si el
 * robot se ha desviado, la corrección se reparte entre las dos ruedas, que es más suave que girar
 * después.
 *
 * @param m Estructura de las primitivas
 * @param mm Distancia a recorrer: **positiva hacia delante, negativa hacia atrás**
 * @return 1 si llegó dentro de tolerancia, 0 si agotó el tiempo o se pidió salir
 * @note Con el giroscopio la referencia es **absoluta**, pero se pone a cero **al empezar cada
 *       llamada**: la celda garantiza que el robot no se tuerce *más*, no que vaya bien alineado. La
 *       alineación es cosa de los giros y del *wall follower*.
 * @note Si la IMU no responde, o falla a mitad de movimiento, el lazo sigue con el respaldo por
 *       odometría y el robot no se queda sin corrección.
 * @see Las constantes `MOTION_AVANCE_*`, `MOTION_GYRO_KP` y `MOTION_GYRO_CORR_MAX_MM_S` son las que se
 *      ajustan en banco; el signo del giroscopio, `MOTION_GYRO_SIGNO_Z`.
 */
uint8_t MOTION_Avanzar(Motion_t *m, float mm)
{
    int32_t  um_izq_0 = m->um_izq_previo;
    int32_t  um_der_0 = m->um_der_previo;
    float    theta_0  = m->odo.theta;
    uint32_t t_inicio = HAL_GetTick();
    uint32_t t_ultimo = t_inicio;
    uint8_t  llegado  = 0U;

    /* El rumbo del giroscopio se mide **desde aquí**, y eso es justo lo que lo hace útil: la
       referencia es el rumbo que el robot tenga en este instante, aunque venga torcido del movimiento
       anterior. Lo que la celda tiene que garantizar es que **no se tuerce más**, no enderezar lo que
       ya estaba torcido —de eso ya se ocupan los giros y el *wall follower*—. */
    m->gyro_rumbo = 0.0f;

    while ((HAL_GetTick() - t_inicio) < MOTION_TIMEOUT_MS) {
        uint32_t ahora;
        float    dt;
        float    avance_mm;
        float    falta_mm;
        float    error_rumbo;
        float    correccion;
        float    vel;

        if (BUTTONS_SalidaSolicitada() != 0U) {
            break;
        }

        avance_mm = (float)((m->um_izq_previo - um_izq_0) + (m->um_der_previo - um_der_0)) / 2000.0f;
        falta_mm  = mm - avance_mm;

        /* El criterio va en **valor absoluto**, y no es un detalle de estilo: con la comparación a
           secas, retroceder **no hacía nada**. `falta_mm` empieza en −180, y `−180 <= 3` se cumple en
           la primera vuelta, así que el movimiento se daba por llegado sin haber movido una rueda. */
        if (fabsf(falta_mm) <= (MOTION_TOL_MM + MOTION_FRENADA_MM)) {
            llegado = 1U;
            break;
        }

        ahora = HAL_GetTick();
        dt       = (float)(ahora - t_ultimo) / 1000.0f;
        t_ultimo = ahora;

        /* La velocidad sale del **signo** de lo que falta, así que la misma cuenta vale para los dos
           sentidos. Los topes hay que recortarlos por los dos lados: al retroceder, el tope inferior
           (`vel < MOTION_AVANCE_VEL_MIN_MM_S`) convertía cualquier velocidad negativa en **+50** y el
           cambio de signo posterior la dejaba en −50, o sea que el robot retrocedía siempre a la
           velocidad mínima, ignorando lo que le faltara. */
        vel = MOTION_AVANCE_KP * falta_mm;
        if (vel > MOTION_VEL_MM_S) {
            vel = MOTION_VEL_MM_S;
        } else if (vel < -MOTION_VEL_MM_S) {
            vel = -MOTION_VEL_MM_S;
        }

        if ((vel > 0.0f) && (vel < MOTION_AVANCE_VEL_MIN_MM_S)) {
            vel = MOTION_AVANCE_VEL_MIN_MM_S;
        } else if ((vel < 0.0f) && (vel > -MOTION_AVANCE_VEL_MIN_MM_S)) {
            vel = -MOTION_AVANCE_VEL_MIN_MM_S;
        }

        /* Rumbo: θ ha de quedarse donde estaba. La corrección va a las dos ruedas en sentidos opuestos,
           porque lo que se corrige es la diferencia, no la velocidad de crucero.

           ⚠️ **El SIGNO es lo que hace que esto sea realimentación NEGATIVA**, y equivocarlo no se nota
           en el código: se nota en el suelo. Girar a la izquierda (θ positivo) es que la rueda
           **derecha** avance más que la izquierda (`dθ = (ds_der − ds_izq) / L`). Si el robot se ha ido
           a la izquierda, `error_rumbo` sale **negativo** y hay que **frenar la derecha**: o sea,
           restarle la corrección a la derecha y sumársela a la izquierda.

           Con los signos al revés el lazo se realimenta solo y el robot **se va en curva**: medido el
           2026-10-01 en la primera pasada de la prueba 20, la celda terminó con **35,8° de desvío** y
           **233 mm** recorridos en vez de 180. Cualquier desvío inicial, por pequeño que sea, se
           multiplica por $e^{(2 K_p / L) t}$ en vez de apagarse.

           La referencia es el **giroscopio**, y el convenio de signo se conserva: `gyro_rumbo` es
           positivo a la izquierda, igual que θ, así que el error es **su opuesto**. Si la IMU no
           responde se cae al respaldo por odometría, que arrastra el error de un movimiento al
           siguiente pero es mejor que ir recto a ciegas. */
        if ((motion_gyro_leer(m, dt) != 0U) && (motion_gyro_fiable(m) != 0U)) {
            error_rumbo = -m->gyro_rumbo;
            correccion  = MOTION_GYRO_KP * error_rumbo;
        } else {
            /* ⚠️ **La diferencia hay que normalizarla.** `theta_0` y `odo.theta` están las dos en
               $(-\pi, \pi]$, así que restarlas en crudo da como mucho $2\pi$ de salto: con el robot a
               170° y un desvío pequeño, la resta puede salir cerca de 360° y la corrección pide **diez
               veces** la velocidad máxima (se va el robot de las manos). Con `motion_error_angulo()`
               el desvío se mide siempre por el camino corto. */
            error_rumbo = motion_error_angulo(theta_0 - m->odo.theta) * MOTION_RAD_A_GRADOS;
            correccion  = MOTION_RUMBO_KP * error_rumbo;
        }

        /* El tope se recorta **igual en los dos sentidos**, así que la saturación no puede introducir
           un sesgo hacia un lado. */
        if (correccion > MOTION_GYRO_CORR_MAX_MM_S) {
            correccion = MOTION_GYRO_CORR_MAX_MM_S;
        } else if (correccion < -MOTION_GYRO_CORR_MAX_MM_S) {
            correccion = -MOTION_GYRO_CORR_MAX_MM_S;
        }

        motion_paso(m, vel - correccion, vel + correccion, dt);

        HAL_Delay(MOTION_PASO_MS);
    }

    motion_asentar(m);

    return llegado;
}

/**
 * @brief Gira sobre el centro el ángulo pedido.
 *
 * La velocidad se pide proporcional al error de ángulo, con un **mínimo** que vence la fricción
 * estática: sin ese mínimo el robot se queda a medio giro pidiendo un duty que no lo mueve, que es el
 * fallo clásico de un lazo de posición en un motor con reductora.
 *
 * @param m Estructura de las primitivas
 * @param grados Ángulo a girar; **positivo a la izquierda** (antihorario), como θ en la odometría
 * @return 1 si llegó dentro de tolerancia, 0 si agotó el tiempo o se pidió salir
 * @note El ángulo se mide con el **giroscopio** (no arrastra el deslizamiento del pivote); si la IMU
 *       no responde se cae al respaldo por odometría.
 * @see `MOTION_GYRO_SIGNO_Z` y el convenio de signo, comunes al avance: positivo = izquierda.
 */
uint8_t MOTION_Girar(Motion_t *m, float grados)
{
    float    objetivo_rad  = m->odo.theta + (grados / MOTION_RAD_A_GRADOS);
    float    objetivo_gyro = m->gyro_total + grados;
    int32_t  um_izq_0     = m->um_izq_previo;
    int32_t  um_der_0     = m->um_der_previo;
    uint32_t t_inicio     = HAL_GetTick();
    uint32_t t_ultimo     = t_inicio;
    uint8_t  llegado      = 0U;

    /* El integrador vive en la llamada y no en `m`: el giro es una maniobra cerrada y no tiene sentido
       arrastrar el error de una esquina a la siguiente. */
    float integrador = 0.0f;

    while ((HAL_GetTick() - t_inicio) < MOTION_TIMEOUT_MS) {
        uint32_t ahora;
        float    dt;
        float    error_grados;
        float    error_gyro;
        float    error_odo;
        float    vel;
        float    vel_min;
        float    desplazamiento_mm;
        float    centro_mm_s;
        float    termino_i;

        if (BUTTONS_SalidaSolicitada() != 0U) {
            break;
        }

        /* El giroscopio se lee **antes** de calcular el error, porque es la referencia. */
        ahora = HAL_GetTick();
        dt       = (float)(ahora - t_ultimo) / 1000.0f;
        t_ultimo = ahora;

        (void)motion_gyro_leer(m, dt);

        /* ⚠️ **La referencia del giro es el GIROSCOPIO**, y no es un capricho: medido el 2026-10-02,
           la odometría sobreestima los giros un **44 %**.

           Un giro sobre el centro obliga a las ruedas a **deslizar lateralmente** para pivotar, así
           que los arcos que cuentan los encoders son mayores que el giro real del robot: medido en el
           banco, con `>MOV_GYR` como testigo, la odometría dijo **+125,4°** donde el robot había girado
           **+87,1°**. Con la odometría como referencia, `MOTION_Girar(90)` corta cuando marca 90° y el
           robot se queda en **62°**: un **30 % menos** en cada esquina, y eso el *wall follower* lo
           acumula.

           ⚠️ Y ojo con la tentación de «arreglarlo» tocando la trocha: el factor 1,44 vale para el
           pivote, pero en el **avance** la odometría es buena (medido el mismo día: **+2,3°** de
           desvío frente a −0,1° del giroscopio). Un solo parámetro no puede servir para los dos, y por
           eso la referencia correcta es el sensor que no depende de las ruedas. */
        /* Los dos errores se calculan **siempre**; cuál manda lo decide el modo. Existe para poder
           comparar las tres referencias en banco con la misma maniobra (orden `GYRO` de la consola). */
        error_gyro = objetivo_gyro - m->gyro_total;
        error_odo  = motion_error_angulo(objetivo_rad - m->odo.theta) * MOTION_RAD_A_GRADOS;

        if (motion_gyro_fiable(m) == 0U) {
            /* Sin giroscopio de fiar no hay nada que elegir. */
            error_grados = error_odo;
        } else {
            switch (motion_modo_giro) {
            case MOTION_GIRO_ODOMETRIA:
                error_grados = error_odo;
                break;
            case MOTION_GIRO_GIROSCOPIO:
                error_grados = error_gyro;
                break;
            default:
                error_grados = 0.5f * (error_odo + error_gyro);
                break;
            }
        }

        if (fabsf(error_grados) <= MOTION_TOL_GRADOS) {
            llegado = 1U;
            break;
        }

        /* Término I, con **anti-windup por tope de la contribución**. Por defecto la ganancia es 0 y no
           aporta nada; está para el día que haya un sesgo que quitar. El tope evita que el integrador
           llegue al objetivo cargado y haya que descargarlo girando de más. */
        termino_i = 0.0f;

        if (motion_giro_ki > 0.0f) {
            integrador += error_grados * dt;
            termino_i    = motion_giro_ki * integrador;

            if (termino_i > MOTION_GIRO_I_MAX_MM_S) {
                termino_i   = MOTION_GIRO_I_MAX_MM_S;
                integrador  = termino_i / motion_giro_ki;
            } else if (termino_i < -MOTION_GIRO_I_MAX_MM_S) {
                termino_i   = -MOTION_GIRO_I_MAX_MM_S;
                integrador  = termino_i / motion_giro_ki;
            }
        }

        /* El mando sale del error **con signo**, más el integrador, menos la **velocidad angular**
           medida. Ese término D es lo que quita el **rebote** del giro, y aquí sale casi gratis: el
           giroscopio mide la velocidad angular directamente, así que no hay que derivar ni filtrar la
           señal.

           ⚠️ **El D está subordinado al P y hay que elegirlos juntos.** Medido el 2026-10-02: con
           `K_p` = 4 y `K_d` = 1,0, a 90 °/s el D pedía 90 mm/s y el P, con 22° de error, solo 88 — el
           D mandaba durante todo el giro, el robot frenaba en seco y **no lo completaba**. */
        vel = (motion_giro_kp * error_grados) + termino_i -
              (motion_giro_kd * m->gyro_velocidad);

        if (vel > MOTION_VEL_MM_S) {
            vel = MOTION_VEL_MM_S;
        } else if (vel < -MOTION_VEL_MM_S) {
            vel = -MOTION_VEL_MM_S;
        }

        /* ⚠️ **El mínimo grande solo si el robot está DE VERDAD parado.**

           Con el robot ya girando la fricción a vencer es la dinámica, mucho menor, y mantener los
           40 mm/s era lo que hacía que llegara al objetivo a **59 °/s** y se pasara de largo una y
           otra vez. Medido el 2026-10-02: el usuario lo describió como «el giro hace rebote», y era lo
           que dispersaba el ángulo final.

           Pero bajarlo a 10 **tampoco vale**: 10 mm/s no vencen la fricción estática de una de las dos
           ruedas, así que **una se queda pegada y el robot pivota sobre ella** —medido, el morro acabó
           **4 cm** por delante. El mínimo vale **0** cuando el robot rueda, y la fricción la vence el
           integrador del lazo de velocidad de cada rueda (`PID_KI` = 0,5). */
        vel_min = (fabsf(m->gyro_velocidad) > motion_giro_vdps)
                      ? motion_giro_vmin
                      : motion_giro_varr;

        if ((vel > 0.0f) && (vel < vel_min)) {
            vel = vel_min;
        } else if ((vel < 0.0f) && (vel > -vel_min)) {
            vel = -vel_min;
        }

        /* **Girar sin desplazarse**: los dos arcos han de ser iguales y opuestos, o el robot se va
           mientras gira. No lo son —un motor vence el rozamiento antes que el otro— y medido el
           2026-10-01 un giro de 90° desplazaba **~18 mm** (72 mm en un cuadrado, sobre celdas de
           180 mm).

           El desplazamiento del centro es la **media** de los dos arcos, así que se corrige con un
           término **común**: lo mismo sumado a las dos ruedas mueve el centro y **no toca la
           diferencia**, o sea que no toca el ángulo. Los dos lazos quedan desacoplados.

           La corrección es proporcional a lo que **ya** se ha desplazado (control sobre la posición,
           no sobre la velocidad), así que no arrastra un error de régimen grande: con
           `MOTION_GIRO_CENTRO_KP` el residuo es de un par de milímetros. */
        desplazamiento_mm = (float)((m->um_izq_previo - um_izq_0) +
                                    (m->um_der_previo - um_der_0)) / 2000.0f;
        centro_mm_s       = -MOTION_GIRO_CENTRO_KP * desplazamiento_mm;

        /* ⚠️ **El tope no es un adorno: sin él el giro se pierde.** El corrector es un término
           **común** a las dos ruedas, así que no toca la diferencia —que es lo que gira— pero sí
           mueve el robot. Y girando **rozando las paredes** la odometría cuenta un desplazamiento
           que no es real: medido el 2026-10-02 en el laberinto, con 100 mm de cuenta falsa y
           `MOTION_GIRO_CENTRO_KP` = 8 el corrector pedía **800 mm/s a las dos ruedas** y el robot
           salía disparado hacia la pared en vez de pivotar (`>WF_ATASCO:1`). */
        if (centro_mm_s > MOTION_GIRO_CENTRO_MAX_MM_S) {
            centro_mm_s = MOTION_GIRO_CENTRO_MAX_MM_S;
        } else if (centro_mm_s < -MOTION_GIRO_CENTRO_MAX_MM_S) {
            centro_mm_s = -MOTION_GIRO_CENTRO_MAX_MM_S;
        }

        /* Girar a la izquierda (θ positivo) es que la rueda derecha avance y la izquierda retroceda:
           dθ = (ds_der − ds_izq) / L. */
        motion_paso(m, -vel + centro_mm_s, vel + centro_mm_s, dt);

        HAL_Delay(MOTION_PASO_MS);
    }

    motion_asentar(m);

    return llegado;
}
