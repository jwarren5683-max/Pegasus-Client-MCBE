$ErrorActionPreference = 'Stop'

$knownVersion = '1.26.4501.0'
$knownTimestamp = [uint32]0x6A8378BA
$knownImageSize = [uint32]0x12888000

$process = Get-Process -Name 'Minecraft.Windows' -ErrorAction SilentlyContinue | Select-Object -First 1
$package = $null
$exePath = $null
$fileVersion = $null
$packageVersion = $null

if ($process) {
    try {
        $module = $process.MainModule
        $exePath = $module.FileName
        $fileVersion = $module.FileVersionInfo.FileVersion
    } catch {
        Write-Host 'Could not read the running Minecraft process metadata; trying the installed package instead.' -ForegroundColor Yellow
    }
}

# Modern Bedrock is an AppX/MSIX package. This fallback makes the report usable
# without asking the user to launch Minecraft first.
try {
    $package = Get-AppxPackage -Name 'Microsoft.MinecraftUWP' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $package) {
        $package = Get-AppxPackage | Where-Object { $_.Name -like '*Minecraft*' -and $_.InstallLocation } | Select-Object -First 1
    }

    if ($package) {
        $packageVersion = $package.Version.ToString()
        if (-not $exePath) {
            $candidate = Join-Path $package.InstallLocation 'Minecraft.Windows.exe'
            if (Test-Path -LiteralPath $candidate) {
                $exePath = $candidate
                $fileVersion = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($exePath).FileVersion
            }
        }
    }
} catch {
    Write-Host 'Installed-package lookup was unavailable.' -ForegroundColor Yellow
}

if (-not $exePath -or -not (Test-Path -LiteralPath $exePath)) {
    Write-Host 'Minecraft Bedrock was found neither as a readable running process nor as a readable installed package.' -ForegroundColor Red
    Write-Host 'If Minecraft is installed, launch it once and rerun this script.'
    exit 2
}

$stream = [System.IO.File]::Open($exePath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
$reader = New-Object System.IO.BinaryReader($stream)
try {
    $stream.Position = 0x3C
    $peOffset = $reader.ReadInt32()
    $stream.Position = $peOffset
    if ($reader.ReadUInt32() -ne 0x00004550) { throw 'Minecraft executable has no valid PE signature.' }

    # IMAGE_FILE_HEADER starts immediately after PE signature.
    $stream.Position = $peOffset + 8
    $timestamp = $reader.ReadUInt32()

    # IMAGE_OPTIONAL_HEADER64 starts at PE + 24. SizeOfImage is +56.
    $stream.Position = $peOffset + 24
    $magic = $reader.ReadUInt16()
    if ($magic -ne 0x20B) { throw ('Expected a 64-bit PE32+ executable; optional-header magic was 0x{0:X}.' -f $magic) }
    $stream.Position = $peOffset + 24 + 56
    $imageSize = $reader.ReadUInt32()
} finally {
    $reader.Dispose()
    $stream.Dispose()
}

$hash = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash
$exactLegacyProfile = ($timestamp -eq $knownTimestamp -and $imageSize -eq $knownImageSize)

Write-Host ''
Write-Host 'Pegasus / Minecraft Bedrock compatibility report' -ForegroundColor Cyan
if ($process) { Write-Host ('Process ID       : {0}' -f $process.Id) }
if ($package) { Write-Host ('Package          : {0}' -f $package.PackageFullName) }
if ($packageVersion) { Write-Host ('Package version  : {0}' -f $packageVersion) }
Write-Host ('Executable       : {0}' -f $exePath)
Write-Host ('File version     : {0}' -f $fileVersion)
Write-Host ('PE timestamp     : 0x{0:X8}' -f $timestamp)
Write-Host ('PE image size    : 0x{0:X8}' -f $imageSize)
Write-Host ('SHA-256          : {0}' -f $hash)
Write-Host ('Legacy profile   : version {0}, timestamp 0x{1:X8}, image 0x{2:X8}' -f $knownVersion, $knownTimestamp, $knownImageSize)
Write-Host ''

if ($exactLegacyProfile) {
    Write-Host 'RESULT: This executable matches the Pegasus 1.26.4501.0 PE profile.' -ForegroundColor Green
    exit 0
}

Write-Host 'RESULT: This executable does NOT match the Pegasus 1.26.4501.0 native-hook profile.' -ForegroundColor Yellow
Write-Host 'Native modules must remain disabled until the new build-specific targets are validated.'
exit 1
