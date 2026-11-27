"""Dedicated-channel wallet scanner (Manual + Auto modes) and cash read/write."""

from __future__ import annotations

import struct
import threading
from typing import Any

from ps5dbg import PS5Debug
from ps5dbg.constants import ScanCompareType, ScanValueType
from ps5dbg.protocol import proc_scan_start


class MoneyMixin:
    """Mixin providing PS5 RAM wallet detection and modification."""

    def _scan_stat_entries_dedicated(self, host: str, port: int, pid: int) -> dict[int, int]:
        """Dedicated-socket scan of 0x1000000000..0x1060000000 for 0x000000011E720953 (never holds self.lock)."""
        scan_ps5 = PS5Debug(host, port=port)
        scan_ps5.connect()
        try:
            scan_ps5.authenticate()
            maps = scan_ps5.maps(pid)
            snap: dict[int, int] = {}
            sig = 0x000000011E720953
            for m in maps:
                if not (
                    (m.prot & 3) == 3
                    and not (m.prot & 4)
                    and m.name == ""
                    and 0x1000000000 <= m.start < 0x1060000000
                    and (m.end - m.start) >= 0x100000
                ):
                    continue
                addr = m.start
                while addr < m.end:
                    length = min(0x10000000, m.end - addr)
                    res = proc_scan_start(
                        scan_ps5.connection,
                        pid,
                        addr,
                        length,
                        int(ScanValueType.UINT64),
                        int(ScanCompareType.EXACT_VALUE),
                        4,
                        sig,
                    )
                    for off, _ in res:
                        val_addr = addr + off + 8
                        try:
                            snap[val_addr] = struct.unpack("<i", scan_ps5.read(pid, val_addr, 4))[0]
                        except Exception:
                            pass
                    addr += length
            return snap
        finally:
            try:
                scan_ps5.close()
            except Exception:
                pass

    def _scan_hud_wallet_pairs_dedicated(self, host: str, port: int, pid: int, amount: int) -> list[int]:
        """Dedicated-socket scan of 0x1100000000..0x1200000000 for live wallet/HUD pairs (never holds self.lock)."""
        scan_ps5 = PS5Debug(host, port=port)
        scan_ps5.connect()
        try:
            scan_ps5.authenticate()
            maps = scan_ps5.maps(pid)
            # Match [0x00010000, amount] instead of [amount, amount] — vastly fewer false hits for small amounts like 16!
            pat_u64 = 0x00010000 | ((amount & 0xFFFFFFFF) << 32)
            found: list[int] = []
            # Active HUD wallet pairs ALWAYS live in 0x1100000000..0x1200000000 (8-byte aligned pre=0x00010000)
            cand_maps = [
                m
                for m in maps
                if (
                    (m.prot & 3) == 3
                    and not (m.prot & 4)
                    and m.name == ""
                    and 0x1100000000 <= m.start < 0x1200000000
                    and (m.end - m.start) >= 0x100000
                )
            ]
            for m in cand_maps:
                if self._money_cancel_event.is_set():
                    break
                addr = m.start
                while addr < m.end:
                    if self._money_cancel_event.is_set():
                        break
                    length = min(0x10000000, m.end - addr)
                    res = proc_scan_start(
                        scan_ps5.connection,
                        pid,
                        addr,
                        length,
                        int(ScanValueType.UINT64),
                        int(ScanCompareType.EXACT_VALUE),
                        8,
                        pat_u64,
                    )
                    for off, _ in res:
                        if self._money_cancel_event.is_set():
                            break
                        a = addr + off
                        try:
                            ctx = scan_ps5.read(pid, a, 24)
                            pre, v1, v2, c1, c2, c3 = struct.unpack("<IiiIII", ctx)
                            if (
                                pre == 0x00010000
                                and v1 == amount
                                and v2 == amount
                                and c2 == 5
                                and c3 == 10
                            ):
                                found.append(a + 4)
                                found.append(a + 8)
                        except Exception:
                            pass
                    addr += length
            return found
        finally:
            try:
                scan_ps5.close()
            except Exception:
                pass

    def _scan_master_wallet_dedicated(
        self, host: str, port: int, pid: int, amount: int
    ) -> list[int]:
        """Scan master RAGEVM/script wallet copies in 0xf0.. (13MB), 0xfa.. (20MB) and 0x11.. (tag 0x6F0A8D01)."""
        scan_ps5 = PS5Debug(host, port=port)
        scan_ps5.connect()
        try:
            scan_ps5.authenticate()
            maps = scan_ps5.maps(pid)
            found: list[int] = []
            for m in maps:
                if self._money_cancel_event.is_set():
                    break
                if (m.prot & 3) != 3 or (m.prot & 4) or m.name != "" or (m.end - m.start) < 0x10000:
                    continue
                is_f0 = 0xF000000000 <= m.start < 0xF100000000
                is_fa = 0xFA00000000 <= m.start < 0xFB00000000
                is_11 = 0x1100000000 <= m.start < 0x1200000000
                if not (is_f0 or is_fa or is_11):
                    continue
                addr = m.start
                while addr < m.end and not self._money_cancel_event.is_set():
                    length = min(0x10000000, m.end - addr)
                    if is_f0 or is_fa:
                        res = proc_scan_start(
                            scan_ps5.connection,
                            pid,
                            addr,
                            length,
                            int(ScanValueType.UINT32),
                            int(ScanCompareType.EXACT_VALUE),
                            4,
                            amount & 0xFFFFFFFF,
                        )
                        for off, _ in res:
                            a = addr + off
                            try:
                                ctx = scan_ps5.read(pid, a - 16, 24)
                                u = struct.unpack("<6I", ctx)
                                if is_f0 and (u[2] & 0x00FFFFFF) == 0x00AF2E50 and u[3] == 0x00000804:
                                    found.append(a)
                                elif is_fa and u[1] == 0x6F0A8D01:
                                    found.append(a)
                            except Exception:
                                pass
                    elif is_11:
                        # Tag 0x6F0A8D01 sits at +20 from cash value; +8 is 0x8000000000000000
                        res = proc_scan_start(
                            scan_ps5.connection,
                            pid,
                            addr,
                            length,
                            int(ScanValueType.UINT32),
                            int(ScanCompareType.EXACT_VALUE),
                            4,
                            0x6F0A8D01,
                        )
                        for off, _ in res:
                            a = addr + off - 20
                            try:
                                ctx = scan_ps5.read(pid, a, 24)
                                v, z, hi_flag = struct.unpack("<iIQ", ctx[:16])
                                if v == amount and z == 0 and hi_flag == 0x8000000000000000:
                                    found.append(a)
                            except Exception:
                                pass
                    addr += length
            return found
        finally:
            try:
                scan_ps5.close()
            except Exception:
                pass

    def start_money_auto_watch(self, expected_amount: int | None = None) -> dict[str, Any]:
        """Start explicit Auto-Detection mode (requires current cash >= 0 to avoid false positives)."""
        if expected_amount is None or str(expected_amount).strip() == "":
            raise ValueError("Enter your current cash first (0 or higher).")
        amt_filter = int(expected_amount)
        if amt_filter < 0:
            raise ValueError("Current cash must be 0 or higher.")

        with self.lock:
            self._require_ready()
            self._money_cancel_event.clear()
            self.money_candidates = []
            self.money_watch_baseline = {}
            self.money_mode = "auto_scanning"
            if amt_filter is not None:
                self.money_watch_note = (
                    f"Tryb AUTO: indeksuję statystyki z kwotą ${amt_filter:,} (ok. 10s)..."
                ).replace(",", " ")
            else:
                self.money_watch_note = "Tryb AUTO: indeksuję statystyki portfela (ok. 10s)..."
            host, port, pid = self.host, self.port, self.pid

        def worker() -> None:
            try:
                snap = self._scan_stat_entries_dedicated(host, port, pid)
                with self.lock:
                    if self._money_cancel_event.is_set() or self.pid != pid or self.money_mode != "auto_scanning":
                        return
                    total = len(snap)
                    if amt_filter is not None:
                        filtered = {a: v for a, v in snap.items() if v == amt_filter}
                        if filtered:
                            self.money_watch_baseline = filtered
                            self.money_mode = "auto_watching"
                            self.money_watch_note = (
                                f"Tryb AUTO aktywny (filtr ${amt_filter:,}: {len(filtered)} z {total} statystyk) — "
                                "zdobądź lub wydaj w grze choćby $1, a portfel przechwyci się sam!"
                            ).replace(",", " ")
                            self._log(
                                f"Auto-money watcher armed with {len(filtered)}/{total} entries matching ${amt_filter}"
                            )
                            return
                        # Fallback to all entries if none matched exact amount
                        self.money_watch_baseline = snap
                        self.money_mode = "auto_watching"
                        self.money_watch_note = (
                            f"Tryb AUTO aktywny (brak dokładnej statystyki=${amt_filter:,}, obserwuję wszystkie {total}) — "
                            "zdobądź lub wydaj w grze choćby $1!"
                        ).replace(",", " ")
                        self._log(
                            f"Auto-money watcher fallback: 0 matched ${amt_filter}, watching all {total} entries"
                        )
                        return

                    self.money_watch_baseline = snap
                    self.money_mode = "auto_watching"
                    self.money_watch_note = (
                        f"Tryb AUTO aktywny ({total} statystyk) — zdobądź lub wydaj w grze choćby $1, "
                        "a portfel przechwyci się sam!"
                    )
                    self._log(f"Auto-money watcher armed with {total} stat entries")
            except Exception as exc:
                with self.lock:
                    if self.money_mode == "auto_scanning":
                        self.money_mode = "idle"
                        self.money_watch_note = f"Błąd trybu AUTO: {exc}"
                    self._log(f"Auto-money watcher init error: {exc}")

        threading.Thread(target=worker, daemon=True).start()
        return {"message": "Włączono tryb Auto-wykrycie portfela."}

    def money_find_auto(self, current_amount: int) -> dict[str, Any]:
        """Explicit Manual mode: scans by exact amount in background so it can be cancelled cleanly."""
        with self.lock:
            self._require_ready()
            current_amount = int(current_amount)
            if current_amount <= 0:
                raise ValueError("W trybie ręcznym wpisz dokładną kwotę > 0 (dla $0 użyj trybu Auto-wykrycie).")
            self._money_cancel_event.clear()
            self.money_candidates = []
            self.money_watch_baseline = {}
            self.money_mode = "manual_scanning"
            self.money_watch_note = f"Tryb RĘCZNY: szukam w pamięci kwoty ${current_amount:,} (ok. 12s)...".replace(",", " ")
            host, port, pid = self.host, self.port, self.pid

        def worker() -> None:
            try:
                matched = self._scan_master_wallet_dedicated(host, port, pid, current_amount)
                if self._money_cancel_event.is_set():
                    return
                matched.extend(self._scan_hud_wallet_pairs_dedicated(host, port, pid, current_amount))
                if self._money_cancel_event.is_set():
                    return
                stat_snap = self._scan_stat_entries_dedicated(host, port, pid)
                if self._money_cancel_event.is_set():
                    return
                for val_addr, val in stat_snap.items():
                    if val == current_amount:
                        matched.append(val_addr)

                seen: set[int] = set()
                unique = []
                for a in matched:
                    if a not in seen:
                        seen.add(a)
                        unique.append(a)

                with self.lock:
                    if self._money_cancel_event.is_set() or self.pid != pid or self.money_mode != "manual_scanning":
                        return
                    if not unique:
                        self.money_mode = "idle"
                        self.money_watch_note = (
                            f"Nie znaleziono kwoty ${current_amount:,}. Sprawdź kwotę na ekranie lub użyj trybu AUTO."
                        ).replace(",", " ")
                        self._log(f"Manual money find: 0 hits for ${current_amount}")
                        return
                    self.money_candidates = unique
                    self.money_mode = "locked"
                    self.money_watch_note = (
                        f"Portfel przechwycony ({len(unique)} adresy): {', '.join(hex(a) for a in unique[:4])}"
                    )
                    self._save_session_cache()
                    self._log(
                        f"Manual money find locked {len(unique)} addrs for ${current_amount}: {[hex(a) for a in unique]}"
                    )
                    self._notify_ps5(f"Wykryto portfel (${current_amount:,})!".replace(",", " "))
            except Exception as exc:
                with self.lock:
                    if self.money_mode == "manual_scanning":
                        self.money_mode = "idle"
                        self.money_watch_note = f"Błąd skanu ręcznego: {exc}"
                    self._log(f"Manual money find error: {exc}")

        threading.Thread(target=worker, daemon=True).start()
        return {"message": f"Rozpoczęto ręczne szukanie kwoty ${current_amount:,}...".replace(",", " ")}

    def money_add(self, delta: int) -> dict[str, Any]:
        with self.lock:
            self._require_ready()
            if not self.money_candidates:
                raise RuntimeError("Najpierw wykryj portfel (tryb Ręczny lub Auto-wykrycie).")
            current = self._read_i32(self.money_candidates[0])
            new_val = max(0, min(2_000_000_000, current + int(delta)))
            raw = struct.pack("<i", new_val)
            written = 0
            for addr in self.money_candidates:
                try:
                    self._proc_write(addr, raw)
                    written += 1
                except Exception:
                    pass
            sign = "+" if delta >= 0 else ""
            return {
                "written": written,
                "money": new_val,
                "message": f"{sign}${delta:,} → teraz ${new_val:,}".replace(",", " "),
            }

    def money_set(self, amount: int, address: str | None = None) -> dict[str, Any]:
        with self.lock:
            self._require_ready()
            amount = max(0, min(2_000_000_000, int(amount)))
            targets: list[int]
            if address:
                targets = [int(address, 16)]
            else:
                if not self.money_candidates:
                    raise RuntimeError("Najpierw wykryj portfel (tryb Ręczny lub Auto-wykrycie).")
                targets = list(self.money_candidates)

            raw = struct.pack("<i", amount)
            written = 0
            for addr in targets:
                try:
                    self._proc_write(addr, raw)
                    written += 1
                except Exception:
                    pass
            return {
                "written": written,
                "money": amount,
                "message": f"Ustawiono kasę na ${amount:,}.".replace(",", " "),
            }

    def money_reset(self) -> dict[str, Any]:
        with self.lock:
            self._money_cancel_event.set()
            self.money_candidates = []
            self.money_watch_baseline = {}
            self.money_mode = "idle"
            self.money_watch_note = "Anulowano / zresetowano. Wybierz tryb Ręczny lub Auto-wykrycie."
            try:
                if self.cache_path.exists():
                    self.cache_path.unlink()
            except Exception:
                pass
            return {"count": 0, "addresses": [], "message": "Zresetowano tryb portfela."}

