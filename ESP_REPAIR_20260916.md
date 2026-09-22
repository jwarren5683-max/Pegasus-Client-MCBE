# 26.50 ESP repair — 2026-09-16

Current scope: ESP first; suspend the unsuccessful extended/block reach hooks.
Earlier experimental release notes are historical, not current readiness claims.

## Repair

- Exact host: Minecraft 1.26.5101.0, timestamp 0x6AA482FD, image size 0x12C01000.
- Read-only probing of the fresh local test world found 22 live client actors,
  including cats, villagers, golems, rabbits and pigs. ActorOwner packed storage,
  local membership, generation IDs, identifiers and local/client distinction
  were checked against live memory without modifying the game.
- The local player vtable is 0xE8E1BC0; tick slot +0xC0 points to 0x475EE60.
  Its unwind boundary, prologue and RCX/player accesses (including +0xD70 client)
  were checked in the captured executable. Hook only this exact slot using an
  atomic compare/exchange; always forward the original tick.
- ESP snapshots now run on that local tick, independently of Reach. Never call
  the ambiguous historical actor-list candidate or exchange STL objects with
  the game. Snapshot reads use ReadProcessMemory and reject tombstones, reused
  generations, wrong registries, oversized/malformed pools and changed headers.
- Camera fields moved eight bytes: ClientInstance basis +0x420, frustum +0x4A0,
  renderer +0x1C0. Renderer player +0x468 and origin +0x660 remain unchanged.
  The live probe validated orthonormal basis, projection scales 0.866/1.540 and
  an origin matching the player's eye position. Reject torn/world-change samples.
- Read-only ESP may display entities already sent by a private server. It does
  not request hidden entities or bypass server enforcement. Server rules still
  apply. Other combat/movement restrictions remain unchanged.
- Suspend 26.50 Reach registration for this release: no new range-gate patches.
- X-ray logs the precise failed native gate. If only block-registry discovery
  fails at attach, install the verified native hook and retry read-only discovery
  in bounded 25-ms slices on native updates instead of permanently reporting N/A.
  Never apply graphics until registry validation succeeds. Lighting instruction
  checks and reversible apply/restore remain required.

## Verification and use

The user confirmed this ESP repair works visibly in the local test world on
2026-09-16. This confirms the in-game result, not only synthetic/probe success.
Private-server behavior and long-session stability are not yet separately verified.

Synthetic tests and a live read-only probe are not proof of visible ESP boxes.
Fresh full Release build: all 21 tests passed, including packed snapshot safety,
RPM capture, shifted camera fixture and deferred block registry recovery.
Live log stages are: local registry/camera validated, entity bounds published,
projected edges submitted. The local-world visible result is now user-confirmed.
X-ray and Fullbright were user-confirmed working before the reach regression;
the startup-retry change needs a fresh-session visual regression check.

Use the launcher in the existing Desktop/Loki Client Folder/Native Xray Fix folder.
Keep its bundled DLL beside it. Do not inject into an already modified session:
the DLL and native callbacks remain pinned until Minecraft closes normally.
Leave Players only OFF for a mob test. Enable ESP in Visual; nearby mob outlines
should appear. Turning ESP off must remove them. Save/quit normally before any
replacement. No game force-termination, security, permissions, registry, driver,
WindowsApps ownership or device configuration changes are part of this repair.

Executable captures and world-memory probe output stay local and are not uploaded.

