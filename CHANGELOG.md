# Changelog

## Minecraft Bedrock 26.50 ESP and X-ray repair — 2026-09-15

- Added a 26.50 ESP path that resolves Bedrock's uniquely matched, thread-safe runtime actor-list function instead of iterating the shared entity registry from the overlay thread.
- Reuses the byte-verified Reach/GameMode path to observe the current local player, with structural pointer and executable-image validation.
- Added adaptive 26.50 BlockGraphics discovery for X-ray. The registry, object vtable, block identifiers, air, stone, and the complete ore set must validate before the module becomes available.
- Kept the old 1.26.45 hooks unchanged and left unrelated unverified gameplay modules fail-closed.
- On 26.50, X-ray affects newly rebuilt chunk meshes; existing meshes refresh as Bedrock rebuilds them during movement or world reload.

## Minecraft Bedrock 26.50 verified Reach restoration — 2026-09-15

- Connected the exact 1.26.50 Reach and maximum-range functions found by the live compatibility probe.
- Connected both exact 1.26.50 vtable references and retained byte-for-byte signature checks before any hook is installed.
- Restored the EntityReach and BlockReach menu controls on 1.26.50 for the verified ray and maximum-range paths.
- Kept the separate Survival entity-cap and final-hit patches disabled until their new 1.26.50 sites are independently verified.
- Preserved full 1.26.45 behavior and restoration logic.

## Minecraft Bedrock 26.50 compatibility foundation — 2026-09-15

- Recognizes the installed Minecraft `1.26.5101.0` executable exactly (`0x6AA482FD`, image size `0x12C01000`).
- Combined the earlier 26.50 preparation work with the enhanced Reach, Trigger Bot, remote-session safety, and Auto Leave source instead of replacing either branch.
- Added a read-only in-process compatibility probe that records candidate 26.50 native RVAs without installing unverified hooks.
- Added a standalone read-only compatibility scanner and GitHub Release build packaging.
- Kept unsupported native modules fail-closed while preserving the working injector and overlay.

### Verification

- 18 of 18 automated tests passed, including exact 1.26.45/1.26.50 profile classification.
- The rebuilt injector verified the DLL hash and loaded it into a disposable x64 test process without touching Minecraft.
- Live 1.26.50 module targets still require a fresh-session probe run before any native feature is marked compatible.

## Auto Leave health safety module — 2026-09-15

- Added **Auto Leave** under Combat with a 0.5–10 heart slider, a 4-heart default, and half-heart steps.
- Reads the verified native health attribute and requires two matching low-health ticks before acting.
- Uses Bedrock's asynchronous leave-game request, returning to the world list without terminating Minecraft or forging a network packet.
- Works in integrated/local and remote worlds. The action is one-shot per session and rearms when the module is toggled or the player/world changes.
- Fails closed when health, player, client-instance, vtable, or executable-code validation fails.

### Verification

- 17 of 17 automated tests passed, including threshold clamping, transient-read rejection, one-shot behavior, slider metadata, and remote-session availability.

## Trigger Bot crash fix and remote-server safety — 2026-09-15

- Removed the re-entrant native `GameMode::attack` call that produced repeatable `0xC0000005` crashes when a target was acquired.
- Replaced it with a spaced left-button input path for integrated/local worlds, including foreground and gameplay-input checks.
- Added integrated-server detection. Gameplay-altering modules fail closed and show `N/A` while connected to a remote server.
- Kept Fullbright and the ArrayList UI available remotely; no anti-cheat bypass or server-validation workaround is included.
- Added automated coverage for local, remote, stale-session, and reset transitions.

### Verification

- 16 of 16 automated tests passed.
- The matching injector completed its disposable-process self-test.

## Enhanced reach and Trigger Bot repair — 2026-09-15

- Increased the entity- and block-reach menu maximum from 7 to 10 blocks while retaining the 7-block default and 0.5-block adjustment steps.
- Added reach range and clamping coverage to the automated tests.
- Previously changed Trigger Bot to call a presumed `GameMode::attack` vtable entry; post-publication testing showed this path was unsafe and it is superseded by the crash fix above.
- Added a final foreground and gameplay-input check at the point the native attack is emitted.
- Added target-picker checks and bounded attack-cadence tests.
- Added a command-line injection mode to the desktop injector and updated its title for the Enhanced build.
- Updated the injector's pinned DLL checksum and synchronized the checked-in EXE and DLL with the tested build.

### Verification

- 15 of 15 automated tests passed.
- Injector self-test passed by validating the DLL checksum and loading it into a disposable x64 process without touching Minecraft.
- Clean injection into Minecraft Bedrock 1.26.4501.0 succeeded; logs confirmed installation of the supported reach and gameplay hooks.

### Known limitation

Extended reach is a client-side feature. Multiplayer servers can enforce their own reach distance and reject attacks or interactions outside that limit.

