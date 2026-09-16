# Changelog

## ESP-first repair; suspend unsuccessful Reach — 2026-09-16

- Replace the ambiguous actor-list ABI with read-only packed snapshots on an exact, verified 26.50 LocalPlayer tick. Validate local membership, generations, dimensions and bounds; reject changing/freed storage.
- Live read-only probe confirmed 22 client actors and the camera's eight-byte field shift: basis +0x420, frustum +0x4A0, renderer +0x1C0. Native snapshots no longer depend on Reach.
- Suspend 26.50 Reach registration and its range gates for this ESP release. Allow read-only ESP for server-supplied entities while preserving other combat/movement remote restrictions.
- Diagnose X-ray native gates individually and retry a not-yet-ready block registry in bounded native-update slices rather than permanently keeping the terrain modules N/A. Preserve instruction checks and restoration.
- Add packed snapshot corruption/race safety tests, real RPM capture fixture, shifted camera fixture and deferred registry recovery regression. A fresh full build and live visual testing are required; probe/test success is not a claim of visible ESP.

## Reach crash / mixed-build layout repair — 2026-09-16

- Windows recorded two Minecraft exits with heap corruption (`0xC0000374`). The shipped incremental build retained a Framework object from 16:25 while ReachModule's header/implementation changed at 16:59–17:00; the runtime also misreported its selected profile. Disassembly confirms its factory allocated 136 bytes (`0x88`) versus the new implementation's 144 bytes. This confirms an allocation-layout defect consistent with heap corruption, not a server kick; post-repair live stability still requires verification.
- Rebuild all native files in a new empty build directory. Add a factory-versus-implementation size check before Reach allocation and move its constructor out of line.
- Add a separately compiled allocation-canary regression test exercising construction, entity/block sliders, enable/disable and destruction. Existing same-file reach tests alone could not catch this release defect.
- Preserve working X-ray/Fullbright code. ESP remains gated pending verified entity-list ABI/layout/thread support; the crash repair was selected as the faster task. Fresh-session stability and private-server behavior still require live validation.

## 26.50 Reach native distance gates — 2026-09-16

- User confirmed Fullbright works. Working X-ray/Fullbright implementation is unchanged by this reach repair.
- Connected exact 26.50 Survival cap reads at `0x4BCC5C`, `0x4BCC65`, `0x4BCDAC` and HitResult distance validation at `0xEC3D80`, restricted to native crosshair return site `0x4BD1A3`. Checked picker entry/call ABI, function unwind boundaries and three reads of the original 3.0 float. Redirect only these reads to private storage; never alter the shared constant.
- Maximum local interaction range now includes entity as well as block sliders; final hit validation still separates entity and block limits. Disabling returns to vanilla ranges.
- Added both-version execution tests for the actual relocated SSE reads, boundary rejection through 10 blocks, mismatched-signature/no-partial-write checks and restoration.
- Added bounded runtime diagnostics showing the actual native targeting callback and final gate received slider values. Kept unrelated new-version Trigger Bot/crosshair integration gated.
- Local targeting/interaction effects need a fresh-session test. Remote servers can reject extended interaction ranges; no server bypass is implemented or promised.

## 26.50 Fullbright renderer repair — 2026-09-16

- User confirmed the native X-ray build works in-world; runtime logs also show application, chunk refresh and disable restoration.
- Ported the four renderer-only mesh lighting sites and six final scalar/vector/End light-lookup sites to the exact 1.26.5101.0 image. All ten original instructions and all five containing-function prologues must match; a mismatch leaves Fullbright unavailable without blocking working X-ray.
- Fullbright light levels 9–14 keep the native lookup and adjust mesh light; level 15 produces a neutral white light lookup so ore texture colors are visible without darkness tint. Disabling restores original instructions and terrain through the native callback. World light propagation and blocks are unchanged.
- Added both-version slider/apply/restore tests, mismatch/no-partial-write checks and native availability gating. Fullbright's new in-world visual effect still requires a fresh-session test.
- Removed five obsolete desktop client folders from the active folder by moving them to a recoverable backup outside it. Retain the confirmed working X-ray release separately before replacing its packaged files.

## Native 26.50 X-ray renderer repair — 2026-09-16

- Connected the exact mapped 26.50 coordinator update (`0x1C9D140`), rebuild-all (`0x1C9C6D0`) and constructor-established vtable (`0xE7D5B80`). Require matching fingerprint, prologue bytes, vtable destructor and both native call sites before installing.
- Apply, restore and request loaded-chunk refresh on the engine's own coordinator thread, independently of overlay/menu ticks. Turning off restores through that same callback.
- Added actual ModuleManager-to-X-ray and native callback tests, including two bounded rebuild passes, original-callback forwarding, disable restoration and no per-frame refresh storms.
- Kept 26.50 Fullbright/old lighting code patches unavailable; kept ESP unavailable pending verified interfaces.
- Rebuilt from a fresh directory to eliminate mixed stale objects. In-world visual validation still requires a clean game restart to load the new DLL.

## X-ray lifecycle and ESP safety repair — 2026-09-16

- Replaced unbounded per-word X-ray memory queries with bounded writable-section window scans and vector-capacity checks.
- Restored adaptive X-ray overrides immediately on disable and destruction; preserve newer engine values.
- Added a synthetic real-code registry/lifecycle test. Live 26.50 registry and immediate mesh rebuild remain unresolved.
- Repaired old-version ESP readiness and the camera-fixture regression; 26.50 historical actor-list candidates are no longer called without ABI/layout/thread verification.
- This supersedes the prior section's claim that a signature match verifies thread-safe ESP. New-version ESP remains N/A.

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
