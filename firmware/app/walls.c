/**
 * @file walls.c
 * @brief Detección de paredes (capa APP): de la medida de los receptores IR a «hay pared / no hay».
 *
 * Cada pared se mide **conmutando su emisor**: se lee el receptor con el emisor apagado y con el emisor
 * encendido, y se resta. La luz ambiente cae en los dos términos y se va en la resta, así que el umbral
 * vale igual en un pasillo con sol que en uno oscuro.
 *
 * @note Este módulo se lleva su propia conmutación de emisores en vez de reutilizar la de
 *       `application.c`, que es `static` y está atada al banco de pruebas de IR. **Es deuda técnica
 *       anotada**: lo suyo es que la medida cruda (emisor y receptor) viva en `sensor/ir.c` y que las
 *       dos partes la compartan. No se ha hecho hoy porque refactorizar `application.c` habría puesto en
 *       riesgo las pruebas de IR que ya están validadas.
 */

#include "app/walls.h"
#include "hal/adc.h"
#include "pinout.h"

/** @brief Pines de los emisores de pared, en el orden en que los numera la telemetría. */
static const uint16_t walls_emisor_pin[4] = { IR_E1_PIN, IR_E2_PIN, IR_E3_PIN, IR_E4_PIN };

/** @brief Un receptor de pared: dónde se lee y qué emisor hay que encender para iluminarlo. */
typedef struct {
    ADC_Pin_t receptor;   /**< Canal del ADC donde está el receptor  */
    uint8_t   emisor;     /**< Emisor que lo ilumina (0..3)          */
} Walls_Sensor_t;

/**
 * @brief Receptores que miran al frente: `R0` (frontal derecho) y `R3` (frontal izquierdo).
 *
 * La pareja emisor↔receptor la mide la prueba 15; aquí se copia porque cada receptor solo ve al emisor
 * que apunta hacia el mismo lado (`ir_pareja_emisor[]`).
 */
static const Walls_Sensor_t walls_frente[2] = {
    { ADC_PIN_A2, 3U },   /**< R0, que ve al emisor IR3 */
    { ADC_PIN_A5, 0U },   /**< R3, que ve al emisor IR0 */
};

/**
 * @brief Receptor de cada lateral, indexado por `Wall_t` a partir de `WALL_DERECHA`.
 *
 * `R1` es el lateral derecho y `R2` el izquierdo, y son los que mantienen el robot pegado a la pared.
 */
static const Walls_Sensor_t walls_lateral[2] = {
    { ADC_PIN_A3, 1U },   /**< R1 → WALL_DERECHA   */
    { ADC_PIN_A4, 2U },   /**< R2 → WALL_IZQUIERDA */
};

static int16_t  walls_umbral[WALL_N];
static int16_t  walls_medida[WALL_N];
static uint8_t  walls_hay[WALL_N];

/**
 * @brief Enciende un emisor de pared y apaga el resto.
 *
 * @param emisor 1..4 para uno concreto, 0 para apagarlos todos
 */
static void walls_emisor(uint8_t emisor)
{
    for (uint8_t i = 0U; i < 4U; i++) {
        uint8_t encender = (uint8_t)(emisor == (uint8_t)(i + 1U));

        HAL_GPIO_WritePin(IR_GPIO_PORT, walls_emisor_pin[i],
                          (encender != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

/**
 * @brief Mide un receptor restando las lecturas con el emisor apagado y encendido.
 *
 * @param receptor Canal del ADC a leer
 * @param emisor Emisor que se conmuta (0..3)
 * @return Cuentas de `encendido − apagado`: positivo si el receptor ve la luz del emisor
 */
static int16_t walls_medir(ADC_Pin_t receptor, uint8_t emisor)
{
    uint16_t apagado;
    uint16_t encendido;

    walls_emisor(0U);
    apagado = ADC_ReadPin(receptor);

    walls_emisor((uint8_t)(emisor + 1U));
    encendido = ADC_ReadPin(receptor);

    walls_emisor(0U);

    return (int16_t)((int32_t)encendido - (int32_t)apagado);
}

/**
 * @brief Clasifica una medida aplicando la histéresis y guarda el resultado.
 *
 * @param cual Pared a la que corresponde la medida
 * @param medida Medida recién tomada, en cuentas
 */
static void walls_decidir(Wall_t cual, int16_t medida)
{
    int16_t umbral = walls_umbral[cual];

    walls_medida[cual] = medida;

    if (walls_hay[cual] != 0U) {
        walls_hay[cual] = (uint8_t)(medida > (int16_t)(umbral - WALLS_HISTERESIS));
    } else {
        walls_hay[cual] = (uint8_t)(medida > (int16_t)(umbral + WALLS_HISTERESIS));
    }
}

/**
 * @brief Deja los umbrales en su valor de arranque y las paredes en «no hay».
 */
void WALLS_Init(void)
{
    for (uint8_t i = 0U; i < (uint8_t)WALL_N; i++) {
        walls_umbral[i] = WALLS_UMBRAL_DEF;
        walls_medida[i] = 0;
        walls_hay[i]    = 0U;
    }
}

/**
 * @brief Mide los receptores y decide si hay pared en cada lado.
 *
 * En el frente manda **el receptor que más ve**: con los dos frontales abiertos hacia fuera, en un
 * pasillo estrecho una pared puede iluminar solo uno de los dos, y quedarse con el otro daría un
 * «no hay pared» falso justo cuando hay que girar.
 */
void WALLS_Actualizar(void)
{
    int16_t frente;
    int16_t otra;

    frente = walls_medir(walls_frente[0].receptor, walls_frente[0].emisor);

    for (uint8_t i = 1U; i < 2U; i++) {
        otra = walls_medir(walls_frente[i].receptor, walls_frente[i].emisor);
        if (otra > frente) {
            frente = otra;
        }
    }

    walls_decidir(WALL_FRENTE, frente);
    walls_decidir(WALL_DERECHA,
                  walls_medir(walls_lateral[0].receptor, walls_lateral[0].emisor));
    walls_decidir(WALL_IZQUIERDA,
                  walls_medir(walls_lateral[1].receptor, walls_lateral[1].emisor));
}

/**
 * @brief Dime si hay pared a ese lado.
 */
uint8_t WALLS_Hay(Wall_t cual)
{
    if ((uint8_t)cual >= (uint8_t)WALL_N) {
        return 0U;
    }

    return walls_hay[cual];
}

/**
 * @brief Última medida de esa pared, en cuentas de `encendido − apagado`.
 */
int16_t WALLS_GetMedida(Wall_t cual)
{
    if ((uint8_t)cual >= (uint8_t)WALL_N) {
        return 0;
    }

    return walls_medida[cual];
}

/**
 * @brief Cambia el umbral de una pared.
 */
uint8_t WALLS_SetUmbral(Wall_t cual, int16_t umbral)
{
    if (((uint8_t)cual >= (uint8_t)WALL_N) || (umbral < 0) || (umbral > 4095)) {
        return 0U;
    }

    walls_umbral[cual] = umbral;

    return 1U;
}

/**
 * @brief Umbral en uso de una pared.
 */
int16_t WALLS_GetUmbral(Wall_t cual)
{
    if ((uint8_t)cual >= (uint8_t)WALL_N) {
        return 0;
    }

    return walls_umbral[cual];
}
