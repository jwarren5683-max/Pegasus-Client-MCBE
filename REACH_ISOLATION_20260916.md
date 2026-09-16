# Reach / BlockReach isolated candidate, 2026-09-16

The user confirmed ChestESP and all currently repaired visual features work.
Confirmed desktop DLL SHA256:
F9DD3077AC41DC94A9C25DF43B6A3B8202E40C54E6358F39E1BBE213927D0D25.
That complete desktop package is backed up in
`Documents/Codex/Pegasus backups/2026-09-16-confirmed-all-before-isolated-reach`.

## Candidate changes

- Restore exact-fingerprint 26.50 Reach hooks for separate local-world testing.
- Never call legacy game-mode observation from 26.50 range callbacks. The
  verified LocalPlayer tick remains the owner of the ESP shared player reference.
- Read the game-mode player with ReadProcessMemory; only matching LocalPlayer
  range queries receive slider values. Missing, unreadable and server-player
  queries retain native ranges. Final distance changes also require LocalPlayer.
- Publish that ownership policy before publishing callable hooks.
- Preserve independent block and entity sliders, exact signature gates and
  instruction restoration. No shared float constant or server validation bypass.

## Verification status

New isolated-process regression cases cover local/foreign/null/unreadable
game modes and preservation of the shared ESP player context. Existing tests
cover independent sliders, exact native SSE gates, restoration and allocation.
Fresh build directory: `C:/pegasus-reach-isolated-20260916`.

This is a candidate, not an in-game success claim. Actual targeting, successful
block breaking and entity damage beyond vanilla distance require a fresh local
Survival world test, with one client loaded. Remote/private-server enforcement
can still reject longer interactions. Do not install over a loaded DLL.
Native executable captures remain local and must not be uploaded.
