# Building Pegasus

## Use the compiled version

Keep the repository-root `Pegasus.exe` and `BedrockUtilityFramework.Xray.dll` together. Their current SHA-256 checksums are recorded in `SHA256SUMS.txt`.

Build and distribute the Release configuration so the DLL uses the standard x64 Visual C++ runtime rather than developer-only debug libraries.

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
cmake --build C:/pegasus-build --config Release --clean-first --parallel 4
ctest --test-dir C:/pegasus-build -C Release --output-on-failure
```

The output is `C:/pegasus-build/Release/BedrockUtilityFramework.Xray.dll`.

Always use a clean build for release packaging, especially after changing a
module header or when source files live under a temporary directory. A reused
local build combined an old Framework object with a larger ReachModule layout;
Windows subsequently reported heap corruption. Unit tests that compile Reach
in one file did not catch that cross-file mismatch. A separate-file allocation
canary test and runtime factory/implementation size guard now cover this risk.
Do not ship a DLL rebuilt from only selected stale objects.

The current source includes unfinished navigation development, which remains disabled because the required native interfaces are not verified. To use an intentionally rebuilt DLL with the injector, calculate its SHA-256 with `Get-FileHash`, update `ExpectedHash` in `Source/Injector/Pegasus.cs`, and rebuild the injector. Keep the resulting EXE and DLL together.

## Verification

The clean rebuilt DLL passes all 20 tests, including a separately compiled Reach allocation-canary test, exact 1.26.50 build classification, X-ray lifecycle, Fullbright restoration, Auto Leave threshold/one-shot behavior, remote-server policy, entity reach, Trigger Bot local input dispatch, focus loss, and menu interaction. The unsafe re-entrant native attack call was removed after repeatable crash reports. These checks do not override server authority or certify native behavior on every Minecraft version; live post-repair stability still requires verification.
