#include "sensor/battery.h"
#include "hal/adc.h"

/* ==================================================================
 * Medida de la batería 2S con divisores de tensión.
 *
 *   Pack (PB0):    VBAT+ ──[ 22 kΩ ]──┬── PB0
 *                                     │
 *                                 [ 10 kΩ ]      (sin condensador: no hay sitio)
 *                                     │
 *                                    GND
 *
 *   Celda 1 (PB1): CEL1  ──[ 3k9 ]──┬── PB1
 *                                    │
 *                                [ 10 kΩ ]
 *                                    │
 *                                   GND
 *
 *   k = R2/(R1+R2)  →  pack  10/32   = 0,3125   (8,4 V ⇒ 2,625 V en el pin)
 *                       celda 10/13,9 = 0,7194   (4,2 V ⇒ 3,02 V en el pin)
 *
 * ⚠️ Los dos divisores **no son iguales**, así que cada canal lleva su propia k (ver battery.h): con una
 *    sola, la celda 2 —que sale por resta— daba negativa y el firmware avisaba de una batería que no
 *    existe.
 *
 * ⚠️ CALIBRAR CON EL MULTÍMETRO (prueba 12 o desde la consola): poner la fuente/batería a una
 * tensión conocida y ejecutar `BAT CAL <mV>`; el módulo recalcula `k` para que `>VBAT` marque esa
 * tensión. Así se corrigen de una vez la tolerancia de las resistencias (1 %) y la de VREF+ = VDDA
 * (±1-2 % del LDO), que sin calibrar desvían la medida un ±5 %.
 *
 * ⚠️ EN EL BANCO la celda 1 puede no estar conectada (el pack se alimenta con una
 * fuente o con una batería de otra química/tensión): PB1 lee 0 V y eso no es una
 * celda agotada, es «no hay medida». El módulo lo detecta y clasifica solo con el
 * pack. Y con medida, la celda tampoco puede ser crítica: solo MATIZA el aviso
 * (ver más abajo). Si no hay tensión ni en el pack (placa alimentada por USB o por
 * el ST-Link), no se clasifica nada: sin medida no hay nada que proteger.
 * ================================================================== */

/* Divisores, en kΩ (documentan los valores de k de battery.h) */
#define BAT_R_ARRIBA_K    22.0f      /* R1 del pack, de la batería al pin */
#define BAT_R_ABAJO_K     10.0f      /* R2, del pin a GND */
#define BAT_R_ARRIBA_CELDA_K  3.9f   /* R1 de la celda 1 (3k9) */

#define BAT_VREF          3.3f       /* VREF+ = VDDA (AMS1117-3.3) */
#define BAT_ADC_MAX       4096.0f    /* 12 bits */

/* Media de N lecturas: quita el ruido del ADC y el rizado de la fuente */
#define BAT_MUESTRAS      16

/* Por debajo de esta tensión se da por hecho que el divisor de la celda 1 no está montado: en el
   banco el pack se alimenta con una fuente y ese canal queda al aire (lee ≈0 V). Una celda de LiPo
   real, incluso agotada, nunca baja de ≈2 V, así que el umbral no se cruza por batería baja. */
#define BAT_V_CELDA_PRESENTE  0.50f

/* Lo mismo para el canal del pack: con alimentación por USB o por el ST-Link, PB0 lee ≈0 V. Sin
   medida no hay batería que proteger, así que el módulo no clasifica nada (si no, el robot arrancaría
   en FAULT y no se podría ni entrar al menú para hacer las pruebas). */
#define BAT_V_PACK_PRESENTE   0.50f

/* ------------------------------------------------------------------ */
/* Umbrales en uso: empiezan en los valores por defecto (BAT_*_DEF, en battery.h) y se pueden cambiar
   en caliente por telemetría. Reservados aparte para que el resto del módulo los lea igual. */
static float umbrales[BAT_UMBRAL_N];

const char *const BATTERY_UMBRAL_NOMBRES[BAT_UMBRAL_N] = {
    "AVISO_PACK",   /* BAT_UMBRAL_PACK_AVISO  */
    "CRIT_PACK",    /* BAT_UMBRAL_PACK_CRIT   */
    "AVISO_CELDA",  /* BAT_UMBRAL_CELDA_AVISO */
    "DESEQ",        /* BAT_UMBRAL_DESEQ       */
};

/* El nombre de cada umbral tiene que cuadrar con su índice: si no, no compila. */
typedef char bat_nombres_cuadran[
    ((sizeof(BATTERY_UMBRAL_NOMBRES) / sizeof(BATTERY_UMBRAL_NOMBRES[0])) == BAT_UMBRAL_N) ? 1 : -1];

/* ------------------------------------------------------------------ */
static uint32_t suma_b0;
static uint32_t suma_b1;
static uint8_t  n_muestras;

static float v_pack;
static float v_celda1;
static float v_celda2;
static float v_deseq;

static Battery_Estado_t estado = BATTERY_OK;
static uint8_t aviso_celda;
static float   aviso_v;
static uint8_t celda1_presente;
static uint8_t pack_presente;

/* Factores de los divisores en uso —**uno por canal**, porque los divisores son distintos— y última
   lectura cruda del pack, que es la que usa BATTERY_CalibrarConPack(). */
static float    k_divisor_pack;
static float    k_divisor_celda;
static uint16_t adc_pack_ultimo;

/* ------------------------------------------------------------------ */
void BATTERY_Init(void)
{
    suma_b0    = 0;
    suma_b1    = 0;
    n_muestras = 0;

    v_pack     = 0.0f;
    v_celda1   = 0.0f;
    v_celda2   = 0.0f;
    v_deseq    = 0.0f;

    estado      = BATTERY_OK;
    aviso_celda = 0;
    aviso_v     = 0.0f;

    celda1_presente = 0;
    pack_presente   = 0;

    umbrales[BAT_UMBRAL_PACK_AVISO]  = BAT_V_AVISO_PACK_DEF;
    umbrales[BAT_UMBRAL_PACK_CRIT]   = BAT_V_CRIT_PACK_DEF;
    umbrales[BAT_UMBRAL_CELDA_AVISO] = BAT_V_AVISO_CELDA_DEF;
    umbrales[BAT_UMBRAL_DESEQ]       = BAT_V_DESEQUILIBRIO_DEF;

    k_divisor_pack  = BAT_K_DIVISOR_PACK_DEF;
    k_divisor_celda = BAT_K_DIVISOR_CELDA_DEF;
    adc_pack_ultimo = 0U;
}

/* ------------------------------------------------------------------ */
/**
 * @brief Pasa una cuenta del ADC a la tensión del canal, deshaciendo **su** divisor.
 *
 * @param adc Cuenta del ADC (0..4095)
 * @param k Factor del divisor de ese canal (k = R2/(R1+R2))
 * @return Tensión en el canal, en voltios
 */
static float bat_adc_a_voltios(uint16_t adc, float k)
{
    return ((float)adc * BAT_VREF / BAT_ADC_MAX) / k;
}

/**
 * @brief Clasifica el pack cuando no hay medida de las celdas.
 *
 * @param pack Tensión total medida (V)
 * @return Estado según los umbrales del pack
 */
static Battery_Estado_t bat_estado_pack(float pack)
{
    if (pack < umbrales[BAT_UMBRAL_PACK_CRIT]) {
        return BATTERY_CRITICA;
    }
    if (pack < umbrales[BAT_UMBRAL_PACK_AVISO]) {
        return BATTERY_AVISO;
    }

    return BATTERY_OK;
}

/* ------------------------------------------------------------------ */
void BATTERY_Update(void)
{
    suma_b0 += ADC_ReadPin(ADC_PIN_B0);
    suma_b1 += ADC_ReadPin(ADC_PIN_B1);
    n_muestras++;

    if (n_muestras < BAT_MUESTRAS) return;

    uint16_t adc_b0 = (uint16_t)(suma_b0 / BAT_MUESTRAS);
    uint16_t adc_b1 = (uint16_t)(suma_b1 / BAT_MUESTRAS);

    suma_b0    = 0;
    suma_b1    = 0;
    n_muestras = 0;

    /* Se guarda la lectura cruda del pack: es la que usa BATTERY_CalibrarConPack() */
    adc_pack_ultimo = adc_b0;

    v_pack   = bat_adc_a_voltios(adc_b0, k_divisor_pack);
    v_celda1 = bat_adc_a_voltios(adc_b1, k_divisor_celda);

    pack_presente   = (uint8_t)(v_pack >= BAT_V_PACK_PRESENTE);
    celda1_presente = (uint8_t)(pack_presente && (v_celda1 >= BAT_V_CELDA_PRESENTE));

    if (!pack_presente) {
        v_celda1        = 0.0f;
        v_celda2        = 0.0f;
        v_deseq         = 0.0f;
        celda1_presente = 0U;
        estado          = BATTERY_OK;
        aviso_celda     = 0U;
        aviso_v         = 0.0f;
        return;
    }

    if (!celda1_presente) {
        v_celda2    = 0.0f;
        v_deseq     = 0.0f;
        estado      = bat_estado_pack(v_pack);
        aviso_celda = 0U;
        aviso_v     = v_pack;
        return;
    }

    v_celda2 = v_pack - v_celda1;
    v_deseq  = v_celda1 - v_celda2;

    /* La celda 2 sale por **resta**: si la pareja es incoherente (divisores distintos, un cable suelto,
       una celda mal) da un valor sin sentido —el 30-sep salió −1,29 V— y el firmware acusaría una
       batería que no existe. Se conservan los números crudos para que el problema se vea en la
       telemetría, pero la clasificación se queda con el pack, el único canal medido directo. */
    if (v_celda2 < BAT_V_CELDA_PRESENTE) {
        estado      = bat_estado_pack(v_pack);
        aviso_celda = 0U;
        aviso_v     = v_pack;
        return;
    }

    float peor_celda = (v_celda1 < v_celda2) ? v_celda1 : v_celda2;

    /* Lo que lleva el robot a CRÍTICA (y de ahí al FAULT) es SOLO el pack: la celda es monitorización,
       y con otra química o con una fuente de laboratorio sus umbrales no aplican. Una celda rara, como
       mucho, matiza el aviso. */
    estado = bat_estado_pack(v_pack);

    if ((estado == BATTERY_OK) &&
        ((peor_celda < umbrales[BAT_UMBRAL_CELDA_AVISO]) ||
         (fabsf(v_deseq) > umbrales[BAT_UMBRAL_DESEQ]))) {
        estado = BATTERY_AVISO;
    }

    /* --- Qué se enseña: la celda si el aviso lo dispara una celda --- */
    aviso_celda = (uint8_t)((peor_celda < umbrales[BAT_UMBRAL_CELDA_AVISO]) ||
                            (fabsf(v_deseq) > umbrales[BAT_UMBRAL_DESEQ]));
    aviso_v     = aviso_celda ? peor_celda : v_pack;
}

/* ------------------------------------------------------------------ */
float BATTERY_GetPackV(void)          { return v_pack; }
float BATTERY_GetCelda1V(void)        { return v_celda1; }
float BATTERY_GetCelda2V(void)        { return v_celda2; }
float BATTERY_GetDesequilibrioV(void) { return v_deseq; }

Battery_Estado_t BATTERY_GetEstado(void) { return estado; }

uint8_t BATTERY_Celda1Presente(void) { return celda1_presente; }

uint8_t BATTERY_PackPresente(void) { return pack_presente; }

uint8_t BATTERY_SetUmbral(Battery_Umbral_t cual, float valor)
{
    if (((int)cual >= (int)BAT_UMBRAL_N) || (valor < BAT_V_UMBRAL_MIN) ||
        (valor > BAT_V_UMBRAL_MAX)) {
        return 0U;
    }

    if ((cual == BAT_UMBRAL_PACK_CRIT) &&
        (valor >= umbrales[BAT_UMBRAL_PACK_AVISO])) {
        return 0U;
    }

    if ((cual == BAT_UMBRAL_PACK_AVISO) &&
        (valor <= umbrales[BAT_UMBRAL_PACK_CRIT])) {
        return 0U;
    }

    umbrales[cual] = valor;

    return 1U;
}

float BATTERY_GetUmbral(Battery_Umbral_t cual)
{
    if ((int)cual >= (int)BAT_UMBRAL_N) {
        return 0.0f;
    }

    return umbrales[cual];
}

float BATTERY_GetKDivisor(void)
{
    return k_divisor_pack;
}

uint8_t BATTERY_CalibrarConPack(float v_real)
{
    float k;

    if ((v_real < BAT_V_CAL_MIN) || (v_real > BAT_V_CAL_MAX) || (adc_pack_ultimo == 0U)) {
        return 0U;
    }

    k = (((float)adc_pack_ultimo * BAT_VREF) / BAT_ADC_MAX) / v_real;

    if ((k < BAT_K_MIN) || (k > BAT_K_MAX)) {
        return 0U;   /* no es un divisor de estos: mejor no tocar nada */
    }

    /* Solo se toca la del pack: lo que corrige `BAT CAL` son las **resistencias de ese divisor**, y las de
       la celda son otras. Heredar el factor a la celda daba 3,94 V donde el multímetro mide 3,79 (30-sep). */
    k_divisor_pack = k;

    return 1U;
}

uint8_t BATTERY_AvisoCelda(void)
{
    return (estado == BATTERY_OK) ? 0U : aviso_celda;
}

float BATTERY_GetAvisoV(void)
{
    return aviso_v;
}
