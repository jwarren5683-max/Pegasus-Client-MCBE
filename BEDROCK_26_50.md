# Minecraft Bedrock 26.50 repair status

Pegasus now recognizes the installed release build `1.26.5101.0` exactly:

- PE timestamp: `0x6AA482FD`
- PE `SizeOfImage`: `0x12C01000`

The existing client was built for `1.26.4501.0` (`0x6A8378BA`, `0x12888000`). The update changed the native executable, so old hook addresses remain fail-closed instead of being reused.

## Work completed

- Preserved the enhanced Reach, Trigger Bot, remote-session safety, and Auto Leave source work.
- Added centralized exact build classification for both 1.26.45 and 1.26.50.
- Added a read-only in-process compatibility probe. On 1.26.50 it records matching native byte patterns and candidate RVAs in `%LOCALAPPDATA%\BedrockUtilityFramework\framework.log` without patching the game.
- Added a standalone read-only scanner for the same signatures. It may need to be run with the same permission level as Minecraft.
- Kept every unverified native feature unavailable rather than applying a guessed address.
- Added CI packaging for the DLL, injector, scanner, tests, and documentation.

## Remaining port order

1. Validate the 1.26.50 player and GameMode identities from the probe output.
2. Restore basic player context and Fullbright first.
3. Restore movement and world/visual modules in small verified groups.
4. Revalidate combat modules last because their hooks carry the greatest crash risk.

Passing unit tests proves the client and safety policies compile and behave under controlled fixtures. An individual native module is only considered 1.26.50-compatible after its exact live signature and object layout are verified in a fresh Minecraft session.
