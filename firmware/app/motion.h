#ifndef APP_MOTION_H
#define APP_MOTION_H

#include <stdint.h>

#include "control/controller.h"
#include "filter/odometry.h"
#include "hal/drv8833.h"

/**
 * @file motion.h
 * @brief Primitivas de movimiento (capa APP): **avanzar una distancia** y **girar un ángulo**.
 *
 * Junta las tres piezas que ya estaban sueltas: la **velocidad** de cada rueda la cierra
 * `control/controller.c` (CONTROL), la **pose** la lleva `filter/odometry.c` (FILTER) y aquí se ata
 * todo —leer los encoders, mover la pose, pedir el duty al driver—, que es justo lo que solo la APP
 * puede hacer: es la única capa autorizada a saltar hacia abajo durante el bring-up.
 *
 * Las dos primitivas son la base de todo lo que viene: la celda del laberinto (`avanzar_celda()`), los
 * giros de 90°, el *wall follower* y el *flood fill*. Por eso lo que se pide de ellas no es velocidad,
 * sino **repetibilidad**: que 180 mm sean 180 mm y 90° sean 90°, hoy y con la batería a la mitad.
 *
 * @note Las dos son **bloqueantes** (no vuelven hasta terminar o agotar el tiempo), como el resto del
 *       banco de pruebas, y salen de inmediato si se pulsa MENU.
 * @note La distancia se mide **con la odometría**, no con el tiempo: por eso se apoya en los µm por
 *       cuenta y en la trocha ya calibrados (pruebas 7 y 19).
 * @see Procedimiento y criterios en `Documentacion/pruebas-bringup.md`.
 */

/** @brief Velocidad de crucero de las primitivas [mm/s]. */
#define MOTION_VEL_MM_S          250.0f

/** @brief Pendiente de la rampa de consigna [mm/s²], para no arrancar de golpe. */
#define MOTION_RAMPA_MM_S2       800.0f

/** @brief Tope de duty que puede pedir el PID durante los movimientos [%]. */
#define MOTION_DUTY_MAX          70.0f

/** @brief Tolerancia de la distancia recorrida [mm]. */
#define MOTION_TOL_MM            3.0f

/**
 * @brief Distancia que el robot recorre **después** de darse por llegado, y que hay que descontar [mm].
 *
 * Estuvo en **19,6 mm** mientras el asentamiento dejaba **rodar** al robot: el integrador del PID
 * seguía empujando mientras se descargaba y la odometría lo contaba. Con el asentamiento frenando de
 * verdad (`motion_asentar` resetea el PID y hace freno dinamico) el recorrido sobrante se queda en unos
 * pocos milímetros y **el descuento ya no hace falta**.
 *
 * @note Se deja el parámetro a **0** en vez de borrarlo: es donde poner la corrección si alguna vez
 *       vuelve a hacer falta, y deja constancia de que un día hizo falta.
 * @warning Descontar una frenada fija **no funciona**: medida en cuatro celdas seguidas salió de 2, 20
 *          y 44 mm, porque depende de cuánto le quedara al integrador. Si vuelve a aparecer el
 *          problema, la solución es quitar el rodaje, no ajustar este número.
 */
#define MOTION_FRENADA_MM        0.0f

/**
 * @brief Tolerancia del ángulo girado [°].
 *
 * ⚠️ **Estaba en 1,5 y era el error dominante del giro**, no el rozamiento ni el sensor. Medido el
 * 2026-10-11 con `MOV UP 4` (360° pedidos) y la escala del giroscopio ya corregida: el lazo suelta
 * **siempre** al rozar el borde de la banda, o sea a **88,55°** ⇒ **1,45° corto por esquina**, que son
 * los **−5,8° de `>MOV_E_ANG`** al cerrar la vuelta entera. El usuario lo confirmó a ojo contra la
 * marca de la mesa («unos 5°»).
 *
 * Un lazo que suelta «cuando le queda poco» tiene un sesgo **sistemático** del tamaño de esa banda, y
 * en un laberinto se acumula esquina a esquina. Bajarla a 0,5° deja el presupuesto de un giro en
 * ~1,1° (0,5 de banda + ~1,0 de retardo), frente a los ~2,5° que daba 1,5.
 *
 * @warning El lazo resuelve 0,5° de sobra a la velocidad mínima de giro (35 mm/s son ~52 °/s ⇒ 0,05°
 *          por vuelta de 1 ms), pero **bajar la tolerancia alarga el tramo final**: si algún día el
 *          giro no llega a converger o empieza a rebotar en el borde de la banda, este es el número
 *          que hay que devolver hacia arriba.
 */
#define MOTION_TOL_GRADOS        0.5f

/** @brief Velocidad mínima de giro [mm/s]: por debajo de esto el motor no vence la fricción estática. */
#define MOTION_GIRO_VEL_MIN_MM_S 40.0f

/**
 * @brief Velocidad mínima de giro **una vez el robot ya está girando** [mm/s].
 *
 * ⚠️ **Medido el 2026-10-02: 35, y constante.** Los dos extremos están medidos y los dos fallan:
 *
 * | Valor | Qué hacía |
 * |:-----:|:----------|
 * | **0** | Con el robot frenando despacio, el mínimo de arranque (`MOTION_GIRO_VEL_MIN_MM_S`) entraba y salía según si el robot rodaba o no, y eso es un **relay**: llegaba despacio, saltaba a 40 mm/s, se pasaba, volvía… El usuario lo describió como «rebota» y en una tanda dejó **el 4º giro 45° corto**. |
 * | **40** | Nunca baja de 59 °/s —$2 \times 40/L$— así que cruzaba el objetivo de largo y **arrastraba el morro 5 cm**. |
 * | **10** | No vence la fricción estática si la rueda va dura: **se queda pegada y el robot pivota sobre ella**, con el morro acabando 4 cm por delante. |
 *
 * ✅ **Con el umbral a 0 el mínimo es constante**, así que no hay relay. El valor es **35**: con **25**
 * los **dos primeros giros de cada tanda salían bien y los dos últimos no** —el robot volvía a pivotar
 * sobre la rueda clavada—, porque el rozamiento sube con el uso y 25 no siempre vencía la fricción.
 * Con 35 los cuatro salen y el camino parásito se queda en **3-7 mm**.
 *
 * @warning **Esto sigue siendo un parche.** El mínimo es una velocidad impuesta, y que haga falta
 *          subirlo según se calienta el mecanismo es la señal de que no es el sitio donde se resuelve.
 *          Lo pendiente es un mínimo **por rueda** (el que necesite cada encoder en ese instante), que
 *          ya no dependería del rozamiento del conjunto.
 *
 * @note **La lección de los tres valores fallidos**: el mínimo intentaba resolver dos problemas a la
 *       vez —vencer la fricción al arrancar y dejar frenar al llegar— y **la histéresis entre los dos
 *       regímenes es lo que oscilaba**. Un mínimo único y moderado no hace ninguna de las dos cosas
 *       perfectamente, pero **no oscila**, y oscilar es lo que hacía impredecible el ángulo final.
 */
#define MOTION_GIRO_VEL_MIN_RODANDO_MM_S 35.0f

/**
 * @brief Velocidad de giro a partir de la cual se considera que el robot **ya rueda** [°/s].
 *
 * ⚠️ **Medido el 2026-10-02: 0**, o sea que el mínimo de arriba se aplica **siempre**.
 *
 * La idea de separar dos regímenes era razonable y resultó ser el fallo: el umbral **separa el mínimo
 * en dos escalones y el salto entre ellos es una histéresis**, que es un relay y por tanto oscila.
 *
 * ⚠️ Y el umbral tiene además un fallo de concepto: **mira la velocidad del chasis**, que no dice nada
 * de cada rueda. Con el robot girando **sobre una rueda clavada**, el chasis gira igual, así que el
 * lazo creía que todo rodaba y **le quitaba el mínimo a la rueda que estaba parada**. El arreglo de
 * verdad es un mínimo **por rueda**, leyendo la velocidad de cada encoder; queda pendiente.
 */
#define MOTION_GIRO_RODANDO_DPS  0.0f

/**
 * @brief Amortiguación del lazo de giro [mm/s por °/s].
 *
 * Es el término **D**, y aquí sale casi gratis: el giroscopio mide la **velocidad angular**
 * directamente, así que no hay que derivar ni filtrar nada (que es lo que suele hacer inútil un D con
 * una señal ruidosa).
 *
 * ⚠️ **Hay que elegirlo junto al P, y con `MOTION_GIRO_KP` = 4 el valor es 0,3.** No es un número
 * suelto: el cociente entre los dos decide quién manda. Medido el 2026-10-02, con **1,0** el D pedía
 * 90 mm/s a 90 °/s mientras el P, con 22° de error, solo pedía 88 — **el D mandaba durante casi todo
 * el giro**, el mando se quedaba en negativo y el robot **frenaba en seco en vez de completarlo**.
 *
 * @note **Cómo se detecta que el D se ha pasado**: el robot llega al objetivo y **vuelve hacia atrás**
 *       —o deja el giro incompleto— porque el mando se le queda en negativo. El clamp de abajo
 *       conserva el signo, así que no se queda parado: retrocede a `-vel_min`.
 */
#define MOTION_GIRO_KD           0.3f

/**
 * @brief Tope de la contribución del término **I** del lazo de giro [mm/s].
 *
 * Es el **anti-windup**: el integrador acumula el error mientras el mando está saturado, y sin tope
 * llegaría al objetivo con una carga enorme que habría que descargar **girando de más**.
 *
 * ⚠️ **Por defecto la ganancia I es 0**, y no es un olvido: el lazo corta al entrar en
 * `MOTION_TOL_GRADOS` (±1,5°), así que el error que el I podría quitar **ya está dentro de la
 * tolerancia**. Con el giroscopio de referencia el error final es de décimas de grado, no un sesgo.
 * Se deja implementado y ajustable porque es la herramienta para el día que aparezca un sesgo de
 * verdad (por ejemplo, si el robot empezara a girar siempre 1° corto por rozamiento).
 */
#define MOTION_GIRO_I_MAX_MM_S   30.0f

/**
 * @brief Ganancia del lazo de ángulo [mm/s por grado de error].
 *
 * El error es el del **giroscopio**, no el de la odometría, y eso cambia lo que se le puede pedir:
 * medido el 2026-10-02, la odometría **sobreestima el pivote un 44 %** (dice 125° donde el robot gira
 * 87°) porque al girar sobre el centro las ruedas deslizan lateralmente. Con la odometría como
 * referencia, `MOTION_Girar(90)` corta a los **62°** reales.
 */
#define MOTION_GIRO_KP           4.0f

/**
 * @brief Ganancia del corrector de **traslación** durante el giro [mm/s por mm desplazado].
 *
 * Un giro sobre el centro pide que los dos arcos sean **iguales y opuestos**, y en la práctica no lo
 * son: un motor vence el rozamiento antes que el otro. Medido el 2026-10-01 (prueba 20), cada giro de
 * 90° desplazaba el robot **~18 mm** — 72 mm en un cuadrado, sobre celdas de 180 mm.
 *
 * ⚠️ **Y está DESACTIVADO (0), porque se realimenta con una medida que miente.** El corrector es un
 * término **común** a las dos ruedas, así que no cambia la diferencia —lo que gira— pero **mueve el
 * robot**. Y su entrada es el desplazamiento medido por la **odometría**, que al pivotar cuenta un
 * movimiento que no existe: medido el 2026-10-02, con la ganancia a 8 el robot acumuló **143 mm de
 * camino parásito en un solo giro** (y 216 mm girando con el giroscopio, que tarda más). En una celda
 * de 180 mm eso es **chocar contra la pared**, que es justo lo que hacía el *wall follower*.
 *
 * El corrector tenía sentido cuando se creía que la odometría medía bien el pivote. Ahora se sabe que
 * **no mide ni la magnitud ni el signo** (en los cuatro giros de la prueba dijo +344° donde el robot
 * giró +45°): compensar con eso es empeorar. Se deja la constante y el código, a 0, porque el día que
 * haya una medida buena de la traslación del pivote **este es el sitio** donde ponerla.
 *
 * @note El residuo que deja sin corrector son los **15-19 mm** que ya se midieron el 2026-10-01, y esos
 *       no estreñan al robot en una celda de 180 mm.
 */
#define MOTION_GIRO_CENTRO_KP    0.0f

/**
 * @brief Tope del corrector de **traslación** del giro [mm/s].
 *
 * ⚠️ **Sin este tope el giro se pierde**, y no es una precaución teórica: medido el 2026-10-02 en el
 * laberinto, el robot se atascaba al girar en una celda de 180 mm.
 *
 * La corrección es un término **común** a las dos ruedas (`-K_p · desplazamiento`), así que su efecto
 * sobre la **diferencia** —que es lo que gira— es nulo: *no frena el giro, mueve el robot*. Y cuando
 * el robot gira **rozando las paredes**, la odometría cuenta un desplazamiento que no es real: con
 * 100 mm de cuenta falsa y $K_p = 8$ el corrector pide **800 mm/s a las dos ruedas**, domina sobre la
 * velocidad del giro y el robot sale disparado hacia un lado en vez de pivotar.
 *
 * Con el tope al **24 %** de la velocidad de crucero, el corrector sigue quitando el desplazamiento
 * de verdad (que son milímetros) pero ya no puede secuestrar el giro.
 */
#define MOTION_GIRO_CENTRO_MAX_MM_S 60.0f

/** @brief Ganancia del corrector de rumbo al avanzar [mm/s por grado de desvío].
 *
 * @note Esta es la del **respaldo por odometría**, que solo se usa si la IMU no responde. No se puede
 *       subir: la odometría deduce el giro de la diferencia de arcos de las ruedas, así que un error
 *       de un tramo se arrastra al siguiente y el lazo se queda corrigiendo un desvío que ya no es el
 *       real. Con $L$ = 77,6 mm la constante de tiempo vale $\tau = L/(2K_p)$ = **12,9 s**, y una celda
 *       de 180 mm a 250 mm/s dura **1 s**: dentro de una celda solo se corrige el **7 %** del desvío.
 */
#define MOTION_RUMBO_KP          3.0f

/**
 * @brief Ganancia del corrector de rumbo cuando lo que se mide es el **giroscopio** [mm/s por grado].
 *
 * La corrección es la misma —una diferencia de velocidad entre las dos ruedas— pero la referencia es
 * **absoluta** y se pone a cero al empezar cada avance, así que el error no se hereda de celda en
 * celda.
 *
 * ⚠️ **El valor está elegido para que el lazo NO sature, y no por rapidez.** Con el tope de
 * `MOTION_GYRO_CORR_MAX_MM_S`, por encima de $e_{sat} = 60/K_p$ grados la corrección se recorta y el
 * lazo deja de ser proporcional: se convierte en un **lazo de relé**, y un relé con retardo **oscila
 * siempre**. Con $K_p = 40$ la saturación empezaba en **1,5°**, así que el lazo estaba en modo relé
 * prácticamente todo el rato y el robot **hacía zigzag** (medido el 2026-10-02 en el laberinto: el
 * desvío crecía 1,5 → 3,4 → 11,4° en tres celdas, que es divergencia).
 *
 * Con **12** la saturación empieza en **5°**, muy por encima del desvío que se ve dentro de una celda,
 * así que el lazo trabaja en su tramo lineal ($\tau = L/(2K_p)$ = 3,2 s) y amortigua en vez de oscilar.
 */
#define MOTION_GYRO_KP           12.0f

/**
 * @brief Tope de la corrección de rumbo [mm/s].
 *
 * Define, junto con `MOTION_GYRO_KP`, dónde empieza a saturar el lazo: $e_{sat} = 60/K_p$ grados. Con
 * el tope al **24 %** de la velocidad de crucero, la velocidad de corrección máxima es de **88 °/s**,
 * que sobra para enderezar el robot sin cruzarlo de lado.
 *
 * ⚠️ Subir este tope **empeora** el zigzag: agranda el tramo saturado. Si hay que corregir desvíos
 * mayores, se baja `MOTION_GYRO_KP`, no se sube esto.
 */
#define MOTION_GYRO_CORR_MAX_MM_S 60.0f

/**
 * @brief Signo del eje Z del giroscopio respecto al convenio de $\theta$ (positivo = a la izquierda).
 *
 * ⚠️ No se puede deducir del código: depende de **cómo esté montado el módulo**. Se mide en un minuto
 * desde la **prueba 20**, que no el giro no depende del giroscopio: girar 90° a la izquierda con **UP**
 * y comparar `>MOV_GYR` con `>MOV_ANG`. Si los dos salen ≈ +90, vale `1.0f` (el valor de aquí); si el
 * del giroscopio sale ≈ −90, hay que poner `-1.0f`.
 */
#define MOTION_GYRO_SIGNO_Z      1.0f

/**
 * @brief Número de muestras que se promedian para medir el sesgo del giroscopio.
 *
 * El sesgo del MPU-9250 puede llegar a varios °/s sin calibrar, y al integrarlo se convierte en un
 * desvío de rumbo que crece con el tiempo. Promediando 50 muestras el ruido baja a la centésima y el
 * error de la calibración se queda en décimas de grado por celda. Se mide **una vez**, desde
 * `MOTION_Init()`, con el robot parado.
 */
#define MOTION_GYRO_SESGO_N      50U

/**
 * @brief Dispersión máxima que se admite en una tanda de calibración del sesgo [°/s].
 *
 * Con el robot quieto sobre el suelo el giroscopio da décimas de °/s de ruido, así que una tanda con
 * dispersión de varios °/s solo puede significar una cosa: **lo están moviendo**. Esa tanda se tira.
 */
#define MOTION_GYRO_SESGO_DISP_MAX 2.0f

/**
 * @brief Cuántas tandas de calibración se intentan antes de rendirse [veces].
 *
 * Tres tandas son 1,5 s esperando a que suelten el robot; si ninguna sale limpia se usa la última
 * media y el refinamiento del asentamiento acaba de corregirlo.
 */
#define MOTION_GYRO_SESGO_INTENTOS 3U

/**
 * @brief Peso de cada refinamiento del sesgo medido con el robot frenado [0..1].
 *
 * En `motion_asentar()` el robot está **frenado**: es el único momento de un movimiento en el que se
 * sabe que no gira, así que el sesgo se puede medir ahí sin que el propio movimiento lo contamine. Se
 * aprovecha el bucle que ya existe (no cuesta tiempo).
 *
 * ⚠️ **Vale 1 (sin filtro) a propósito, y es lo contrario de lo que pide la intuición**: el sensor tiene
 * **deriva térmica**. Medido el 2026-10-02, con el chip recién encendido leía −6,5 °/s y en dos
 * movimientos bajó a ≈−1 °/s ⇒ un filtro lento iba **persiguiendo** un blanco que se mueve. La muestra
 * del asentamiento es buena (robot frenado y solo la segunda mitad), así que se toma tal cual: así el
 * sesgo **sigue** la deriva en vez de quedarse atrás.
 */
#define MOTION_GYRO_REFINAR_ALFA 1.0f

/** @brief Muestras mínimas del asentamiento para que el refinamiento del sesgo valga [muestras]. */
#define MOTION_GYRO_REFINAR_MIN 8U

/**
 * @brief Cambio máximo que un solo refinamiento puede darle al sesgo [°/s].
 *
 * Es la red que protege: aunque una medida se cuele con el robot aún moviéndose, el sesgo no puede dar
 * la vuelta de un golpe. Medido el 2026-10-02: sin este
 * tope, muestrear al principio del asentamiento (con el robot girando por inercia tras el frenado)
 * dejó el sesgo falseado y **el giro de media vuelta dejó de verse**.
 */
#define MOTION_GYRO_REFINAR_MAX_PASO 20.0f

/**
 * @brief Factor de escala del giroscopio: cuánto mide de menos [adimensional].
 *
 * ⚠️ **El módulo es un clon** (el IC va marcado `MP65888LB1`, que no corresponde a ningún InvenSense)
 * y su sensibilidad **no es la del datasheet**: integrando en crudo mide un **1 % de menos**, así que
 * el factor de corrección es **1,01**.
 *
 * Medido el 2026-10-11 con **cuatro giros de 90° pedidos** (`MOV UP 4` = 360°) contra **dos líneas
 * perpendiculares marcadas en la mesa**: una alineada con el morro al empezar y otra a 90°. La escala
 * se barrió **en caliente** con la orden `GYRO ESC <valor>`, sin regrabar (un flasheo reinicia el
 * robot y con él la calibración del sesgo, así que dos tandas no se podrían ni comparar):
 *
 * | `GYR_ESC` | `>MOV_ANG` por giro | Lo que hizo el robot |
 * |:---------:|:-------------------:|:--------------------:|
 * | 1,20 | 79,9° · 81,4° | **~80°** ⇒ 10° corto **en cada esquina** |
 * | 1,07 | 88,6° | ~85° |
 * | **1,01** | **91,8°** | **~5° corto tras la vuelta entera, y esos 5° son la tolerancia** |
 *
 * ⚠️ **`MOV_GYR` NO sirve para medir esta escala, y por ahí se perdió una noche entera.** El lazo
 * cierra sobre `gyro_total`, **que ya está escalado**: si se sube la escala, el robot gira menos, pero
 * el lazo sigue cortando cuando su cuenta llega a 90 y la telemetría vuelve a decir 90. **La
 * referencia se valida a sí misma** — con cualquier escala `MOV_GYR` da 88,5° por esquina, y encima
 * con ±0,03° de dispersión entre los cuatro giros. Para calibrar hace falta una referencia **externa**.
 *
 * ⚠️ **Y esa referencia externa hay que medirla BIEN.** La tabla del 2026-10-02, que daba 1,20, queda
 * **retirada**: no medía la orientación final del robot sino el **desplazamiento del morro** contra una
 * cinta, convertido a ángulo con $d = 2L\sin(\Delta/2)$ y $L$ = 62 mm. Esa conversión solo vale si el
 * pivote es exactamente sobre el centro, y esa noche el robot **pivotaba sobre una rueda dura** — el
 * propio registro lo dice. Con la escala corregida a 1,01, un `ESC` de 1,20 habría dejado al robot
 * unos **57° cortos** de la marca, y aquella medida decía «0°». Comparar la **orientación** contra dos
 * líneas perpendiculares no tiene ese punto débil.
 *
 * @note **Un error de escala no se ve en un giro suelto:** 90° se quedan en 80 y parecen «un giro que
 *       se ha quedado corto», que es justo lo que se achaca al rozamiento. Lo delata pedir **varias
 *       vueltas**: el error se multiplica y sale a la vista.
 *
 * @note Con la escala corregida, lo que queda por girar de menos es **`MOTION_TOL_GRADOS`**: el lazo
 *       suelta a 88,5° y eso son **1,45° por esquina**, que es exactamente lo que mide el −5,8° de
 *       `>MOV_E_ANG` en la vuelta entera. Esa es la palanca que queda, no este factor.
 */
#define MOTION_GYRO_ESCALA       1.01f

/**
 * @brief Sesgo por encima del cual **no se usa el giroscopio** [°/s].
 *
 * ⚠️ **Un giroscopio con el sesgo mal medido es PEOR que la odometría.** Medido el 2026-10-02 en el
 * laberinto: con 6,5 °/s de sesgo el lazo cree que el robot gira y lo gira de verdad ⇒ se desvía 13°
 * por celda y acaba **empujando contra la pared** hasta agotar el timeout.
 *
 * El sensor tiene **deriva térmica**: recién encendido (chip frío) medía −6,5 °/s y en dos movimientos
 * bajó a ≈−1 °/s. Y eso no se arregla calibrando una sola vez al entrar.
 *
 * Mientras el sesgo esté por encima de este umbral el robot **usa el respaldo por odometría**.
 *
 * ⚠️ **El umbral es 10 y no 3**: los 3 °/s salían de suponer que el giroscopio era el sospechoso, pero
 * medido el 2026-10-02 el gyro siguió aciertos **con ±7 °/s de sesgo** (reprodujo un giro a la derecha
 * que la odometría daba como izquierda), así que un umbral estrecho quitaba la referencia buena. Con
 * 10 solo se cae al respaldo cuando el sesgo es ya de otro orden.
 */
#define MOTION_GYRO_SESGO_MAX_USABLE 10.0f

/**
 * @brief Tiempo máximo de un movimiento [ms]; al agotarse se para y se avisa.
 *
 * ⚠️ **Bajado de 8000 a 3500 el 2026-10-02, medido en el laberinto.** Cuando el robot se atasca
 * **empujando contra una pared**, este tope es exactamente el tiempo que se pasa arrastrándose: medido,
 * el avance de una celda se quedó clavado contra la pared y el `>WF_ATASCO:1` no llegó hasta
 * **11 segundos** después, con 8 de ellos de rueda patinando.
 *
 * **No se baja más**, y el motivo está medido: el movimiento legítimo más largo es la **media vuelta**
 * (180°), que tarda ~2 s, así que un tope de 2,5 s cortaría giros que están bien y el *wall follower*
 * los contaría como atascos — que es peor que arrastrarse, porque **inventa un fallo donde no lo hay**.
 * Con 3500 quedan 1,5 s de margen sobre el peor caso.
 *
 * @note Es el mismo tope para avanzar y para girar, así que hay que dimensionarlo por el movimiento
 *       **más lento**, no por el más frecuente.
 */
#define MOTION_TIMEOUT_MS        3500U

/**
 * @brief Estado de las primitivas: los dos lazos de velocidad y la pose que van construyendo.
 *
 * La odometría vive **aquí dentro** y no se reinicia entre movimientos: así el rumbo y el ángulo son
 * continuos dentro de una maniobra encadenada (`avanzar`, `girar`, `avanzar`…).
 */
typedef struct {
    Controller_t     izq;           /**< Lazo de la rueda izquierda     */
    Controller_t     der;           /**< Lazo de la rueda derecha       */
    Odometry_t       odo;           /**< Pose acumulada desde MOTION_Init() */
    DRV8833_Motor_t *motor_izq;     /**< Driver de la rueda izquierda   */
    DRV8833_Motor_t *motor_der;     /**< Driver de la rueda derecha     */
    int32_t          um_izq_previo; /**< µm de la rueda izquierda en la última lectura */
    int32_t          um_der_previo; /**< µm de la rueda derecha en la última lectura    */
    float            gyro_rumbo;    /**< Ángulo girado según el giroscopio desde el último cero [°] */
    float            gyro_total;    /**< Lo mismo, acumulado desde `MOTION_Init()` (para telemetría) */
    float            gyro_velocidad;/**< Velocidad angular medida [°/s]: es la entrada del término D */
    float            gyro_sesgo;    /**< Sesgo medido del eje Z [°/s] */
    uint8_t          gyro_ok;       /**< 1 si la IMU responde y el giroscopio se puede usar */
} Motion_t;

/**
 * @brief Prepara las primitivas: arranca los dos lazos y pone la pose en el origen.
 *
 * Los lazos arrancan **con las ganancias a cero**: hay que dárselas con `MOTION_SetGanancias()` antes
 * de pedir el primer movimiento. Se separó así porque las ganancias son **por rueda** (los dos N20 no
 * son iguales) y seis floats sueltos en el constructor dan una firma que se lee mal.
 *
 * @param m Estructura a inicializar
 * @param motor_izq Driver de la rueda izquierda (los motores son de la APP: `application.c`)
 * @param motor_der Driver de la rueda derecha
 * @note Los motores se pasan por parámetro en vez de usar los globales de `application.c`: así este
 *       módulo no depende de un estado que no controla.
 */
void MOTION_Init(Motion_t *m,
                 DRV8833_Motor_t *motor_izq,
                 DRV8833_Motor_t *motor_der);

/**
 * @brief Pone las ganancias de los dos lazos de velocidad, una por rueda.
 *
 * @param m Estructura de las primitivas
 * @param kp_izq Ganancia proporcional del lazo izquierdo
 * @param ki_izq Ganancia integral del lazo izquierdo
 * @param kd_izq Ganancia derivativa del lazo izquierdo
 * @param kp_der Ganancia proporcional del lazo derecho
 * @param ki_der Ganancia integral del lazo derecho
 * @param kd_der Ganancia derivativa del lazo derecho
 * @note Conviene pasarle las ganancias que se hayan sintonizado en la prueba 18 (orden `PID`).
 * @note `PID_SetGanancias()` resetea el integrador: cambiar ganancias en caliente no puede dejar
 *       dentro el término de salida que acumuló la combinación anterior.
 */
void MOTION_SetGanancias(Motion_t *m,
                         float kp_izq, float ki_izq, float kd_izq,
                         float kp_der, float ki_der, float kd_der);

/**
 * @brief Avanza en línea recta la distancia pedida.
 *
 * Mantiene el rumbo con la odometría: si el robot se desvía, reparte la diferencia de velocidad entre
 * las dos ruedas. Es lo que evita que el robot se vaya de lado al cruzar un pasillo largo.
 *
 * @param m Estructura de las primitivas
 * @param mm Distancia a recorrer, positiva hacia delante
 * @return 1 si llegó dentro de tolerancia, 0 si agotó el tiempo o se pidió salir
 */
uint8_t MOTION_Avanzar(Motion_t *m, float mm);

/**
 * @brief Referencia que cierra el lazo de **giro**.
 *
 * Existe para poder **comparar las tres en banco con la misma maniobra**: girar 90° a un lado y 90° al
 * otro tiene que devolver el robot al sitio, y el error de cierre dice cuál acierta. Se cambia en
 * caliente con la orden `GYRO` de la consola.
 *
 * @note Si el giroscopio no es de fiar (sesgo fuera de rango) manda la odometría **siempre**, sea cual
 *       sea el modo: un gyro mal calibrado es peor que no tenerlo.
 */
typedef enum {
    MOTION_GIRO_ODOMETRIA = 0,  /**< Solo odometría: deduce el giro de los arcos de las ruedas */
    MOTION_GIRO_GIROSCOPIO,     /**< Solo giroscopio: mide el giro, pero arrastra su sesgo */
    MOTION_GIRO_FUSION,         /**< Media de los dos errores */
    MOTION_GIRO_MODO_N          /**< Número de modos: no es un modo válido */
} Motion_GiroModo_t;

/**
 * @brief Elige la referencia del lazo de giro.
 *
 * @param modo Modo pedido (fuera de rango se ignora)
 */
void MOTION_SetModoGiro(Motion_GiroModo_t modo);

/**
 * @brief Referencia que está usando el lazo de giro.
 *
 * @return Modo en uso
 */
Motion_GiroModo_t MOTION_GetModoGiro(void);

/**
 * @brief Ganancias del lazo de giro que se pueden ajustar **en caliente**.
 *
 * La orden `GYRO` de la consola las cambia sin reflashear, y por eso son variables y no `#define`. La
 * razón es de tiempo: cada sintonía cuesta un flasheo, un reinicio —que borra la calibración del
 * giroscopio— y volver a montar el puente. Son tres minutos por prueba, y el robot no está para eso.
 */
typedef enum {
    MOTION_GIRO_GANANCIA_KP = 0,   /**< Proporcional al error de ángulo [mm/s por °] */
    MOTION_GIRO_GANANCIA_KI,       /**< Integral del error [mm/s por (°·s)] */
    MOTION_GIRO_GANANCIA_KD,       /**< Amortiguación: frena segun la velocidad angular [mm/s por °/s] */
    MOTION_GIRO_GANANCIA_ESCALA,   /**< Escala del giroscopio [adimensional] */
    MOTION_GIRO_GANANCIA_VMIN,     /**< Velocidad mínima con el robot ya girando [mm/s] */
    MOTION_GIRO_GANANCIA_VARR,     /**< Velocidad mínima para arrancar del todo [mm/s] */
    MOTION_GIRO_GANANCIA_VDPS,     /**< Velocidad angular a partir de la cual se considera que rueda [°/s] */
    MOTION_GIRO_GANANCIA_N         /**< Número de ganancias: no es una ganancia válida */
} Motion_GiroGanancia_t;

/**
 * @brief Cambia una ganancia del lazo de giro.
 *
 * @param cual Ganancia a tocar
 * @param valor Valor nuevo
 * @return 1 si se aceptó, 0 si la ganancia no existe o el valor está fuera de rango
 * @note Los rangos son deliberadamente anchos: están para **detectar un dedazo** (un cero de más, un
 *       signo), no para imponer un criterio de sintonía.
 */
uint8_t MOTION_SetGiroGanancia(Motion_GiroGanancia_t cual, float valor);

/**
 * @brief Lee una ganancia del lazo de giro.
 *
 * @param cual Ganancia a leer
 * @return El valor en uso; 0 si la ganancia no existe
 */
float MOTION_GetGiroGanancia(Motion_GiroGanancia_t cual);

/**
 * @brief Gira sobre el centro el ángulo pedido.
 *
 * El ángulo lo mide el **giroscopio** (si la IMU no responde, la odometría): es lo que evita el error
 * sistemático del pivote, donde las ruedas deslizan y la odometría cuenta un 44 % más de giro del que
 * el robot hace de verdad.
 *
 * @param m Estructura de las primitivas
 * @param grados Ángulo a girar; **positivo a la izquierda** (antihorario), como θ en la odometría
 * @return 1 si llegó dentro de tolerancia, 0 si agotó el tiempo o se pidió salir
 */
uint8_t MOTION_Girar(Motion_t *m, float grados);

/**
 * @brief Frena los dos motores y deja los lazos parados.
 *
 * @param m Estructura de las primitivas
 */
void MOTION_Parar(Motion_t *m);

/**
 * @brief Integra el giroscopio con el mismo acumulador que usan las primitivas.
 *
 * Existe para las pruebas que necesitan el ángulo del giroscopio **sin** pasar por las primitivas de
 * movimiento —la de odometría, que se hace empujando el robot a mano y con las ruedas libres—. El
 * acumulador es el mismo (`motion_gyro_leer()`): **no hay dos maneras de contar el ángulo**.
 *
 * @param m Estructura de las primitivas (aquí solo se usa la parte del giroscopio)
 * @param dt Segundos desde la llamada anterior
 * @note El sesgo hay que haberlo calibrado antes (`MOTION_Init()`, con el robot parado).
 */
void MOTION_GyroActualizar(Motion_t *m, float dt);

/**
 * @brief Ángulo que llevas girado desde el último MOTION_Init() [°].
 *
 * @param m Estructura de las primitivas
 * @return Orientación estimada
 */
float MOTION_GetAngulo(const Motion_t *m);

/**
 * @brief Ángulo girado según el **giroscopio**, acumulado desde el último `MOTION_Init()` [°].
 *
 * No interviene en el control: existe para poder **comparar** lo que dice la odometría con lo que dice
 * el giroscopio. Si los dos números se separan, la odometría es la que miente (la medida del
giroscopio no depende de las ruedas).
 *
 * @param m Estructura de las primitivas
 * @return Ángulo según el giroscopio, positivo a la izquierda
 * @note Si la IMU no responde el número se queda congelado: comprobarlo con `MOTION_GyroDisponible()`.
 */
float MOTION_GetGyroAngulo(const Motion_t *m);

/**
 * @brief Dice si el corrector de rumbo está usando el giroscopio o el respaldo por odometría.
 *
 * @param m Estructura de las primitivas
 * @return 1 si la IMU se detectó y responde, 0 si se está usando la odometría
 */
uint8_t MOTION_GyroDisponible(const Motion_t *m);

/**
 * @brief Sesgo del giroscopio que está usando el lazo [°/s].
 *
 * Es para poder **verlo**: un sesgo de decenas de °/s con el robot quieto es la firma del fallo del
 * 2026-10-02 (el sesgo se mide mal y el lazo integra ese error todo el rato).
 *
 * @param m Estructura de las primitivas
 * @return Sesgo en °/s
 */
float MOTION_GetGyroSesgo(const Motion_t *m);

#endif /* APP_MOTION_H */
