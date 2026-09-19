# Auto Bridge test candidate — Minecraft Bedrock 1.26.5101.0

This is a separate, reversible test build. Existing Pegasus clients are not
replaced.

## How it works

Auto Bridge is a guarded input assist, not a native placement or inventory
writer. While the module is enabled, it emits a normal right-click pulse about
every 110 ms only when all of these are true:

- Minecraft is the foreground window and the player is interactive.
- The session has not been classified as a remote server.
- The player holds **V + W + Space** and looks at a downward angle, or holds
  **LB/RB/RT + left-stick-forward + A** on an XInput controller while looking
  down with the right stick.
- A plausible hotbar slot is selected.

The user still controls movement, aim, block selection, and the jump. The game
performs normal placement and inventory checks, so it cannot place without a
usable block stack or against an invalid face. Release **V** immediately to
stop the pulses. Do not aim at menus or containers while testing.

## Compatibility and limits

Minecraft 1.26.5101.0 does not expose a verified placement/inventory API in the
current profile. Therefore this candidate deliberately avoids native placement
calls and does not claim automatic block-type detection. The hotbar index is
only a plausibility check; the selected item must be a building block. It is
intended for a local test world first; leave it disabled on public servers. If
placement is rejected, the normal game behavior is preserved.

The candidate was built from a clean x64 Release build. All 23 policy and
integration tests pass, including the new Auto Bridge fail-closed cadence test.

Candidate DLL SHA256:

`6B4A756CC816477C077EDD8CD524631ADC6B8E6D5BE9133CBC8D178029C618D9`

Candidate launcher SHA256:

`D1478C45D89987F2879D99F925FB704A0ACA18204C32FF27D5B3B179355C2AFC`

