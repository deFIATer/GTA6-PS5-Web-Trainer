# Changelog

All notable changes to **GTA VI PS5 Web Trainer** are documented in this file.

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
