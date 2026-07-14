# OnyxBox Verification Script
# Run on BOTH the host (after installing OnyxBox) AND inside the VM (after hardening).
# Checks all known VM detection vectors.

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  OnyxBox Verification" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$allPass = $true
$isVM = $env:COMPUTERNAME -ne $null  # true if we're in any VM

# --- 1. SMBIOS / DMI ---
Write-Host "[1] SMBIOS/DMI Strings" -ForegroundColor Yellow
try {
    $manufacturer = (Get-WmiObject Win32_ComputerSystem).Manufacturer
    $model = (Get-WmiObject Win32_ComputerSystem).Model
    Write-Host "     Manufacturer: $manufacturer" -ForegroundColor White
    Write-Host "     Model: $model" -ForegroundColor White

    if ($manufacturer -match "innotek|VirtualBox|Oracle|VMware|QEMU|Bochs") {
        Write-Host "     [FAIL] VM manufacturer detected!" -ForegroundColor Red
        $allPass = $false
    } elseif ($model -match "Virtual|VMware|VirtualBox|QEMU|Bochs") {
        Write-Host "     [FAIL] VM model detected!" -ForegroundColor Red
        $allPass = $false
    } else {
        Write-Host "     [PASS] Clean" -ForegroundColor Green
    }
} catch {
    Write-Host "     [SKIP] SMBIOS check failed: $_" -ForegroundColor DarkYellow
}

# --- 2. ACPI / BIOS ---
Write-Host "[2] ACPI OEM ID" -ForegroundColor Yellow
try {
    $bios = Get-WmiObject Win32_BIOS
    Write-Host "     BIOS Vendor: $($bios.Manufacturer)" -ForegroundColor White
    Write-Host "     BIOS Version: $($bios.SMBIOSBIOSVersion)" -ForegroundColor White

    if ($bios.Manufacturer -match "innotek|VirtualBox|Oracle|VMware|QEMU|Bochs|VBOX") {
        Write-Host "     [FAIL] VM BIOS vendor detected!" -ForegroundColor Red
        $allPass = $false
    } else {
        Write-Host "     [PASS] Clean" -ForegroundColor Green
    }
} catch {
    Write-Host "     [SKIP] ACPI check failed: $_" -ForegroundColor DarkYellow
}

# --- 3. Disk ---
Write-Host "[3] Disk Model Strings" -ForegroundColor Yellow
try {
    $disks = Get-WmiObject Win32_DiskDrive
    foreach ($disk in $disks) {
        $model = $disk.Model
        Write-Host "     $($disk.Index): $model" -ForegroundColor White
        if ($model -match "VBOX|QEMU|VMware|Virtual") {
            Write-Host "     [FAIL] VM disk detected!" -ForegroundColor Red
            $allPass = $false
        }
    }
    if ($disks.Count -gt 0) {
        $hasFail = $disks | Where-Object { $_.Model -match "VBOX|QEMU|VMware|Virtual" }
        if (!$hasFail) { Write-Host "     [PASS] Clean" -ForegroundColor Green }
    }
} catch {
    Write-Host "     [SKIP] Disk check failed: $_" -ForegroundColor DarkYellow
}

# --- 4. MAC Address ---
Write-Host "[4] MAC OUI" -ForegroundColor Yellow
try {
    $nics = Get-WmiObject Win32_NetworkAdapter | Where-Object { $_.MACAddress -ne $null }
    $vmOuis = @("08:00:27", "00:0F:4B", "00:1C:42", "00:50:56", "00:05:69", "00:0C:29", "00:16:E3")
    $found = $false
    foreach ($nic in $nics) {
        $mac = $nic.MACAddress.ToUpper()
        $oui = $mac.Substring(0, 8)
        Write-Host "     $($nic.NetConnectionID): $mac" -ForegroundColor White
        foreach ($vmOui in $vmOuis) {
            if ($mac.StartsWith($vmOui.ToUpper())) {
                Write-Host "     [FAIL] VM OUI detected: $vmOui" -ForegroundColor Red
                $allPass = $false
                $found = $true
            }
        }
    }
    if (!$found) { Write-Host "     [PASS] Clean" -ForegroundColor Green }
} catch {
    Write-Host "     [SKIP] MAC check failed: $_" -ForegroundColor DarkYellow
}

# --- 5. CPUID Hypervisor Bit ---
Write-Host "[5] CPUID Hypervisor Bit" -ForegroundColor Yellow
try {
    # Use PowerShell to check CPUID via Win32_Processor
    $cpu = Get-WmiObject Win32_Processor
    Write-Host "     CPU: $($cpu.Name)" -ForegroundColor White

    # Check if we can detect hypervisor via Win32_ComputerSystem
    $hypervisorPresent = (Get-WmiObject Win32_ComputerSystem).HypervisorPresent
    if ($hypervisorPresent -eq $true) {
        Write-Host "     [WARN] HypervisorPresent = True (may be host VBS)" -ForegroundColor DarkYellow
        $allPass = $false
    } else {
        Write-Host "     [PASS] HypervisorPresent = False" -ForegroundColor Green
    }
} catch {
    Write-Host "     [SKIP] CPUID check failed: $_" -ForegroundColor DarkYellow
}

# --- 6. Backdoor Port (Host only, needs admin) ---
Write-Host "[6] VMMDev Backdoor Port (0x5050)" -ForegroundColor Yellow
try {
    # Quick test: try to connect to port 0x5050
    $socket = New-Object System.Net.Sockets.TcpClient
    $result = $socket.BeginConnect("127.0.0.1", 0x5050, $null, $null)
    $wait = $result.AsyncWaitHandle.WaitOne(500)
    if ($wait -and $socket.Connected) {
        Write-Host "     [FAIL] Port 0x5050 is open - VMMDev detected!" -ForegroundColor Red
        $allPass = $false
        $socket.Close()
    } else {
        Write-Host "     [PASS] Port 0x5050 not responding" -ForegroundColor Green
        $socket.Close()
    }
} catch {
    # Connection failed as expected
    Write-Host "     [PASS] Port 0x5050 not responding" -ForegroundColor Green
}

# --- 7. Guest Additions (VM only) ---
if ($isVM) {
    Write-Host "[7] Guest Additions Artifacts" -ForegroundColor Yellow
    $foundAdditions = $false

    # Check services
    $vboxServices = @("VBoxGuest", "VBoxTray", "VBoxMouse", "VBoxSF")
    foreach ($svc in $vboxServices) {
        $s = Get-Service -Name $svc -ErrorAction SilentlyContinue
        if ($s -and $s.Status -eq "Running") {
            Write-Host "     [FAIL] $svc service is running" -ForegroundColor Red
            $foundAdditions = $true
            $allPass = $false
        }
    }

    # Check registry
    $vboxRegPaths = @(
        "HKLM:\SYSTEM\CurrentControlSet\Services\VBoxGuest",
        "HKLM:\SYSTEM\CurrentControlSet\Services\VBoxMouse",
        "HKLM:\SYSTEM\CurrentControlSet\Services\VBoxSF"
    )
    foreach ($path in $vboxRegPaths) {
        if (Test-Path $path) {
            Write-Host "     [FAIL] Registry artifact: $path" -ForegroundColor Red
            $foundAdditions = $true
            $allPass = $false
        }
    }

    # Check device objects
    $vboxDevices = @("\\.\VBoxGuest", "\\.\VBoxGuestR0Lib")
    foreach ($dev in $vboxDevices) {
        try {
            $f = [System.IO.File]::Open($dev, 'Open', 'Read', 'None')
            $f.Close()
            Write-Host "     [FAIL] Device object: $dev" -ForegroundColor Red
            $foundAdditions = $true
            $allPass = $false
        } catch {
            # Expected: device not found
        }
    }

    if (!$foundAdditions) {
        Write-Host "     [PASS] No Guest Additions artifacts found" -ForegroundColor Green
    }

    # Check MachineGuid
    Write-Host "[8] MachineGuid" -ForegroundColor Yellow
    try {
        $guid = Get-ItemProperty -Path "HKLM:\SOFTWARE\Microsoft\Cryptography" -Name "MachineGuid" -ErrorAction Stop
        $guidValue = $guid.MachineGuid
        Write-Host "     $guidValue" -ForegroundColor White
        if ($guidValue -match "^[0]{8}-[0]{4}-[0]{4}-[0]{4}-[0]{12}$") {
            Write-Host "     [FAIL] Default/zero GUID detected!" -ForegroundColor Red
            $allPass = $false
        } else {
            Write-Host "     [PASS] Randomized" -ForegroundColor Green
        }
    } catch {
        Write-Host "     [SKIP] Could not read MachineGuid" -ForegroundColor DarkYellow
    }

    # Check Volume Serial
    Write-Host "[9] Volume Serial" -ForegroundColor Yellow
    try {
        $vol = Get-WmiObject Win32_LogicalDisk -Filter "DeviceID='C:'" | Select-Object VolumeSerialNumber
        $serial = $vol.VolumeSerialNumber
        Write-Host "     C:\ serial: $serial" -ForegroundColor White
        # VirtualBox default is often all-zeros or has a pattern
        if ($serial -eq "00000000") {
            Write-Host "     [FAIL] Zero serial detected!" -ForegroundColor Red
            $allPass = $false
        } else {
            Write-Host "     [PASS] Randomized" -ForegroundColor Green
        }
    } catch {
        Write-Host "     [SKIP] Could not read volume serial" -ForegroundColor DarkYellow
    }
}

# --- Summary ---
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
if ($allPass) {
    Write-Host "  ALL CHECKS PASSED - OnyxBox is working!" -ForegroundColor Green
    Write-Host "  Your VM should not be detected as virtualized." -ForegroundColor Green
} else {
    Write-Host "  SOME CHECKS FAILED - Review the red entries above." -ForegroundColor Red
    Write-Host "  Run the check again after applying the recommended fixes." -ForegroundColor Yellow
}
Write-Host "========================================" -ForegroundColor Cyan

if ($allPass) {
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor White
    Write-Host "  - Install your game" -ForegroundColor White
    Write-Host "  - Use Parsec/Moonlight to play remotely" -ForegroundColor White
    Write-Host "  - The game should not detect a VM" -ForegroundColor White
}

Write-Host ""
pause
