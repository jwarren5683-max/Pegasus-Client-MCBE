# Minecraft 1.26.50 ESP and X-ray test

This build adds version-specific compatibility paths for Minecraft Bedrock
`1.26.5101.0`.

## ESP

ESP resolves Bedrock's runtime actor-list function by a wildcarded executable
signature and requires exactly one match. The current player is accepted only
through the byte-verified GameMode range hook and structural pointer checks.
Camera and entity data are sanity-checked before a box is rendered.

## X-ray

X-ray discovers the live BlockGraphics registry instead of using the old
1.26.45 address. It requires a consistent object vtable, valid Minecraft block
identifiers, air, stone, and the configured ore set before becoming available.
It changes no world blocks. Existing chunk meshes update when Bedrock rebuilds
them; re-entering the world is the quickest full refresh after toggling.

## Test procedure

1. Fully close Minecraft and any older Pegasus launcher.
2. Start Minecraft and enter a local world.
3. Run `START THIS - Pegasus 26.50 ESP Xray Fix.exe` once.
4. Press Tab. Confirm ESP and x-ray do not show `N/A`.
5. Enable ESP near a mob. Enable x-ray, then re-enter the world if already
   loaded terrain has not refreshed.

If a required signature or registry validation fails, the affected module stays
`N/A` and writes the reason to
`%LOCALAPPDATA%\BedrockUtilityFramework\framework.log` rather than guessing an
address.

