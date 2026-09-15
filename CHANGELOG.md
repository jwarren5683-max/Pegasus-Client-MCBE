# Changelog

## Enhanced reach and Trigger Bot repair — 2026-09-15

- Increased the entity- and block-reach menu maximum from 7 to 10 blocks while retaining the 7-block default and 0.5-block adjustment steps.
- Added reach range and clamping coverage to the automated tests.
- Corrected Trigger Bot to call the verified `GameMode::attack` vtable entry at index 14 with its native two-argument signature.
- Added a final foreground and gameplay-input check at the point the native attack is emitted.
- Added exact survival/creative attack-target signature checks and native-dispatch tests.
- Added a command-line injection mode to the desktop injector and updated its title for the Enhanced build.
- Updated the injector's pinned DLL checksum and synchronized the checked-in EXE and DLL with the tested build.

### Verification

- 15 of 15 automated tests passed.
- Injector self-test passed by validating the DLL checksum and loading it into a disposable x64 process without touching Minecraft.
- Clean injection into Minecraft Bedrock 1.26.4501.0 succeeded; logs confirmed installation of the supported reach and gameplay hooks.

### Known limitation

Extended reach is a client-side feature. Multiplayer servers can enforce their own reach distance and reject attacks or interactions outside that limit.

