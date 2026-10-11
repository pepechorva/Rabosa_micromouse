#ifndef APP_MENU_H
#define APP_MENU_H

#include <stdint.h>

/**
 * @file menu.h
 * @brief Menú en la pantalla OLED gobernado por los 4 botones (capa APP).
 *
 * Es un módulo de datos: el árbol se describe con tablas `const` y el motor solo navega, dibuja y
 * devuelve lo que se ha elegido, de modo que añadir una opción es añadir una fila, no tocar código.
 *
 * Reparto de responsabilidades con quien lo usa: el menú resuelve la navegación (moverse, entrar en
 * un submenú y volver al padre); cuando se pulsa OK sobre una fila accionable, la devuelve y es la
 * máquina de estados la que decide qué hacer.
 *
 * @note Las etiquetas se dibujan con la fuente de `oled.c`: solo admite sus glifos y van sin acentos.
 * @note Estados, árbol y fases: `Documentacion/maquina-estados-menu.md`.
 */

#define MENU_MAX_PROFUNDIDAD   4U    /* niveles de anidamiento que se recuerdan */
#define MENU_FILAS_VISIBLES    3U    /* filas de opciones en 128×32 (4×8 px)    */

/** @brief Qué hace una entrada del menú al pulsar OK. */
typedef enum {
    MENU_TIPO_NINGUNA = 0,   /* fila informativa: OK no hace nada             */
    MENU_TIPO_LANZAR,        /* devuelve la entrada para ejecutar `ejecutar()` */
    MENU_TIPO_SUBMENU,       /* entra en el submenú `submenu`                 */
    MENU_TIPO_PARAR          /* devuelve la entrada: quien llama debe parar   */
} Menu_Tipo_t;

/** @brief Una fila del menú. */
typedef struct {
    const char *etiqueta;
    Menu_Tipo_t tipo;
    void      (*ejecutar)(void);
    uint8_t     submenu;
} Menu_Entrada_t;

/** @brief Una pantalla del menú. */
typedef struct {
    const char           *titulo;
    const Menu_Entrada_t *entradas;
    uint8_t               n;
} Menu_Tabla_t;

/**
 * @brief Arranca el menú en la tabla raíz.
 * @param raiz       Tabla de primer nivel
 * @param submenus   Array de punteros a las tablas de los submenús
 * @param n_submenus Cuántos submenús hay (los índices de `submenu` los usan)
 */
void MENU_Init(const Menu_Tabla_t *raiz,
               const Menu_Tabla_t *const *submenus,
               uint8_t n_submenus);

/**
 * @brief Navega y dibuja. NO bloquea.
 * @return NULL mientras se navega (o si la tabla está vacía); la entrada
 *         elegida cuando el usuario pulsa OK sobre una fila de tipo LANZAR o
 *         PARAR. El menú ya la ha consumido: solo se devuelve una vez.
 *
 * @note Solo redibuja cuando el usuario navega (moverse, entrar o salir de un submenú): un
 *       `OLED_Update()` son ~50 ms de bus I2C y hacerlo por tiempo dejaba el bucle principal a ~20 Hz.
 */
const Menu_Entrada_t *MENU_Update(void);

/** @brief Redibuja la tabla activa (lo usa la APP al volver de una prueba). */
void MENU_Dibujar(void);

/**
 * @brief Fila de la tabla actual donde está el cursor.
 *
 * La APP lo publica como `>MENU_IDX` al lanzar una prueba, para saber cuál se ha elegido sin mirar
 * la pantalla.
 */
uint8_t MENU_IndiceActual(void);

/** @brief Título de la tabla activa. */
const char *MENU_TituloActual(void);

/** @brief Profundidad actual: 0 = raíz, 1 = submenú… */
uint8_t MENU_Profundidad(void);

/**
 * @brief Fija el voltaje de batería que se enseña en la cabecera de la raíz.
 *
 * El menú **no lee la batería**: se lo empuja la APP, y así este módulo no depende de
 * `sensor/battery.h`. Se dibuja solo en la raíz y **sin unidades ni etiqueta**, solo el número
 * ("7.7"), pegado al borde derecho.
 *
 * @param voltios Tensión del pack en voltios
 */
void MENU_SetVoltaje(float voltios);

/**
 * @brief Repinta solo el voltaje de la cabecera, sin limpiar la pantalla.
 *
 * Para tener la lectura fresca sin el parpadeo de `MENU_Dibujar()`, que hace `OLED_Clear()`. Cuesta
 * igualmente los ~50 ms del `OLED_Update()`, así que se llama cada pocos segundos, no cada vuelta.
 *
 * @note No hace nada si no hay voltaje fijado o si no se está en la raíz.
 */
void MENU_RefrescarVoltaje(void);

#endif /* APP_MENU_H */
