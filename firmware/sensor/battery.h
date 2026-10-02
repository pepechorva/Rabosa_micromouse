#ifndef SENSOR_BATTERY_H
#define SENSOR_BATTERY_H

#include <stdint.h>
#include "peripherals.h"
/*
 * Capa SENSOR — medida de la batería LiPo 2S.
 *
 * Hardware (ver `Documentacion/hardware.md` §4.6): dos divisores iguales de
 * 22 kΩ / 10 kΩ (sin condensador en el pin: no hay sitio en el chasis).
 *
 *   PB0 → tensión TOTAL del pack
 *   PB1 → tensión de la CELDA 1 (la que está pegada a GND)
 *   celda 2 = pack − celda 1
 *
 * Si el divisor de PB1 no está montado (banco: el pack se alimenta con una fuente), el módulo lo
 * detecta y clasifica solo con el pack — ver `BATTERY_Celda1Presente()`. Si tampoco hay tensión en
 * PB0 (placa alimentada por USB), no clasifica nada — ver `BATTERY_PackPresente()`.
 *
 * Esta capa solo MIDE y CLASIFICA. Enseñar el aviso (LED, pantalla) es cosa de
 * la capa APP; cortar los motores, de CONTROL/APP.
 *
 * Uso:
 *   BATTERY_Init();
 *   BATTERY_Update();            // en el bucle principal (cada 1 ms)
 *   if (BATTERY_GetEstado() != BATTERY_OK) { ... }
 */

/* Valores POR DEFECTO de los umbrales: `BATTERY_Init()` los pone, y a partir de ahí se pueden cambiar
 * en caliente por telemetría (`BAT AVISO_PACK 6.6`, ver `Documentacion/comandos-serie.md`).
 *
 * Decisión del usuario (30-sep): **solo se avisa cuando alguna celda se acerca a 3,30 V** —el pack a
 * 6,60 V, que es lo mismo—. Los valores anteriores (3,45 V/celda y 7,00 V de pack) saltaban con la
 * batería sana, así que el robot se pasaba el rato en AVISO.
 *
 * SOLO el pack puede llevar el estado a `BATTERY_CRITICA`: los umbrales de celda son **informativos**
 * (matizan el aviso, no protegen). Con otra química o con una fuente de laboratorio los valores por
 * celda no significan lo mismo, y una celda sin medida no es una celda agotada. */
#define BAT_V_AVISO_PACK_DEF     6.60f   /* 3.30 V/celda */
#define BAT_V_AVISO_CELDA_DEF    3.30f   /* informativo */
#define BAT_V_CRIT_PACK_DEF      6.00f   /* 3.00 V/celda: único umbral que bloquea (FAULT) */
#define BAT_V_DESEQUILIBRIO_DEF  0.40f   /* informativo: con 0,15 V saltaba con la batería sana */

/* Margen que se admite al fijar un umbral por telemetría */
#define BAT_V_UMBRAL_MIN         0.10f
#define BAT_V_UMBRAL_MAX        15.00f

/* Factores de los divisores montados, **uno por canal**: k = R2/(R1+R2).
 *
 * | Canal | R1 (arriba) | R2 (abajo) | k |
 * |:------|:-----------:|:----------:|:--|
 * | **Pack** (PB0) | 22 kΩ | 10 kΩ | **0,3125** |
 * | **Celda 1** (PB1) | 3k9 | 10 kΩ | **0,7194** |
 *
 * ⚠️ **Los dos divisores NO son iguales**, y por eso hay una constante por canal: con una sola, la celda 1
 *    se dividía por el factor del pack (2,39 veces menos de lo que toca) y salía ≈9 V, y la celda 2 —que
 *    sale por **resta**— daba **negativa** (−1,29 V medidos el 30-sep), disparando avisos de batería que
 *    no existen.
 * ⚠️ El ORDEN importa: la resistencia grande va **arriba**. Al revés, el pin del pack se iría a 5,09 V, y
 *    PB0 **no** es tolerante a 5 V (pin de ADC: máximo absoluto 4,0 V, sin la marca FT).
 *
 * El valor de ARRANQUE del pack es el **calibrado contra el multímetro** (30-sep): con la fuente a 7,40 V
 * el pin medía 2,241 V y el ADC leía 2,231 V, así que 0,3125 quedaba un 3,5 % alto (marcaba 7,14 V).
 * 0,3015 = 2,231 V / 7,40 V es lo que hace que `>VBAT` cuadre; lo que queda de tolerancia (VDDA) lo cierra
 * `BAT CAL <mV>` desde la consola (prueba 12).
 *
 * @note **La calibración del pack NO toca la celda.** Se comprobó el 30-sep: con la $k$ teórica de la
 *       celda (0,7194) el firmware daba **3,80 V** y el multímetro **3,79 V** (0,3 % de error), mientras
 *       que heredando la corrección del pack daba 3,94 V (4 % arriba). ⇒ El 0,9648 que se aparta el pack
 *       es de **sus resistencias** (tolerancia del 5 %: 0,3015 en vez de 0,3125), no del ADC, así que no
 *       hay nada que heredar.
 */
#define BAT_K_DIVISOR_PACK_DEF   0.3015f
#define BAT_K_DIVISOR_CELDA_DEF  0.7194f
#define BAT_K_MIN                0.05000f
#define BAT_K_MAX                0.90000f
#define BAT_V_CAL_MIN            2.00f   /* tensión mínima que se acepta al calibrar */
#define BAT_V_CAL_MAX           15.00f

/** @brief Umbrales configurables (indice de `BATTERY_SetUmbral()` / `BATTERY_GetUmbral()`). */
typedef enum {
    BAT_UMBRAL_PACK_AVISO = 0, /**< Pack por debajo del cual se avisa (V)              */
    BAT_UMBRAL_PACK_CRIT,      /**< Pack por debajo del cual es crítica ⇒ FAULT (V)   */
    BAT_UMBRAL_CELDA_AVISO,    /**< Celda por debajo de la cual se avisa (V)          */
    BAT_UMBRAL_DESEQ,          /**< Desequilibrio entre celdas que avisa (V)          */
    BAT_UMBRAL_N               /**< Cuántos umbrales hay                              */
} Battery_Umbral_t;

/** @brief Nombre de cada umbral, en el mismo orden que el enum (para la consola). */
extern const char *const BATTERY_UMBRAL_NOMBRES[BAT_UMBRAL_N];

typedef enum {
    BATTERY_OK = 0,     /* todo por encima de los umbrales */
    BATTERY_AVISO,      /* conviene cambiar la batería pronto */
    BATTERY_CRITICA     /* hay que aterrizar ya */
} Battery_Estado_t;

/** @brief Reinicia acumuladores y estado. Llamar una vez al arrancar. */
void BATTERY_Init(void);

/**
 * @brief Lee los dos canales y actualiza la medida.
 *        Llamar en el bucle principal: acumula medias, así que cuanto más a
 *        menudo se llame, más suave sale la lectura (2 conversiones ≈ 42 µs).
 */
void BATTERY_Update(void);

float BATTERY_GetPackV(void);          /**< tensión total del pack (V) */
float BATTERY_GetCelda1V(void);        /**< tensión de la celda 1 (V) */
float BATTERY_GetCelda2V(void);        /**< tensión de la celda 2 (V) */
float BATTERY_GetDesequilibrioV(void); /**< V1 − V2 (0 = equilibradas) */

/**
 * @brief Si el divisor de la celda 1 está montado.
 *
 * @return 1 si PB1 da una tensión de celda normal · 0 si el canal lee ≈0 V. Con 0 (divisor sin
 *         montar: en el banco el pack se alimenta con una fuente y esa celda no se usa) la
 *         clasificación se hace SOLO con el pack, `BATTERY_GetCelda2V()` y
 *         `BATTERY_GetDesequilibrioV()` devuelven 0 y no se avisa de desequilibrio.
 */
uint8_t BATTERY_Celda1Presente(void);

/**
 * @brief Si hay medida del pack.
 *
 * @return 0 si PB0 lee ≈0 V: no hay batería que medir (banco: la placa se alimenta por USB o por el
 *         ST-Link). En ese caso el módulo deja el estado en `BATTERY_OK` y no clasifica nada — un
 *         canal sin medida no debe bloquear el robot en FAULT. Con 0, `BATTERY_Celda1Presente()`
 *         también devuelve 0.
 */
uint8_t BATTERY_PackPresente(void);

/** @brief Estado actual: lo decide el **pack** (la celda solo puede matizar un aviso, nunca una
 *         crítica). Lo que mira el firmware para protegerse es este valor. */
Battery_Estado_t BATTERY_GetEstado(void);

/**
 * @brief Si el aviso lo dispara una celda (y no el pack).
 *        Sirve para que la pantalla sepa si enseñar "BATERIA" o "CELDA".
 *        Es **informativo**: una celda baja o desequilibrada no bloquea nada.
 */
uint8_t BATTERY_AvisoCelda(void);

/** @brief Tensión que hay que enseñar en el aviso: la celda más baja si el
 *         aviso es por celda, el pack si es por el pack (V). */
float BATTERY_GetAvisoV(void);

/**
 * @brief Cambia un umbral en caliente.
 *
 * @param cual Umbral que se cambia
 * @param valor Nuevo valor (V)
 * @return 1 si se aplicó · 0 si se rechazó (fuera de `BAT_V_UMBRAL_MIN`..`BAT_V_UMBRAL_MAX`, o
 *         incoherente: el crítico del pack tiene que quedar por debajo del de aviso)
 */
uint8_t BATTERY_SetUmbral(Battery_Umbral_t cual, float valor);

/** @return Valor actual de un umbral (V), o 0 si el índice no existe. */
float BATTERY_GetUmbral(Battery_Umbral_t cual);

/** @return Factor del divisor del **pack** en uso (k = R2/(R1+R2)); arranca en `BAT_K_DIVISOR_PACK_DEF`. */
float BATTERY_GetKDivisor(void);

/**
 * @brief Ajusta el factor del divisor con una tensión medida a mano (calibrado de la prueba 12).
 *
 * Toma la **última medida del canal del pack** y calcula el k que hace que el firmware marque
 * exactamente la tensión del multímetro: así se corrigen de una vez la tolerancia de las resistencias
 * (1 %) y la de VREF+ = VDDA (±2 % del LDO), que es lo que desvía la medida.
 *
 * @param v_real Tensión real del pack (V), medida con el multímetro
 * @return 1 si se aplicó · 0 si no hay medida o la tensión no es razonable (`BAT_V_CAL_MIN`..`MAX`)
 */
uint8_t BATTERY_CalibrarConPack(float v_real);

#endif /* SENSOR_BATTERY_H */
