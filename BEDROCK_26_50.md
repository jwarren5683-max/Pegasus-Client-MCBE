# Minecraft Bedrock 26.50 repair status

Loki now recognizes the installed release build `1.26.5101.0` exactly:

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
- Verified the 26.50 chat submit target (`0x4DE18B0`), input field (`ChatScreenController + 0xD60`), screen model (`+0xD48`), client handle layout, `GuiData` acquisition slot (`+0x6D0`), and local display target/ABI (`0x155F190`).
- Restored local period commands, `[Loki] Loaded`, and Death Position chat output with exact PE and byte fingerprints. Native calls are guarded and the returned UI references follow the cleanup sequence emitted by the game.
- Confirmed the existing 26.50 working set initializes after live injection: Reach, Block Reach, X-Ray, Fullbright, ESP, ChestESP, Auto Leave, and Auto Bridge. Other unverified modules remain `N/A`.

## Current scope

This release finalizes chat and the already-verified working module set. It deliberately does not port unsupported modules from 26.45 addresses. Those modules stay `N/A` until separately verified against an exact future target.

Passing unit tests proves the client and safety policies compile and behave under controlled fixtures. An individual native module is only considered 1.26.50-compatible after its exact live signature and object layout are verified in a fresh Minecraft session.
