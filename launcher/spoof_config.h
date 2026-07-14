#pragma once
#include <windows.h>
#include <winioctl.h>

/* ─── SPOOF_CONFIG — matches Ophion's kernel struct exactly ─── */
#pragma pack(push, 1)
typedef struct {
    /* v1 fields */
    ULONGLONG SmbiosSerial;
    UCHAR     SmbiosUuid[16];
    WCHAR     SmbiosProductName[64];
    WCHAR     SmbiosManufacturer[64];
    UCHAR     MacAddress[6];
    USHORT    GpuVendorId;
    USHORT    GpuDeviceId;
    USHORT    TpmVendorId;
    USHORT    TpmDeviceId;
    ULONGLONG DiskSerial;
    ULONG     VolumeSerial;
    WCHAR     MachineGuid[64];
    WCHAR     ProductId[16];
    BOOLEAN   EnableSmbios;
    BOOLEAN   EnableDisk;
    BOOLEAN   EnableVolume;
    BOOLEAN   EnableMac;
    BOOLEAN   EnableGpu;
    BOOLEAN   EnableTpm;
    BOOLEAN   EnableRegistry;

    /* v2 extended config */
    BOOLEAN   EnableSpd;
    BOOLEAN   EnableTpmFifo;
    BOOLEAN   EnableUsbRegistry;
    UCHAR     SpdDimm0[38];
    UCHAR     SpdDimm1[38];
    UCHAR     SpdDimm2[38];
    UCHAR     SpdDimm3[38];
    UCHAR     SpdDimm4[38];
    UCHAR     SpdDimm5[38];
    UCHAR     SpdDimm6[38];
    UCHAR     SpdDimm7[38];
    ULONG     TpmFirmwareVersion;
    WCHAR     TpmVendorString1[5];
    WCHAR     TpmManufacturerStr[64];
    WCHAR     TpmVendorStr2[5];
    WCHAR     TpmVendorStr3[5];
    WCHAR     TpmVendorStr4[5];
    WCHAR     UsbVendorStr[64];
    WCHAR     UsbProductStr[64];
    WCHAR     UsbSerialStr[64];
    USHORT    UsbVidPid;
} SPOOF_CONFIG;
#pragma pack(pop)

/* ─── Ophion IOCTL codes ─── */
#define IOCTL_BASE          0x800
#define IOCTL_HV_STATUS     CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 0, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HV_SPOOF_CONF CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HV_VMXOFF     CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 2, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* ─── Helper: zero-initialized default config (ASUS UX305UA identity) ─── */
static SPOOF_CONFIG g_default_spoof = {
    .SmbiosSerial = 0xDEADBEEF12345678ULL,
    .SmbiosUuid = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x46, 0x77,
                   0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
    .SmbiosProductName = L"UX305UA",
    .SmbiosManufacturer = L"ASUS",
    .MacAddress = {0x00, 0x1B, 0x21, 0x3C, 0x4D, 0x5E},
    .GpuVendorId = 0x8086,
    .GpuDeviceId = 0x1916,
    .TpmVendorId = 0x11D4,
    .TpmDeviceId = 0xED5F,
    .DiskSerial = 0xDEADBEEF12345678ULL,
    .VolumeSerial = 0x12345678,
    .MachineGuid = L"F47AC10B-58CC-4372-A567-0E02B2C3D479",
    .ProductId = L"00330-80000-000",  /* must fit in WCHAR[16] */
    .EnableSmbios = TRUE,
    .EnableDisk = TRUE,
    .EnableVolume = TRUE,
    .EnableMac = TRUE,
    .EnableGpu = TRUE,
    .EnableTpm = TRUE,
    .EnableRegistry = TRUE,
    .EnableSpd = TRUE,
    .EnableTpmFifo = TRUE,
    .EnableUsbRegistry = TRUE,
    .TpmFirmwareVersion = 0x01260000,
    .TpmVendorString1 = L"STM ",
    .TpmManufacturerStr = L"STMicroelectronics",
    .TpmVendorStr2 = L"STM ",
    .TpmVendorStr3 = L"TPM ",
    .TpmVendorStr4 = L"2.0 ",
    .UsbVendorStr = L"Generic",
    .UsbProductStr = L"USB Input Device",
    .UsbSerialStr = L"1234567890AB",
    .UsbVidPid = 0x046D,
};
