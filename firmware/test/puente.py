#!/usr/bin/env python3
"""Servidor TCP del banco: la otra punta del puente WiFi del robot.

Con la prueba **`WIFI AL PC`** (24) el ESP8266 pone la USART3 del STM32 en modo transparente
(`AT+CIPMODE=1`) y se conecta **como cliente** a este servidor. A partir de ahi la USART3 entera
--telemetria y consola-- viaja por el socket, y el robot se maneja **sin cable**.

Uso:  puente.py [puerto] [--log FICHERO]        (puerto por defecto 3333, log /tmp/puente.log)

  - Lo que se teclee se manda al robot al pulsar Enter (admite `parpadeo [n]` y `salir`).
  - Todo lo que llega del robot se apunta al fichero de log, **con marca de tiempo**.

⚠️ **Tiene que estar escuchando ANTES de que el robot haga `AT+CIPSTART`**, o ese paso falla aunque
todo lo demas este bien.

⚠️ **Es callado a proposito: la telemetria va al fichero, no a la pantalla.** Imprimir cada linea
recibida hace que el terminal nunca se quede quieto (acaba interrumpiendo el proceso) y que cada
orden tarde medio minuto en volver. El registro se lee con `grep`/`tail`, que para eso esta.

⚠️ Acepta **reconexiones**: si el robot se reinicia, vuelve por su cuenta. Y el modulo se reconecta
solo cuando se reinicia este servidor, asi que no hay que pulsar nada en el robot.
"""

import argparse
import os
import select
import socket
import sys
import time


def enviar(clientes, datos):
    """Manda algo a todos los clientes vivos; el que falle se ignora (se recogera al leer)."""
    for c, _ in clientes:
        try:
            c.sendall(datos)
        except OSError:
            pass


def teclado(clientes):
    """Manda al robot lo que se escriba por la entrada estandar."""
    orden = sys.stdin.readline().strip()

    if not orden:
        return True

    if orden in ("salir", "quit"):
        print("cerrando...", flush=True)
        return False

    # `parpadeo [n]`: se hace **aqui**, con las pausas, y no mandando las ordenes de una en una.
    # Es la unica forma de que se vea: entre dos ordenes escritas a mano pasan decenas de segundos,
    # y un LED que cambia cada medio minuto no es un latido, es un cambio de estado.
    if orden.lower().startswith("parpadeo"):
        campos = orden.split()
        veces = int(campos[1]) if len(campos) > 1 else 6

        print(f"--> parpadeo de {veces} veces", flush=True)

        for _ in range(veces):
            enviar(clientes, b"LED 1\n")
            time.sleep(0.25)
            enviar(clientes, b"LED 0\n")
            time.sleep(0.25)

        return True

    # La consola del robot ejecuta al recibir `\n` o `\r`: con el salto de linea basta.
    enviar(clientes, (orden + "\n").encode("ascii", "replace"))
    print(f"--> {orden}", flush=True)
    return True


def servir(puerto, ruta_log):
    """Bucle principal: acepta clientes **siempre**, y atiende a todos los que esten vivos.

    ⚠️ **Aceptar aunque ya haya uno conectado no es un lujo, es la diferencia entre recibir y no
    recibir nada.** Cuando el ESP se reinicia abre una conexion **nueva** sin cerrar la vieja, y el
    socket viejo se queda **abierto y mudo** en el kernel (un zombi: `ss` lo muestra `ESTAB` con
    `Recv-Q 0`). Con el diseno anterior —aceptar solo cuando no hubiera ningun cliente— esa conexion
    zombi dejaba al servidor en `select()` para siempre: la telemetria se acumulaba **sin leer** en el
    bufer del socket nuevo (visto en el banco el 2026-10-02: **17 030 B** en `Recv-Q`) y el registro
    no crecia ni un byte. El sintoma enganya: el puente parece roto cuando el que no lee es este.
    """
    servidor = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    servidor.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    servidor.bind(("0.0.0.0", puerto))
    servidor.listen(4)

    print(f"escuchando en 0.0.0.0:{puerto} (la telemetria va a {ruta_log})", flush=True)

    fichero = open(ruta_log, "a", buffering=1)
    clientes = []   # [(socket, bufer de la linea en curso)]

    while True:
        listos, _, _ = select.select([servidor, sys.stdin] + [c for c, _ in clientes], [], [])

        if servidor in listos:
            nuevo, direccion = servidor.accept()
            print(f"=== conectado desde {direccion[0]}:{direccion[1]} ===", flush=True)

            # Un salto de linea al conectar limpia cualquier `>` suelto que haya quedado del arranque.
            nuevo.sendall(b"\n")
            clientes.append((nuevo, b""))

        vivos = []

        for c, bufer in clientes:
            if c not in listos:
                vivos.append((c, bufer))
                continue

            try:
                datos = c.recv(4096)
            except OSError:
                datos = b""

            if not datos:
                print("el robot ha cerrado la conexion (el servidor sigue escuchando)", flush=True)
                c.close()
                continue

            bufer += datos

            while b"\n" in bufer:
                cruda, bufer = bufer.split(b"\n", 1)
                linea = cruda.decode("ascii", "replace").strip()

                if linea:
                    fichero.write(f"{time.time():.3f} {linea}\n")

            vivos.append((c, bufer))

        clientes = vivos

        if sys.stdin in listos and not teclado(clientes):
            for c, _ in clientes:
                c.close()

            servidor.close()
            fichero.close()
            return


def main():
    parser = argparse.ArgumentParser(description="Servidor TCP del puente WiFi del Rabosa-MVR.")
    parser.add_argument("puerto", nargs="?", type=int, default=3333)
    parser.add_argument("--log", default="/tmp/puente.log")
    args = parser.parse_args()

    try:
        servir(args.puerto, args.log)
    except KeyboardInterrupt:
        print("interrumpido", flush=True)
    except OSError as error:
        sys.exit(f"no puedo escuchar en el {args.puerto}: {error} "
                 "(¿esta ya corriendo otro servidor en ese puerto?)")


if __name__ == "__main__":
    # Sin buffer en la salida: asi los mensajes de conexion se ven al momento.
    sys.stdout.reconfigure(line_buffering=True) if hasattr(sys.stdout, "reconfigure") else None
    main()
