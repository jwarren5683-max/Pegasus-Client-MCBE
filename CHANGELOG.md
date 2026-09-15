# Changelog

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
