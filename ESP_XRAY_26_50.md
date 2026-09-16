# Minecraft 1.26.50 ESP and X-ray test

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

The live 26.50 registry and immediate chunk rebuild integration remain
unverified. Existing meshes only update when Bedrock rebuilds them. Passing
these tests does not prove X-ray works in-game.

## Test procedure

1. Fully close Minecraft and any older Pegasus launcher.
2. Start Minecraft and enter a local world.
3. Run `START THIS - Pegasus 26.50 ESP Xray Fix.exe` once.
4. Check the log for registry discovery results. ESP is intentionally N/A on
   26.50 pending native-interface verification.
5. X-ray remains experimental; validate enable AND disable in a disposable
   local world, including a terrain refresh. Do not treat it as finished.

During this pass Minecraft had no visible window; the remaining process denied
memory access. No security, permissions or WindowsApps ownership was changed.
All 19 local tests passed. The prior GitHub build failed a camera fixture test;
that fixture regression is also repaired in this pass.

If a required signature or registry validation fails, the affected module stays
`N/A` and writes the reason to
`%LOCALAPPDATA%\BedrockUtilityFramework\framework.log` rather than guessing an
address.

