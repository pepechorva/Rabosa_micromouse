# 🔌 Hardware de Rabosa-MVR

Aquí vive el diseño **físico** del robot, y solo el diseño físico. El firmware está en
[`../firmware/`](../firmware/), y la teoría, las medidas y las decisiones en
[`../Documentacion/`](../Documentacion/).

## Un diseño eléctrico, varios formatos

El **mismo robot** se monta en más de un formato. El diseño eléctrico —qué va conectado a qué— es
**uno**, y vive escrito una sola vez en [`../Documentacion/hardware.md`](../Documentacion/hardware.md).
Lo que cambia entre formatos es **cómo se materializa**: con cables, o sobre una PCB.

| Carpeta | Formato | Qué contiene | Estado |
|:--|:--|:--|:--|
| `cableado/` | Prototipo: placas cableadas entre sí | Conexionado, carcasa impresa, fotos | ⏳ **Pendiente de mover** desde `mechanical/` |
| [`pcb-v1/`](./pcb-v1/kicad/README.md) | V1: todo en una PCB que hace de chasis | Esquemático, PCB, chasis de 70 × 90 | 🚧 **En diseño** (2026-11-12) |

## Qué se hereda y qué no

Es el mismo hardware electrónico, así que la V1 **no invalida la geometría medida**: µm/cuenta
(287,0) y trocha (77,6 mm de partida) vienen del mismo motor, la misma rueda y el mismo encoder. Lo
que sí hay que volver a medir son los **umbrales de pared** y la **calibración del ADC**, porque
cambia dónde van los sensores y cuánto ruido hay. El detalle, en
[`../Documentacion/hardware.md`](../Documentacion/hardware.md) y en la tabla de paridad de
[`../Documentacion/estado-del-arte.md`](../Documentacion/estado-del-arte.md).

## Lo que NO va aquí

- Lógica, perfiles, calibración: eso es `firmware/` (y sus números, en `firmware/modules/`).
- Hojas de datos: `../Datasheets/`.
- La especificación eléctrica y las cuentas de diseño: `../Documentacion/hardware.md`.

## Versionado

La lista blanca del [`.gitignore`](../.gitignore) deja entrar **solo** los `README.md` de estas
carpetas. Los ficheros de CAD y de KiCad se dan de alta **a mano**, un tipo cada vez, cuando existan
de verdad: es deliberado, para que nada entre al repositorio sin que alguien lo haya decidido.
