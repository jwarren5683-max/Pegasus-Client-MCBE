# Auto Bridge test candidate — Minecraft Bedrock 1.26.5101.0

This is a separate, reversible test build. Existing Pegasus clients are not
replaced.

## How it works

Auto Bridge is a guarded input assist, not a native placement or inventory
writer. While the module is enabled, it emits a normal right-click pulse about
every 110 ms only when all of these are true:

- Minecraft is the foreground window and the player is interactive.
- The session has not been classified as a remote server.
- The player holds **V + W + Space** and looks at a downward angle.
- A valid hotbar slot is selected.

The user still controls movement, aim, block selection, and the jump. The game
performs normal placement and inventory checks, so it cannot place without a
usable block stack or against an invalid face. Release **V** immediately to
stop the pulses. Do not aim at menus or containers while testing.

## Compatibility and limits

Minecraft 1.26.5101.0 does not expose a verified placement/inventory API in the
current profile. Therefore this candidate deliberately avoids native placement
calls and does not claim automatic block-type detection. It is intended for a
local test world first; leave it disabled on public servers. If placement is
rejected, the normal game behavior is preserved.

The candidate was built from a clean x64 Release build. All 23 policy and
integration tests pass, including the new Auto Bridge fail-closed cadence test.

Candidate DLL SHA256:

`8A0347A55368C2B79601BC1E5F181AF108493D36A5A376268147C582AE2629E6`

