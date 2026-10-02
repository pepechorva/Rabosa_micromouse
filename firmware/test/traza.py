#!/usr/bin/env python3
"""Tabula el volcado de la traza (app/trace.c) y deduce que umbral lo explica.

`TRACE_Descargar()` manda **cuatro lineas por muestra**, en este orden:

    >TR_SET  >TR_IZQ  >TR_DER  >TR_EXTRA

y cierra el volcado con `>TR_FIN:1`. Con varios volcados en el mismo fichero se
analiza **el ultimo** (que es el recorrido que se acaba de hacer).

Lo util no es la tabla, es la **inversion**: dada la regla de decision y las tres
medidas de cada celda, se busca QUE UMBRALES reproducen las decisiones que tomo el
robot. Si ninguno lo hace, el problema no son los umbrales: es la regla.

Uso:  traza.py [fichero] [--mano der|izq]      (por defecto /tmp/puente.log y mano derecha)
"""

import argparse
import re
import sys

DECISIONES = {1: "AVANZA", 2: "GIRA-IZQ", 3: "GIRA-DER", 4: "VUELTA"}
CANALES = ("TR_SET", "TR_IZQ", "TR_DER", "TR_EXTRA")
LINEA = re.compile(r"^([0-9.]+)\s+>(TR_[A-Z]+):(-?\d+)\s*$")


def leer(ruta):
    """Devuelve la lista de muestras (frente, izq, der, decision) del ULTIMO volcado.

    `TR_FIN` va **al final** del volcado, asi que no se "reinicia" con el: lo que hace es
    **cerrar** el volcado en curso y guardarlo. Se analiza el ultimo cerrado, que es el
    recorrido que se acaba de hacer.
    """
    cerrados = []
    en_curso = []
    actual = {}

    with open(ruta, encoding="utf-8", errors="replace") as fichero:
        for linea in fichero:
            campos = LINEA.match(linea.strip())

            if not campos:
                continue

            _, etiqueta, valor = campos.groups()

            if etiqueta == "TR_FIN":
                if en_curso:
                    cerrados = en_curso
                en_curso = []
                actual = {}
                continue

            actual[etiqueta] = int(valor)

            # `TR_EXTRA` cierra la muestra: hasta aqui van las cuatro.
            if etiqueta == "TR_EXTRA" and all(c in actual for c in CANALES):
                en_curso.append(tuple(actual[c] for c in CANALES))
                actual = {}

    # Si el fichero se corto antes del `TR_FIN` (volcado a medias), vale el que hay en curso.
    return cerrados if cerrados else en_curso


def decision_esperada(frente, izq, der, umbral, mano_der):
    """La regla de `wf_decidir()`: la mano, luego el frente, luego el otro lado, luego media vuelta."""
    hay_f = frente >= umbral
    hay_i = izq >= umbral
    hay_d = der >= umbral

    if mano_der:
        if not hay_d:
            return 3
        if not hay_f:
            return 1
        if not hay_i:
            return 2
        return 4

    if not hay_i:
        return 2
    if not hay_f:
        return 1
    if not hay_d:
        return 3
    return 4


def umbrales_compatibles(muestras, mano_der):
    """Umbrales (rango continuo) que reproducen EXACTAMENTE las decisiones observadas."""
    medidas = [m for s in muestras for m in s[:3]]

    if not medidas:
        return []

    return [t for t in range(min(medidas) - 1, max(medidas) + 2)
            if all(decision_esperada(f, i, d, t, mano_der) == dec
                   for f, i, d, dec in muestras)]


def main():
    parser = argparse.ArgumentParser(description="Analiza el volcado de la traza del wall follower.")
    parser.add_argument("fichero", nargs="?", default="/tmp/puente.log")
    parser.add_argument("--mano", choices=("der", "izq"), default="der")
    args = parser.parse_args()

    try:
        muestras = leer(args.fichero)
    except OSError as error:
        sys.exit(f"no puedo leer {args.fichero}: {error}")

    if not muestras:
        sys.exit(f"no hay ninguna muestra >TR_* en {args.fichero} "
                 "(¿se mando `TRACE` despues de hacer el recorrido?)")

    print(f"== {len(muestras)} celdas ({args.fichero}, mano {args.mano}) ==\n")
    print(" celda | FRENTE   IZQ   DER | decision")
    print("-------+-------------------+----------")

    for n, (frente, izq, der, decision) in enumerate(muestras):
        print(f" {n:5d} | {frente:6d} {izq:5d} {der:5d} | {DECISIONES.get(decision, decision)}")

    print()
    for nombre, indice in (("FRENTE", 0), ("IZQ", 1), ("DER", 2)):
        valores = [s[indice] for s in muestras]
        print(f"  {nombre:<7} {min(valores):5d} .. {max(valores):5d}")

    cuenta = {}
    for muestra in muestras:
        cuenta[muestra[3]] = cuenta.get(muestra[3], 0) + 1
    resumen = ", ".join(f"{DECISIONES.get(k, k)} x{v}" for k, v in sorted(cuenta.items()))
    print(f"\nDecisiones: {resumen}\n")

    posibles = umbrales_compatibles(muestras, args.mano == "der")

    if not posibles:
        print("!! NINGUN umbral explica las decisiones observadas.")
        print("   Eso NO es un problema de umbrales: es la regla de decision, la mano,")
        print("   o la propia traza. Hay que mirar `wf_decidir()`.")
    else:
        print(f"Umbral que explica las decisiones: {min(posibles)}..{max(posibles)}")
        if max(posibles) - min(posibles) < 2:
            print("   !!! La banda es de un solo valor: el recorrido va al limite. "
                  "Busca un umbral mas en el centro.")


if __name__ == "__main__":
    main()
