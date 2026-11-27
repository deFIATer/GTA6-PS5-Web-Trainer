# Changelog

All notable changes to **GTA VI PS5 Web Trainer** are documented in this file.

## [v1.1.6] - 2026-11-27
### Changed
- **Money / Cash Module Marked as Beta (ELF & Web UI)**: Added a clear **Beta / Experimental** warning in the Money tab (`Money (Beta)` / `Kasa (Beta)`) and documentation noting that the Cash module in `GTA6WebTrainer.elf` is currently unreliable or may not work properly in-game.
- **Roadmap to `v2.0.0`**: Once a 100% reliable, fully working Cash system is completed, it will be announced and released as **`v2.0.0`**. In the meantime, the **Player**, **Vehicles (Spawn)**, and **World** tabs are fully stable and recommended.

## [v1.1.5] - 2026-11-27
### Fixed
- **Unique 64-bit Cash Inventory Signature (`0x6F0A8D016FC2266B`)**: Upgraded `0x11...` master wallet scanning in both `GTA6WebTrainer.elf` and Python from the generic 32-bit `0x6F0A8D01` tag (which matched 50+ unrelated items when cash was `$0` or `$1` and overflowed the address cap, leaving shops thinking you had no money!) to the exact 64-bit cash item ID `0x6F0A8D016FC2266B` (**1 unique hit in 2 GB RAM, 0 false positives**). Shops and car repairs now immediately recognize and spend your modified cash!
- **Prioritized Master Scan Order**: Scans `0xf0...` (RAGEVM) and `0x11...` (`0x6F0A8D016FC2266B`) first before `0xfa...`, ensuring authoritative shop/wallet addresses are always captured first.
- **ELF Startup Notification**: Displays `GTA VI Trainer / ELF v1.1.5 by deFIATer/czupryneq (Port 8080)` on launch.

## [v1.1.4] - 2026-11-27
### Fixed
- **ELF Money Scanner 3s Socket Timeout**: Dedicated PS5 RAM scanner sockets (`money_scan_stats_c`, `money_scan_master_c`, `money_scan_hud_c`) now use a **30-second timeout** (`tcp_connect_timeout(..., 30)`) and 128 MB chunks instead of the 3-second RPC timeout, preventing `CMD_PROC_SCAN_START` on `0x11...` from aborting silently before finding Master and HUD wallet addresses.
- **`0xf0...` Master Wallet Truncation (27,282 → 8 hits in 0.04s)**: Upgraded `0xf0...` RAGEVM scanning in both ELF and Python to match 64-bit `[0x00000804, amount]` (`align=4`), eliminating buffer truncation and locating the authoritative RAGEVM wallet instantly.
- **Mode 2 Auto-Detect Race Condition (ELF & Python)**: Auto-Detect now stays in `auto_scanning` (`Detected $X! Locking Master wallet & HUD (~8s)...`) until all Master (`0xf0...`, `0xfa...`, `0x11...`), HUD, and Stat addresses are collected, only then switching to `locked` and notifying the user — so clicking `+$1M` or `Set $1,000,000` immediately updates the real in-game balance on the very first click.

## [v1.1.3] - 2026-11-27
### Changed
- **Mode 2 · Auto-Detect requires current cash (`>= 0`)**: Entering your current cash balance (e.g. `0` or `250`) is now mandatory in both UI and backend (ELF & Python) to eliminate false-positive stat captures.

## [v1.1.2] - 2026-11-27
### Fixed
- **ELF Master Wallet Scanner (`0xf0...` / `0xfa...` / `0x11...`)**: Fixed `align=1` + 1024-hit truncation when scanning for small cash amounts (e.g. `$14`). Now scans with `align=4` (4096-hit heap buffer) directly by rare structural signatures (`0x00000804` + `0xAF2E50` in `0xf0...`, and `0x6F0A8D01` in `0xfa...` / `0x11...`) so all 9 wallet addresses are reliably captured in `GTA6WebTrainer.elf`.

## [v1.1.1] - 2026-11-27
### Fixed
- **Real In-Game Cash Modification (ELF & Python)**: Added scanning and updating of the 3 authoritative master wallet copies in `0xf000000000` (RAGEVM register, `50 2e af ?? 04 08 00 00`), `0xfa00000000` (script wallet, tag `0x6F0A8D01`), and `0x11...` (`0x8000000000000000` + `0x6F0A8D01`) alongside stats (`0x10...`) and HUD pairs (`0x11...`). Cash now updates immediately on screen and persists across transactions!

## [v1.1.0] - 2026-11-27
### Added
- **Working Money Tab in Standalone `GTA6WebTrainer.elf`**: Implemented full Manual (`/api/money/find`), Auto-Detect (`/api/money/watch`), Add/Set/Reset wallet scanner in C on a dedicated TCP channel (HUD pairs in `0x11...` + stat entries in `0x10...`).
- **⭐ Favorite Vehicles**: Star any vehicle to pin it to a persistent **⭐ Favorites** quick-spawn bar above Quick Spawn (saved in `localStorage`).
### Changed
- **Voiceover → `Lektor PL (Beta)`**: Shown only when language is switched to Polish (`PL`); hidden automatically in English (`EN`) mode (including DualSense `L1 / R1` navigation).
- **Cleaned Vehicle List (252 verified models)**: Live-audited all 254 models against PS5 `ArchetypeMap` (`PPSA38500 01.002.001`) and removed the 2 missing models (`annihilator`, `jetmax`).
### Removed
- Removed non-working **Next Weather** card from the World tab.

## [v1.0.6] - 2026-11-27
### Changed
- **One-time Media Tile Installation**: `GTA6WebTrainer.elf` now checks if `/user/app/PPSA38599/sce_sys/param.json` and the latest `icon0.png` are already installed and skips re-registering on subsequent launches.

## [v1.0.5] - 2026-11-27
### Fixed
- **ELF Stability**: Added `signal(SIGPIPE, SIG_IGN)` so closed browser/phone/PS5 sockets never kill or freeze `GTA6WebTrainer.elf`.
- **Non-blocking PS5 Notifications**: Moved `CMD_CONSOLE_NOTIFY` to a dedicated background thread with 3s pacing (no longer holds `G.lock` or blocks HTTP).
- **Auto-Recovery**: Broken `ps5debug-NG` sockets are automatically closed and reconnected cleanly when GTA VI restarts; added 2s HTTP client socket timeouts.
- **Safer Vehicle Spawning**: Added `ped != 0`, active GOT hook `tick_count`, and `state == 0` guards before writing call arguments to prevent in-game crashes.

## [v1.0.4] - 2026-11-27
### Changed
- Separated **`💰 Money`** and **`🎮 PS5`** into two dedicated tabs across the desktop sidebar, mobile bottom nav, and DualSense `L1 / R1` navigation.

## [v1.0.3] - 2026-11-27
### Fixed
- Fixed `CMD_PROC_MAPS` entry size in `GTA6WebTrainer.elf` (`uint16_t prot` = 58 bytes instead of 60 bytes), resolving the TCP wire desync that broke the GOT hook and vehicle spawning (`Model not found in ArchetypeMap`).

## [v1.0.2] - 2026-11-27
### Added
- **DualSense Right-Stick Scrolling**: Smooth vertical scrolling using the right analog stick (`axes[3]`).
### Changed
- Removed the combined `Dashboard` tab so the UI opens directly into clean individual tabs (`L1 / R1`).

## [v1.0.1] - 2026-11-27
### Added
- **Trainer Version** readout (`ELF v1.0.x` / `Python v1.0.x`) in the sidebar and PS5 tab.
- **🔌 Disconnect PS5** button (safely restores the original `scePadReadState` GOT pointer).
- **🛑 Stop ELF Server** button (cleanly frees port `8080` for seamless `.elf` updates without rebooting PS5).
- Full streamed vehicle spawning (`ArchetypeMap` + `StreamRequest` / `StreamReady` / `StreamRelease`) in `GTA6WebTrainer.elf`.
### Fixed
- Single `PPSA38599` Media tile with full-bleed `icon0.png` (no white corners).
- Added missing `import time` in `core/vehicles.py`.

## [v1.0.0] - 2026-11-27
### Added
- Initial release of **GTA VI PS5 Web Trainer** (Python modular version + standalone `GTA6WebTrainer.elf` payload for PS5 FW 13.60 via `ps5debug-NG`).
- Automatic PS5 Media tile (`PPSA38599`) registration, DualSense gamepad navigation, 254 categorized vehicles, Player/World controls, PS5 on-screen notifications, Live Subtitle Voiceover (EN/PL), and Light/Dark themes.
