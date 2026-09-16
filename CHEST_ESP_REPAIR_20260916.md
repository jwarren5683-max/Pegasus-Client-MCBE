# ChestESP repair for Minecraft 1.26.5101.0

## Changes

- Connect ChestESP to the existing verified 26.50 LocalPlayer tick. Do not add
  another player hook or depend on the suspended Reach hooks. The working entity
  ESP, camera projection, X-ray and Fullbright renderer code is preserved.
- Keep ChestESP readiness separate from unsupported combat/movement features.
  Require the exact host fingerprint and block/chunk/source function prologues,
  plus validated local player membership and camera projection.
- Live read-only probing identified BlockSource getBlock 0x2D80170, getChunk
  0x2D7F490 and ChunkSource lookup 0x32F3600. Unwind boundaries and argument
  accesses were checked against the captured executable. getBlock takes RCX
  source and RDX xyz position; getChunk takes RCX source and RDX chunk xz.
- Container bounds moved eight bytes to BlockActor +0x50. Live probe confirmed
  matching positions and sane bounds, including the placed test containers.
  Dimension, loaded chunk map, block actor map and position/key layouts remain
  consistent with the prior path. Preserve the old +0x48 profile for 26.45.
- Read object metadata and identifiers with ReadProcessMemory instead of raw
  foreign-pointer reads. Reject invalid/stale positions, bounds and changing or
  broken lists; clear stale snapshots on world changes. Refresh at most 5 times
  per second with a 10-ms traversal deadline. Do not scan arbitrary world blocks,
  generate chunks, edit blocks, read chest contents, or modify device settings.
- Preserve separate toggles/colors for barrels, chests, shulker boxes, copper
  chests and trapped chests. Double containers can contribute both native bounds.
  Read-only ChestESP can display loaded data already sent by a private server;
  it does not bypass server enforcement. Respect server rules.

## Testing

Fresh full Release build and forced fixture rebuild: all 22 tests passed.

New regression fixtures replay the real map/RPM traversal with mock native
lookups and both bounds profiles, negative coordinates, stale packed keys,
dimension transitions, broken lists and malformed bounds. Synthetic tests are
not proof of a visible in-game result. Live stages are native storage profile
verified, validated storage bounds published, projected edges submitted.

Enable ChestESP under Visual near a placed container. Right-click ChestESP to
check each storage family is enabled. Its box should disappear when the feature
or that family is turned off. Check entity ESP still works, and that X-ray and
Fullbright remain available. Private-server behavior and long-session stability
require separate testing.

The DLL remains pinned until Minecraft closes normally. Never inject this over
an existing loaded client. The prior user-confirmed ESP package must be kept in
a recoverable backup before replacing the desktop package. Executable captures
and world probe output stay local; only client sources/docs/releases are uploaded.
