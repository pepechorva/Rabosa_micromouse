# 🧩 KiCad de la V1 — todo el robot en una PCB

Aquí va el proyecto de KiCad de la **V1**: la misma electrónica del prototipo, pero en una sola
placa de 70 × 90 mm que hace de PCB y de chasis a la vez
(contorno en [`../../../Documentacion/chasis-v1.md`](../../../Documentacion/chasis-v1.md)).

> 📅 **Alta: 2026-11-12.** La carpeta nace **vacía a propósito**: se creó antes de empezar a
> dibujar el esquemático para que la lista blanca del `.gitignore` y la documentación apunten al
> sitio correcto desde el primer día.

## Qué va a contener

| Fichero | Qué es |
|:--|:--|
| `Rabosa-v1.kicad_pro` | Proyecto (reglas de DRC, netclasses) |
| `Rabosa-v1.kicad_sch` | **Esquemático**: la especificación eléctrica, dibujada |
| `Rabosa-v1.kicad_pcb` | **PCB**: contorno desde el `Edge.Cuts` del chasis, colocación y ruteo |
| `Rabosa-v1.kicad_sym` | Símbolos propios, si hacen falta |

El contorno de la placa sale del DXF que genera el chasis (el `Edge.Cuts` de
`mechanical/freecad/chasis-v1.dxf`); con la placa haciendo de chasis, **ese contorno es el
diseño**, y por eso acaba aquí y no en un STEP suelto.

## Qué NO se versiona (y no hace falta tocar el `.gitignore` para ello)

`*-backups/`, los `_autosave-*`, los `*.lck` y los ZIP de respaldo automático de KiCad. La lista
blanca ya los deja fuera por construcción: **solo entra lo que se nombra**.

## Cómo se da de alta

Cuando existan los ficheros, **descomentar** el bloque `KiCad` al final del `.gitignore` de la
raíz. Están las cuatro extensiones puestas, con el comentario de qué es cada una, para que la
decisión de versionarlas sea explícita y no un efecto colateral.
