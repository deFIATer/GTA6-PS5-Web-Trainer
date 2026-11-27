"""Core PS5Debug-NG client, main-thread GOT hook, notifications, and Self/World actions."""

from __future__ import annotations

import datetime
import json
import queue
import struct
import subprocess
import threading
import time
from pathlib import Path
from typing import Any

from ps5dbg import PS5Debug
from ps5dbg.protocol import expect_success

from .constants import (
    CMD_CONSOLE_NOTIFY,
    CMD_PROC_CALL,
    CMD_PROC_PROTECT,
    CMD_PROC_WRITE,
    OFF_CHEAT_HEAL_REPAIR,
    OFF_CHEAT_TABLE,
    OFF_CHEAT_WANTED_DOWN,
    OFF_CHEAT_WANTED_UP,
    OFF_CHEAT_WEATHER,
    OFF_CLOCK_SET_TIME,
    OFF_GAME_CLOCK,
    OFF_GET_PLAYER_COMP,
    OFF_GOT_PAD_READ_STATE,
    OFF_HEALTH_RESTORE,
    OFF_LOCAL_PLAYER_PED,
    OFF_LOCAL_PLAYER_VEH,
    OFF_SET_PLAYER_WANTED,
    OFF_VEHICLE_FIX,
    OFF_WANTED_LEVEL,
)
from .money import MoneyMixin
from .subtitles import SubtitlesMixin
from .vehicles import VehiclesMixin


def build_pad_hook(ctrl_addr: int) -> bytes:
    """Build x86-64 PadReadStateHook that executes queued calls on the game's main thread."""
    prologue = (
        bytes.fromhex("53")  # push rbx
        + bytes.fromhex("4154")  # push r12
        + bytes.fromhex("4883ec28")  # sub rsp, 0x28
        + bytes.fromhex("48bb") + struct.pack("<Q", ctrl_addr)  # mov rbx, ctrl_addr
        + bytes.fromhex("ff13")  # call qword ptr [rbx] (orig_pad)
        + bytes.fromhex("4189c4")  # mov r12d, eax
        + bytes.fromhex("ff430c")  # inc dword ptr [rbx+0x0c]
        + bytes.fromhex("b801000000")  # mov eax, 1
        + bytes.fromhex("b902000000")  # mov ecx, 2
        + bytes.fromhex("f00fb14b08")  # lock cmpxchg [rbx+0x08], ecx
    )
    body = (
        bytes.fromhex("488b4348")  # mov rax, [rbx+0x48] (arg7)
        + bytes.fromhex("48890424")  # mov [rsp], rax
        + bytes.fromhex("488b4350")  # mov rax, [rbx+0x50] (arg8)
        + bytes.fromhex("4889442408")  # mov [rsp+8], rax
        + bytes.fromhex("488b7b18")  # mov rdi, [rbx+0x18]
        + bytes.fromhex("488b7320")  # mov rsi, [rbx+0x20]
        + bytes.fromhex("488b5328")  # mov rdx, [rbx+0x28]
        + bytes.fromhex("488b4b30")  # mov rcx, [rbx+0x30]
        + bytes.fromhex("4c8b4338")  # mov r8,  [rbx+0x38]
        + bytes.fromhex("4c8b4b40")  # mov r9,  [rbx+0x40]
        + bytes.fromhex("488b4310")  # mov rax, [rbx+0x10] (fn)
        + bytes.fromhex("ffd0")  # call rax
        + bytes.fromhex("48894358")  # mov [rbx+0x58], rax (result)
        + bytes.fromhex("c7430803000000")  # mov dword ptr [rbx+0x08], 3 (done)
    )
    epilogue = (
        bytes.fromhex("4489e0")  # mov eax, r12d
        + bytes.fromhex("4883c428")  # add rsp, 0x28
        + bytes.fromhex("415c")  # pop r12
        + bytes.fromhex("5b")  # pop rbx
        + bytes.fromhex("c3")  # ret
    )
    assert len(body) < 127
    jne_done = bytes([0x75, len(body)])
    return prologue + jne_done + body + epilogue


class PS5Trainer(VehiclesMixin, MoneyMixin, SubtitlesMixin):
    def __init__(self, host: str, port: int, vehicles: list[dict[str, Any]]) -> None:
        self.host = host
        self.port = port
        self.vehicles = vehicles
        self.vehicles_by_name = {v["name"].lower(): v for v in vehicles}
        self.lock = threading.RLock()
        self.work_dir = Path(__file__).resolve().parent.parent
        self.log_path = self.work_dir / "trainer.log"
        self.cache_path = self.work_dir / "session_cache.json"

        self.ps5: PS5Debug | None = None
        self.pid: int = 0
        self.base: int = 0
        self.titleid: str = ""
        self.app_ver: str = ""
        self.fw_str: str = ""
        self.kstuff_active: bool = False
        self.etahen_active: bool = False
        self.version_ok: bool = False
        self.ctrl_addr: int = 0
        self.hook_addr: int = 0
        self.orig_pad: int = 0
        self.hook_installed: bool = False

        self.god_mode: bool = False
        self.never_wanted: bool = False
        self.freeze_time: bool = False
        self.ps5_notify: bool = True
        self.frozen_h: int = 12
        self.frozen_m: int = 0
        self.money_candidates: list[int] = []
        self.money_authed: bool = False
        self.money_mode: str = "idle"  # idle | manual_scanning | auto_scanning | auto_watching | locked
        self.money_watch_baseline: dict[int, int] = {}
        self.money_watch_note: str = "Wybierz tryb poniżej: Ręczny (po kwocie) lub Auto-wykrycie (od $0)."
        self._money_cancel_event = threading.Event()
        self.lektor_enabled: bool = False
        self.lektor_mac_say: bool = True
        self.lektor_read_speaker: bool = False
        self.lektor_rate: int = 195
        self.subtitle_seq: int = 0
        self.subtitle_last_key: str = ""
        self.subtitle_current: dict[str, Any] | None = None
        self.subtitle_history: list[dict[str, Any]] = []
        self._say_proc: subprocess.Popen | None = None
        self.last_error: str = ""

        self._stop_event = threading.Event()
        self._notify_queue: queue.Queue[str] = queue.Queue(maxsize=32)
        self._say_queue: queue.Queue[str] = queue.Queue(maxsize=12)
        self._notify_thread = threading.Thread(target=self._notify_loop, daemon=True)
        self._notify_thread.start()
        self._say_thread = threading.Thread(target=self._say_worker_loop, daemon=True)
        self._say_thread.start()
        self._subtitle_thread = threading.Thread(target=self._subtitle_loop, daemon=True)
        self._subtitle_thread.start()
        self._bg_thread = threading.Thread(target=self._background_loop, daemon=True)
        self._bg_thread.start()

    def _log(self, msg: str) -> None:
        ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        line = f"[{ts}] {msg}"
        print(line, flush=True)
        try:
            with self.log_path.open("a", encoding="utf-8") as f:
                f.write(line + "\n")
        except Exception:
            pass

    def _notify_ps5(self, text: str) -> None:
        """Enqueue a native PS5 system notification on its dedicated non-blocking channel."""
        if not self.ps5_notify or not text or self.pid <= 0:
            return
        try:
            self._notify_queue.put_nowait(text)
        except queue.Full:
            pass

    def _notify_loop(self) -> None:
        """Dedicated TCP connection for PS5 on-screen notifications (1 toast at a time, 3s pacing)."""
        conn: PS5Debug | None = None
        last_target: tuple[str, int] | None = None
        last_sent: float = 0.0
        while not self._stop_event.is_set():
            try:
                text = self._notify_queue.get(timeout=0.5)
            except queue.Empty:
                continue
            # If multiple rapid clicks queued up, drain and keep only the latest message
            while True:
                try:
                    text = self._notify_queue.get_nowait()
                except queue.Empty:
                    break
            if not self.ps5_notify or self.pid <= 0:
                continue
            # Pace notifications by 3.0s so PS5 doesn't stack a long list of toasts
            wait = 3.0 - (time.monotonic() - last_sent)
            if wait > 0:
                self._stop_event.wait(wait)
                if self._stop_event.is_set():
                    break
                # Grab newer message if one arrived while waiting
                while True:
                    try:
                        text = self._notify_queue.get_nowait()
                    except queue.Empty:
                        break
            target = (self.host, self.port)
            try:
                if conn is None or last_target != target:
                    if conn is not None:
                        try:
                            conn.close()
                        except Exception:
                            pass
                    conn = PS5Debug(self.host, port=self.port)
                    conn.connect()
                    conn.authenticate()
                    last_target = target
                # ps5dbg.notify() packs header+payload in one send_request (protocol bug causing timeout);
                # ps5debug-NG expects 8-byte header (type, len) followed by raw NUL-terminated UTF-8 payload.
                payload = f"GTA VI Trainer\n{text}".encode("utf-8") + b"\x00"
                conn.connection.send_request(CMD_CONSOLE_NOTIFY, struct.pack("<II", 222, len(payload)))
                conn.connection._sendall(payload)
                expect_success(conn.connection, context="CMD_CONSOLE_NOTIFY")
                last_sent = time.monotonic()
            except Exception:
                if conn is not None:
                    try:
                        conn.close()
                    except Exception:
                        pass
                    conn = None
        if conn is not None:
            try:
                conn.close()
            except Exception:
                pass

    def _save_session_cache(self) -> None:
        try:
            payload = {
                "pid": self.pid,
                "base": self.base,
                "money_candidates": self.money_candidates,
            }
            self.cache_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        except Exception:
            pass

    def _load_session_cache(self) -> bool:
        if not self.cache_path.exists():
            return False
        try:
            data = json.loads(self.cache_path.read_text(encoding="utf-8"))
            if data.get("pid") == self.pid and data.get("base") == self.base:
                addrs = [int(a) for a in data.get("money_candidates", [])]
                if addrs:
                    # Quick read check to verify memory is still valid
                    _ = self._read_i32(addrs[0])
                    self.money_candidates = addrs
                    self.money_mode = "locked"
                    self.money_watch_note = (
                        f"Portfel przechwycony z pamięci sesji ({len(addrs)} adresy)."
                    )
                    self._log(f"Restored {len(addrs)} cached money addresses for PID={self.pid}")
                    return True
        except Exception:
            pass
        return False

    def _proc_write(self, addr: int, data: bytes) -> None:
        assert self.ps5 is not None
        conn = self.ps5.connection
        conn.send_request(CMD_PROC_WRITE, struct.pack("<IQI", self.pid, addr, len(data)))
        expect_success(conn, context="CMD_PROC_WRITE (ack)")
        conn._sendall(data)
        expect_success(conn, context="CMD_PROC_WRITE (final)")

    def _proc_protect(self, addr: int, length: int, prot: int) -> None:
        assert self.ps5 is not None
        conn = self.ps5.connection
        conn.send_request(CMD_PROC_PROTECT, struct.pack("<IQII", self.pid, addr, length, prot))
        expect_success(conn, context="CMD_PROC_PROTECT")

    def _game_call(
        self,
        fn: int,
        rdi: int = 0,
        rsi: int = 0,
        rdx: int = 0,
        rcx: int = 0,
        r8: int = 0,
        r9: int = 0,
        arg7: int = 0,
        arg8: int = 0,
        timeout: float = 3.0,
    ) -> int:
        """Execute fn(...) safely on the game's main thread via GOT_scePadReadState hook."""
        assert self.ps5 is not None
        if not self.hook_installed or not self.ctrl_addr:
            raise RuntimeError("Hook na głównym wątku gry nie jest zainstalowany.")

        # Ensure hook is idle
        st = self._read_u32(self.ctrl_addr + 0x08)
        if st == 3:
            self._proc_write(self.ctrl_addr + 0x08, struct.pack("<I", 0))
        elif st != 0:
            raise RuntimeError(f"Poprzednie wywołanie jeszcze trwa (state={st}).")

        # Write fn + 8 arguments at +0x10..+0x57
        args_blob = struct.pack("<9Q", fn, rdi, rsi, rdx, rcx, r8, r9, arg7, arg8)
        self._proc_write(self.ctrl_addr + 0x10, args_blob)
        # Trigger execution on main game thread
        self._proc_write(self.ctrl_addr + 0x08, struct.pack("<I", 1))

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            st = self._read_u32(self.ctrl_addr + 0x08)
            if st == 3:
                res = self._read_u64(self.ctrl_addr + 0x58)
                self._proc_write(self.ctrl_addr + 0x08, struct.pack("<I", 0))
                return res
            time.sleep(0.015)

        raise RuntimeError("Timeout oczekiwania na główny wątek gry (czy gra nie jest zapauzowana/zawieszona?).")

    def _install_pad_hook(self) -> int:
        """Install GOT hook on scePadReadState so all engine calls run on the game thread."""
        assert self.ps5 is not None
        slot = self.base + OFF_GOT_PAD_READ_STATE
        orig = self._read_u64(slot)
        if not orig:
            raise RuntimeError("Slot GOT_scePadReadState jest pusty.")

        # Reuse existing hook if we reconnected without unhooking
        try:
            head = self.ps5.read(self.pid, orig, 8)
            if head[:7] == bytes.fromhex("5341544883ec28"):
                # Extract ctrl_addr from mov rbx, imm64 at offset 9
                ctrl = struct.unpack("<Q", self.ps5.read(self.pid, orig + 9, 8))[0]
                self.ctrl_addr = ctrl
                self.hook_addr = orig
                self.orig_pad = self._read_u64(ctrl)
                self.hook_installed = True
                return self._read_u32(ctrl + 0x0C)
        except Exception:
            pass

        ctrl_addr = self.ps5.alloc(self.pid, 0x4000)
        hook_addr = self.ps5.alloc(self.pid, 0x4000)

        # Initialize control block (orig_pad, state=0, tick_count=0)
        self._proc_write(ctrl_addr, struct.pack("<QII", orig, 0, 0) + b"\x00" * 0x50)

        # Write hook shellcode and mark RX if not already executable
        code = build_pad_hook(ctrl_addr)
        self._proc_write(hook_addr, code)
        hook_map = next(
            (m for m in self.ps5.maps(self.pid) if m.start <= hook_addr < m.end),
            None,
        )
        if not hook_map or not (hook_map.prot & 4):
            try:
                self._proc_protect(hook_addr, 0x4000, 5)  # PROT_READ | PROT_EXEC
            except Exception:
                self._proc_protect(hook_addr, 0x4000, 7)  # PROT_READ | PROT_WRITE | PROT_EXEC

        # Make GOT page writable if needed, then patch slot
        got_page = slot & ~0x3FFF
        try:
            self._proc_protect(got_page, 0x4000, 3)  # PROT_READ | PROT_WRITE
        except Exception:
            pass
        self._proc_write(slot, struct.pack("<Q", hook_addr))

        self.ctrl_addr = ctrl_addr
        self.hook_addr = hook_addr
        self.orig_pad = orig
        self.hook_installed = True

        # Wait briefly to verify game thread is ticking the hook
        t0 = self._read_u32(ctrl_addr + 0x0C)
        deadline = time.monotonic() + 1.0
        while time.monotonic() < deadline:
            t1 = self._read_u32(ctrl_addr + 0x0C)
            if t1 > t0:
                return t1
            time.sleep(0.05)
        return self._read_u32(ctrl_addr + 0x0C)

    def _uninstall_pad_hook(self) -> None:
        if self.ps5 is not None and self.hook_installed and self.orig_pad and self.base:
            try:
                slot = self.base + OFF_GOT_PAD_READ_STATE
                self._proc_write(slot, struct.pack("<Q", self.orig_pad))
            except Exception:
                pass
        self.hook_installed = False
        self.ctrl_addr = 0
        self.hook_addr = 0
        self.orig_pad = 0

    def _read_u32(self, addr: int) -> int:
        assert self.ps5 is not None
        return struct.unpack("<I", self.ps5.read(self.pid, addr, 4))[0]

    def _read_i32(self, addr: int) -> int:
        assert self.ps5 is not None
        return struct.unpack("<i", self.ps5.read(self.pid, addr, 4))[0]

    def _read_u64(self, addr: int) -> int:
        assert self.ps5 is not None
        return struct.unpack("<Q", self.ps5.read(self.pid, addr, 8))[0]

    def disconnect(self) -> None:
        with self.lock:
            self._uninstall_pad_hook()
            if self.ps5 is not None:
                try:
                    self.ps5.close()
                except Exception:
                    pass
            self.ps5 = None
            self.pid = 0
            self.base = 0
            self.fw_str = ""
            self.kstuff_active = False
            self.etahen_active = False
            self.version_ok = False
            self.money_authed = False
            self.money_candidates = []
            self.money_mode = "idle"
            self.money_watch_baseline = {}
            self.money_watch_note = "Rozłączony."
            self._money_cancel_event.set()

    def connect(
        self, host: str | None = None, port: int | None = None, auto_watch: bool = True
    ) -> dict[str, Any]:
        with self.lock:
            if host:
                self.host = host
            if port:
                self.port = port
            self.disconnect()
            self.last_error = ""

            ps5 = PS5Debug(self.host, port=self.port)
            ps5.connect()
            self.ps5 = ps5

            try:
                fw = ps5.fw_version()
                self.fw_str = f"{fw // 100}.{fw % 100:02d}" if fw else ""
            except Exception:
                self.fw_str = ""
            try:
                pnames = [p.name.lower() for p in ps5.procs()]
                self.kstuff_active = any("kstuff" in n for n in pnames)
                self.etahen_active = any("etahen" in n for n in pnames)
            except Exception:
                self.kstuff_active = False
                self.etahen_active = False

            fg = ps5.foreground_app()
            if fg and fg.pid > 0:
                self.pid = fg.pid
                self.titleid = fg.titleid
                self.app_ver = fg.app_ver
            else:
                procs = ps5.procs()
                match = next((p for p in procs if "eboot.bin" in p.name.lower()), None)
                if not match:
                    raise RuntimeError("Nie znaleziono uruchomionej gry (eboot.bin).")
                self.pid = match.pid
                self.titleid = "PPSA38500?"
                self.app_ver = "unknown"

            maps = ps5.maps(self.pid)
            eboot_maps = [
                m
                for m in maps
                if (
                    "eboot" in m.name.lower()
                    or ("executable" in m.name.lower() and m.start < 0x100000000)
                )
            ]
            if not eboot_maps:
                raise RuntimeError("Nie znaleziono segmentu eboot.bin w mapach procesu.")
            self.base = min(m.start for m in eboot_maps)

            # Verify CheatTable pointers match PPSA38500 01.002.001
            table_bytes = ps5.read(self.pid, self.base + OFF_CHEAT_TABLE, 16 * 8)
            ptrs = struct.unpack("<16Q", table_bytes)
            self.version_ok = (
                ptrs[2] == self.base + OFF_CHEAT_HEAL_REPAIR
                and ptrs[3] == self.base + OFF_CHEAT_WANTED_UP
                and ptrs[4] == self.base + OFF_CHEAT_WANTED_DOWN
                and ptrs[6] == self.base + OFF_CHEAT_WEATHER
            )

            if self.version_ok:
                self._install_pad_hook()
                self._log(
                    f"Connected PID={self.pid} {self.titleid} {self.app_ver} FW={self.fw_str} "
                    f"base={self.base:#x} hook={self.hook_installed}"
                )
                if not self._load_session_cache():
                    self.money_mode = "idle"
                    self.money_watch_note = "Połączono. Wybierz tryb wykrywania portfela poniżej."

            return self.status()

    def _require_ready(self) -> None:
        if self.ps5 is None or self.pid <= 0 or self.base <= 0:
            raise RuntimeError("Brak połączenia z PS5. Kliknij najpierw Połącz.")
        if not self.version_ok:
            raise RuntimeError(
                f"Niezgodna wersja gry ({self.titleid} {self.app_ver}). "
                "Wymagana: PPSA38500 01.002.001."
            )
        if not self.hook_installed:
            raise RuntimeError("Hook na głównym wątku gry nie jest aktywny. Połącz się ponownie.")

    def _get_ped_and_handle(self) -> tuple[int, int]:
        ped = self._read_u64(self.base + OFF_LOCAL_PLAYER_PED)
        if not ped:
            return 0, 0
        handle = self._read_u32(ped + 0x9C)
        return ped, handle

    def _read_wanted(self, ped: int, handle: int) -> int:
        if not ped or (((handle >> 17) & 0x1F) != 3):
            return -1
        comp = self._game_call(self.base + OFF_GET_PLAYER_COMP, handle)
        if not comp:
            return -1
        return self._read_i32(comp + OFF_WANTED_LEVEL)

    def status(self) -> dict[str, Any]:
        acquired = self.lock.acquire(blocking=False)
        if not acquired:
            return {
                "connected": self.ps5 is not None and self.pid > 0,
                "host": self.host,
                "port": self.port,
                "pid": self.pid,
                "base": hex(self.base) if self.base else None,
                "titleid": self.titleid,
                "app_ver": self.app_ver,
                "trainer_version": "Python v1.1.4",
                "is_elf": False,
                "fw": self.fw_str,
                "kstuff": self.kstuff_active,
                "etahen": self.etahen_active,
                "hook": self.hook_installed,
                "version_ok": self.version_ok,
                "god_mode": self.god_mode,
                "never_wanted": self.never_wanted,
                "freeze_time": self.freeze_time,
                "ps5_notify": self.ps5_notify,
                "frozen_time": f"{self.frozen_h:02d}:{self.frozen_m:02d}",
                "hour": -1,
                "minute": -1,
                "wanted": -1,
                "money": -1,
                "money_ready": len(self.money_candidates) > 0,
                "money_mode": self.money_mode,
                "money_watch_note": self.money_watch_note,
                "money_addrs": [hex(a) for a in self.money_candidates[:4]],
                "lektor_enabled": self.lektor_enabled,
                "lektor_mac_say": self.lektor_mac_say,
                "lektor_read_speaker": self.lektor_read_speaker,
                "lektor_rate": self.lektor_rate,
                "subtitle_seq": self.subtitle_seq,
                "subtitle_current": self.subtitle_current,
                "subtitle_history": self.subtitle_history[:25],
                "ped": None,
                "in_vehicle": False,
                "last_error": self.last_error,
            }
        try:
            connected = self.ps5 is not None and self.pid > 0
            data: dict[str, Any] = {
                "connected": connected,
                "host": self.host,
                "port": self.port,
                "pid": self.pid,
                "base": hex(self.base) if self.base else None,
                "titleid": self.titleid,
                "app_ver": self.app_ver,
                "trainer_version": "Python v1.1.4",
                "is_elf": False,
                "fw": self.fw_str,
                "kstuff": self.kstuff_active,
                "etahen": self.etahen_active,
                "hook": self.hook_installed,
                "version_ok": self.version_ok,
                "god_mode": self.god_mode,
                "never_wanted": self.never_wanted,
                "freeze_time": self.freeze_time,
                "ps5_notify": self.ps5_notify,
                "frozen_time": f"{self.frozen_h:02d}:{self.frozen_m:02d}",
                "hour": -1,
                "minute": -1,
                "wanted": -1,
                "money": -1,
                "money_ready": len(self.money_candidates) > 0,
                "money_mode": self.money_mode,
                "money_watch_note": self.money_watch_note,
                "money_addrs": [hex(a) for a in self.money_candidates[:4]],
                "lektor_enabled": self.lektor_enabled,
                "lektor_mac_say": self.lektor_mac_say,
                "lektor_read_speaker": self.lektor_read_speaker,
                "lektor_rate": self.lektor_rate,
                "subtitle_seq": self.subtitle_seq,
                "subtitle_current": self.subtitle_current,
                "subtitle_history": self.subtitle_history[:25],
                "ped": None,
                "in_vehicle": False,
                "last_error": self.last_error,
            }
            if connected and self.version_ok and self.ps5 is not None:
                try:
                    clock_raw = self.ps5.read(self.pid, self.base + OFF_GAME_CLOCK, 24)
                    ints = struct.unpack("<6i", clock_raw)
                    data["hour"] = ints[4]
                    data["minute"] = ints[5]
                    ped, handle = self._get_ped_and_handle()
                    data["ped"] = hex(ped) if ped else None
                    veh = self._read_u64(self.base + OFF_LOCAL_PLAYER_VEH)
                    data["in_vehicle"] = bool(veh)
                    if ped:
                        data["wanted"] = self._read_wanted(ped, handle)
                    if self.money_candidates:
                        try:
                            data["money"] = self._read_i32(self.money_candidates[0])
                        except Exception:
                            pass
                except Exception as exc:
                    data["last_error"] = str(exc)
            return data
        finally:
            self.lock.release()

    def heal_repair(self) -> str:
        with self.lock:
            self._require_ready()
            ped, handle = self._get_ped_and_handle()
            if not ped:
                raise RuntimeError("Nie znaleziono gracza (wejdź do Story Mode).")
            self._game_call(self.base + OFF_CHEAT_HEAL_REPAIR)
            self._game_call(self.base + OFF_HEALTH_RESTORE, handle)
            veh = self._read_u64(self.base + OFF_LOCAL_PLAYER_VEH)
            if veh:
                self._game_call(self.base + OFF_VEHICLE_FIX, veh)
                return "Uleczono gracza, dodano pancerz i naprawiono pojazd."
            return "Uleczono gracza i dodano pancerz."

    def heal_only(self) -> str:
        with self.lock:
            self._require_ready()
            ped, handle = self._get_ped_and_handle()
            if not ped:
                raise RuntimeError("Nie znaleziono gracza (wejdź do Story Mode).")
            self._game_call(self.base + OFF_HEALTH_RESTORE, handle)
            return "Uleczono gracza (100% HP)."

    def armor_only(self) -> str:
        with self.lock:
            self._require_ready()
            ped, _ = self._get_ped_and_handle()
            if not ped:
                raise RuntimeError("Nie znaleziono gracza (wejdź do Story Mode).")
            self._game_call(self.base + OFF_CHEAT_HEAL_REPAIR)
            return "Dodano pełny pancerz."

    def repair_car(self) -> str:
        with self.lock:
            self._require_ready()
            veh = self._read_u64(self.base + OFF_LOCAL_PLAYER_VEH)
            if not veh:
                raise RuntimeError("Nie siedzisz w żadnym pojeździe.")
            self._game_call(self.base + OFF_VEHICLE_FIX, veh)
            return "Naprawiono pojazd."

    def set_wanted(self, level: int) -> str:
        with self.lock:
            self._require_ready()
            level = max(0, min(5, int(level)))
            ped, _ = self._get_ped_and_handle()
            if not ped:
                raise RuntimeError("Nie znaleziono gracza (wejdź do Story Mode).")
            self._game_call(self.base + OFF_SET_PLAYER_WANTED, ped, level, 1, 1, 1)
            return f"Ustawiono Wanted Level na {level}."

    def wanted_delta(self, up: bool) -> str:
        with self.lock:
            self._require_ready()
            fn = OFF_CHEAT_WANTED_UP if up else OFF_CHEAT_WANTED_DOWN
            self._game_call(self.base + fn)
            return "Wanted +1" if up else "Wanted -1"

    def next_weather(self) -> str:
        with self.lock:
            self._require_ready()
            self._game_call(self.base + OFF_CHEAT_WEATHER)
            return "Zmieniono pogodę na następną."

    def set_time(self, hour: int, minute: int = 0) -> str:
        with self.lock:
            self._require_ready()
            hour = max(0, min(23, int(hour)))
            minute = max(0, min(59, int(minute)))
            self._game_call(self.base + OFF_CLOCK_SET_TIME, hour, minute, 0)
            self.frozen_h = hour
            self.frozen_m = minute
            return f"Ustawiono godzinę na {hour:02d}:{minute:02d}."

    def set_toggle(self, name: str, enabled: bool) -> str:
        with self.lock:
            if name == "ps5_notify":
                self.ps5_notify = enabled
                if enabled:
                    self._notify_ps5("Powiadomienia PS5: WŁĄCZONE")
                return f"Powiadomienia na ekranie PS5: {'WŁĄCZONE' if enabled else 'WYŁĄCZONE'}"
            if name == "lektor_enabled":
                self.lektor_enabled = enabled
                if not enabled:
                    self._stop_say()
                return f"Lektor PL (nasłuch napisów): {'WŁĄCZONY' if enabled else 'WYŁĄCZONY'}"
            if name == "lektor_mac_say":
                self.lektor_mac_say = enabled
                if not enabled:
                    self._stop_say()
                return f"Lektor na Macu (Zosia): {'WŁĄCZONY' if enabled else 'WYŁĄCZONY'}"
            if name == "lektor_read_speaker":
                self.lektor_read_speaker = enabled
                return f"Czytanie imienia postaci: {'WŁĄCZONE' if enabled else 'WYŁĄCZONE'}"
            if name == "god_mode":
                self.god_mode = enabled
                if enabled and self.ps5 and self.version_ok and self.hook_installed:
                    try:
                        ped, handle = self._get_ped_and_handle()
                        if ped:
                            self._game_call(self.base + OFF_CHEAT_HEAL_REPAIR)
                            self._game_call(self.base + OFF_HEALTH_RESTORE, handle)
                    except Exception:
                        pass
                return f"God Mode (Auto-Heal): {'WŁĄCZONE' if enabled else 'WYŁĄCZONE'}"
            if name == "never_wanted":
                self.never_wanted = enabled
                if enabled and self.ps5 and self.version_ok and self.hook_installed:
                    try:
                        ped, _ = self._get_ped_and_handle()
                        if ped:
                            self._game_call(self.base + OFF_SET_PLAYER_WANTED, ped, 0, 1, 1, 1)
                    except Exception:
                        pass
                return f"Never Wanted: {'WŁĄCZONE' if enabled else 'WYŁĄCZONE'}"
            if name == "freeze_time":
                self.freeze_time = enabled
                if enabled and self.ps5 and self.version_ok:
                    try:
                        clock_raw = self.ps5.read(self.pid, self.base + OFF_GAME_CLOCK, 24)
                        ints = struct.unpack("<6i", clock_raw)
                        self.frozen_h = ints[4]
                        self.frozen_m = ints[5]
                    except Exception:
                        pass
                return (
                    f"Freeze Time: {'WŁĄCZONE (' + f'{self.frozen_h:02d}:{self.frozen_m:02d})' if enabled else 'WYŁĄCZONE'}"
                )
            raise ValueError(f"Nieznany przełącznik: {name}")


    def _background_loop(self) -> None:
        last_wanted = 0.0
        last_time = 0.0
        last_money_poll = 0.0
        while not self._stop_event.is_set():
            time.sleep(0.25)
            watching_money = self.money_mode == "auto_watching" and bool(self.money_watch_baseline)
            if not (self.god_mode or self.never_wanted or self.freeze_time or watching_money):
                continue
            with self.lock:
                if self.ps5 is None or not self.version_ok or not self.hook_installed:
                    continue
                try:
                    now = time.monotonic()
                    if self.god_mode:
                        ped, handle = self._get_ped_and_handle()
                        if ped:
                            self._game_call(self.base + OFF_HEALTH_RESTORE, handle)
                            veh = self._read_u64(self.base + OFF_LOCAL_PLAYER_VEH)
                            if veh:
                                self._game_call(self.base + OFF_VEHICLE_FIX, veh)
                    if self.never_wanted and (now - last_wanted) >= 1.0:
                        last_wanted = now
                        ped, handle = self._get_ped_and_handle()
                        if ped:
                            wanted = self._read_wanted(ped, handle)
                            if wanted > 0:
                                self._game_call(
                                    self.base + OFF_SET_PLAYER_WANTED, ped, 0, 1, 1, 1
                                )
                    if self.freeze_time and (now - last_time) >= 0.5:
                        last_time = now
                        self._game_call(
                            self.base + OFF_CLOCK_SET_TIME, self.frozen_h, self.frozen_m, 0
                        )
                    if (
                        self.money_mode == "auto_watching"
                        and self.money_watch_baseline
                        and (now - last_money_poll) >= 1.5
                    ):
                        last_money_poll = now
                        changed: list[tuple[int, int]] = []
                        for addr, old_val in list(self.money_watch_baseline.items()):
                            try:
                                cur_val = self._read_i32(addr)
                                if cur_val != old_val and 0 <= cur_val <= 2_000_000_000:
                                    changed.append((addr, cur_val))
                            except Exception:
                                pass
                        if changed:
                            by_val: dict[int, list[int]] = {}
                            for addr, val in changed:
                                by_val.setdefault(val, []).append(addr)
                            best_val, stat_hits = max(by_val.items(), key=lambda kv: len(kv[1]))
                            if stat_hits:
                                # Keep in auto_scanning until Master + HUD addresses are locked so +$1M always works on first click!
                                self.money_mode = "auto_scanning"
                                self.money_watch_note = (
                                    f"Wykryto ${best_val:,}! Blokuję Master Wallet i HUD (~8s)..."
                                ).replace(",", " ")
                                self._log(
                                    f"Auto-detected money=${best_val} stat_addrs={[hex(a) for a in stat_hits]}; scanning Master+HUD..."
                                )
                                h_host, h_port, h_pid, h_val = self.host, self.port, self.pid, best_val
                                h_stats = list(stat_hits)

                                def _enrich_hud() -> None:
                                    try:
                                        master_hits = self._scan_master_wallet_dedicated(
                                            h_host, h_port, h_pid, h_val
                                        )
                                        hud_hits = self._scan_hud_wallet_pairs_dedicated(
                                            h_host, h_port, h_pid, h_val
                                        )
                                        extra = master_hits + hud_hits
                                        with self.lock:
                                            if (
                                                self._money_cancel_event.is_set()
                                                or self.pid != h_pid
                                                or self.money_mode != "auto_scanning"
                                            ):
                                                return
                                            merged = list(dict.fromkeys(extra + h_stats))
                                            self.money_candidates = merged
                                            self.money_mode = "locked"
                                            self.money_watch_note = (
                                                f"Portfel gotowy (${h_val:,}, {len(merged)} adresy: Master+HUD+Stats)!"
                                            ).replace(",", " ")
                                            self._save_session_cache()
                                            self._log(
                                                f"Auto-money locked {len(merged)} addrs={[hex(a) for a in merged]}"
                                            )
                                            self._notify_ps5(
                                                f"Portfel gotowy (${h_val:,}, {len(merged)} adr.)!".replace(",", " ")
                                            )
                                    except Exception:
                                        pass

                                threading.Thread(target=_enrich_hud, daemon=True).start()
                except Exception as exc:
                    self.last_error = str(exc)


