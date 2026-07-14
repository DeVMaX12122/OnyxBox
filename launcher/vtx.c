#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <intrin.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#define MAX_PROFILE_PATH 260
#define MAX_STR 512
#define MAX_VERIFY_STEPS 32
#define VTX_VERSION "0.1.0"

static char *g_driver_service_name;
static char g_verify_detail[MAX_STR];

/* ─── ANSI colors ─── */
#define CYN  "\x1b[36m"
#define GRN  "\x1b[32m"
#define YLW  "\x1b[33m"
#define RED  "\x1b[31m"
#define MAG  "\x1b[35m"
#define RST  "\x1b[0m"
#define BLD  "\x1b[1m"

/* ─── JSON parser (minimal, handles our format) ─── */
typedef enum { J_NULL, J_OBJ, J_ARR, J_STR, J_NUM, J_BOOL } JsonType;

typedef struct JsonNode {
    JsonType type;
    char *key;
    struct {
        char *str;
        double num;
        int boolean;
    } val;
    struct JsonNode *child;   /* first child (for obj/arr) */
    struct JsonNode *next;    /* sibling */
} JsonNode;

static JsonNode *js_alloc(void) {
    JsonNode *n = (JsonNode*)calloc(1, sizeof(JsonNode));
    if (!n) { fprintf(stderr, "OOM\n"); exit(1); }
    return n;
}

static const char *js_skip(const char *p) {
    while (*p && *p <= ' ') p++;
    return p;
}

static JsonNode *js_parse_val(const char **pp);

static JsonNode *js_parse_obj(const char **pp) {
    JsonNode head = {0}, *tail = &head;
    *pp = js_skip(*pp + 1);
    if (**pp == '}') { *pp = js_skip(*pp + 1); return 0; }
    while (1) {
        *pp = js_skip(*pp);
        if (**pp != '"') { fprintf(stderr, "Expected key in object\n"); exit(1); }
        const char *start = *pp + 1;
        const char *end = strchr(start, '"');
        if (!end) { fprintf(stderr, "Unterminated string\n"); exit(1); }
        int klen = (int)(end - start);
        *pp = js_skip(end + 1);
        if (**pp != ':') { fprintf(stderr, "Expected ':'\n"); exit(1); }
        *pp = js_skip(*pp + 1);
        JsonNode *n = js_parse_val(pp);
        n->key = (char*)malloc(klen + 1);
        memcpy(n->key, start, klen); n->key[klen] = 0;
        tail->next = n; tail = n;
        *pp = js_skip(*pp);
        if (**pp == '}') break;
        if (**pp != ',') { fprintf(stderr, "Expected ',' or '}'\n"); exit(1); }
        *pp = js_skip(*pp + 1);
    }
    *pp = js_skip(*pp + 1);
    return head.next;
}

static JsonNode *js_parse_arr(const char **pp) {
    JsonNode head = {0}, *tail = &head;
    *pp = js_skip(*pp + 1);
    if (**pp == ']') { *pp = js_skip(*pp + 1); return 0; }
    while (1) {
        JsonNode *n = js_parse_val(pp);
        tail->next = n; tail = n;
        *pp = js_skip(*pp);
        if (**pp == ']') break;
        if (**pp != ',') { fprintf(stderr, "Expected ',' or ']'\n"); exit(1); }
        *pp = js_skip(*pp + 1);
    }
    *pp = js_skip(*pp + 1);
    return head.next;
}

static JsonNode *js_parse_val(const char **pp) {
    *pp = js_skip(*pp);
    JsonNode *n = js_alloc();
    if (**pp == '{') { n->type = J_OBJ; n->child = js_parse_obj(pp); }
    else if (**pp == '[') { n->type = J_ARR; n->child = js_parse_arr(pp); }
    else if (**pp == '"') {
        n->type = J_STR; (*pp)++;
        const char *start = *pp;
        int len = 0;
        while (**pp && **pp != '"') { (*pp)++; len++; if (**pp == '\\') (*pp)++; }
        if (**pp == '"') { (*pp)++; }
        n->val.str = (char*)malloc(len + 1);
        memcpy(n->val.str, start, len); n->val.str[len] = 0;
    }
    else if (**pp == 't' || **pp == 'f') {
        n->type = J_BOOL; n->val.boolean = (**pp == 't');
        while (*pp && *((*pp)+1) && **pp >= 'a' && **pp <= 'z') (*pp)++;
        *pp = js_skip(*pp);
    }
    else if (**pp == 'n') {
        n->type = J_NULL;
        while (*pp && **pp != ',' && **pp != '}' && **pp != ']') (*pp)++;
    }
    else {
        n->type = J_NUM; n->val.num = strtod(*pp, (char**)pp);
    }
    return n;
}

static JsonNode *js_parse(const char *json) {
    const char *p = json;
    JsonNode *root = js_parse_val(&p);
    return root;
}

static JsonNode *js_find(JsonNode *n, const char *key) {
    if (!n) return 0;
    if (n->type == J_OBJ) n = n->child;
    while (n) {
        if (n->key && strcmp(n->key, key) == 0) return n;
        n = n->next;
    }
    return 0;
}

static const char *js_str(JsonNode *n) {
    return (n && n->type == J_STR) ? n->val.str : "";
}

static int js_bool(JsonNode *n) {
    return n && n->type == J_BOOL && n->val.boolean;
}

static void js_free(JsonNode *n) {
    if (!n) return;
    js_free(n->child);
    js_free(n->next);
    free(n->key);
    if (n->type == J_STR) free(n->val.str);
    free(n);
}

/* ─── Profile ─── */
typedef struct {
    char name[MAX_STR];
    char game[MAX_STR];
    int fHideProcess;
    int fMaskVmExitTiming;
    int fFullCpuidCache;
    int fRandomizeTsc;
    int fHideVboxPages;
    int fSpoofDebugRegs;
    char verify_items[MAX_VERIFY_STEPS][MAX_STR];
    int verify_count;
} Profile;

static Profile g_profile;
static char g_profiles_dir[MAX_PROFILE_PATH];
static char g_launcher_dir[MAX_PROFILE_PATH];

static void get_dirs(void) {
    GetModuleFileNameA(NULL, g_launcher_dir, sizeof(g_launcher_dir));
    char *p = strrchr(g_launcher_dir, '\\');
    if (p) *p = 0;
    snprintf(g_profiles_dir, sizeof(g_profiles_dir), "%s\\profiles", g_launcher_dir);
}

static int profile_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc(sz + 1);
    fread(buf, 1, sz, f); fclose(f);
    buf[sz] = 0;
    JsonNode *root = js_parse(buf);
    if (!root || root->type != J_OBJ) { free(buf); js_free(root); return -1; }
    strncpy(g_profile.name, js_str(js_find(root, "name")), MAX_STR-1);
    strncpy(g_profile.game, js_str(js_find(root, "game")), MAX_STR-1);
    JsonNode *oph = js_find(root, "ophion");
    if (oph && oph->type == J_OBJ) {
        g_profile.fHideProcess      = js_bool(js_find(oph, "fHideProcess"));
        g_profile.fMaskVmExitTiming = js_bool(js_find(oph, "fMaskVmExitTiming"));
        g_profile.fFullCpuidCache   = js_bool(js_find(oph, "fFullCpuidCache"));
        g_profile.fRandomizeTsc     = js_bool(js_find(oph, "fRandomizeTsc"));
        g_profile.fHideVboxPages    = js_bool(js_find(oph, "fHideVboxPages"));
        g_profile.fSpoofDebugRegs   = js_bool(js_find(oph, "fSpoofDebugRegs"));
    }
    JsonNode *vfy = js_find(root, "verify");
    if (vfy && vfy->type == J_ARR) {
        g_profile.verify_count = 0;
        for (JsonNode *c = vfy->child; c && g_profile.verify_count < MAX_VERIFY_STEPS; c = c->next) {
            if (c->type == J_STR)
                strncpy(g_profile.verify_items[g_profile.verify_count++], c->val.str, MAX_STR-1);
        }
    }
    js_free(root); free(buf);
    return 0;
}

static int profile_list(void) {
    WIN32_FIND_DATAA fd;
    char pattern[MAX_PROFILE_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*.json", g_profiles_dir);
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) { printf("  (no profiles found)\n"); return 0; }
    do {
        printf("  %s\n", fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return 0;
}

static int profile_apply(const char *name) {
    char path[MAX_PROFILE_PATH];
    snprintf(path, sizeof(path), "%s\\%s", g_profiles_dir, name);
    if (strchr(name, '.')) {}
    else snprintf(path, sizeof(path), "%s\\%s.json", g_profiles_dir, name);
    if (profile_load(path) != 0) {
        /* try with .json */
        snprintf(path, sizeof(path), "%s\\%s.json", g_profiles_dir, name);
        if (profile_load(path) != 0) {
            printf(RED "Profile not found: %s\n" RST, name);
            return -1;
        }
    }
    printf(GRN "Loaded profile: %s\n" RST, g_profile.name);
    printf("  Game: %s\n", g_profile.game);
    printf("  Hide Process:     %s\n", g_profile.fHideProcess     ? "yes" : "no");
    printf("  Mask VM-Exit TSC: %s\n", g_profile.fMaskVmExitTiming? "yes" : "no");
    printf("  Full CPUID Cache: %s\n", g_profile.fFullCpuidCache  ? "yes" : "no");
    printf("  Randomize TSC:    %s\n", g_profile.fRandomizeTsc    ? "yes" : "no");
    printf("  Hide VBox Pages:  %s\n", g_profile.fHideVboxPages   ? "yes" : "no");
    printf("  Spoof Debug Regs: %s\n", g_profile.fSpoofDebugRegs  ? "yes" : "no");
    return 0;
}

/* ─── Verify engine ─── */

typedef struct {
    const char *name;
    const char *label;
    int (*check)(void);
    int result; /* 0=pass, 1=fail, -1=skip */
    char detail[MAX_STR];
} VerifyStep;

static int check_smbios(void) {
    HKEY hk;
    char man[MAX_STR]="", mod[MAX_STR]="";
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &hk) == ERROR_SUCCESS) {
        DWORD sz = sizeof(man);
        RegQueryValueExA(hk, "SystemManufacturer", 0, 0, (BYTE*)man, &sz);
        sz = sizeof(mod);
        RegQueryValueExA(hk, "SystemProductName", 0, 0, (BYTE*)mod, &sz);
        RegCloseKey(hk);
    }
    int fail = (strstr(man, "innotek") || strstr(man, "Oracle") || strstr(mod, "VirtualBox"));
    snprintf(g_verify_detail, MAX_STR, "SMBIOS: %s / %s", man, mod);
    return fail;
}

static int check_mac(void) {
    PIP_ADAPTER_INFO pAdapterInfo = NULL;
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
    pAdapterInfo = (IP_ADAPTER_INFO*)malloc(ulOutBufLen);
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)malloc(ulOutBufLen);
    }
    int fail = 0;
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == NO_ERROR) {
        for (PIP_ADAPTER_INFO p = pAdapterInfo; p; p = p->Next) {
            if (p->AddressLength >= 3) {
                int vbox = (p->Address[0]==0x08 && p->Address[1]==0x00 && p->Address[2]==0x27);
                int vmw  = (p->Address[0]==0x00 && p->Address[1]==0x0C && p->Address[2]==0x29);
                int vms  = (p->Address[0]==0x00 && p->Address[1]==0x50 && p->Address[2]==0x56);
                if (vbox || vmw || vms) {
                    snprintf(g_verify_detail, MAX_STR, "MAC: %.2X:%.2X:%.2X:... VM prefix detected", 
                        p->Address[0], p->Address[1], p->Address[2]);
                    fail = 1; break;
                }
            }
        }
    }
    free(pAdapterInfo);
    if (!fail) snprintf(g_verify_detail, MAX_STR, "MAC: no VM prefix found");
    return fail;
}

static int check_port504(void) {
    WSADATA wsa;
    int fail = 0;
    if (WSAStartup(MAKEWORD(2,2), &wsa) == 0) {
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        if (s != INVALID_SOCKET) {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(504);
            addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                fail = 1;
                snprintf(g_verify_detail, MAX_STR, "Port 0x504: OPEN (VBox backdoor)");
            } else {
                snprintf(g_verify_detail, MAX_STR, "Port 0x504: closed");
            }
            closesocket(s);
        }
        WSACleanup();
    }
    return fail;
}

static int check_guest_additions(void) {
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    int fail = 0;
    if (scm) {
        SC_HANDLE svc = OpenServiceA(scm, "VBoxGuest", SERVICE_QUERY_STATUS);
        if (svc) {
            fail = 1;
            snprintf(g_verify_detail, MAX_STR, "VBoxGuest service: PRESENT");
            CloseServiceHandle(svc);
        } else {
            snprintf(g_verify_detail, MAX_STR, "VBoxGuest service: not found");
        }
        CloseServiceHandle(scm);
    }
    return fail;
}

static int check_disk(void) {
    HKEY hk;
    int fail = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\Scsi\\Scsi Port 0\\Scsi Bus 0\\Target Id 0\\Logical Unit Id 0", 0, KEY_READ, &hk) == ERROR_SUCCESS) {
        char id[MAX_STR]="";
        DWORD sz = sizeof(id);
        RegQueryValueExA(hk, "Identifier", 0, 0, (BYTE*)id, &sz);
        if (strstr(id, "VBOX") || strstr(id, "VMware")) {
            fail = 1;
            snprintf(g_verify_detail, MAX_STR, "Disk: %s (VM string)", id);
        } else {
            snprintf(g_verify_detail, MAX_STR, "Disk: %s", id);
        }
        RegCloseKey(hk);
    }
    return fail;
}

static int check_cpuid_hvp(void) {
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    int hvBit = (cpuInfo[2] >> 31) & 1;
    if (hvBit) {
        snprintf(g_verify_detail, MAX_STR, "CPUID.1.ECX[31] HVP bit: SET");
        return 1;
    }
    /* Check hypervisor leaves */
    __cpuid(cpuInfo, 0x40000000);
    char sig[13];
    memcpy(sig, &cpuInfo[1], 4);
    memcpy(sig+4, &cpuInfo[2], 4);
    memcpy(sig+8, &cpuInfo[3], 4);
    sig[12] = 0;
    int hyp = (cpuInfo[0] >= 0x40000000);
    if (hyp) {
        snprintf(g_verify_detail, MAX_STR, "CPUID hypervisor leaf 0x40000000: %.4s signature", sig);
        return 1;
    }
    snprintf(g_verify_detail, MAX_STR, "CPUID: clean (HVP=0, no hypervisor leaves)");
    return 0;
}

/* Ophion-dependent checks */
static int check_pci_vendor(void) {
    snprintf(g_verify_detail, MAX_STR, "PCI vendor: N/A (requires Ophion)");
    return -1;
}
static int check_tsc_timing(void) {
    snprintf(g_verify_detail, MAX_STR, "TSC timing: N/A (requires Ophion)");
    return -1;
}
static int check_memory_scan(void) {
    snprintf(g_verify_detail, MAX_STR, "Memory scan: N/A (requires Ophion)");
    return -1;
}

static VerifyStep g_verify_steps[] = {
    {"smbios",      "SMBIOS/DMI strings",       check_smbios},
    {"mac",         "MAC address prefix",        check_mac},
    {"port504",     "Backdoor port 0x504",       check_port504},
    {"guestadd",    "Guest Additions services",  check_guest_additions},
    {"disk",        "Disk model string",         check_disk},
    {"cpuid",       "CPUID HVP + hypervisor leaves", check_cpuid_hvp},
    {"pci_vendor",  "PCI vendor ID",             check_pci_vendor},
    {"tsc_timing",  "TSC timing analysis",       check_tsc_timing},
    {"memory_scan", "Memory/VMM signature scan", check_memory_scan},
    {0,0,0}
};

static int verify_run(int verbose) {
    int passed = 0, failed = 0, skipped = 0;
    printf(BLD "\nVTX Verify Report\n" RST);
    printf("==================\n\n");
    for (int i = 0; g_verify_steps[i].name; i++) {
        int should_run = 0;
        if (g_profile.verify_count == 0) should_run = 1;
        else for (int j = 0; j < g_profile.verify_count; j++)
            if (strcmp(g_verify_steps[i].name, g_profile.verify_items[j]) == 0) should_run = 1;
        if (!should_run) continue;
        g_verify_detail[0] = 0;
        int r = g_verify_steps[i].check();
        g_verify_steps[i].result = r;
        strncpy(g_verify_steps[i].detail, g_verify_detail, MAX_STR-1);
        const char *sym = "?";
        const char *color = RST;
        if (r == 0)  { sym = "PASS"; color = GRN; passed++; }
        else if (r == 1) { sym = "FAIL"; color = RED; failed++; }
        else { sym = "SKIP"; color = YLW; skipped++; }
        printf("  %s%s%s %s\n", color, sym, RST, g_verify_steps[i].label);
        if (verbose || r == 1) {
            if (g_verify_steps[i].detail[0])
                printf("         %s\n", g_verify_steps[i].detail);
        }
    }
    printf("\n  " BLD "%d passed" RST ", " RED "%d failed" RST ", " YLW "%d skipped" RST "\n\n", passed, failed, skipped);
    return failed;
}

/* ─── Build command ─── */
static int cmd_build(int argc, char **argv) {
    int build_all = 1, build_onyxbox = 0, build_ophion = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "onyxbox") == 0) { build_onyxbox = 1; build_all = 0; }
        if (strcmp(argv[i], "ophion") == 0)  { build_ophion = 1; build_all = 0; }
    }
    char cmd[2048];
    if (build_all || build_onyxbox) {
        printf("Building OnyxBox...\n");
        snprintf(cmd, sizeof(cmd), "powershell -NoProfile -ExecutionPolicy Bypass -File \"%s\\..\\build\\build_onyxbox.ps1\" 2>&1", g_launcher_dir);
        int rc = system(cmd);
        if (rc != 0) { printf(RED "OnyxBox build failed (rc=%d)\n" RST, rc); return 1; }
        printf(GRN "OnyxBox build OK\n" RST);
    }
    if (build_all || build_ophion) {
        printf("Building Ophion...\n");
        snprintf(cmd, sizeof(cmd), "powershell -NoProfile -ExecutionPolicy Bypass -File \"%s\\..\\subagent\\loader\\build_loader.ps1\" 2>&1", g_launcher_dir);
        int rc = system(cmd);
        if (rc != 0) { printf(RED "Ophion build failed (rc=%d)\n" RST, rc); return 1; }
        printf(GRN "Ophion build OK\n" RST);
    }
    return 0;
}

/* ─── Driver management ─── */
static const char *g_drivers[] = {"TdeIo64.sys", "DDDriver.sys", "IUForceDelete.sys", "AIDA64Driver.sys", 0};

static int driver_load(const char *name, const char *path) {
    /* Install as service and start */
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) { printf(RED "Can't open SCM (not admin?)\n" RST); return -1; }
    /* Generate a random service name */
    char svc[64];
    snprintf(svc, sizeof(svc), "VtxDrv%04x", rand() & 0xFFFF);
    SC_HANDLE h = CreateServiceA(scm, svc, svc, SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
        path, NULL, NULL, NULL, NULL, NULL);
    if (!h) {
        printf(RED "  Can't create service %s (already blocked?)\n" RST, name);
        CloseServiceHandle(scm);
        return -1;
    }
    if (!StartServiceA(h, 0, NULL)) {
        printf(RED "  Can't start %s (already blocked/patched)\n" RST, name);
        DeleteService(h); CloseServiceHandle(h); CloseServiceHandle(scm);
        return -1;
    }
    printf(GRN "  Loaded: %s (as %s)\n" RST, name, svc);
    g_driver_service_name = _strdup(svc);
    CloseServiceHandle(h);
    CloseServiceHandle(scm);
    return 0;
}

static int driver_unload(void) {
    if (!g_driver_service_name) return 0;
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) return -1;
    SC_HANDLE h = OpenServiceA(scm, g_driver_service_name, SERVICE_ALL_ACCESS);
    if (h) {
        SERVICE_STATUS ss;
        ControlService(h, SERVICE_CONTROL_STOP, &ss);
        DeleteService(h);
        CloseServiceHandle(h);
    }
    CloseServiceHandle(scm);
    free(g_driver_service_name);
    g_driver_service_name = NULL;
    return 0;
}

/* ─── Ophion injection ─── */
static int inject_ophion(void) {
    /* TODO: Use the loaded vulnerable driver to map Ophion.sys into the kernel.
       Requires physical memory read/write via the driver's IOCTL interface.
       Each driver has a different IOCTL code — will need driver-specific code.
       For now, this shells out to a separate mapper tool. */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "\"%s\\..\\subagent\\loader\\ophion_loader.exe\"", g_launcher_dir);
    printf("  Injecting Ophion via ghost loader...\n");
    int rc = system(cmd);
    if (rc != 0) {
        printf(RED "  Ophion injection failed. Fallback: use kdmapper manually.\n" RST);
        printf(YLW "  kdmapper Ophion.sys\n" RST);
        return -1;
    }
    printf(GRN "  Ophion injected OK\n" RST);
    return 0;
}

/* ─── Launch command ─── */
static int cmd_launch(int argc, char **argv) {
    const char *profile_name = "default";
    int skip_inject = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--profile") == 0 && i+1 < argc) profile_name = argv[++i];
        if (strcmp(argv[i], "--no-inject") == 0) skip_inject = 1;
    }
    if (profile_apply(profile_name) != 0) return 1;
    printf("\n" BLD "VTX Launch Sequence\n" RST);
    printf("=====================\n\n");
    /* Step 1: Load vulnerable driver */
    printf("1. Loading vulnerable driver...\n");
    int loaded = 0;
    for (int i = 0; g_drivers[i] && !loaded; i++) {
        char path[MAX_PROFILE_PATH];
        snprintf(path, sizeof(path), "%s\\drivers\\%s", g_launcher_dir, g_drivers[i]);
        if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
            printf(YLW "  (not found: %s)\n" RST, g_drivers[i]);
            continue;
        }
        if (driver_load(g_drivers[i], path) == 0) loaded = 1;
    }
    if (!loaded) {
        printf(RED "  No usable driver found. Drop a vulnerable .sys into launcher/drivers/\n" RST);
        return 1;
    }
    /* Step 2: Inject Ophion */
    if (!skip_inject) {
        printf("2. Injecting Ophion...\n");
        if (inject_ophion() != 0) {
            printf("3. Running verification anyway...\n\n");
            verify_run(1);
            driver_unload();
            return 1;
        }
    }
    /* Step 3: Apply Ophion config via IOCTL */
    printf("3. Applying Ophion config...\n");
    /* TODO: Send SPOOF_CONFIG_V3 via IOCTL to \\.\Ophion */
    HANDLE h = CreateFileA("\\\\.\\Ophion", GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        printf(GRN "  Connected to Ophion device\n" RST);
        /* Build config struct */
        /* ... IOCTL send ... */
        CloseHandle(h);
    } else {
        printf(YLW "  Ophion device not available (not injected?)\n" RST);
    }
    /* Step 4: Verify */
    printf("4. Running verification...\n");
    verify_run(1);
    /* Clean up driver (keep Ophion running) */
    driver_unload();
    printf(BLD "\nLaunch complete. Start your game.\n" RST);
    return 0;
}

/* ─── Verify command ─── */
static int cmd_verify(int argc, char **argv) {
    int verbose = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) verbose = 1;
        if (strcmp(argv[i], "--pafish") == 0) {
            /* Only run non-Ophion checks */
            printf("Running PAFish-style checks...\n");
        }
    }
    if (g_profile.verify_count == 0) {
        /* Load default profile for verify list */
        profile_apply("default");
    }
    return verify_run(verbose);
}

/* ─── Profile command ─── */
static int cmd_profile(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: vtx profile <list|apply|create>\n");
        return 0;
    }
    if (strcmp(argv[2], "list") == 0) return profile_list();
    if (strcmp(argv[2], "apply") == 0 && argc >= 4) return profile_apply(argv[3]);
    if (strcmp(argv[2], "create") == 0) {
        printf("Interactive profile creation (TODO)\n");
        return 0;
    }
    printf("Unknown profile subcommand: %s\n", argv[2]);
    return 1;
}

/* ─── Help ─── */
static void print_help(void) {
    printf(BLD "VTX Launcher v" VTX_VERSION RST " — Unified OnyxBox + Ophion tool\n\n");
    printf("Usage:\n");
    printf("  " BLD "vtx build" RST " [onyxbox|ophion]        Build project(s)\n");
    printf("  " BLD "vtx launch" RST " [--profile <name>]    Load driver + inject Ophion + verify\n");
    printf("                             [--no-inject]       Launch without Ophion injection\n");
    printf("  " BLD "vtx verify" RST " [--verbose|-v]         Run detection checks\n");
    printf("  " BLD "vtx profile" RST " list                  List available profiles\n");
    printf("  " BLD "vtx profile" RST " apply <name>          Load a profile\n");
    printf("  " BLD "vtx profile" RST " create                Create a new profile\n");
    printf("  " BLD "vtx help" RST "                          Show this help\n\n");
    printf("Profiles directory: %s\n", g_profiles_dir);
    printf("Drivers directory:  %s\\drivers\\\n", g_launcher_dir);
    printf("Place .sys files in launcher/drivers/ (not tracked by git)\n");
}

/* ─── Entry ─── */
int main(int argc, char **argv) {
    get_dirs();
    if (argc < 2) { print_help(); return 0; }
    if (strcmp(argv[1], "build")   == 0) return cmd_build(argc, argv);
    if (strcmp(argv[1], "launch")  == 0) return cmd_launch(argc, argv);
    if (strcmp(argv[1], "verify")  == 0) return cmd_verify(argc, argv);
    if (strcmp(argv[1], "profile") == 0) return cmd_profile(argc, argv);
    if (strcmp(argv[1], "help")    == 0 || strcmp(argv[1], "--help") == 0) { print_help(); return 0; }
    printf("Unknown command: %s\n", argv[1]);
    print_help();
    return 1;
}
