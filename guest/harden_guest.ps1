# OnyxBox Guest Hardening Script
# Run INSIDE the Windows VM as Administrator after Windows installation.

#Requires -RunAsAdministrator

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  OnyxBox Guest Hardening" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# --- Helper: Generate random hex string ---
function Get-RandomHex {
    param([int]$Length = 32)
    $bytes = [byte[]]::new($Length / 2)
    [System.Security.Cryptography.RandomNumberGenerator]::Fill($bytes)
    return [System.BitConverter]::ToString($bytes) -replace '-', ''
}

# --- Helper: Generate random GUID ---
function Get-RandomGuid {
    return [System.Guid]::NewGuid().ToString()
}

# --- 1. MachineGuid ---
Write-Host "[1/6] Randomizing MachineGuid..." -ForegroundColor Yellow
$machineGuidPath = "HKLM:\SOFTWARE\Microsoft\Cryptography"
$newGuid = Get-RandomGuid
try {
    Set-ItemProperty -Path $machineGuidPath -Name "MachineGuid" -Value $newGuid -Force
    Write-Host "  [OK] MachineGuid → $newGuid" -ForegroundColor Green
} catch {
    Write-Host "  [FAIL] Could not set MachineGuid: $_" -ForegroundColor Red
}

# --- 2. ProductId ---
Write-Host "[2/6] Randomizing ProductId..." -ForegroundColor Yellow
$productIdPath = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion"
$newProductId = "{0:A}-{1}-{2}-{3}" -f @(
    (Get-RandomHex 10).ToUpper(),
    (Get-RandomHex 5).ToUpper(),
    (Get-RandomHex 5).ToUpper(),
    (Get-RandomHex 7).ToUpper()
)
try {
    Set-ItemProperty -Path $productIdPath -Name "ProductId" -Value $newProductId -Force
    Write-Host "  [OK] ProductId → $newProductId" -ForegroundColor Green
} catch {
    Write-Host "  [FAIL] Could not set ProductId: $_" -ForegroundColor Red
}

# --- 3. Remove VirtualBox registry artifacts ---
Write-Host "[3/6] Cleaning VirtualBox registry artifacts..." -ForegroundColor Yellow
$vboxRegPaths = @(
    "HKLM:\HARDWARE\DEVICEMAP\Scsi\Scsi Port 0",
    "HKLM:\HARDWARE\DEVICEMAP\Scsi\Scsi Port 1",
    "HKLM:\HARDWARE\DESCRIPTION\System\BIOS",
    "HKLM:\SYSTEM\CurrentControlSet\Services\VBoxGuest",
    "HKLM:\SYSTEM\CurrentControlSet\Services\VBoxMouse",
    "HKLM:\SYSTEM\CurrentControlSet\Services\VBoxSF",
    "HKLM:\SYSTEM\CurrentControlSet\Services\VBoxTray"
)
foreach ($path in $vboxRegPaths) {
    if (Test-Path $path) {
        try {
            Remove-Item -Path $path -Recurse -Force -ErrorAction SilentlyContinue
            Write-Host "  [DEL] $path" -ForegroundColor DarkYellow
        } catch {
            Write-Host "  [SKIP] Could not delete $path" -ForegroundColor DarkYellow
        }
    }
}

# --- 4. Disable hibernation ---
Write-Host "[4/6] Disabling hibernation..." -ForegroundColor Yellow
try {
    powercfg /h off | Out-Null
    Write-Host "  [OK] Hibernation disabled" -ForegroundColor Green
} catch {
    Write-Host "  [FAIL] Could not disable hibernation" -ForegroundColor Red
}

# --- 5. Set high performance power scheme ---
Write-Host "[5/6] Setting High Performance power scheme..." -ForegroundColor Yellow
try {
    powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c | Out-Null
    Write-Host "  [OK] High Performance active" -ForegroundColor Green
} catch {
    Write-Host "  [FAIL] Could not set power scheme" -ForegroundColor Red
}

# --- 6. Disable unnecessary services ---
Write-Host "[6/6] Disabling VM-related services..." -ForegroundColor Yellow
$services = @(
    @{Name="VBoxGuest"; Display="VirtualBox Guest Additions"},
    @{Name="VBoxTray"; Display="VirtualBox Tray"},
    @{Name="vmusbmouse"; Display="VMware USB Mouse"},
    @{Name="VMTools"; Display="VMware Tools"}
)
foreach ($svc in $services) {
    $s = Get-Service -Name $svc.Name -ErrorAction SilentlyContinue
    if ($s) {
        try {
            Stop-Service $s.Name -Force -ErrorAction SilentlyContinue
            Set-Service $s.Name -StartupType Disabled
            Write-Host "  [DISABLED] $($svc.Display)" -ForegroundColor DarkYellow
        } catch {
            Write-Host "  [SKIP] $($svc.Display) - could not disable" -ForegroundColor DarkYellow
        }
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Guest hardening complete!" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Recommended next steps:" -ForegroundColor White
Write-Host "  1. Run spoof_volume_serial.ps1" -ForegroundColor White
Write-Host "  2. Install Parsec or Moonlight for remote access" -ForegroundColor White
Write-Host "  3. Reboot" -ForegroundColor White
Write-Host "  4. Run verify_onyxbox.ps1 to confirm" -ForegroundColor White
Write-Host ""
pause
