#!/usr/bin/env python3
"""GTA VI (PPSA38500 01.002.001) Web Trainer for PS5 via ps5debug-NG (port 744).

Ported from OnlyMoisties/basic-menu-src (credits: XeMo).
Runs on your Mac/PC and exposes a web UI (default http://0.0.0.0:8080)
accessible from your computer or phone on the same Wi-Fi network.
"""

from __future__ import annotations

import argparse
from http.server import ThreadingHTTPServer
from pathlib import Path

from core import PS5Trainer, load_vehicles
from server import get_lan_ip, make_handler


def main() -> int:
    parser = argparse.ArgumentParser(description="GTA VI PS5 Web Trainer (PS5Debug-NG)")
    parser.add_argument("--ps5-ip", default="192.168.1.100", help="IP konsoli PS5")
    parser.add_argument("--ps5-port", type=int, default=744, help="Port ps5debug-NG")
    parser.add_argument("--listen", default="0.0.0.0", help="Adres nasłuchu HTTP")
    parser.add_argument("--http-port", type=int, default=8080, help="Port panelu Web")
    args = parser.parse_args()

    here = Path(__file__).resolve().parent
    vehicles = load_vehicles(here / "Vehicles.h")
    print(f"[+] Załadowano {len(vehicles)} pojazdów z {here / 'Vehicles.h'}")

    trainer = PS5Trainer(args.ps5_ip, args.ps5_port, vehicles)
    server = ThreadingHTTPServer(
        (args.listen, args.http_port),
        make_handler(trainer, args.ps5_ip, args.ps5_port),
    )

    lan_ip = get_lan_ip()
    print(f"[+] Web Trainer działa!")
    print(f"    Na Macu:      http://localhost:{args.http_port}")
    print(f"    Na telefonie: http://{lan_ip}:{args.http_port}")
    print(f"    Cel PS5:      {args.ps5_ip}:{args.ps5_port} (zmienisz też w przeglądarce)")
    print("    Zatrzymanie:  Ctrl+C")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[*] Zamykanie...")
    finally:
        trainer._stop_event.set()
        trainer.disconnect()
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
