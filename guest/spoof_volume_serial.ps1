# OnyxBox Volume Serial Spoofer
# Run INSIDE the Windows VM as Administrator.
# Randomizes the volume serial number of the system drive (or specified drive).

#Requires -RunAsAdministrator

param(
    [string]$DriveLetter = "C",
    [switch]$Force
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  OnyxBox Volume Serial Spoofer" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# --- Generate random serial ---
$random = [System.Random]::new()
$serial = $random.Next(0x10000000, 0x7FFFFFFF)
$serialHex = "0x{0:X8}" -f $serial
$serialDec = [Convert]::ToString($serial)

Write-Host "Target drive: $DriveLetter`:\" -ForegroundColor White
Write-Host "New serial:   $serialHex ($serialDec)" -ForegroundColor White
Write-Host ""

# --- Method 1: Use VolumeID.exe if available ---
$volumeIdPath = Get-Command "VolumeID.exe" -ErrorAction SilentlyContinue
if ($volumeIdPath) {
    Write-Host "[1/2] Using VolumeID.exe..." -ForegroundColor Yellow
    try {
        & $volumeIdPath.Source "${DriveLetter}:" $serialHex
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  [OK] Volume serial changed to $serialHex" -ForegroundColor Green
            Write-Host "       Reboot required for changes to take effect." -ForegroundColor Yellow
            exit 0
        }
    } catch {
        Write-Host "  [FAIL] VolumeID.exe failed: $_" -ForegroundColor Red
    }
} else {
    Write-Host "[1/2] VolumeID.exe not found. Download from Sysinternals:" -ForegroundColor Yellow
    Write-Host "       https://learn.microsoft.com/en-us/sysinternals/downloads/volumeid" -ForegroundColor Yellow
    Write-Host "       Place VolumeID.exe in the same directory as this script." -ForegroundColor Yellow
}

# --- Method 2: Use Windows API via C# (no external tools) ---
Write-Host "[2/2] Using direct API call..." -ForegroundColor Yellow
try {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public class VolumeSerial
{
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern int GetVolumeInformation(
        string lpRootPathName,
        StringBuilder lpVolumeNameBuffer,
        int nVolumeNameSize,
        out int lpVolumeSerialNumber,
        out int lpMaximumComponentLength,
        out int lpFileSystemFlags,
        StringBuilder lpFileSystemNameBuffer,
        int nFileSystemNameSize
    );

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool SetVolumeLabel(
        string lpRootPathName,
        string lpVolumeName
    );

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern int FormatDrive(
        int uType,
        uint uDriveID,
        int uCapacity,
        int uFlags
    );

    public static int GetSerial(string drive)
    {
        StringBuilder vol = new StringBuilder(256);
        StringBuilder fs = new StringBuilder(256);
        int serial, maxComp, flags;
        int result = GetVolumeInformation(drive, vol, 256, out serial, out maxComp, out flags, fs, 256);
        if (result == 0)
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        return serial;
    }
}
"@

    $oldSerial = [VolumeSerial]::GetSerial("${DriveLetter}:\")
    Write-Host "       Old serial: 0x{0:X8}" -f $oldSerial
    Write-Host "       Note: Changing volume serial without VolumeID.exe requires"
    Write-Host "       formatting the drive or using a third-party tool."
    Write-Host ""
    Write-Host "       Download VolumeID.exe from:" -ForegroundColor Yellow
    Write-Host "       https://live.sysinternals.com/VolumeID.exe" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "       Then run: VolumeID.exe ${DriveLetter}': $serialHex" -ForegroundColor Cyan
} catch {
    Write-Host "  [FAIL] $_" -ForegroundColor Red
}

Write-Host ""
pause
