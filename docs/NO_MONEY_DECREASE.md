# No Money Decrease (native patch)

Adds a toggleable **No Money Decrease** native patch to the trainer.

When enabled, the game's money-subtraction routines are bypassed so cash/bank money is not deducted
(purchases, fines, etc. still run, but the balance is preserved).

## Source of the patch

Patch bytes and offsets come from the community **HEN Cheats Collection** table for this exact game
version (`PPSA38500` `01.002.001`):

- `cheats/json/PPSA38500_01.002.001_5726f32e.json`, mod **“No Money Decrease”**, credits **TeeKay87**
- 4 memory entries: two code caves plus two jump patches, each with `on`/`off` byte strings

They are embedded in `elf_trainer/native_patches.h` exactly as published (no modifications), and
applied/restored through the trainer's existing native-patch engine
(`Game.native_patch_on[]`, `dbg_protect` + `dbg_write` on toggle).

## Changes

- `elf_trainer/native_patches.h`
  - `NP_no_money_decrease_0..3_ON/OFF` byte arrays
  - `NP_no_money_decrease_ENTRIES[]` with the 4 `{offset, on, off, len}` entries
  - new `NATIVE_PATCHES[]` entry `{"no_money_decrease", "No Money Decrease", ...}`,
    `NATIVE_PATCHES_COUNT` 5 -> 6
- `elf_trainer/main.c`
  - status JSON exposes `"no_money_decrease": true/false`
- `web/index.html` + `web/app.js`
  - switch row **💰 No Money Decrease** in the Native Game Patches card (on/off toggle)
- `elf_trainer/web_assets.h` regenerated with `tools/gen_web_assets.py`

## Behavior

- Default **OFF** after every trainer/game restart (native patches are re-applied fresh)
- Toggling on writes the 4 patch entries; toggling off restores the original bytes
- No save-game modification; purely runtime code patches

## Verified

- Toggle on -> status `no_money_decrease: true`, toggle off -> false, no crash (FW 11.60, game
  `01.002.001`)
- Money no longer decreases while enabled (community table patch, same behaviour as its
  CheatRunner/etaHEN implementations)

## Notes / risk

Memory patches are applied to the running game process. If the game updates (different app
version), the offsets will not match — this table is version-specific. Disable the patch before
reporting unrelated issues.
