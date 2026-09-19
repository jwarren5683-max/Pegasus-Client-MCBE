# Testing the Minecraft 26.50 repair build

1. Fully close Minecraft so no older Pegasus DLL remains loaded.
2. Extract the entire GitHub artifact into a new folder.
3. Start Minecraft Bedrock and leave it at the title screen.
4. Run `Pegasus.exe` with the same permission level as Minecraft and load the mod.
5. Press **Tab**. The menu should open without crashing.
6. Close Minecraft normally.
7. Read `%LOCALAPPDATA%\BedrockUtilityFramework\framework.log`.

For the recognized `1.26.5101.0` build, the log should include the PE profile followed by `26.50 probe:` entries. Those entries are read-only evidence used to port the existing modules. Features that still show `N/A` are deliberately disabled until their new targets are validated.

The optional `tools\BedrockCompatibilityScanner.exe` performs the same kind of read-only scan from outside the game. If Windows reports access denied, run it with the same permission level used for Pegasus.
