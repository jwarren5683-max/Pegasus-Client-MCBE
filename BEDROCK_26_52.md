# Minecraft Bedrock 26.52 profile

This release targets the exact Windows executable for Minecraft
`1.26.5203.0` (Bedrock 26.52). Every native hook and patch is gated by the
build fingerprint and expected instruction bytes. A mismatch leaves the
affected feature unavailable.

## Available on the exact profile

- Reach and Block Reach
- X-ray and Fullbright
- ESP and ChestESP
- Auto Leave and Auto Bridge
- Local period commands and world-seed access

Remote-server restrictions remain unchanged. Features whose 26.52 native
interfaces have not been verified, including Phase, Speed, Ghost Hand,
AntiKnockback, Criticals, and Baritone, remain `N/A`.

## Verification completed

- Clean x64 Release build
- 26 of 26 automated tests
- Injector checksum and disposable-process self-test
- Live injection into `1.26.5203.0`
- Exact-profile hook initialization at the title screen with Minecraft still
  responsive

A fresh-session local-world test is still required to visually confirm each
gameplay and rendering effect.
