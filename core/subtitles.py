"""Live GTA VI subtitle scanner (:~s~) and local FIFO Polish voiceover (macOS say)."""

from __future__ import annotations

import datetime
import queue
import re
import struct
import subprocess
import time

from ps5dbg import PS5Debug
from ps5dbg.constants import ScanCompareType, ScanValueType
from ps5dbg.protocol import proc_scan_start


class SubtitlesMixin:
    """Mixin providing live RAM subtitle scanning and FIFO macOS voiceover."""

    def _stop_say(self) -> None:
        # Drain pending queue first
        while True:
            try:
                self._say_queue.get_nowait()
            except queue.Empty:
                break
        if self._say_proc is not None and self._say_proc.poll() is None:
            try:
                self._say_proc.terminate()
            except Exception:
                pass
        self._say_proc = None

    def _say_worker_loop(self) -> None:
        """Dedicated FIFO speech queue worker for macOS 'say -v Zosia' so lines finish without cutting off."""
        while not self._stop_event.is_set():
            try:
                text = self._say_queue.get(timeout=0.4)
            except queue.Empty:
                continue
            if not text:
                continue
            try:
                proc = subprocess.Popen(
                    ["say", "-v", "Zosia", "-r", str(self.lektor_rate), text],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                self._say_proc = proc
                proc.wait()
            except Exception as exc:
                self._log(f"macOS say error: {exc}")
            finally:
                self._say_proc = None

    def speak_mac(self, text: str, interrupt: bool = False) -> None:
        if not text:
            return
        if interrupt:
            self._stop_say()
        try:
            self._say_queue.put_nowait(text)
        except queue.Full:
            # Drop oldest queued item if backlog is full, then enqueue latest
            try:
                self._say_queue.get_nowait()
            except queue.Empty:
                pass
            try:
                self._say_queue.put_nowait(text)
            except queue.Full:
                pass

    def set_lektor_rate(self, rate: int) -> str:
        self.lektor_rate = max(110, min(320, int(rate)))
        return f"Szybkość lektora (Mac): {self.lektor_rate} słów/min"

    @staticmethod
    def _parse_subtitle_chunk(raw: bytes, marker_pos: int) -> tuple[bool, str, str] | None:
        """Extract (has_clean_tag, speaker, dialogue) around a ':~s~' marker from a raw heap slice."""
        # Walk backwards from marker_pos to NUL or control byte
        start = marker_pos
        while start > 0 and raw[start - 1] >= 0x20 and raw[start - 1] != 0x7F:
            start -= 1
        started_at_nul = start > 0 and raw[start - 1] == 0x00
        # Walk forwards to NUL terminator
        end = raw.find(b"\x00", marker_pos)
        if end == -1:
            end = len(raw)
        s = raw[start:end].decode("utf-8", "ignore").strip()
        if ":~s~" not in s:
            return None
        left, right = s.split(":~s~", 1)
        has_clean_tag = False
        if "~" in left:
            # Everything up to and including the last '~' is GTA color/format tag (e.g. ~HC_0x05f59b5a~)
            has_clean_tag = "~HC_" in left or "0x" in left
            speaker = left.rsplit("~", 1)[-1].strip()
        else:
            speaker = left.strip()
            # If no '~' tag and not cleanly NUL-preceded, strip possible heap-header garbage before Capitalized name
            if not started_at_nul and speaker:
                m = re.search(r"([A-ZĄĆĘŁŃÓŚŹŻ][a-ząćęłńóśźż0-9 .'-]{1,30})$", speaker)
                if m:
                    speaker = m.group(1).strip()
        speaker = re.sub(r"^[^A-Za-zĄĆĘŁŃÓŚŹŻąćęłńóśźż0-9]+", "", speaker).strip()
        # Strip GTA tags like ~HC_0x...~, ~s~, ~r~, ~b~, ~n~ from dialogue
        tag_re = re.compile(r"~[^~]{1,32}~")
        dialogue = tag_re.sub(" ", right)
        dialogue = dialogue.replace("~", " ")
        dialogue = re.sub(r"\s+", " ", dialogue).strip()
        if len(dialogue) < 2 or len(speaker) > 40:
            return None
        return has_clean_tag, speaker, dialogue

    def _subtitle_loop(self) -> None:
        """Dedicated TCP channel that scans for live GTA VI subtitles (:~s~) without blocking trainer."""
        conn: PS5Debug | None = None
        last_target: tuple[str, int, int] | None = None
        marker_u32 = struct.unpack("<I", b":~s~")[0]
        prev_ram_texts: set[str] = set()
        recent_spoken: dict[str, float] = {}
        cooldown_sec = 25.0
        while not self._stop_event.is_set():
            if not self.lektor_enabled or self.pid <= 0 or not self.version_ok:
                prev_ram_texts.clear()
                time.sleep(0.4)
                continue
            target = (self.host, self.port, self.pid)
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
                    prev_ram_texts.clear()

                res = proc_scan_start(
                    conn.connection,
                    target[2],
                    0x1030000000,
                    0x10000000,
                    int(ScanValueType.UINT32),
                    int(ScanCompareType.EXACT_VALUE),
                    1,
                    marker_u32,
                )
                # Collect all live subtitles currently in RAM, deduplicated by normalized dialogue text,
                # preferring hits with clean GTA ~HC_...~ tags for accurate speaker names.
                current_map: dict[str, tuple[bool, str, str]] = {}
                for off, _ in res[:16]:
                    a = 0x1030000000 + off
                    pre = 64
                    raw = conn.read(target[2], max(0, a - pre), 384)
                    parsed = self._parse_subtitle_chunk(raw, pre)
                    if not parsed:
                        continue
                    has_tag, speaker, dialogue = parsed
                    norm_text = re.sub(r"\s+", " ", dialogue).strip().lower()
                    prev_best = current_map.get(norm_text)
                    if prev_best is None or (has_tag and not prev_best[0]):
                        current_map[norm_text] = (has_tag, speaker, dialogue)

                now = time.monotonic()
                # Prune old cooldown entries
                if len(recent_spoken) > 100:
                    recent_spoken = {
                        k: ts for k, ts in recent_spoken.items() if (now - ts) < cooldown_sec
                    }

                # Trigger only genuinely new subtitles not present in previous RAM snapshot and outside cooldown
                new_items = [
                    (norm, val)
                    for norm, val in current_map.items()
                    if norm not in prev_ram_texts and (now - recent_spoken.get(norm, 0.0)) >= cooldown_sec
                ]
                prev_ram_texts = set(current_map.keys())

                if new_items:
                    # Prefer clean-tagged hits first, and enqueue all genuinely new lines in order
                    new_items.sort(key=lambda item: item[1][0], reverse=True)
                    for norm, (_, speaker, dialogue) in new_items:
                        recent_spoken[norm] = now
                        self.subtitle_last_key = norm
                        self.subtitle_seq += 1
                        entry = {
                            "seq": self.subtitle_seq,
                            "time": datetime.datetime.now().strftime("%H:%M:%S"),
                            "speaker": speaker,
                            "text": dialogue,
                        }
                        self.subtitle_current = entry
                        self.subtitle_history.insert(0, entry)
                        if len(self.subtitle_history) > 40:
                            self.subtitle_history.pop()
                        self._log(f"SUBTITLE [{speaker}]: {dialogue}")
                        if self.lektor_mac_say:
                            spoken = (
                                f"{speaker}: {dialogue}"
                                if (self.lektor_read_speaker and speaker)
                                else dialogue
                            )
                            self.speak_mac(spoken, interrupt=False)
            except Exception as exc:
                self._log(f"Subtitle channel error: {exc}")
                if conn is not None:
                    try:
                        conn.close()
                    except Exception:
                        pass
                    conn = None
                time.sleep(1.5)
                continue
            time.sleep(0.35)

        if conn is not None:
            try:
                conn.close()
            except Exception:
                pass

