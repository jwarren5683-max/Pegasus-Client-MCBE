# Bedrock 26.50 compatibility handoff

## Current status

Pegasus is **not yet certified for Bedrock 26.50**. The existing native integration is intentionally fail-closed and targets the older executable profile identified in source as Minecraft `1.26.4501.0`.

Do not replace RVAs or signatures with guesses. The menu/overlay can be substantially version-independent, but native modules must only be enabled after their target bytes and layouts have been validated against the current `Minecraft.Windows.exe`.

## Known legacy executable profile

- File-version label used by Pegasus: `1.26.4501.0`
- PE timestamp: `0x6A8378BA`
- PE `SizeOfImage`: `0x12888000`

Run `tools/check-bedrock-compat.ps1` while Minecraft is open to print the currently installed executable's file version, PE timestamp, image size, and SHA-256. Save that output for the 26.50 port.

## Version-sensitive areas already identified

### Core player/game context

`Source/Mod/src/integration/GameContext.hpp`

- Hard-coded player vtable identities at `0xE820EC0` and `0xE833530`.
- Revalidate before any module trusts a captured player pointer.

### Reach

`Source/Mod/src/modules/ReachModule.cpp`

- Validates the legacy PE timestamp and image size.
- Uses legacy picker RVAs, vtable slots, entity-range read sites, and byte signatures.
- The current safety checks should remain fail-closed until every 26.50 location is verified.

### Gameplay modules

`Source/Mod/src/modules/GameplayModules.cpp`

- Validates the legacy PE timestamp and image size before installing shared hooks.
- Contains legacy player/server vtables, tick callbacks, mining callbacks, air-jump/ejection hooks, native helper RVAs, and Triggerbot validation sites.
- This file has the largest 26.50 validation surface and should be ported in small groups rather than by applying one global address delta.

### Local chat commands

`Source/Mod/src/integration/ChatCommands.cpp`

- Validates the legacy PE timestamp/image size and several exact instruction sequences.
- Revalidate the submit target, input-field layout, display-message function, and send-path signature.

### Splash text

`Source/Mod/src/integration/SplashTextHook.cpp`

- Validates the legacy executable profile and exact loader prologue.
- Revalidate both the loader target and the native string-object layout before enabling it.

### Other native integrations

Review `NativeNavigationAdapter.cpp`, `NavigationNativeLayout.hpp`, `NavigationWorldAccess.hpp`, `GhostHandModule.cpp`, `AntiKnockbackModule.cpp`, `CriticalsModule.cpp`, `SpeedModule.cpp`, and `XrayModule.cpp` for additional build-specific addresses/layouts before marking 26.50 complete.

## Port checklist

1. Run `tools/check-bedrock-compat.ps1` on the updated game and record its report.
2. Preserve the old `1.26.4501.0` profile as a reference; do not overwrite it with unverified numbers.
3. Locate each target in the 26.50 executable by validated byte pattern/function analysis, not by assuming a constant RVA shift.
4. Update executable-profile metadata and each module's signatures/RVAs together.
5. Revalidate object layouts and vtable identities used to distinguish the local player and server replica.
6. Build and run the CTest suite.
7. Test a fresh Minecraft session in stages: DLL load, overlay only, player context, then individual native modules.
8. Keep modules fail-closed when any expected signature is absent.
9. When the native DLL changes, regenerate the injector's `ExpectedHash` before packaging.
10. Use the GitHub Actions `Build Pegasus` workflow to produce a downloadable Windows artifact after the port is committed.

## GitHub build support added during this update

`.github/workflows/build-pegasus.yml` performs a clean Windows x64 build, runs tests, copies the rebuilt DLL, recalculates and pins its SHA-256 into the injector source for that CI build, rebuilds `Pegasus.exe`, and uploads the EXE + DLL as a GitHub Actions artifact.

That workflow proves that the source compiles and its unit tests pass. It does **not** by itself prove that native 26.50 signatures are correct; in-game validation is still required.
