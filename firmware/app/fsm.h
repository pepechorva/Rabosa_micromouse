#ifndef APP_FSM_H
#define APP_FSM_H

/*
 * Capa APP — máquinas de estados escritas como el diagrama.
 *
 * Patrón de Remo Dentato (*Finite States Machines in C*, dev.to): cada estado es un bloque con
 * etiqueta y cada transición un `goto` explícito. Se gana que el código se lee igual que el
 * diagrama, que el depurador salta de etiqueta en etiqueta (breakpoint por estado) y que no hay
 * `switch` con `break` olvidados: si un estado no dice a dónde va, se sale de la máquina.
 *
 * Uso (ver `menu.c` y `application.c`):
 *
 *     fsm {
 *         fsmstate(START) { ...despacho y guardas...; fsmexit; }
 *         fsmstate(REPOSO) {
 *             if (nada_que_hacer) fsmexit;
 *             fsmgoto(TRABAJANDO);
 *         }
 *         fsmstate(TRABAJANDO) { ...; fsmgoto(REPOSO); }
 *     }
 *
 * ⚠️ REGLAS (las impone el patrón, no son opcionales):
 *   1. **Un `fsm { }` por función**: las etiquetas `fsm_state_*` y `fsm_exit` son fijas.
 *   2. **Exactamente un estado `START`**.
 *   3. **Nada de código ejecutable entre estados**: no corre. Los estados van seguidos.
 *   4. Los nombres de estado son identificadores (se concatenan a `fsm_state_`).
 *   5. Declarar las variables **dentro** de su estado (o fuera del `fsm`): si el despachador salta
 *      a un estado, el código que hubiera antes de él no se ejecuta.
 *
 * ⚠️ ESTA MÁQUINA SE REANUDA DE LLAMADA EN LLAMADA, no corre de una tirada como los ejemplos del
 * artículo (un tokenizador consume hasta el final). Por eso `START` es un **despachador**: mira en
 * qué estado estaba y salta a él. Ahí es donde van también las **guardas globales** (FAULT por
 * batería, por ejemplo), que deben poder interrumpir cualquier estado.
 *
 * @note Para depurar, cada módulo publica su estado con telemetría numérica al transicionar
 *       (`>APP_EST`, `>MENU_EST`); la tabla de nombres está en la documentación del módulo.
 * @see Referencia del patrón: https://dev.to/rdentato/finite-states-machines-in-c-129n
 */

/** @brief Abre la máquina. Entra siempre por `START` y define la salida `fsm_exit`. */
#define fsm          if (1) { goto fsm_state_START; fsm_exit: ; } else

/**
 * @brief Declara un estado.
 *
 * Si el flujo llega a la etiqueta por caída (fall-through), sale de la máquina: dentro de un estado
 * hay que decir explícitamente a dónde se va (`fsmgoto`) o salir (`fsmexit`).
 */
#define fsmstate(s)  for (;;) for (;;) if (1) goto fsm_exit; else fsm_state_ ## s:

/** @brief Transición al estado indicado. */
#define fsmgoto(s)   goto fsm_state_ ## s

/** @brief Termina este paso de la máquina y sigue por donde iba el programa. */
#define fsmexit      goto fsm_exit

#endif /* APP_FSM_H */
