# Pegasus Enhanced utility mod

Source and matching Windows binaries for the enhanced Pegasus utility mod and its desktop injector. See [BUILDING.md](BUILDING.md) for build and verification instructions and [CHANGELOG.md](CHANGELOG.md) for tested updates.

Minecraft `1.26.5101.0` (Bedrock 26.50) is now recognized by an exact executable profile. The overlay, injector, build pipeline, safety gates, and existing module source are preserved. Version-sensitive native modules remain `N/A` until their 26.50 targets are verified; this repair build records read-only signature evidence in the framework log instead of reusing 1.26.45 addresses.

## Notes

- Entity and block reach can be adjusted from 3 to 10 blocks in 0.5-block steps. The default remains 7.
- Extended reach and other gameplay-altering modules are limited to integrated/local worlds. In remote-server sessions they show `N/A` and any previously enabled state is turned off.
- Trigger Bot no longer calls `GameMode::attack` from inside the player tick. It emits a spaced left-button press only in an integrated/local world while Minecraft is foreground and gameplay input is active.
- **Auto Leave** appears under Combat. Its slider selects a threshold from 0.5 to 10 hearts in half-heart steps (default 4). Two valid low-health ticks trigger Bedrock's normal asynchronous leave flow once, returning to the world list without closing Minecraft.
- On remote servers, Fullbright, ArrayList, and Auto Leave remain available; combat automation, reach, X-ray, movement modification, ESP, Ghost Hand, and Baritone do not. Server rules remain authoritative, including rules about automatic actions.
- Phase module doesn't work, and the speed module works but is janky.
- You can type '.help' in the chat while the client is loaded in your game and it will show you some commands the client provides.
- Type '.loki' to show the current Loki build and the features confirmed working on 26.50.
- Server restrictions remain authoritative. This build does not attempt to evade anti-cheat or server-side validation.
- I am trying to add a baritone style autominer to the client but its not ready yet. I also would like to add many more modules in the future.
- If the client breaks, restart minecraft. This shouldn't happen in most circumstances. It can happen if you eject and re-inject the client however. 
- Feel free to use the hooks in this client and the reverse engineering research I've done to help you develop your own clients.

## Start

1. Keep `Pegasus.exe` and `BedrockUtilityFramework.Xray.dll` together in this folder.
2. Start a fresh Minecraft Bedrock 64-bit session. Native hooks remain signature-gated. Minecraft 1.26.4501.0 is the last fully verified native profile; 1.26.5101.0 is recognized and under active staged repair.
3. Open `Pegasus.exe`. Select your game session, then choose **Load utility mod**. Use Refresh if you started Minecraft afterward.
4. In a Minecraft world, press **Tab** for the menu (clickgui) or use the arrow keys to activate modules.

Restart Minecraft before loading again or switching DLL versions. 

## Requirements

- Windows 10/11 x64, with .NET Framework 4.8 for the injector.
- Release builds use the standard x64 Visual C++ runtime. GitHub artifacts are built in Release configuration so they do not require the developer-only debug runtime.
- The game must be able to read this folder. The injector grants packaged applications read/execute access to the bundled DLL only. A restricted parent folder can still prevent loading; use an accessible local folder if needed.
- If access is denied, run Pegasus with the same privilege level as Minecraft or administrator privledges.

## Release contents and source provenance

- `BedrockUtilityFramework.Xray.dll`: current tested Enhanced mod build.
- `Pegasus.exe`: x64 desktop injector with a dark purple GUI. It validates the bundled DLL's SHA-256 and uses the standard Windows DLL loader. It also supports `--inject` and a disposable-process `--self-test`.
- `Source/Injector`: complete C# GUI/injector source and build script.
- `Source/Mod`: full current C++ module source, tests, CMake configuration, and optional probe sources referenced by that configuration.
- `SHA256SUMS.txt`: checksums provided alongside the downloadable release assets.

The checked-in DLL is built from the current source represented by this update. Baritone/navigation and other version-sensitive features remain unavailable on 1.26.5101.0 until their required native interfaces have been verified.

## Build the injector

From PowerShell in this folder:

```powershell
& '.\Source\Injector\build.ps1'
```

The script uses the Windows .NET Framework compiler and writes `Pegasus.exe` here. No external packages are required. If intentionally distributing a different DLL, update `ExpectedHash` in the injector source before rebuilding.

## Build the mod source

Requires Visual Studio 2022 with Desktop development with C++, a Windows SDK, and CMake 3.24 or newer. From this folder, choose a short writable build directory:

```powershell
cmake -S '.\Source\Mod' -B C:/pegasus-build -G 'Visual Studio 17 2022' -A x64
cmake --build C:/pegasus-build --config Release
ctest --test-dir C:/pegasus-build -C Release --output-on-failure
```

The output is `C:/pegasus-build/Release/BedrockUtilityFramework.Xray.dll`. Update the injector's pinned hash and rebuild the injector whenever replacing this DLL.

