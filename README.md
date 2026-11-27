# GTA VI PS5 Web Trainer (PS5Debug-NG)

Responsive Web / PWA trainer for **GTA VI (`PPSA38500 01.002.001`)** on jailbroken **PS5** (tested on **FW 13.60** with `ps5debug-NG` on port `744`).  
Works on **PC, Mac, Linux, Tablet, and Phone** over Wi-Fi — **English (default) & Polish (`🌐 EN / PL` toggle)**, Light & Dark mode.

---

## Features

- **Main-thread GOT Hook** (`scePadReadState`) — executes RAGE engine functions safely on the game's main thread.
- **Player** *(Recommended)*: All-in-One (Heal + Armor + Repair), separate Heal / Armor / Repair, **God Mode** (auto-heal & auto-repair), **Never Wanted**, Wanted Level 0–5 & +/-1, **PS5 On-Screen Notifications** (3s paced dedicated TCP channel), plus **5 Native `eboot.bin` Patches**: **Unlimited Health (Native)**, **Unlimited Stamina**, **Infinite Ammo**, **Max Relationship**, and **Max Criminal Profile**.
- **GoldHEN / etaHEN Cheat JSON**: Included in [`cheats/PPSA38500_01.002.001.json`](cheats/PPSA38500_01.002.001.json) with all 11 native x86-64 code-cave cheats (Health, Stamina, Ammo, Wanted, Relationship, Criminal Profile, Time Presets 08:00/12:00/19:00/00:00, and Vehicle Auto-Repair).
- **Vehicles (252)** *(Recommended)*: Verified against live PS5 `ArchetypeMap`, categorized using GTA Fandom Wiki names, instant search, persistent **⭐ Favorites**, Quick Spawn, and **Available in Game Only** live filter.
- **World** *(Recommended)*: Time presets (Morning, Noon, Sunset, Midnight), custom time, **Freeze Time**.
- **Money (Beta / Experimental)**: Currently unreliable / may not work in `GTA6WebTrainer.elf`. A fully working Cash module will be announced and released in **`v2.0.0`** — for now, please use the **Player**, **Vehicles (Spawn)**, and **World** tabs.
- **Lektor PL (Beta)** *(Polish mode only)*: Reads live in-game subtitles from PS5 RAM with FIFO queue (macOS `say` and/or Browser/Phone Web Speech API).

---

## Quick Start

### Option A · Standalone PS5 Payload (`.elf`) — **NO PC NEEDED!**
You can run the entire Web Trainer **directly inside your PS5** (with full DualSense gamepad navigation — no folder copying or ShadowMount required!):
1. Make sure **ps5debug-NG** (port `744`) is running on your PS5.
2. Send [`elf_trainer/GTA6WebTrainer.elf`](elf_trainer/GTA6WebTrainer.elf) to your PS5 ELF loader (port `9021`) or place it in `/data/etaHEN/payloads/`.
3. On startup, **`GTA6WebTrainer.elf` automatically installs the `GTA VI Web Trainer` (`PPSA38599`) tile in the PS5 Media tab** and auto-connects to GTA VI in the background every 3s.
4. Click the **GTA VI Web Trainer** tile in **Media** on your PS5 (navigate with `L1/R1`, `D-Pad`, right-stick scroll, `X`), or open **`http://<PS5_IP>:8080`** on your phone!

### Option B · Run on PC / Mac / Linux (Full version + Local Voiceover)
### Requirements
- Jailbroken PS5 with **ps5debug-NG** running (port `744`) and **GTA VI (`PPSA38500 01.002.001`)** in Story Mode.
- **Python 3.10+** on Windows, macOS, or Linux.

### Run
- **macOS / Linux**: run `./start_mac_linux.sh` (or `python3 -m venv .venv && .venv/bin/pip install -r requirements.txt && .venv/bin/python trainer.py --ps5-ip <PS5_IP>`)
- **Windows**: double-click `start_windows.bat` (or run `python trainer.py --ps5-ip <PS5_IP>`)

Then open **`http://localhost:8080`** on your computer (or **`http://<YOUR_PC_IP>:8080`** on your phone), enter your PS5 IP, and click **Connect PS5**.

---

## Wersja polska (PL)

Web Trainer (PWA) do **GTA VI (`PPSA38500 01.002.001`)** na przerobione **PS5** (przetestowane na **FW 13.60** z `ps5debug-NG` na porcie `744`).  
Język przełączysz w prawym górnym rogu przyciskiem **`🌐 EN / PL`**.

- **NOWOŚĆ · Bezpośrednio na PS5 (bez komputera i bez kopiowania folderów!)**:
  1. Włącz `ps5debug-NG` (port `744`) na PS5 i załaduj [`elf_trainer/GTA6WebTrainer.elf`](elf_trainer/GTA6WebTrainer.elf) przez port `9021` lub `/data/etaHEN/payloads/`.
  2. **`GTA6WebTrainer.elf` sam automatycznie zainstaluje kafelek `GTA VI Web Trainer` (`PPSA38599`) w zakładce Multimedia** i co 3s będzie automatycznie łączył się z GTA VI.
  3. Kliknij kafelek **GTA VI Web Trainer** w **Multimedia** na PS5 (pełna obsługa DualSense: `L1/R1`, krzyżak, scroll prawą gałką, `X`) albo otwórz `http://<IP_PS5>:8080` na telefonie.
- **macOS / Linux**: uruchom `./start_mac_linux.sh`
- **Windows**: kliknij dwukrotnie `start_windows.bat`
- Otwórz `http://localhost:8080`, wpisz IP swojej PS5 i kliknij **Połącz z PS5**.

---

## Credits

- Game offsets (`PPSA38500 01.002.001`) & vehicle list (`Vehicles.h`) based on **[basic-menu-src](https://github.com/OnlyMoisties/basic-menu-src)** by **XeMo**.
- Vehicle display names & categories: **GTA Fandom Wiki**.
