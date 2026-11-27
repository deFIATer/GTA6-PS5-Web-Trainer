# Teleport to Waypoint

Adds an engine-level **Teleport to Waypoint**: reads the player's map waypoint from game memory
and moves the player (or the vehicle being driven) exactly to it.

Verified on **PPSA38500, app version 01.002.001, FW 11.60**.

## Memory

Offsets relative to the eboot image base (`0x400000`):

| Item | Offset | Notes |
|---|---|---|
| Waypoint x / y / z / present flag | `0xAF93FC0` (`+0,+4,+8,+0x0C`) | float32; flag `1.0` when a waypoint exists |
| Local player ped pointer | `0xDCDBCB0` | `ped*` |
| Local player vehicle pointer | `0xDCEBCE0` | `vehicle*` or 0 |
| Placement matrix (entity) | `ent + 0x40` | 4x4; translation in `m[12..14]`, sectors packed in `m[15]` |
| Entity world position | `[ped+0x38] + 0x90` | float3 (vehicle entity used when driving) |
| Entity SetMatrix vtable slot | `vtable + 0x178` | `fn(ent, matrix, 1, 1, 0)` |

## Algorithm

1. Read waypoint `x,y,z` from `base + 0xAF93FC0`; require the present flag.
2. Pick the entity: vehicle if `LocalPlayerVeh != 0`, else the ped.
3. Copy the 4x4 matrix from `ent + 0x40` and set the translation:
   `m[12] = x - 32*sx`, `m[13] = y - 32*sy`, `m[14] = z + 4.0`, `int16 sx,sy` packed into `m[15]`.
4. Allocate a **fresh** in-process buffer for the matrix (buffers are invalid after a game restart).
5. Call the entity SetMatrix vtable slot `0x178` on the game main thread (existing pad hook).
6. Verify against the entity world position (`ent + 0x90`; vehicle when driving).

## Why these details matter

- `z + 4.0` is required: `z+1` spawned ~3 m under the ground, `z+2` left the legs underground.
- **Never call the community "script lock" helpers** (`base+0x1050`, `base+0x1000`) from the ELF
  payload — in testing those calls crashed the game and once the console. The single vtable call
  without them is stable.
- Teleport must be issued during normal gameplay (paused / map / loading => commit is not applied).
- Verification must read the **vehicle** world position when driving; the ped matrix is stale then.

## API

| Endpoint | Method | Result |
|---|---|---|
| `/api/waypoint` | GET | `{ok, present, x, y, z}` |
| `/api/teleport_waypoint` | POST | teleports player/vehicle to the waypoint |

## UI

A floating **📌 Teleport to Waypoint** button is injected by `web/app.js`, showing the live
waypoint coordinates next to it. Regenerate `elf_trainer/web_assets.h` with
`python tools/gen_web_assets.py` after editing `web/`.

## Tested

- Waypoint markers set at different map locations; teleported up to ~2.3 km away.
- Landed on the waypoint X/Y with the intended height, position stayed after walking/driving.
- Vehicle teleport verified (`in_vehicle:true` -> vehicle and player moved to the waypoint).
- Multiple consecutive teleports and game relaunches (per-call buffer fix).


## Summon Car (same commit)

Adds POST /api/summon_vehicle: moves the **last vehicle the player drove** to ~4 m in front of the
player (z + 1 m), using the same entity SetMatrix path on the vehicle entity.

- the trainer tracks the last vehicle from LocalPlayerVeh while driving (status polling) and
  re-tracks it whenever a vehicle is currently occupied
- sanity checks: entity type byte eh+0x30 == 2, plausible world position; stale/dead vehicles
  are cleared with a message asking to drive the car again
- UI: floating **Summon Car** button next to Teleport to Waypoint
- verified on hardware: drove a car, exited, pressed Summon Car -> the exact car was moved next to
  the player
