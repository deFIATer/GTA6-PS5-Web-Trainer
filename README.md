# GTA VI PS5 Web Trainer (PS5Debug-NG)

Responsive Web / PWA trainer for **GTA VI (`PPSA38500 01.002.001`)** on jailbroken **PS5** (tested on **FW 13.60** with `ps5debug-NG` on port `744`).  
Works on **PC, Mac, Linux, Tablet, and Phone** over Wi-Fi — **English (default) & Polish (`🌐 EN / PL` toggle)**, Light & Dark mode.

---

## Features

- **Main-thread GOT Hook** (`scePadReadState`) — executes RAGE engine functions safely on the game's main thread.
- **Player**: All-in-One (Heal + Armor + Repair), separate Heal / Armor / Repair, **God Mode** (auto-heal & auto-repair), **Never Wanted**, Wanted Level 0–5 & +/-1, **PS5 On-Screen Notifications** (3s paced dedicated TCP channel).
- **Vehicles (254)**: Categorized using GTA Fandom Wiki names, instant search, Quick Spawn, and **Available in Game Only** live filter.
- **World**: Time presets (Morning, Noon, Sunset, Midnight), custom time, **Freeze Time**, **Next Weather**.
- **Money (experimental)**: Manual (exact cash) & Auto-Detect modes on dedicated TCP sockets.
- **Live Subtitle Voiceover (100% Local, No API)**: Reads live in-game subtitles from PS5 RAM with FIFO queue (macOS `say` and/or Browser/Phone Web Speech API).

---

## Quick Start

### Option A · Standalone PS5 Payload (`.elf`) + ShadowMount Tile (`PPSA38599`) — **NO PC NEEDED!**
You can run the entire Web Trainer **directly inside your PS5** (with full DualSense gamepad navigation):
1. Make sure **ps5debug-NG** (port `744`) is running on your PS5.
2. Copy the **`PPSA38599`** folder to **`/data/homebrew/PPSA38599`** on your PS5 (to mount it as a Home Screen tile with **ShadowMount**), and send [`elf_trainer/GTA6WebTrainer.elf`](elf_trainer/GTA6WebTrainer.elf) to port `9021` or `/data/etaHEN/payloads/`.
   - *One-click FTP installer:* `python3 install_to_ps5.py --ip <PS5_IP>`
3. Launch **`GTA6WebTrainer.elf`** (it auto-connects to GTA VI in the background every 3s).
4. Open the **GTA VI Web Trainer** tile (`PPSA38599`) on your PS5 — or open **`http://127.0.0.1:8080`** in the PS5 browser / **`http://<PS5_IP>:8080`** on your phone!

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

- **NOWOŚĆ · Bezpośrednio na PS5 (bez komputera!)**:
  1. Wrzuć folder **`PPSA38599`** na PS5 do **`/data/homebrew/PPSA38599`** (do zamontowania jako kafelek przez **ShadowMount**).
  2. Załaduj [`elf_trainer/GTA6WebTrainer.elf`](elf_trainer/GTA6WebTrainer.elf) przez port `9021` lub `/data/etaHEN/payloads/` *(lub użyj automatycznego instalatora: `python3 install_to_ps5.py --ip <IP_PS5>`)*.
  3. Kliknij kafelek **GTA VI Web Trainer** na PS5 (obsługa padem DualSense!) albo otwórz `http://<IP_PS5>:8080` na telefonie.
- **macOS / Linux**: uruchom `./start_mac_linux.sh`
- **Windows**: kliknij dwukrotnie `start_windows.bat`
- Otwórz `http://localhost:8080`, wpisz IP swojej PS5 i kliknij **Połącz z PS5**.

---

## Credits

- Game offsets (`PPSA38500 01.002.001`) & vehicle list (`Vehicles.h`) based on **[basic-menu-src](https://github.com/OnlyMoisties/basic-menu-src)** by **XeMo**.
- Vehicle display names & categories: **GTA Fandom Wiki**.
