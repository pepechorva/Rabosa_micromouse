#ifndef APP_CONSOLE_H
#define APP_CONSOLE_H

/*
 * Capa APP — consola de comandos por USART3 (el mismo puerto de la telemetría).
 *
 * La telemetría (app/telemetry.c) es la SALIDA: `>ETIQUETA:valor`. Aquí está la ENTRADA: las órdenes
 * que el usuario escribe desde Teleplot o desde cualquier monitor serie. Sirve para ajustar cosas que
 * dependen del montaje sin recompilar ni flashear.
 *
 * Órdenes (protocolo completo en `Documentacion/comandos-serie.md`):
 *   BAT LIST                    publica umbrales, factor del divisor y tensión medida
 *   BAT AVISO_PACK 7.4          cambia un umbral: con punto se escribe en VOLTIOS
 *   BAT CRIT_PACK 6600          ...y sin punto, en MILIVOLTIOS
 *   BAT AVISO_CELDA 3.45
 *   BAT DESEQ 0.15
 *   BAT CAL 7400                CALIBRA el divisor: la tensión real del pack ahora mismo
 *   HELP                        igual que BAT LIST (lista los valores)
 *
 * Respuestas: `>BAT_<UMBRAL>`, `>BAT_K` y `>VBAT` al aplicar (y en `BAT LIST`), y `>CMD_ERR:<código>`
 * si algo va mal (1 umbral desconocido · 2 valor no válido · 3 rechazado por rango o incoherencia ·
 * 4 orden desconocida · 5 no se pudo calibrar). Como el resto de la telemetría, todo son números.
 *
 * Uso:
 *   CONSOLE_Init();        // una vez, después de MX_USART3_UART_Init()
 *   CONSOLE_Update();      // en el bucle principal (lee del buffer de la UART y ejecuta)
 */

/** @brief Arranca la recepción por interrupción y deja lista la consola. */
void CONSOLE_Init(void);

/**
 * @brief Saca los bytes que hayan llegado, monta las líneas y ejecuta las órdenes.
 *
 * @note No bloquea: si no hay nada, no hace nada. Llamar a menudo (el bucle principal va a 1 kHz).
 *       Los bytes no se pierden aunque se tarde en llamar, porque los guarda la interrupción de la
 *       UART; lo que sí se descarta es una línea demasiado larga.
 */
void CONSOLE_Update(void);

#endif /* APP_CONSOLE_H */
