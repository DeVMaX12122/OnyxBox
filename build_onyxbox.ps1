# OnyxBox Build Script
# Builds a patched VirtualBox from source on Windows.
# Run from the root of the cloned VirtualBox repository after applying patches.

param(
    [string]$QtDir = "",
    [string]$Config = "Release",
    [switch]$SkipConfigure
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  OnyxBox Build Script" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# --- Prerequisites ---
Write-Host "[1/5] Checking prerequisites..." -ForegroundColor Yellow

# Visual Studio
$vsWhere = "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vsWhere)) {
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
}
if (!(Test-Path $vsWhere)) {
    Write-Error "Visual Studio not found. Install Visual Studio 2022 Community with Desktop C++ workload."
    exit 1
}
$vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsPath) {
    Write-Error "Visual Studio C++ tools not found. Install the 'Desktop development with C++' workload."
    exit 1
}
Write-Host "  [OK] Visual Studio: $vsPath" -ForegroundColor Green

# Python
try {
    $py = Get-Command python3 -ErrorAction Stop
} catch {
    try {
        $py = Get-Command python -ErrorAction Stop
    } catch {
        Write-Error "Python 3 not found. Install from python.org"
        exit 1
    }
}
Write-Host "  [OK] Python: $($py.Source)" -ForegroundColor Green

# Qt
if ([string]::IsNullOrEmpty($QtDir)) {
    $possibleQtDirs = @(
        "C:\Qt\6.8.0\msvc2022_64",
        "C:\Qt\6.7.0\msvc2022_64",
        "C:\Qt\6.6.0\msvc2022_64",
        "C:\Qt\5.15.2\msvc2022_64",
        "C:\Qt\5.15.2\msvc2019_64"
    )
    foreach ($dir in $possibleQtDirs) {
        if (Test-Path "$dir\bin\qmake.exe") {
            $QtDir = $dir
            break
        }
    }
    if ([string]::IsNullOrEmpty($QtDir)) {
        Write-Error "Qt not found. Pass -QtDir parameter or install Qt from qt.io."
        exit 1
    }
}
Write-Host "  [OK] Qt: $QtDir" -ForegroundColor Green

# --- Validate source tree ---
Write-Host "`n[2/5] Validating source tree..." -ForegroundColor Yellow
if (!(Test-Path "configure.py") -or !(Test-Path "src\VBox")) {
    Write-Error "Not a VirtualBox source tree. Run this script from the root of the cloned repo."
    exit 1
}
Write-Host "  [OK] Source tree valid" -ForegroundColor Green

# --- Apply patches if not already applied ---
Write-Host "`n[3/5] Applying OnyxBox patches..." -ForegroundColor Yellow
$patchDir = Join-Path $PSScriptRoot ".." "patches"
$patchDir = Resolve-Path $patchDir
$patches = Get-ChildItem "$patchDir\*.patch"
$applied = $false
foreach ($patch in $patches) {
    $result = git apply --check $patch.FullName 2>&1
    if ($LASTEXITCODE -eq 0) {
        git apply $patch.FullName
        Write-Host "  [OK] $($patch.Name)" -ForegroundColor Green
        $applied = $true
    } else {
        Write-Host "  [SKIP] $($patch.Name) (already applied or not needed)" -ForegroundColor DarkYellow
    }
}
if (!$applied) {
    Write-Host "  [INFO] No new patches to apply." -ForegroundColor DarkYellow
}

# --- Configure ---
if (!$SkipConfigure) {
    Write-Host "`n[4/5] Configuring build..." -ForegroundColor Yellow
    $env:VBOX_WITH_QTDIR = $QtDir
    & $py configure.py --with-qt-dir="$QtDir" --enable-webservice
    if ($LASTEXITCODE -ne 0) {
        Write-Error "configure.py failed. Check the output for missing dependencies."
        exit 1
    }
    Write-Host "  [OK] Configuration complete" -ForegroundColor Green
} else {
    Write-Host "`n[4/5] Skipping configure (-SkipConfigure)" -ForegroundColor DarkYellow
}

# --- Build ---
Write-Host "`n[5/5] Building..." -ForegroundColor Yellow
$env:KBUILD_TYPE = $Config
$env:KBUILD_HOST = "windows"
$env:VBOX_CPU = "amd64"

# Set up VS environment
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" && set" | ForEach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
        Set-Content "env:\$($matches[1])" $matches[2]
    }
}

# Run kmk
if (Get-Command "kmk" -ErrorAction SilentlyContinue) {
    kmk
} else {
    & "$env:KBUILD_PATH\kmk\kmk.exe"
}

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed. Check the build output for errors."
    exit 1
}

# --- Collect output ---
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  Build Complete!" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$outDir = "out\windows.$Config\x86_64\$Config\bin"
if (Test-Path $outDir) {
    Write-Host "`nOutput directory: $outDir" -ForegroundColor Green
    Write-Host ""
    Write-Host "Key files:" -ForegroundColor White
    Get-ChildItem "$outDir\OnyxBox.exe" -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "  OnyxBox.exe" -ForegroundColor Cyan
    }
    Get-ChildItem "$outDir\VBoxC.dll" -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "  VBoxC.dll" -ForegroundColor Cyan
    }
    Get-ChildItem "$outDir\VBoxRT.dll" -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "  VBoxRT.dll" -ForegroundColor Cyan
    }
    Get-ChildItem "$outDir\VBoxSVC.exe" -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "  VBoxSVC.exe" -ForegroundColor Cyan
    }
    Write-Host ""
    Write-Host "Run as Admin to install:" -ForegroundColor Yellow
    Write-Host "  $outDir\OnyxBox.exe --install" -ForegroundColor Yellow
} else {
    Write-Warning "Build output not found at expected path: $outDir"
}

Write-Host ""
pause
