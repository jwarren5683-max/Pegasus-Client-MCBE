# Building Pegasus

## Use the compiled version

Keep the repository-root `Pegasus.exe` and `BedrockUtilityFramework.Xray.dll` together. Their current SHA-256 checksums are recorded in `SHA256SUMS.txt`.

The compiled mod requires the x64 Visual Studio C++ debug runtimes. A standard Visual C++ Redistributable installation alone does not provide those debug libraries. See README for the full requirements.

## Build the injector

On Windows x64 with .NET Framework 4.8, run from the repository root:

```powershell
& '.\Source\Injector\build.ps1'
```

This creates `Pegasus.exe` at the repository root using the Windows .NET compiler. No NuGet packages are needed. The injector's `ExpectedHash` is pinned to the DLL uploaded with this release.

## Build the current mod source

Install Visual Studio 2022 with Desktop development with C++, a Windows SDK, and CMake 3.24 or newer. Open a developer PowerShell prompt in the repository root:

```powershell
cmake -S '.\Source\Mod' -B C:/pegasus-build -G 'Visual Studio 17 2022' -A x64
cmake --build C:/pegasus-build --config Debug
ctest --test-dir C:/pegasus-build -C Debug --output-on-failure
```

The output is `C:/pegasus-build/Debug/BedrockUtilityFramework.Xray.dll`.

The current source includes unfinished navigation development, which remains disabled because the required native interfaces are not verified. To use an intentionally rebuilt DLL with the injector, calculate its SHA-256 with `Get-FileHash`, update `ExpectedHash` in `Source/Injector/Pegasus.cs`, and rebuild the injector. Keep the resulting EXE and DLL together.

## Verification

The Enhanced DLL and injector passed a disposable-process injection check. The current source passes all 15 tests, including entity reach, Trigger Bot native attack dispatch, focus loss, and menu interaction. A clean Minecraft 1.26.4501.0 injection also confirmed that the supported native hooks installed. These checks do not override multiplayer server validation or certify behavior on every Minecraft version.

