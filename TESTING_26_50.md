# Pegasus Bedrock 26.50 test guide

This build is a **compatibility test build**, not a claim that every native module supports 26.50 yet.

## First test

1. Extract the entire GitHub Actions ZIP into one folder.
2. Make sure `Pegasus.exe` and `BedrockUtilityFramework.Xray.dll` stay together in that folder.
3. Start Minecraft Bedrock and stop at the title screen.
4. Run `Pegasus.exe`.
5. If Pegasus reports that it loaded, return to Minecraft.
6. Press `Tab` once.

## What to check first

For the first run, only check whether:

- Minecraft stays open instead of crashing.
- Pegasus reports that the DLL loaded.
- Pressing `Tab` opens the Pegasus menu/overlay.

Do not assume a module works merely because its toggle appears. Native modules that cannot validate the installed Minecraft build are expected to remain unavailable/fail closed until their 26.50 targets are validated.

## What to report

Send back one of these descriptions:

- `A: Injector failed before loading the DLL.`
- `B: DLL loaded, but Minecraft crashed.`
- `C: DLL loaded and Minecraft stayed open, but Tab did nothing.`
- `D: DLL loaded and the Tab menu opened.`

If an error box appears, include a screenshot or copy the exact text. If the menu opens, that is enough for the first test; test native gameplay modules separately only after the basic load/overlay path is confirmed.
