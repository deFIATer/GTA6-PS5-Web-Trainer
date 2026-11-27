# GTA VI PS5 Web Trainer & Cheat Pack (`PPSA38500 01.002.001`)

Complete, 3-in-1 trainer & cheat suite for **GTA VI (`PPSA38500 01.002.001`)** on jailbroken **PS5** (tested on **FW 13.60** with `ps5debug-NG` on port `744` & `etaHEN`).  
**English (default) & Polish (`🌐 EN / PL` toggle)** · Light & Dark mode · Full DualSense & Mobile PWA support.

---

## 📦 What's Included in This Package (3 Ways to Use)

You can pick whichever method fits your setup best — all 3 are included in this repository and in every Release:

| Component | File / Location | Best For | Needs PC? |
| :--- | :--- | :--- | :---: |
| **1. Standalone PS5 ELF** | [`elf_trainer/GTA6WebTrainer.elf`](elf_trainer/GTA6WebTrainer.elf) | Running the full Web Trainer **directly on PS5** (auto-installs Media tile + DualSense + Phone control) | **No** |
| **2. Full PC / Mac / Linux Suite** | [`trainer.py`](trainer.py), [`start_mac_linux.sh`](start_mac_linux.sh), [`start_windows.bat`](start_windows.bat) | Full Web Trainer hosted on Windows / macOS / Linux + **100% Local Subtitle Voiceover** (`macOS say` / Web Speech API) | Yes |
| **3. Native Cheat JSON** | [`cheats/PPSA38500_01.002.001.json`](cheats/PPSA38500_01.002.001.json) | **etaHEN / GoldHEN / PS5 Cheat Manager / CheatRunner** — **33 native x86-64 cheats** directly in the PS5 cheat menu | **No** |

---

## ✨ Features

- **Main-thread GOT Hook** (`scePadReadState`) — executes RAGE engine functions safely on the game's main thread.
- **Player** *(Recommended)*:
  - All-in-One (Heal + Armor + Repair), separate Heal / Armor / Repair, **God Mode** (auto-heal & auto-repair), **Never Wanted**, Wanted Level `0–5` & `+/-1`, **PS5 On-Screen Notifications** (3s paced dedicated TCP channel).
  - **5 Native `eboot.bin` Patches** (toggleable ON/OFF safely): **Unlimited Health (Native)**, **Unlimited Stamina**, **Infinite Ammo**, **Max Relationship**, **Max Criminal Profile**.
- **Vehicles (252)** *(Recommended)*: Verified against live PS5 `ArchetypeMap`, categorized using GTA Fandom Wiki names, instant search, persistent **⭐ Favorites**, Quick Spawn, and **Available in Game Only** live filter.
- **World** *(Recommended)*: Time presets (Morning `08:00`, Noon `12:00`, Sunset `19:00`, Midnight `00:00`), custom time, **Freeze Time**.
- **Money (Beta / Experimental)**: Currently unreliable / may not work in `GTA6WebTrainer.elf`. A fully working Cash module will be announced and released in **`v2.0.0`** — for now, please use the **Player**, **Vehicles (Spawn)**, and **World** tabs.
- **Lektor PL (Beta)** *(Polish mode only)*: Reads live in-game subtitles from PS5 RAM with FIFO queue (macOS `say` and/or Browser/Phone Web Speech API).
- **GoldHEN / etaHEN / CheatRunner JSON (33 Cheats)**: [`cheats/PPSA38500_01.002.001.json`](cheats/PPSA38500_01.002.001.json) includes Health, Stamina, Infinite Ammo, Clear Wanted, Max Relationship, Max Criminal Profile, 4 Time Presets, Vehicle Auto-Repair, and **22 Top Vehicle Spawners** (`Thrax`, `Emerus`, `Vitello`, `Itali RSX`, `Comet Retro`, `Infernus '96`, `Gauntlet Hellfire/Classic`, `Vigero ZX`, `SFB 1000`, `Alvino V1`, `Carbon RS`, `Sanchez`, `Cliffhanger`, `Manchez`, `P-996 LAZER`, `Shamal`, `Nimbus`, `Mallard`, `Buzzard`, `Valkyrie`, `Volatus`).

> [!WARNING]
> **Important — Vehicle Spawn Cheats in `PPSA38500_01.002.001.json`**:  
> Toggle a `Spawn: ...` cheat **ON**, wait **~3 seconds** for the vehicle to appear, and **immediately toggle it back OFF before getting into the vehicle**. Entering a vehicle while a Spawn cheat is still active can cause an error and freeze the game!

---

## 🚀 Quick Start

### 1. Standalone PS5 Payload (`GTA6WebTrainer.elf`) — **NO PC NEEDED!**
Run the entire Web Trainer **directly inside your PS5** (with full DualSense gamepad navigation — no folder copying or ShadowMount required):
1. Make sure **ps5debug-NG** (port `744`) is running on your PS5.
2. Send [`elf_trainer/GTA6WebTrainer.elf`](elf_trainer/GTA6WebTrainer.elf) to your PS5 ELF loader (port `9021`) or place it in `/data/etaHEN/payloads/`.
3. On startup, **`GTA6WebTrainer.elf` automatically installs the `GTA VI Web Trainer` (`PPSA38599`) tile in the PS5 Media tab** (once) and auto-connects to GTA VI every 3s.
4. Open **GTA VI Web Trainer** from the PS5 **Media** tab (DualSense: `L1/R1` tabs, `D-Pad` focus, right-stick scroll, `X` click), or open **`http://<PS5_IP>:8080`** on your phone!

### 2. Full PC / Mac / Linux Package (Python Web Trainer)
- **Requirements**: Jailbroken PS5 with **ps5debug-NG** (port `744`), **GTA VI (`PPSA38500 01.002.001`)** in Story Mode, and **Python 3.10+**.
- **macOS / Linux**: run `./start_mac_linux.sh`
- **Windows**: double-click `start_windows.bat`
- Open **`http://localhost:8080`** on your PC (or **`http://<YOUR_PC_IP>:8080`** on your phone), enter your PS5 IP, and click **Connect PS5**.

### 3. Cheat JSON for etaHEN / GoldHEN / PS5 Cheat Manager / CheatRunner
1. Copy [`cheats/PPSA38500_01.002.001.json`](cheats/PPSA38500_01.002.001.json) to your PS5 cheat directory (e.g. `/data/etaHEN/cheats/json/` or your CheatRunner / PS5 Cheat Manager folder).
2. Launch **GTA VI (`PPSA38500 01.002.001`)** and open your cheat menu to toggle any of the **33 native cheats**.
3. For `Spawn: ...` cheats: toggle **ON** → wait **~3 seconds** → toggle **OFF** before entering the vehicle.

---

## 🇵🇱 Wersja polska (PL) — Co zawiera paczka?

Paczka zawiera **3 osobne, gotowe sposoby uruchamiania** — wybierz ten, który jest dla Ciebie najwygodniejszy:

1. **Samodzielny plik ELF na konsolę (`elf_trainer/GTA6WebTrainer.elf`) — BEZ KOMPUTERA!**
   - Uruchamiasz raz na PS5 (port `9021` lub `/data/etaHEN/payloads/`) przy włączonym `ps5debug-NG` (port `744`).
   - ELF sam instaluje kafelek **`GTA VI Web Trainer` (`PPSA38599`)** w zakładce **Multimedia** i co 3s automatycznie łączy się z GTA VI.
   - Obsługujesz go padem **DualSense** (`L1/R1`, krzyżak, scroll prawą gałką, `X`) bezpośrednio na TV albo przez telefon pod adresem `http://<IP_PS5>:8080`.
2. **Pełny zestaw komputerowy (PC / Mac / Linux — `trainer.py`)**
   - Kompletny Web Trainer uruchamiany na komputerze: `./start_mac_linux.sh` (macOS/Linux) lub `start_windows.bat` (Windows), a następnie `http://localhost:8080` (lub z telefonu po Wi-Fi).
   - Zawiera wszystkie funkcje Trainera (Postać + 5 natywnych patchy, 252 pojazdy + ⭐ Ulubione, Świat) oraz w 100% lokalnego **Lektora PL (Beta)** (`macOS say` / Web Speech API).
3. **Osobny plik cheatów JSON dla etaHEN / GoldHEN / CheatRunner / PS5 Cheat Manager ([`cheats/PPSA38500_01.002.001.json`](cheats/PPSA38500_01.002.001.json))**
   - **33 natywne cheaty x86-64** działające prosto z menu cheatów na konsoli (bez przeglądarki):
     - `Unlimited Health`, `Unlimited Stamina`, `Infinite Ammo`, `Clear Wanted Level`, `Max Relationship`, `Max Criminal Profile`
     - `Time: Morning (08:00) / Noon (12:00) / Sunset (19:00) / Midnight (00:00)` + `Vehicle Auto-Repair`
     - **22 spawny TOP pojazdów** (Supersamochody, Sportowe, Muscle, Motocykle, Samoloty/Odrzutowce, Śmigłowce).
   > [!WARNING]
   > **WAŻNE (cheaty `Spawn: ...` w JSON-ie)**: Po włączeniu cheata spawnu na **ON** odczekaj **ok. 3 sekundy** aż pojazd się pojawi, a następnie **obowiązkowo wyłącz go z powrotem na OFF zanim wsiądziesz do auta** — wejście do pojazdu z aktywnym cheatem spawnu może wywołać błąd i zawiesić grę!

---

## Credits

- Created by **deFIATer / czupryneq**.
- Game offsets (`PPSA38500 01.002.001`) & vehicle list (`Vehicles.h`) based on **[basic-menu-src](https://github.com/OnlyMoisties/basic-menu-src)** by **XeMo**.
- Vehicle display names & categories: **GTA Fandom Wiki**.
