# Pruebas del proyecto — Rabosa-MVR

Aquí conviven dos tipos de prueba, con propósitos distintos:

| Tipo | Dónde vive | Qué valida |
|:-----|:-----------|:-----------|
| **Banco de pruebas del robot** | `app/application.c` (`App_Test_*`) | Periféricos y sensores **sobre el hardware**, con telemetría por USART3 |
| **Banco de pruebas del PC** | este directorio | Módulos de **lógica pura** (matemática), compilados y ejecutados en el ordenador |

Las pruebas del robot se catalogan y se registran en `Documentacion/pruebas-bringup.md`;
se lanzan desde el **menú** de la OLED, no hace falta recompilar.

> ⚠️ Este directorio **no entra en el build del firmware**: `extra_script.py` solo añade las capas
> `core`, `app`, `control`, `filter`, `sensor`, `hal` y `hw`. Los ficheros de aquí se compilan a mano
> con `gcc` para el PC.

---

## `profile_test.c` — perfiles trapezoidales (capa CORE)

El perfil es matemática pura —no toca el hardware ni el reloj—, así que se simula entero en el PC: se
le hacen correr nueve movimientos (celdas, medias celdas, giros de 90°, marcha atrás, distancias
diminutas) y se comprueba que **recorre lo que se le pide**, que **no se pasa de la velocidad de
crucero** y que **frena antes de llegar**.

```bash
gcc -Wall -Wextra -I firmware -o /tmp/profile_test \
    firmware/test/profile_test.c firmware/core/profile.c -lm
/tmp/profile_test
```

Resultado esperado (2026-10-30): 9 movimientos y **0 fallos**; el error de posición va de −0,11 mm
(celda de 180 mm a 600 mm/s) a +0,45 mm (perfil que termina rodando a 150 mm/s), dentro de la
tolerancia de 0,125 unidades del módulo.

> ⚠️ Lo que aquí **no** se valida es la distancia real recorrida: el perfil pide velocidad, y quien la
> convierte en movimiento es el lazo de velocidad con la odometría. Ver
> `Documentacion/estado-del-arte.md`.

---

## `pid_sim.c` — lazo de velocidad (capa CONTROL)

Simula una planta de motor de primer orden y comprueba la lógica del PID, la rampa de consigna y el
anti-windup. Es lo único que valida el lazo **antes** de tener el robot montado.

```bash
gcc -I firmware -O2 -Wall -o /tmp/pid_sim firmware/test/pid_sim.c \
    firmware/control/pid.c firmware/control/setpoint.c firmware/control/controller.c -lm
/tmp/pid_sim
```

Sale con código 0 si todo pasa y distinto de 0 si algún criterio incumple. Resultado esperado
(2026-09-28): rampa → sobreimpulso −0.2 % y error 0.2 %; escalón → −0.1 % y 0.1 %; el integrador se
libera en 20 ms tras 5 s saturado.

> La sintonía (`Kp`, `Ki`, `Kd`) **no** se valida aquí: depende del robot real y se mide con la
> prueba 18 del menú. Ver `Documentacion/control-pid.md`.

---

## `odometry_sim.c` — odometría diferencial (capa FILTER)

Mueve la pose con maniobras de resultado conocido (recto, cuadrado de 4 lados, círculo y 10 vueltas
sobre sí mismo) y comprueba que **cierran** con error nulo. La odometría es matemática pura: recibe
incrementos de arco de cada rueda, así que se puede validar entera en el PC.

```bash
gcc -I firmware -O2 -Wall -o /tmp/odometry_sim firmware/test/odometry_sim.c \
    firmware/filter/odometry.c -lm
/tmp/odometry_sim
```

Resultado esperado (2026-09-28): error de cierre 0,00 mm y 0,00° en las tres maniobras, camino
1885 mm en el círculo (2π·300 mm) y θ siempre dentro de ±180°.

> Aquí se valida la **integración**, no la geometría del robot: la trocha efectiva se mide con la
> prueba 19 (ver `Documentacion/pruebas-bringup.md` §15) y el deslizamiento de las ruedas no se puede
> simular. Ver `Documentacion/odometria.md`.
