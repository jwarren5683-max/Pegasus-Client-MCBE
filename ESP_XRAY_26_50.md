# Minecraft 1.26.50 ESP and X-ray test

**Historical release notes. Current repair status and testing instructions are
in [ESP_REPAIR_20260916.md](ESP_REPAIR_20260916.md).**

This is an experimental repair for `1.26.5101.0`, not a verified working client.

## ESP

The verified 1.26.45 path's readiness regression is repaired. The 26.50
actor-list signature is only a historical candidate. Its return ABI, required
calling thread and entity/camera layouts remain unverified. ESP stays N/A on
26.50 and never calls that candidate from the overlay.

## X-ray

X-ray discovers the live BlockGraphics registry instead of using the old
1.26.45 address. It requires a consistent object vtable, valid Minecraft block
identifiers, air, stone, and the configured ore set before becoming available.
Discovery now checks only writable sections using copied 64-KiB windows and a
time budget, instead of expensive per-word memory queries across all sections.
Disabling immediately restores adaptive overrides without a subsequent tick.
Synthetic tests cover boundary discovery, application and restoration while
preserving newer engine values. No world blocks are changed.

The running 26.50 executable has now been captured and inspected locally.
The exact native coordinator update (`0x1C9D140`), rebuild-all function
(`0x1C9C6D0`) and constructor-established vtable (`0xE7D5B80`) are connected.
The fingerprint, stack-only patch boundary, rebuild prologue, destructor and
both engine call sites must match before hooking. The hook applies/restores
graphics and requests two loaded-chunk refresh passes on the native engine
thread, independent of menu ticks. Stable enabled state does not refresh every
frame. Fullbright uses the separately verified 26.50 renderer lighting profile;
if any lighting check fails, it stays unavailable while X-ray remains usable.

The user has now confirmed that this native X-ray build works in-world. Its
runtime log also confirms graphics application, rebuild requests and disable
restoration. Fullbright has subsequently been ported to the exact 26.50 mesh
and final light-lookup sites, with all ten instruction checks and five function
prologue checks. Fullbright's new visual effect is not yet live-verified.
Levels 9–14 retain native light colors; level 15 uses neutral maximum lighting
so ore textures are distinguishable. The tradeoff is flatter lighting and less
shadow contrast while enabled; disabling restores normal rendering.

The earlier adaptive build confirmed registry discovery, but not terrain application.
The new native callback path is tested with a synthetic coordinator/registry;
X-ray is user-verified in-world, but the new Fullbright effect is NOT yet
verified. Passing tests is not proof of a working in-game effect. Minecraft
must restart because the previous DLL is
pinned in the currently running process; do not inject a second copy.

## Test procedure

### Reach repair test

The first reach package was found to contain mixed object layouts: its factory
allocated 136 bytes while the updated implementation required 144. Windows
reported heap corruption. The replacement is fully rebuilt in a new directory,
has an explicit constructor and checks factory/implementation sizes before
allocation. A separate-file allocation canary test covers the mismatch risk.
Look for `Reach build layout validated` and the correct `1.26.5101.0` Reach
profile in the live log. This repair still needs a clean-session stability test.

The same packaged launcher now also connects the verified 26.50 Survival
entity cap and final hit-distance gate. In a disposable local world, test
EntityReach alone, BlockReach alone and both together at 7 blocks, then disable
both and check vanilla ranges return. Test actual entity interaction and block
mining/placement, not only crosshair highlighting. The log reports native pick
callback slider ranges and the final crosshair gate when they execute. Working
X-ray and Fullbright code is unchanged. Remote servers can still reject extended
interactions; this repair does not bypass server enforcement.

1. Fully close Minecraft and any older Pegasus launcher.
2. Start Minecraft and enter a local world.
3. Run `START THIS - Pegasus Native Xray Fix.exe` once and select the fresh
   visible game session. The diagnostic `--inject-pid PID` option selects an
   exact Minecraft process without changing file permissions; it rejects
   other applications and uses the existing duplicate-DLL safeguard.
4. Check the log for registry discovery results. ESP is intentionally N/A on
   26.50 pending native-interface verification.
5. Enable X-ray. The log should show `enable requested`, then graphics applied
   and loaded chunks rebuilding. Disabling must restore terrain and refresh it.
   Validate both in a disposable local world before treating it as finished.

The scanner now supports `--pid PID` so it can inspect the visible game rather
than choosing the inaccessible background process. `--dump-image NEW_FILE`
captures only the main executable image locally, never arbitrary heap/world
memory, and refuses to overwrite a file. Never upload the executable capture.
No security, permissions or WindowsApps ownership was changed.
The prior GitHub build failed a camera fixture test; that regression is repaired.

If a required signature or registry validation fails, the affected module stays
`N/A` and writes the reason to
`%LOCALAPPDATA%\BedrockUtilityFramework\framework.log` rather than guessing an
address.

