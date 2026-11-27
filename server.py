"""HTTP API and static web asset server for GTA VI PS5 Web Trainer."""

from __future__ import annotations

import json
import socket
from http.server import BaseHTTPRequestHandler
from pathlib import Path
from typing import Any

from core.constants import CATEGORY_ORDER, QUICK_VEHICLES
from core.ps5_client import PS5Trainer

ICON_SVG = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512">
  <defs>
    <linearGradient id="g" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#ff3d8b"/>
      <stop offset="50%" stop-color="#a64dff"/>
      <stop offset="100%" stop-color="#00d4ff"/>
    </linearGradient>
  </defs>
  <rect width="512" height="512" rx="112" fill="#0d1018"/>
  <rect x="24" y="24" width="464" height="464" rx="92" fill="none" stroke="url(#g)" stroke-width="18"/>
  <text x="256" y="248" text-anchor="middle" font-family="-apple-system,BlinkMacSystemFont,sans-serif" font-weight="900" font-size="132" fill="url(#g)">GTA VI</text>
  <text x="256" y="346" text-anchor="middle" font-family="-apple-system,BlinkMacSystemFont,sans-serif" font-weight="700" font-size="56" fill="#e8ecf5" letter-spacing="8">TRAINER</text>
</svg>""".encode("utf-8")

MANIFEST_JSON = json.dumps(
    {
        "name": "GTA VI PS5 Trainer",
        "short_name": "GTA VI",
        "start_url": "/",
        "display": "standalone",
        "background_color": "#0b0d14",
        "theme_color": "#0b0d14",
        "icons": [
            {"src": "/icon.svg", "sizes": "any", "type": "image/svg+xml", "purpose": "any maskable"}
        ],
    }
).encode("utf-8")


def get_lan_ip() -> str:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("192.168.1.100", 744))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


def make_handler(trainer: PS5Trainer, default_ip: str, default_port: int):
    web_dir = Path(__file__).resolve().parent / "web"
    page_bytes = (
        (web_dir / "index.html")
        .read_text(encoding="utf-8")
        .replace("__DEFAULT_IP__", default_ip)
        .replace("__DEFAULT_PORT__", str(default_port))
        .encode("utf-8")
    )
    css_bytes = (web_dir / "styles.css").read_bytes()
    js_bytes = (web_dir / "app.js").read_bytes()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt: str, *args: Any) -> None:
            pass

        def _send_json(self, code: int, obj: dict[str, Any]) -> None:
            raw = json.dumps(obj).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)

        def _read_json(self) -> dict[str, Any]:
            length = int(self.headers.get("Content-Length", "0") or "0")
            raw = self.rfile.read(length) if length > 0 else b"{}"
            return json.loads(raw.decode("utf-8") or "{}")

        def do_GET(self) -> None:
            if self.path in ("/", "/index.html"):
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(page_bytes)))
                self.end_headers()
                self.wfile.write(page_bytes)
                return
            if self.path == "/styles.css":
                self.send_response(200)
                self.send_header("Content-Type", "text/css; charset=utf-8")
                self.send_header("Content-Length", str(len(css_bytes)))
                self.end_headers()
                self.wfile.write(css_bytes)
                return
            if self.path == "/app.js":
                self.send_response(200)
                self.send_header("Content-Type", "application/javascript; charset=utf-8")
                self.send_header("Content-Length", str(len(js_bytes)))
                self.end_headers()
                self.wfile.write(js_bytes)
                return
            if self.path == "/manifest.json":
                self.send_response(200)
                self.send_header("Content-Type", "application/manifest+json; charset=utf-8")
                self.send_header("Content-Length", str(len(MANIFEST_JSON)))
                self.end_headers()
                self.wfile.write(MANIFEST_JSON)
                return
            if self.path == "/icon.svg":
                self.send_response(200)
                self.send_header("Content-Type", "image/svg+xml")
                self.send_header("Content-Length", str(len(ICON_SVG)))
                self.end_headers()
                self.wfile.write(ICON_SVG)
                return
            if self.path == "/api/status":
                self._send_json(200, trainer.status())
                return
            if self.path == "/api/vehicles":
                self._send_json(
                    200,
                    {
                        "vehicles": trainer.vehicles,
                        "quick": QUICK_VEHICLES,
                        "categories": CATEGORY_ORDER,
                    },
                )
                return
            self._send_json(404, {"error": "Not found"})

        def do_POST(self) -> None:
            try:
                body = self._read_json()
                if self.path == "/api/connect":
                    st = trainer.connect(body.get("ip"), body.get("port"))
                    msg = (
                        f"Połączono z {st['titleid']} {st['app_ver']} (PID {st['pid']})"
                        if st["version_ok"]
                        else f"Połączono z PID {st['pid']}, ale niezgodna wersja gry!"
                    )
                    trainer._log(f"API /api/connect -> {msg}")
                    if st["version_ok"]:
                        trainer._notify_ps5("Połączono! Gotowy do gry.")
                    self._send_json(200, {"message": msg, "status": st})
                elif self.path == "/api/disconnect":
                    trainer.disconnect()
                    trainer._log("API /api/disconnect -> Disconnected from PS5")
                    self._send_json(200, {"message": "Disconnected from PS5.", "status": trainer.status()})
                elif self.path == "/api/heal":
                    msg = trainer.heal_repair()
                    trainer._log(f"API /api/heal -> {msg}")
                    trainer._notify_ps5(msg)
                    self._send_json(200, {"message": msg, "status": trainer.status()})
                elif self.path == "/api/heal_only":
                    msg = trainer.heal_only()
                    trainer._log(f"API /api/heal_only -> {msg}")
                    trainer._notify_ps5(msg)
                    self._send_json(200, {"message": msg, "status": trainer.status()})
                elif self.path == "/api/armor_only":
                    msg = trainer.armor_only()
                    trainer._log(f"API /api/armor_only -> {msg}")
                    trainer._notify_ps5(msg)
                    self._send_json(200, {"message": msg, "status": trainer.status()})
                elif self.path == "/api/repair_car":
                    msg = trainer.repair_car()
                    trainer._log(f"API /api/repair_car -> {msg}")
                    trainer._notify_ps5(msg)
                    self._send_json(200, {"message": msg, "status": trainer.status()})
                elif self.path == "/api/wanted":
                    msg = trainer.set_wanted(body.get("level", 0))
                    trainer._log(f"API /api/wanted -> {msg}")
                    trainer._notify_ps5(msg)
                    self._send_json(200, {"message": msg, "status": trainer.status()})
                elif self.path == "/api/wanted_delta":
                    msg = trainer.wanted_delta(bool(body.get("up", True)))
                    trainer._log(f"API /api/wanted_delta -> {msg}")
                    trainer._notify_ps5(msg)
                    self._send_json(200, {"message": msg, "status": trainer.status()})
                elif self.path == "/api/weather":
                    msg = trainer.next_weather()
                    trainer._log(f"API /api/weather -> {msg}")
                    trainer._notify_ps5(msg)
                    self._send_json(200, {"message": msg, "status": trainer.status()})
                elif self.path == "/api/time":
                    msg = trainer.set_time(body.get("hour", 12), body.get("minute", 0))
                    trainer._log(f"API /api/time -> {msg}")
                    trainer._notify_ps5(msg)
                    self._send_json(200, {"message": msg, "status": trainer.status()})
                elif self.path == "/api/toggle":
                    msg = trainer.set_toggle(body.get("name", ""), bool(body.get("enabled")))
                    trainer._log(f"API /api/toggle -> {msg}")
                    if body.get("name") != "ps5_notify":
                        trainer._notify_ps5(msg)
                    self._send_json(200, {"message": msg, "status": trainer.status()})
                elif self.path == "/api/spawn":
                    msg = trainer.spawn_vehicle(body.get("name", ""))
                    trainer._log(f"API /api/spawn ({body.get('name', '')}) -> {msg}")
                    trainer._notify_ps5(msg)
                    self._send_json(200, {"message": msg, "status": trainer.status()})
                elif self.path == "/api/filter_vehicles":
                    avail = trainer.filter_available_vehicles()
                    msg = f"Znaleziono {len(avail)} z {len(trainer.vehicles)} dostępnych pojazdów."
                    trainer._log(f"API /api/filter_vehicles -> {msg}")
                    self._send_json(
                        200,
                        {
                            "message": msg,
                            "vehicles": avail,
                            "status": trainer.status(),
                        },
                    )
                elif self.path == "/api/money/find":
                    res = trainer.money_find_auto(body.get("amount", 0))
                    trainer._log(f"API /api/money/find -> {res.get('message')}")
                    res["status"] = trainer.status()
                    self._send_json(200, res)
                elif self.path == "/api/money/watch":
                    res = trainer.start_money_auto_watch(body.get("amount"))
                    trainer._log(f"API /api/money/watch -> {res.get('message')}")
                    res["status"] = trainer.status()
                    self._send_json(200, res)
                elif self.path == "/api/money/add":
                    res = trainer.money_add(body.get("delta", 0))
                    trainer._log(f"API /api/money/add -> {res.get('message')}")
                    trainer._notify_ps5(res.get("message", ""))
                    res["status"] = trainer.status()
                    self._send_json(200, res)
                elif self.path == "/api/money/set":
                    res = trainer.money_set(body.get("amount", 1000000), body.get("address"))
                    trainer._log(f"API /api/money/set -> {res.get('message')}")
                    trainer._notify_ps5(res.get("message", ""))
                    res["status"] = trainer.status()
                    self._send_json(200, res)
                elif self.path == "/api/money/reset":
                    res = trainer.money_reset()
                    trainer._log(f"API /api/money/reset -> {res.get('message')}")
                    res["status"] = trainer.status()
                    self._send_json(200, res)
                elif self.path == "/api/lektor/rate":
                    msg = trainer.set_lektor_rate(body.get("rate", 195))
                    trainer._log(f"API /api/lektor/rate -> {msg}")
                    self._send_json(200, {"message": msg, "status": trainer.status()})
                elif self.path == "/api/lektor/speak":
                    txt = str(body.get("text", "")).strip()
                    if txt:
                        trainer.speak_mac(txt, interrupt=True)
                    self._send_json(200, {"message": "Odtwarzam test lektora...", "status": trainer.status()})
                else:
                    self._send_json(404, {"error": "Unknown endpoint"})
            except Exception as exc:
                trainer._log(f"ERROR {self.path}: {exc}")
                self._send_json(400, {"error": str(exc), "status": trainer.status()})

    return Handler


