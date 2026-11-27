"""Vehicle catalog loading, categorization, model lookup, and spawning."""

from __future__ import annotations

import json
import re
import struct
import time
from pathlib import Path
from typing import Any

from ps5dbg import PS5Debug

from .constants import (
    OFF_ARCHETYPE_MAP,
    OFF_LAST_CHEAT_VEH,
    OFF_SPAWN_VEHICLE,
    OFF_STREAM_CTX,
    OFF_STREAM_READY_A,
    OFF_STREAM_READY_B,
    OFF_STREAM_RELEASE,
    OFF_STREAM_REQUEST,
    VEHICLE_CATEGORIES,
)


def classify_vehicle(name: str) -> str:
    low = name.lower()
    for cat, names in VEHICLE_CATEGORIES.items():
        if low in names:
            return cat
    return "Inne"


def load_vehicles(header_path: Path) -> list[dict[str, Any]]:
    if not header_path.exists():
        return []
    wiki_path = header_path.parent / "vehicles_wiki.json"
    wiki_map: dict[str, dict[str, Any]] = {}
    if wiki_path.exists():
        try:
            wiki_map = json.loads(wiki_path.read_text(encoding="utf-8"))
        except Exception:
            wiki_map = {}

    text = header_path.read_text(encoding="utf-8", errors="ignore")
    matches = re.findall(r'\{\s*"([^"]+)"\s*,\s*(0x[0-9A-Fa-f]+)ull\s*\}', text)
    missing_in_game = {"annihilator", "jetmax"}
    result = []
    for name, h in matches:
        if name.lower() in missing_in_game:
            continue
        w = wiki_map.get(name.lower(), {})
        display = w.get("display") or name
        cat = w.get("category") or classify_vehicle(name)
        result.append(
            {
                "name": name,
                "display": display,
                "hash": int(h, 16),
                "hash_hex": h,
                "category": cat,
            }
        )
    return result



class VehiclesMixin:
    """Mixin providing GTA VI vehicle spawning and availability filtering."""

    def _lookup_model_on(self, ps5: PS5Debug, pid: int, base: int, model_hash: int) -> int | None:
        amap = base + OFF_ARCHETYPE_MAP
        buckets = struct.unpack("<I", ps5.read(pid, amap + 4, 4))[0]
        if not buckets:
            return None
        b = amap + 8 + (model_hash % buckets) * 0x58
        b_data = ps5.read(pid, b, 0x58)
        n = struct.unpack_from("<I", b_data, 4)[0]
        keys_ptr = struct.unpack_from("<Q", b_data, 8)[0]
        vals_ptr = struct.unpack_from("<Q", b_data, 0x18)[0]
        ex = struct.unpack_from("<I", b_data, 0x24)[0]
        exl_ptr = struct.unpack_from("<Q", b_data, 0x28)[0]
        arr_ptr = struct.unpack_from("<Q", b_data, 0x48)[0]
        cnt = struct.unpack_from("<H", b_data, 0x50)[0]

        found = False
        val = 0
        if n and keys_ptr and vals_ptr and n <= 65536:
            keys_raw = ps5.read(pid, keys_ptr, 8 * n)
            vals_raw = ps5.read(pid, vals_ptr, 4 * n)
            for i in range(n):
                if struct.unpack_from("<Q", keys_raw, 8 * i)[0] == model_hash:
                    val = struct.unpack_from("<I", vals_raw, 4 * i)[0]
                    found = True
                    break

        if not found and arr_ptr and cnt and cnt <= 65536:
            arr_raw = ps5.read(pid, arr_ptr, 16 * cnt)
            for i in range(cnt):
                if struct.unpack_from("<Q", arr_raw, 16 * i)[0] == model_hash:
                    val = struct.unpack_from("<I", arr_raw, 16 * i + 8)[0]
                    found = True
                    break

        if not found:
            return None

        if ex and exl_ptr and ex <= 65536:
            exl_raw = ps5.read(pid, exl_ptr, 4 * ex)
            for i in range(ex):
                ev = struct.unpack_from("<I", exl_raw, 4 * i)[0]
                if ((val ^ ev) & 0x3FFFFF) == 0:
                    return None
        return val

    def _lookup_model(self, model_hash: int) -> int | None:
        assert self.ps5 is not None
        return self._lookup_model_on(self.ps5, self.pid, self.base, model_hash)

    def spawn_vehicle(self, name: str) -> str:
        with self.lock:
            self._require_ready()
            vm = self.vehicles_by_name.get(name.lower())
            if not vm:
                raise ValueError(f"Nieznany pojazd: {name}")
            model_id = self._lookup_model(vm["hash"])
            if model_id is None:
                raise RuntimeError(f"Nie znaleziono modelu '{vm['name']}' w ArchetypeMap.")

            req_ptr = self.ctrl_addr + 0x100
            model_ptr = self.ctrl_addr + 0x110
            aux_ptr = self.ctrl_addr + 0x120

            self._proc_write(req_ptr, b"\x00\x00\x00\x00")
            self._proc_write(model_ptr, struct.pack("<I", model_id))
            self._proc_write(aux_ptr, b"\x00" * 16)

            # StreamRequest(&req, &model, StreamCtx, 0, 9, 1, 0, aux) on game's main thread
            self._game_call(
                self.base + OFF_STREAM_REQUEST,
                req_ptr,
                model_ptr,
                self.base + OFF_STREAM_CTX,
                0,
                9,
                1,
                0,
                aux_ptr,
            )

            deadline = time.monotonic() + 6.0
            loaded = False
            while time.monotonic() < deadline:
                req_val = struct.unpack("<H", self.ps5.read(self.pid, req_ptr, 2))[0]  # type: ignore[union-attr]
                if req_val & 0x3FFF:
                    a = self._game_call(self.base + OFF_STREAM_READY_A, req_ptr) & 0xFF
                    b = self._game_call(self.base + OFF_STREAM_READY_B, req_ptr) & 0xFF
                    if a and b:
                        loaded = True
                        break
                time.sleep(0.08)

            try:
                if loaded:
                    self._proc_write(self.base + OFF_LAST_CHEAT_VEH, struct.pack("<I", 0x1FFFF))
                    self._game_call(self.base + OFF_SPAWN_VEHICLE, vm["hash"])
                    return f"Zespawnowano pojazd: {vm['name']}"
                raise RuntimeError(f"Timeout ładowania modelu: {vm['name']}")
            finally:
                req_val = struct.unpack("<H", self.ps5.read(self.pid, req_ptr, 2))[0]  # type: ignore[union-attr]
                if req_val & 0x3FFF:
                    try:
                        self._game_call(self.base + OFF_STREAM_RELEASE, req_ptr)
                    except Exception:
                        pass

    def filter_available_vehicles(self) -> list[dict[str, Any]]:
        """Uses its own dedicated TCP connection so game actions & UI never stall."""
        with self.lock:
            self._require_ready()
            host, port, pid, base = self.host, self.port, self.pid, self.base
        scan_ps5 = PS5Debug(host, port=port)
        scan_ps5.connect()
        try:
            available = []
            for vm in self.vehicles:
                if self._lookup_model_on(scan_ps5, pid, base, vm["hash"]) is not None:
                    available.append(vm)
            return available
        finally:
            try:
                scan_ps5.close()
            except Exception:
                pass

