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


def teclado(cliente):
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
            cliente.sendall(b"LED 1\n")
            time.sleep(0.25)
            cliente.sendall(b"LED 0\n")
            time.sleep(0.25)

        return True

    # La consola del robot ejecuta al recibir `\n` o `\r`: con el salto de linea basta.
    cliente.sendall((orden + "\n").encode("ascii", "replace"))
    print(f"--> {orden}", flush=True)
    return True


def servir(puerto, ruta_log):
    """Bucle principal: un cliente cada vez, y vuelve a escuchar cuando se cae."""
    servidor = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    servidor.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    servidor.bind(("0.0.0.0", puerto))
    servidor.listen(1)

    print(f"escuchando en 0.0.0.0:{puerto} (la telemetria va a {ruta_log})", flush=True)

    fichero = open(ruta_log, "a", buffering=1)
    cliente = None
    buffer = b""

    while True:
        if cliente is None:
            cliente, direccion = servidor.accept()
            buffer = b""
            print(f"=== conectado desde {direccion[0]}:{direccion[1]} ===", flush=True)

            # Un salto de linea al conectar limpia cualquier `>` suelto que haya quedado del arranque.
            cliente.sendall(b"\n")

        listos, _, _ = select.select([sys.stdin, cliente], [], [])

        if cliente in listos:
            try:
                datos = cliente.recv(512)
            except OSError:
                datos = b""

            if not datos:
                print("el robot ha cerrado la conexion (el servidor sigue escuchando)", flush=True)
                cliente.close()
                cliente = None
                continue

            buffer += datos

            while b"\n" in buffer:
                cruda, buffer = buffer.split(b"\n", 1)
                linea = cruda.decode("ascii", "replace").strip()

                if linea:
                    fichero.write(f"{time.time():.3f} {linea}\n")

        if sys.stdin in listos and not teclado(cliente):
            cliente.close()
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
