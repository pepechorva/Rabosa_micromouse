/**
 * @file menu.c
 * @brief Motor del menú de la capa APP: máquina de estados + dibujo.
 *
 * Lo que se puede elegir está en las tablas del que llama (`application.c`); este módulo no conoce
 * ninguna prueba, solo entradas de menú. La navegación es una FSM (patrón de `app/fsm.h`) que se
 * reanuda de llamada en llamada:
 *
 *   LISTA --UP/DOWN--> MOVIENDO --repinta--> LISTA
 *   LISTA --OK--> ENTRANDO --submenu--> LISTA        (entra y repinta)
 *   LISTA --OK--> ENTRANDO --accion--> LANZANDO      (se devuelve a quien llama)
 *   LISTA --MENU--> VOLVIENDO --repinta--> LISTA     (o nada, si ya es la raíz)
 *
 * Perfil de la pantalla y convenciones: `Documentacion/maquina-estados-menu.md`.
 */

#include "app/menu.h"
#include "app/buttons.h"
#include "app/format.h"
#include "app/fsm.h"
#include "app/oled.h"
#include "app/telemetry.h"
#include "peripherals.h"

#include <stdio.h>

/**
 * @brief Estados del menú.
 *
 * El orden es el contrato de `>MENU_EST`: no reordenar sin actualizar
 * `Documentacion/maquina-estados-menu.md`.
 */
typedef enum {
    MENU_EST_LISTA = 0,  /**< Mostrando una lista, sin nada pulsado        */
    MENU_EST_MOVIENDO,   /**< UP/DOWN: mover el cursor y repintar          */
    MENU_EST_ENTRANDO,   /**< OK: entrar en un submenú o elegir una acción */
    MENU_EST_VOLVIENDO   /**< MENU: subir al menú padre                    */
} Menu_Estado_t;

static const Menu_Tabla_t *pila[MENU_MAX_PROFUNDIDAD];
static uint8_t             prof;
static uint8_t             idx;
static uint8_t             top;

static const Menu_Tabla_t *const *tablas_submenu;
static uint8_t                    n_submenus;

static Menu_Estado_t         menu_estado;
static uint8_t               menu_boton;    /* botón de este paso (0 = ninguno) */
static const Menu_Entrada_t *menu_elegida;  /* salida de la máquina             */

static float   menu_voltaje;     /* voltaje de batería que se pinta en la raíz */
static uint8_t menu_voltaje_on;  /* 0 mientras nadie haya fijado un voltaje    */

/**
 * @brief Anota el estado y lo publica por telemetría.
 *
 * @param estado Estado nuevo
 */
static void menu_publicar_estado(Menu_Estado_t estado)
{
    menu_estado = estado;
    TELEM_SendInt("MENU_EST", (int32_t)estado);
}

/** @brief Transición con traza: publica el estado destino y salta a él. */
#define MENU_GOTO(s)   do { menu_publicar_estado(MENU_EST_##s); fsmgoto(s); } while (0)

/**
 * @brief Tabla que se está mostrando.
 *
 * @return Tabla del nivel actual
 */
static const Menu_Tabla_t *menu_tabla_actual(void)
{
    return pila[prof];
}

/**
 * @brief Deja el cursor en la primera fila.
 */
static void menu_reiniciar_cursor(void)
{
    idx = 0U;
    top = 0U;
}

/**
 * @brief Mueve el cursor dentro de la tabla actual.
 *
 * La lista da la vuelta por los extremos y la ventana visible sigue al cursor, para que nunca se
 * salga de la pantalla.
 *
 * @param paso Filas a mover, positivo hacia abajo
 */
static void menu_mover_cursor(int8_t paso)
{
    const Menu_Tabla_t *tabla = menu_tabla_actual();
    int16_t             destino;

    if ((tabla == NULL) || (tabla->n == 0U)) {
        return;
    }

    destino = (int16_t)idx + (int16_t)paso;

    while (destino < 0) {
        destino = (int16_t)(destino + (int16_t)tabla->n);
    }
    while (destino >= (int16_t)tabla->n) {
        destino = (int16_t)(destino - (int16_t)tabla->n);
    }

    idx = (uint8_t)destino;

    if (idx < top) {
        top = idx;
    } else if (idx >= (uint8_t)(top + MENU_FILAS_VISIBLES)) {
        top = (uint8_t)(idx - MENU_FILAS_VISIBLES + 1U);
    }
}

/**
 * @brief Entra en un submenú, si el índice es válido y queda sitio en la pila.
 *
 * @param indice Posición del submenú dentro de la lista que se pasó a MENU_Init()
 */
static void menu_entrar_submenu(uint8_t indice)
{
    if ((indice >= n_submenus) || ((uint8_t)(prof + 1U) >= MENU_MAX_PROFUNDIDAD)) {
        return;
    }

    prof++;
    pila[prof] = tablas_submenu[indice];
    menu_reiniciar_cursor();
}

/**
 * @brief Sube un nivel: vuelve al menú padre.
 */
static void menu_volver_padre(void)
{
    if (prof > 0U) {
        prof--;
        menu_reiniciar_cursor();
    }
}

/**
 * @brief Un paso de la máquina del menú: mira el botón y navega.
 *
 * @note El despacho de `START` permite reanudar la máquina donde se quedó. Los estados que cambian
 *       algo son los que repintan: un `OLED_Update()` son ~50 ms de bus I2C, así que el dibujo nunca
 *       se hace por tiempo (hacerlo cada 40 ms dejaba el bucle principal a ~20 Hz y las pulsaciones
 *       cortas no se registraban).
 */
static void menu_fsm_paso(void)
{
    fsm {
        fsmstate(START) {
            /* Despacho al estado en que se quedó la máquina */
            switch (menu_estado) {
            case MENU_EST_MOVIENDO:  fsmgoto(MOVIENDO);
            case MENU_EST_ENTRANDO:  fsmgoto(ENTRANDO);
            case MENU_EST_VOLVIENDO: fsmgoto(VOLVIENDO);
            case MENU_EST_LISTA:
            default:                 fsmgoto(LISTA);
            }
        }

        fsmstate(LISTA) {
            /* El botón se lee y se CONSUME aquí: es el único estado que espera uno. Al volver a
               LISTA (desde MOVIENDO/ENTRANDO/VOLVIENDO) la lectura ya devuelve 0, así que no se
               repite la transición en bucle. */
            menu_boton = BUTTONS_GetEvent();

            if (menu_boton == BTN_NUM_UP)   MENU_GOTO(MOVIENDO);
            if (menu_boton == BTN_NUM_DOWN) MENU_GOTO(MOVIENDO);
            if (menu_boton == BTN_NUM_MENU) MENU_GOTO(VOLVIENDO);
            if (menu_boton == BTN_NUM_OK)   MENU_GOTO(ENTRANDO);

            fsmexit;   /* sin botón no se hace nada: ni se publica ni se repinta */
        }

        fsmstate(MOVIENDO) {
            menu_mover_cursor((menu_boton == BTN_NUM_UP) ? -1 : +1);
            MENU_Dibujar();
            MENU_GOTO(LISTA);
        }

        fsmstate(ENTRANDO) {
            const Menu_Tabla_t   *tabla = menu_tabla_actual();
            const Menu_Entrada_t *entrada;

            if ((tabla == NULL) || (tabla->n == 0U)) {
                MENU_GOTO(LISTA);
            }

            entrada = &tabla->entradas[idx];

            if (entrada->tipo == MENU_TIPO_SUBMENU) {
                menu_entrar_submenu(entrada->submenu);
                MENU_Dibujar();
                MENU_GOTO(LISTA);
            }

            if ((entrada->tipo == MENU_TIPO_LANZAR) || (entrada->tipo == MENU_TIPO_PARAR)) {
                menu_elegida = entrada;   /* lo devuelve MENU_Update() a quien llama */
                MENU_GOTO(LISTA);
            }

            MENU_GOTO(LISTA);   /* fila informativa: OK no hace nada */
        }

        fsmstate(VOLVIENDO) {
            uint8_t prof_antes = prof;

            menu_volver_padre();

            if (prof != prof_antes) {
                MENU_Dibujar();
            }

            MENU_GOTO(LISTA);
        }
    }
}

/**
 * @brief Publica por telemetría en qué punto del menú se está.
 *
 * Se hace al dibujar, que es exactamente cuando algo ha cambiado (nivel o fila), así que no hace
 * falta un anuncio aparte ni enviar nada por tiempo.
 */
static void menu_anunciar_posicion(void)
{
    TELEM_SendInt("MENU_PROF", (int32_t)prof);
    TELEM_SendInt("MENU_IDX", (int32_t)idx);
}

/**
 * @brief Pinta la cabecera de la tabla, con la posición del cursor si la lista no cabe entera.
 *
 * La ventana enseña `MENU_FILAS_VISIBLES` filas y las listas dan la vuelta por los extremos, así que
 * sin el contador no hay forma de saber que quedan opciones por debajo: el motor no dibuja ningún
 * indicador de scroll y las pruebas nuevas, que se añaden al final, parecen no existir.
 *
 * @param tabla Tabla a pintar
 */
/* El voltaje va pegado al borde derecho de la cabecera: 128 px de ancho menos tres caracteres de
   seis píxeles. "8.4" es lo que ocupa una LiPo de dos celdas con un decimal. */
#define MENU_VOLT_X     110U
#define MENU_VOLT_CHARS 3U

/**
 * @brief Pinta el voltaje de batería en la esquina superior derecha.
 *
 * Va **sin unidades ni etiqueta**, solo el número, para que no compita con el título. Se alinea a la
 * derecha, así que un valor de cuatro cifras ("10.2") se sale un carácter hacia la izquierda en vez
 * de descuadrar el margen.
 *
 * @param borrar 1 para limpiar el hueco antes de escribir (hace falta al repintar sin limpiar el
 *               framebuffer entero), 0 para escribir encima
 */
static void menu_dibujar_voltaje(uint8_t borrar)
{
    char    texto[10];
    uint8_t ancho;
    uint8_t x;

    if (menu_voltaje_on == 0U) {
        return;
    }

    ancho = FORMAT_Float(texto, sizeof(texto), menu_voltaje, 1U);

    if ((borrar != 0U) || (ancho == 0U)) {
        /* Cuatro espacios: tapan los tres caracteres del hueco y el que puede ocupar de más un
           valor de cuatro cifras, que es lo único que crece hacia la izquierda. */
        OLED_Print((uint8_t)(MENU_VOLT_X - 6U), 0U, 1U, "    ");
    }

    if (ancho == 0U) {
        return;
    }

    x = (ancho < MENU_VOLT_CHARS)
            ? (uint8_t)(MENU_VOLT_X + ((uint8_t)(MENU_VOLT_CHARS - ancho) * 6U))
            : (uint8_t)(MENU_VOLT_X - ((uint8_t)(ancho - MENU_VOLT_CHARS) * 6U));

    OLED_Print(x, 0U, 1U, texto);
}

/**
 * @brief Pinta la cabecera de la tabla, con la posición del cursor si la lista no cabe entera.
 *
 * La ventana enseña `MENU_FILAS_VISIBLES` filas y las listas dan la vuelta por los extremos, así que
 * sin el contador no hay forma de saber que quedan opciones por debajo: el motor no dibuja ningún
 * indicador de scroll y las pruebas nuevas, que se añaden al final, parecen no existir.
 *
 * En la **raíz** la cabecera lleva además el voltaje de batería a la derecha; en los submenús no,
 * porque ahí ya manda el contador de filas.
 *
 * @param tabla Tabla a pintar
 */
static void menu_dibujar_cabecera(const Menu_Tabla_t *tabla)
{
    char linea[24];

    if (tabla->titulo == NULL) {
        return;
    }

    if (tabla->n > MENU_FILAS_VISIBLES) {
        snprintf(linea, sizeof(linea), "%s %u/%u",
                 tabla->titulo, (unsigned)(idx + 1U), (unsigned)tabla->n);
        OLED_Print(0U, 0U, 1U, linea);
    } else {
        OLED_Print(0U, 0U, 1U, tabla->titulo);
    }

    if (prof == 0U) {
        menu_dibujar_voltaje(0U);
    }
}

/**
 * @brief Pinta las filas visibles, marcando con el cursor la seleccionada.
 *
 * @param tabla Tabla a pintar
 */
static void menu_dibujar_opciones(const Menu_Tabla_t *tabla)
{
    char    linea[24];
    uint8_t fila;

    for (fila = 0U; fila < MENU_FILAS_VISIBLES; fila++) {
        uint8_t indice = (uint8_t)(top + fila);

        if (indice >= tabla->n) {
            break;
        }

        snprintf(linea, sizeof(linea), "%c%s",
                 (indice == idx) ? '>' : ' ',
                 tabla->entradas[indice].etiqueta);

        OLED_Print(0U, (uint8_t)(8U + (fila * 8U)), 1U, linea);
    }
}

/**
 * @brief Inicializa el menú en la tabla raíz.
 *
 * @param raiz Tabla de primer nivel
 * @param submenus Lista de tablas de submenú, indexadas por el campo `submenu` de cada entrada
 * @param n_submenus Tamaño de esa lista
 */
void MENU_Init(const Menu_Tabla_t *raiz,
               const Menu_Tabla_t *const *submenus,
               uint8_t n_submenus_arg)
{
    tablas_submenu = submenus;
    n_submenus     = n_submenus_arg;

    pila[0] = raiz;
    prof    = 0U;
    menu_reiniciar_cursor();

    menu_estado  = MENU_EST_LISTA;
    menu_boton   = BTN_NUM_NINGUNO;
    menu_elegida = NULL;

    MENU_Dibujar();
}

/**
 * @brief Un paso de la máquina del menú.
 *
 * @return La entrada elegida si se ha pulsado OK sobre una fila accionable, o NULL si solo se ha
 *         navegado (la decisión de qué hacer con ella es de la máquina de estados de la APP)
 */
const Menu_Entrada_t *MENU_Update(void)
{
    menu_elegida = NULL;

    menu_fsm_paso();

    return menu_elegida;
}

/**
 * @brief Redibuja la tabla activa desde cero y publica la posición.
 */
void MENU_Dibujar(void)
{
    const Menu_Tabla_t *tabla = menu_tabla_actual();

    if (tabla == NULL) {
        return;
    }

    OLED_Clear();
    menu_dibujar_cabecera(tabla);
    menu_dibujar_opciones(tabla);
    OLED_Update();

    menu_anunciar_posicion();
}

/**
 * @brief Fila del menú donde está el cursor.
 *
 * @return Índice de la entrada seleccionada en la tabla actual
 */
uint8_t MENU_IndiceActual(void)
{
    return idx;
}

/**
 * @brief Título de la tabla activa, para telemetría.
 *
 * @return Título, o "?" si no hay
 */
const char *MENU_TituloActual(void)
{
    const Menu_Tabla_t *tabla = menu_tabla_actual();

    return ((tabla != NULL) && (tabla->titulo != NULL)) ? tabla->titulo : "?";
}

/**
 * @brief Profundidad actual del menú.
 *
 * @return 0 en la raíz, 1 en un submenú…
 */
uint8_t MENU_Profundidad(void)
{
    return prof;
}

/**
 * @brief Fija el voltaje de batería que se enseña en la cabecera de la raíz.
 *
 * El menú **no lee la batería**: se lo empuja quien lo llama (la APP), y así este módulo sigue sin
 * conocer `sensor/battery.h`. Tampoco se publica por telemetría; de eso ya se encarga la APP.
 *
 * @param voltios Tensión del pack en voltios
 */
void MENU_SetVoltaje(float voltios)
{
    menu_voltaje    = voltios;
    menu_voltaje_on = 1U;
}

/**
 * @brief Repinta solo el voltaje, sin tocar el resto de la pantalla.
 *
 * Existe porque `MENU_Dibujar()` limpia el framebuffer entero y eso **se ve como un parpadeo**. Esta
 * versión pisa únicamente los caracteres del voltaje, así que se puede llamar cada pocos segundos
 * para tener la lectura fresca. El `OLED_Update()` manda igualmente los 512 bytes del framebuffer
 * (unos 50 ms por I2C), pero el contenido no se apaga por el camino.
 *
 * @note No hace nada si todavía no hay voltaje fijado, o si no se está en la raíz.
 */
void MENU_RefrescarVoltaje(void)
{
    if ((menu_voltaje_on == 0U) || (prof != 0U)) {
        return;
    }

    menu_dibujar_voltaje(1U);
    OLED_Update();
}
