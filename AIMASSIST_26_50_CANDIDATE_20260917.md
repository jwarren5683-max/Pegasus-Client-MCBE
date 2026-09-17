# Aim Assist test candidate — Minecraft Bedrock 1.26.5101.0

This is a separate, reversible local-test build. Existing Pegasus clients are
not replaced.

## Controls and behavior

Enable **Aim Assist** under Combat, then hold **C** while aiming with the
mouse. With an XInput controller, hold **LT** while aiming with the right stick.
The helper chooses the nearest validated player box within a 180-pixel screen
field of view and sends a small relative mouse correction (gain 0.28, capped at
10 pixels per tick). It never presses attack, changes inventory, writes native
game state, or changes camera sensitivity.

The correction is gated by the foreground Minecraft window, active gameplay
input, a live player/dimension pointer, and local-world classification. Release
the hold key or trigger to stop immediately. Navigation/menu focus suppresses
the module.

## Compatibility and limits

This first candidate is intended for a local test world on Bedrock
1.26.5101.0. The current profile does not yet provide a verified integrated-
server discriminator or an occlusion/line-of-sight query for the packed actor
snapshot. It therefore stays disabled when the session is classified as remote,
but users should still test only locally and should not assume that walls are
filtered. Only entities already validated as player boxes are considered;
non-player entities are ignored. XInput controllers are supported; DirectInput
or PlayStation pads require an XInput emulation layer.

The candidate was built as x64 Release. The policy and integration test suite
must pass before packaging. Do not load it while another Pegasus DLL is already
loaded in Minecraft; save and close the game first.

Candidate DLL SHA256:

`BCF51E3E420ED25AFB2E822581EDC0CB62256C1FF9CFE7D3A11314B824C07496`

Candidate launcher SHA256:

`BDC4F69E434F1034AC324DBBAFDD41A9E7FFC3331A6E26A92AF298FB5090BB7D`

