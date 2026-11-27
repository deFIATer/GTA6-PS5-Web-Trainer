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

- **macOS / Linux**: uruchom `./start_mac_linux.sh`
- **Windows**: kliknij dwukrotnie `start_windows.bat`
- Otwórz `http://localhost:8080`, wpisz IP swojej PS5 i kliknij **Połącz z PS5**.

---

## Credits

- Game offsets (`PPSA38500 01.002.001`) & vehicle list (`Vehicles.h`) based on **[basic-menu-src](https://github.com/OnlyMoisties/basic-menu-src)** by **XeMo**.
- Vehicle display names & categories: **GTA Fandom Wiki**.
