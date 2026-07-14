# OnyxBox

**A VirtualBox fork with baked-in anti-detection for anti-cheat compatibility.**

OnyxBox patches 6 source files in the VirtualBox codebase to remove all VM detection artifacts that EAC, BattlEye, ACE, nProtect, Themida, VMProtect, and others use to identify virtualized environments. No binary patching, no kernel drivers, no unsigned code — just compiled open-source GPLv2 software.

---

## What OnyxBox Changes

| Detection Vector | File | Default → OnyxBox |
|-----------------|------|--------------------|
| **SMBIOS/DMI** | `DevFwCommon.cpp` | `"innotek GmbH"` → `"ASUS"`, `"VirtualBox"` → `"UX305UA"`, `"Oracle Corporation"` → `"ASUS"`, `"Virtual Machine"` → `"Desktop"` |
| **ACPI Tables** | `DevACPI.cpp` | `"VBOX  "` → `"ASUS "`, `"VBOX"` → `"ASUS"` |
| **Disk Identify** | `DevATA.cpp` | `"VBOX HARDDISK"` → `"Samsung SSD 980"`, `"VBOX"` → `"Samsung"`, serial `"VB"` → `"S5"` |
| **MAC OUI** | `HostImpl.cpp` | `"080027"` → `"001B21"` (Intel), `"000F4B"` → `"001B21"` |
| **CPUID Hypervisor** | `GIMR3Minimal.cpp` | HVP bit removed, hypervisor leaves zeroed |
| **VMMDev Backdoor** | `VMMDev.cpp` | I/O port 0x5050 handler removed |

### Not Patched (Guest-Side Fixes)

| Artifact | Fix |
|----------|-----|
| Volume serial | `guest/spoof_volume_serial.ps1` — randomize |
| MachineGuid | `guest/harden_guest.ps1` — random GUID |
| ProductId | `guest/harden_guest.ps1` — random value |
| Guest Additions | Don't install — use Parsec/Moonlight/Steam Link instead |

---

## Build Process

### Prerequisites

- **Visual Studio 2022 Community** with "Desktop development with C++" workload
- **Windows SDK** (10.0.22621.0+)
- **Python 3** (3.10+)
- **Qt 5.15.x** or **Qt 6.x** (download from qt.io)

### Steps

1. **Fork and clone:**
   ```
   git clone https://github.com/YOUR_USER/virtualbox.git onyxbox
   cd onyxbox
   ```

2. **Apply patches:**
   ```
   ..\OnyxBox\patches\apply_all.bat
   ```
   Or manually: `git apply ../OnyxBox/patches/*.patch`

3. **Build:**
   ```
   ..\OnyxBox\build\build_onyxbox.ps1
   ```
   Or manually:
   ```
   python configure.py --with-qt-dir=C:\Qt\6.x.x\msvc2022_64
   kmk
   ```

4. **Output:** `out\windows.x86_64\release\bin\OnyxBox.exe`

5. **Install:** Run as Administrator — performs normal VirtualBox install.

### Driver Signing

The VirtualBox kernel drivers (`VBoxDrv.sys`, `VBoxNetLwf.sys`, etc.) need to be signed. Two options:
- **Test mode:** `bcdedit /set testsigning on` (least hassle)
- **Self-sign:** Generate a self-signed cert and sign the drivers
- **KDMapper:** Load the drivers via KDMapper (if you already have it from Ophion)

---

## VM Setup

After installing OnyxBox:

1. **Create a VM** using the provided example config:
   ```
   copy configs\onyxbox_vm.vbox C:\Users\%USERNAME%\VirtualBox VMs\MyVM\
   ```

2. **Install Windows** normally in the VM.

3. **Do NOT install Guest Additions.**

4. **Run guest hardening:**
   ```
   # Inside the VM as Administrator
   .\guest\harden_guest.ps1
   .\guest\spoof_volume_serial.ps1
   ```

5. **Reboot** the VM.

6. **Install Parsec/Moonlight** for remote access.

---

## Verification

Run `.\verify_onyxbox.ps1` on the host AND inside the VM to confirm all vectors are clean.

Expected output:
```
[PASS] SMBIOS: "ASUS" found, no "innotek" or "VirtualBox"
[PASS] ACPI: no "VBOX" OEM ID
[PASS] Disk: no "VBOX" in model strings
[PASS] MAC: OUI is 00:1B:21 (Intel), not 08:00:27
[PASS] CPUID: Hypervisor bit not set
[PASS] Port 0x5050: No response
[PASS] MachineGuid: randomized
[PASS] Volume serial: randomized
```

---

## Known Limitations

- **Vanguard (Valorant):** Not bypassed — uses additional kernel-level checks beyond VM detection
- **Temperature/fan sensors:** OnyxBox doesn't emulate these; WMIC returns "N/A" (few anti-cheats check)
- **Timing attacks (RDTSC):** Not patched at source level; if EAC adds RDTSC-latency checks, a kernel-side RDTSC handler would be needed

---

## License

OnyxBox is VirtualBox (GPLv2) with patches. The patches are provided under the same GPLv2 license.
