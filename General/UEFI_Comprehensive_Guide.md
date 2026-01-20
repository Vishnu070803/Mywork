# UEFI Comprehensive Guide
## Based on `_A_01_UEFI_Boot_Flow_Pres.pdf` & `_A_02_UEFI_Aware_OS_Pres.pdf`

---

## Table of Contents
1. [Introduction to UEFI](#1-introduction-to-uefi)
2. [Platform Initialization (PI) Architecture](#2-platform-initialization-pi-architecture)
3. [Phase 1: SEC (Security)](#3-phase-1-sec-security)
4. [Phase 2: PEI (Pre-EFI Initialization)](#4-phase-2-pei-pre-efi-initialization)
5. [Phase 3: DXE (Driver Execution Environment)](#5-phase-3-dxe-driver-execution-environment)
6. [Phase 4: BDS (Boot Device Selection)](#6-phase-4-bds-boot-device-selection)
7. [Phase 5-7: TSL, RT, AL](#7-phases-5-7-tsl-rt-al)
8. [UEFI Services: Boot & Runtime](#8-uefi-services-boot--runtime)
9. [UEFI Aware OS](#9-uefi-aware-os)
10. [Key Concepts Deep Dive](#10-key-concepts-deep-dive)

---

## 1. Introduction to UEFI

### What is UEFI?
**UEFI (Unified Extensible Firmware Interface)** is a modern firmware specification that replaced the legacy BIOS. It defines:
*   **Interface**: Between platform firmware and the Operating System
*   **Boot Services**: APIs for hardware initialization and OS loading
*   **Runtime Services**: APIs available to the OS after boot
*   **Data Structures**: System Table, Protocol Database, Variable Storage

### Why UEFI Over BIOS?
| Feature | BIOS | UEFI |
|---------|------|------|
| Boot Mode | 16-bit Real Mode | 32/64-bit Protected/Long Mode |
| Disk Support | MBR (2TB limit) | GPT (9.4ZB limit) |
| Security | None | Secure Boot, Measured Boot |
| Extensibility | Limited (Option ROMs) | Protocol-based driver model (e.g., GOP) |
| User Interface | Text only | Graphics, Mouse support |

---

## 2. Platform Initialization (PI) Architecture

The **PI Specification** defines the internal firmware architecture. It complements UEFI (which defines firmware-to-OS interface).

### The 7 Boot Phases

```
Power On --> SEC --> PEI --> DXE --> BDS --> TSL --> RT --> AL --> Power Off
```

Each phase has:
*   **Input**: State from previous phase
*   **Responsibility**: Specific initialization tasks
*   **Output**: State passed to next phase

---

## 3. Phase 1: SEC (Security)

### Overview
**SEC** is the **Root of Trust**. It's the first code to execute after power-on.

### Execution Environment
*   **Location**: Executes from SPI Flash (XIP - Execute In Place)
*   **Memory**: NO RAM available yet
*   **Mode**: Starts in 16-bit Real Mode
*   **Language**: Assembly (cannot use C without a stack)

### Key Responsibilities

#### 1. Reset Vector
*   CPU fetches first instruction from **0xFFFFFFF0** (4GB - 16 bytes)
*   This address maps to the top of the SPI flash chip
*   Contains a `JMP` instruction to the actual SEC code

#### 2. Mode Transition
```
16-bit Real Mode (1MB addressable)
    ↓
32-bit Protected Mode (4GB addressable)
    ↓
[optionally] 64-bit Long Mode
```

#### 3. Temporary RAM (Cache-as-RAM / CAR)
**Problem**: Main RAM isn't initialized. No stack = No C code.

**Solution**: Configure CPU Cache as RAM
*   Disable cache eviction (No Eviction Mode - NEM)
*   CPU's L1/L2 cache becomes a small (typically 256KB-1MB) writable region
*   This provides a **stack** for C code in the next phase

#### 4. Root of Trust
*   **Measured Boot** (TPM): Extend PCR with hash of PEI firmware
*   **Verified Boot** (Boot Guard): Verify signature of PEI using hardware public key

### Data Passed to PEI
*   **Temporary RAM location** (stack pointer)
*   **Firmware Volume** (where to find PEI modules)
*   **Boot mode** (Cold boot, S3 resume, Recovery, etc.)

---

## 4. Phase 2: PEI (Pre-EFI Initialization)

### Overview
PEI is the **"Hardware Discovery"** phase. Its primary mission: **Initialize Main Memory (DRAM)**.

### Execution Environment
*   **Location**: Executes from Flash initially, then Temporary RAM
*   **Memory**: Cache-as-RAM (limited, ~256KB-1MB)
*   **Language**: C code now possible (we have a stack!)

### Architecture

```
PEI Foundation (Core)
    ↓ dispatches
PEIMs (Pre-EFI Initialization Modules)
    ↓ communicate via
PPIs (PEIM-to-PEIM Interfaces)
    ↓ produce
HOBs (Hand-Off Blocks)
```

### Key Components

#### 1. PEI Foundation (PEI Core)
*   The "kernel" of the PEI phase
*   Responsibilities:
    - Dispatch PEIMs in correct order
    - Provide **PEI Services** (memory allocation, PPI database)
    - Manage HOB list creation

#### 2. PEIMs (Modules)
*   Small, single-purpose drivers
*   Examples:
    - **CPU PEIM**: Initialize CPU (microcode, MTRRs)
    - **Memory Init PEIM**: THE critical one (Memory Reference Code - MRC)
    - **Platform PEIM**: Board-specific GPIO, chipset early init
*   **Format**: Terse Executable (TE) - compressed PE/COFF to save space

#### 3. PPIs (PEIM-to-PEIM Interfaces)
*   Lightweight function pointers
*   Identified by **GUIDs** (Globally Unique Identifiers)
*   PEI equivalent of "Protocols" (DXE phase)
*   Example: `EFI_PEI_CPU_IO_PPI` - Allows PEIMs to read/write I/O ports

#### 4. Dependency Expressions (DepEx)
*   Each PEIM has a dependency section: **"I need PPI X before I can run"**
*   PEI Dispatcher evaluates these (Reverse Polish Notation)
*   Example:
    ```
    DepEx: gEfiPeiMemoryDiscoveredPpiGuid
    Meaning: "Don't run me until memory is initialized"
    ```

### The Memory Initialization Process (MRC)

This is the **most complex and critical** part of PEI.

```
1. Detect DIMM SPD (Serial Presence Detect) via I2C
   ↓
2. Determine DDR type (DDR3/DDR4/DDR5), speed, capacity
   ↓
3. Training: Fine-tune DDR timing (Write Leveling, Read Training)
   ↓
4. Test memory (basic read/write checks)
   ↓
5. Create memory HOBs
   ↓
6. Install "Memory Discovered" PPI
```

**Training** can take 100ms-500ms! This is why cold boot is slower than warm boot (training results can be cached for S3 resume).

### HOBs (Hand-Off Blocks)

HOBs are the **ONLY** data structure passed from PEI to DXE.

**Structure**:
```c
typedef struct {
  UINT16  HobType;
  UINT16  HobLength;
  UINT32  Reserved;
} EFI_HOB_GENERIC_HEADER;
```

**Common HOB Types**:
*   **Memory Allocation HOB**: "I allocated this memory region for purpose X"
*   **Resource Descriptor HOB**: "This region is System Memory / MMIO / Reserved"
*   **Firmware Volume HOB**: "There's a firmware volume at address Y"
*   **CPU HOB**: "CPU has 8 cores, 2.4GHz"
*   **GUID Extension HOB**: Custom platform data

**Example**:
```c
HOB[0]: Type=Memory, Base=0x0, Length=0x80000000 (2GB)
HOB[1]: Type=FV, Base=0xFF000000 (DXE firmware volume)
HOB[2]: Type=CPU, Cores=4
HOB[...]: (linked list)
```

### Transition to DXE
1. PEI locates the **DXE Core** (from a Firmware Volume HOB)
2. Migrates stack from Cache-as-RAM to **Permanent RAM**
3. Calls `TempRamExit()`FSP - Tears down NEM mode, cache returns to normal
4. Jumps to DXE Entry Point, passing **HOB List Pointer**

---

## 5. Phase 3: DXE (Driver Execution Environment)

### Overview
DXE is the **"Service Production"** phase. It initializes the rest of the platform and produces UEFI Services.

### Execution Environment
*   **Location**: Permanent RAM
*   **Memory**: Full main memory available
*   **Language**: C/C++
*   **Binary Format**: PE/COFF (`.efi` files)

### Architecture

```
DXE Foundation (Core)
    ↓ consumes
HOB List (from PEI)
    ↓ dispatches
DXE Drivers
    ↓ produce/consume
Protocols (GUID-based interfaces)
    ↓ result in
Boot Services + Runtime Services
```

### DXE Foundation (DXE Core)

The "operating system" of firmware.

**Responsibilities**:
1.  **Dispatcher**: Load and execute DXE drivers in correct order
2.  **Global Coherency Domain (GCD)**: Manage memory/MMIO map
3.  **Protocol Database**: Registry of all installed protocols
4.  **Event/Timer Support**: Callbacks and scheduling
5.  **Boot & Runtime Services Tables**: The APIs exposed to OS

### DXE Drivers

**Types of Drivers**:
| Type | Purpose | Example |
|------|---------|---------|
| **Chip-set** | Initialize silicon | PCH, Memory Controller |
| **Bus Driver** | Discover child devices | PCI Bus Driver |
| **Device Driver** | Control specific device | USB Host Controller |
| **Platform Driver** | Board-specific | ACPI Tables, SMBIOS |

**Driver Binding Protocol**: The "attachment" mechanism
```c
EFI_DRIVER_BINDING_PROTOCOL {
  Supported();  // Can I drive this device?
  Start();      // Start managing this device
  Stop();       // Release this device
}
```

### Protocols

**What is a Protocol?**
*   An interface defined by a GUID
*   Contains function pointers and data
*   Similar to COM objects or Linux kernel modules

**Example**: `EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL`
```c
struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
  EFI_TEXT_RESET                Reset;
  EFI_TEXT_STRING               OutputString;
  EFI_TEXT_TEST_STRING          TestString;
  EFI_TEXT_QUERY_MODE           QueryMode;
  EFI_TEXT_SET_MODE             SetMode;
  EFI_TEXT_SET_ATTRIBUTE        SetAttribute;
  EFI_TEXT_CLEAR_SCREEN         ClearScreen;
  EFI_TEXT_SET_CURSOR_POSITION  SetCursorPosition;
  EFI_TEXT_ENABLE_CURSOR        EnableCursor;
  SIMPLE_TEXT_OUTPUT_MODE       *Mode;
};
```

**To use**: `gBS->LocateProtocol(&gEfiSimpleTextOutProtocolGuid, NULL, &ConOut);`

### Architectural Protocols (APs)

**Definition**: The **mandatory** protocols that DXE Core waits for before entering BDS.

**List of Architectural Protocols**:
1.  **Security Architectural Protocol** - Image verification
2.  **CPU Architectural Protocol** - Exception handlers, interrupts
3.  **Metronome Architectural Protocol** - 10µs delay service
4.  **Timer Architectural Protocol** - System timer tick
5.  **BDS Architectural Protocol** - Boot Device Selection policy
6.  **Watchdog Timer Architectural Protocol** - Auto-reset on hang
7.  **Runtime Architectural Protocol** - Services for OS
8.  **Variable Architectural Protocol** - NVRAM variable storage
9.  **Variable Write Architectural Protocol** - Write access to variables
10. **Monotonic Counter Architectural Protocol** - Secure counter
11. **Reset Architectural Protocol** - Warm/Cold/Shutdown reset
12. **Real Time Clock Architectural Protocol** - Date/Time
13. **Capsule Architectural Protocol** - Firmware updates

Once **all** are installed, DXE Core signals `gEfiDxeArchProtocolGuid` event and transitions to BDS.

### Dependency Expressions in DXE

Like PEI, DXE drivers can have **DepEx** sections.

**Example**:
```
DXE_DRIVER UsbBusDxe {
  DEPEX = gEfiPciIoProtocolGuid AND gEfiDevicePathProtocolGuid
}
```

**Meaning**: "I need PCI bus to be enumerated first"

### GCD (Global Coherency Domain)

The **memory map manager**.

**Regions**:
*   **System Memory**: DRAM (managed with AllocatePages)
*   **Memory-Mapped I/O**: MMIO regions (PCI BAR space, LAPIC, etc.)
*   **Reserved**: Off-limits regions (Boot firmware volume, SMM memory)

**Services**:
*   `gDS->AddMemorySpace()` - Register a new region
*   `gDS->AllocateMemorySpace()` - Claim a region for specific use
*   `gDS->SetMemorySpaceAttributes()` - Set cacheability, access rights

---

## 6. Phase 4: BDS (Boot Device Selection)

### Overview
BDS is **"Connect and Launch"**. Connect all I/O, present UI, boot the OS.

### Key Responsibilities

#### 1. Connect Console
```c
gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid) --> GOP (Framebuffer)
gBS->ConnectController(UsbControllerHandle) --> USB Keyboards
```

**Result**: User can see output and provide input

#### 2. Execute Platform Policy
**Policy** = What to do before showing boot menu
*   Connect all storage devices?
*   Run network boot (PXE)?
*   Display logo?

Defined in **PlatformBdsLib**

#### Load Boot Options
**Boot Options** stored as NVRAM variables:
*   `Boot0000`, `Boot0001`, etc.
*   `BootOrder` = `{0002, 0000, 0001}` (try Boot0002 first)

Each option contains:
*   Device Path (e.g., `HD(1,GPT,GUID)/\EFI\ubuntu\grubx64.efi`)
*   Description ("Ubuntu")
*   Optional data (kernel command line)

#### 4. Boot Manager UI
If user interrupts (F2, F7, ESC):
*   Display **Setup Menu** (BIOS settings)
*   Display **Boot Menu** (select boot device)
*   Implemented using **HII (Human Interface Infrastructure)**

#### 5. Load and Start OS Loader
```c
EFI_HANDLE ImageHandle;
gBS->LoadImage(FALSE, ParentHandle, DevicePath, NULL, 0, &ImageHandle);
gBS->StartImage(ImageHandle, NULL, NULL);
```

**If successful**: Control passes to OS Loader (TSL phase)

---

## 7. Phases 5-7: TSL, RT, AL

### Phase 5: TSL (Transient System Load)

**Who's Running**: OS Loader (e.g., GRUB, Windows Boot Manager, systemd-boot)

**State**:
*   **Boot Services**: STILL AVAILABLE
*   OS Loader can call:
    - `gBS->AllocatePages()` - Allocate memory for kernel
    - `gBS->LoadImage()` - Load kernel image
    - `gBS->LocateProtocol()` - Access file systems, block devices
    - `gBS->GetMemoryMap()` - Get final memory layout

**The Critical Call**: `gBS->ExitBootServices(ImageHandle, MapKey)`

**What happens**:
1.  Firmware frees all Boot Services memory
2.  Disables interrupts managed by Boot Services
3.  Returns final memory map to OS
4.  **Point of No Return**: If successful, Boot Services are dead

**After `ExitBootServices()`**: Only Runtime Services remain

### Phase 6: RT (Runtime)

**Who's in Control**: Operating System Kernel

**UEFI's Role**: Minimal. Only **Runtime Services** available.

**Runtime Services Table**:
```c
typedef struct {
  // Time Services
  EFI_GET_TIME                  GetTime;
  EFI_SET_TIME                  SetTime;
  EFI_GET_WAKEUP_TIME           GetWakeupTime;
  EFI_SET_WAKEUP_TIME           SetWakeupTime;

  // Virtual Memory Services
  EFI_SET_VIRTUAL_ADDRESS_MAP   SetVirtualAddressMap;
  EFI_CONVERT_POINTER           ConvertPointer;

  // Variable Services
  EFI_GET_VARIABLE              GetVariable;
  EFI_GET_NEXT_VARIABLE_NAME    GetNextVariableName;
  EFI_SET_VARIABLE              SetVariable;

  // Miscellaneous
  EFI_GET_NEXT_HIGH_MONO_COUNT  GetNextHighMonotonicCount;
  EFI_RESET_SYSTEM              ResetSystem;

  // Firmware Update
  EFI_UPDATE_CAPSULE            UpdateCapsule;
  EFI_QUERY_CAPSULE_CAPABILITIES QueryCapsuleCapabilities;
  EFI_QUERY_VARIABLE_INFO       QueryVariableInfo;
} EFI_RUNTIME_SERVICES;
```

### Phase 7: AL (After Life)

**Trigger**: OS calls `ResetSystem(EfiResetShutdown)` or system crashes

**Activities**:
*   Write crash logs (if enabled)
*   Prepare for next boot (set recovery flag if needed)
*   Assert hardware reset signal

**Next**: Back to SEC phase (if reboot) or S5 (power off)

---

## 8. UEFI Services: Boot & Runtime

### Boot Services

**Available During**: SEC (limited) → PEI (PPI version) → DXE → BDS → TSL  
**Gone After**: `ExitBootServices()`

**Categories**:

#### Event, Timer, and Task Priority Services
```c
CreateEvent()         // Register callback
CloseEvent()
SignalEvent()
WaitForEvent()
SetTimer()            // Periodic or one-shot
```

#### Memory Management
```c
AllocatePages()       // Page-aligned allocation
FreePages()
AllocatePool()        // Malloc-like
FreePool()
GetMemoryMap()        // Query all memory regions
```

#### Protocol Handler Services
```c
InstallProtocolInterface()
UninstallProtocolInterface()
RegisterProtocolNotify()  // Callback when protocol installed
LocateHandle()            // Find handles with protocol
LocateProtocol()          // Direct pointer to protocol
```

#### Image Services
```c
LoadImage()           // Load PE/COFF into memory
StartImage()          // Execute loaded image
UnloadImage()
Exit()                // Return from image
ExitBootServices()    // THE CRITICAL ONE
```

#### Driver Support Services
```c
ConnectController()   // Bind drivers to device
DisconnectController()
```

### Runtime Services

**Available During**: DXE → BDS → TSL → **RT (forever)**  
**Never Gone**

#### Time Services
```c
GetTime()             // Read RTC
SetTime()             // Write RTC
GetWakeupTime()       // RTC alarm for wake-from-S5
SetWakeupTime()
```

#### Variable Services
```c
GetVariable(L"BootOrder", &gEfiGlobalVariableGuid, ...)
SetVariable(L"BootOrder", &gEfiGlobalVariableGuid, ...)
GetNextVariableName() // Enumerate all variables
QueryVariableInfo()   // Storage capacity
```

**Variable Attributes**:
*   `EFI_VARIABLE_NON_VOLATILE` - Persist across reboots
*   `EFI_VARIABLE_BOOTSERVICE_ACCESS` - Readable before `ExitBootServices()`
*   `EFI_VARIABLE_RUNTIME_ACCESS` - Readable after `ExitBootServices()`
*   `EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS` - Secure Boot

#### Virtual Memory Services
```c
SetVirtualAddressMap() // Tell firmware new virtual addresses
ConvertPointer()       // Update a pointer to virtual
```

#### Miscellaneous Services
```c
ResetSystem(EfiResetCold)      // Cold reboot
ResetSystem(EfiResetWarm)      // Warm reboot
ResetSystem(EfiResetShutdown)  // Power off
GetNextHighMonotonicCount()    // Anti-rollback counter
```

#### Capsule Services (Firmware Update)
```c
UpdateCapsule()                // Queue firmware update
QueryCapsuleCapabilities()
```

---

## 9. UEFI Aware OS

### What Makes an OS "UEFI Aware"?

1.  **Boot via UEFI Boot Manager** (not MBR boot sector)
2.  **Use Boot Services** during OS Loader phase
3.  **Call `ExitBootServices()` correctly**
4.  **Use Runtime Services** once kernel is running
5.  **Handle SetVirtualAddressMap** if using virtual memory

### The OS Loader's Job

#### Step 1: Initialize (Still in Boot Services)
```c
EFI_SYSTEM_TABLE *ST;  // Pointer passed by firmware
EFI_BOOT_SERVICES *BS = ST->BootServices;
EFI_RUNTIME_SERVICES *RT = ST->RuntimeServices;
```

#### Step 2: Load Kernel
```c
// Option A: Use Simple File System Protocol
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
BS->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid, NULL, &fs);
EFI_FILE_PROTOCOL *root, *kernelFile;
fs->OpenVolume(fs, &root);
root->Open(root, &kernelFile, L"\\vmlinuz", EFI_FILE_MODE_READ, 0);
// Read kernel into memory
kernelFile->Read(kernelFile, &kernelSize, kernelBuffer);

// Option B: Parse filesystem manually using Block I/O Protocol
```

#### Step 3: Get Memory Map
```c
UINTN memMapSize, mapKey, descriptorSize;
UINT32 descriptorVersion;
EFI_MEMORY_DESCRIPTOR *memMap;

BS->GetMemoryMap(&memMapSize, memMap, &mapKey, &descriptorSize, &descriptorVersion);
```

#### Step 4: Exit Boot Services
```c
Status = BS->ExitBootServices(ImageHandle, mapKey);
if (EFI_ERROR(Status)) {
  // Memory map changed! Get new map and try again
  BS->GetMemoryMap(...);
  Status = BS->ExitBootServices(ImageHandle, newMapKey);
}
```

**Critical**: `mapKey` ensures atomicity. If memory map changes between `GetMemoryMap()` and `ExitBootServices()`, call fails.

#### Step 5: Switch to Virtual Memory (Kernel's Job)
```c
// Kernel sets up page tables
// Then tells firmware where runtime services are mapped

for (each runtime memory region in memMap) {
  region->VirtualStart = TranslateToVirtual(region->PhysicalStart);
}

RT->SetVirtualAddressMap(memMapSize, descriptorSize, descriptorVersion, memMap);
```

**After this**: All runtime service function pointers use virtual addresses.

### Using Runtime Services from OS

#### Example: Reading BIOS Settings
```c
// Linux kernel (drivers/firmware/efi/vars.c)
efi_status_t status;
efi_char16_t name[] = L"BootOrder";
efi_guid_t vendor = EFI_GLOBAL_VARIABLE_GUID;
u32 attributes;
unsigned long size = sizeof(boot_order);
u16 boot_order[10];

status = efi.get_variable(name, &vendor, &attributes, &size, boot_order);
```

#### Example: Requesting Firmware Update
```c
// Prepare capsule (new firmare image)
EFI_CAPSULE_HEADER capsule;
capsule.CapsuleGuid = FW_MGMT_CAPSULE_GUID;
capsule.CapsuleImageSize = fw_image_size;
capsule.Flags = CAPSULE_FLAGS_PERSIST_ACROSS_RESET;

RT->UpdateCapsule(&capsule, 1, physical_address);
RT->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, NULL);
// On next boot, firmware applies update
```

---

## 10. Key Concepts Deep Dive

### Firmware Volumes (FV)

**Definition**: Container for firmware code/data

**Structure**:
```
+---------------------------+
| FV Header                 |  (GUID, Size, Signature)
+---------------------------+
| FFS File 1 (DXE Core)     |
| | File Header             |
| | | Section: PE/COFF      |
+---------------------------+
| FFS File 2 (Driver)       |
| | File Header             |
| | | Section: Depex        |
| | | Section: PE/COFF      |
| | | Section: UI (name)    |
+---------------------------+
| ...                       |
+---------------------------+
```

**File Types**:
*   `EFI_FV_FILETYPE_PEI_CORE` - PEI Foundation
*   `EFI_FV_FILETYPE_PEIM` - PEI Module
*   `EFI_FV_FILETYPE_DXE_CORE` - DXE Foundation
*   `EFI_FV_FILETYPE_DRIVER` - DXE Driver

**Physical Location**: SPI flash chip, partitioned into regions

### GUIDs (Globally Unique Identifiers)

**Everything in UEFI uses GUIDs**:
*   Protocols: `EFI_LOADED_IMAGE_PROTOCOL_GUID`
*   Variables: `gEfiGlobalVariableGuid` (standard UEFI variables namespace)
*   HOB types: `gEfiHobMemoryAllocBspStoreGuid`

**Format**: `{12345678-1234-1234-1234-123456789ABC}`

**In Code**:
```c
#define EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID \
  { 0x387477c2, 0x69c7, 0x11d2, \
    { 0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b } }
```

### Device Paths

**Purpose**: Universal way to describe hardware location

**Example**: `PciRoot(0x0)/Pci(0x1F,0x2)/Sata(0x0,0xFFFF,0x0)/HD(1,GPT,GUID,0x800,0x100000)`

**Translation**: "SATA port 0 → Partition 1 (GPT, starts at sector 0x800)"

**Usage**: Boot variables, loading drivers, identifying devices

### ACPI Tables

**Generated in**: DXE phase  
**Consumed by**: OS kernel  
**Purpose**: Describe hardware to OS

**Key Tables**:
*   **MADT** (APIC): CPU topology, interrupt routing
*   **MCFG**: PCIe configuration space
*   **SRAT/SLIT**: NUMA topology
*   **BGRT**: Boot logo image
*   **FADT**: Fixed ACPI Description (power button, RTC, etc.)

**How it works**:
1.  DXE driver calls `AcpiSupport` protocol
2.  Installs table: `InstallAcpiTable(&Madt, sizeof(Madt))`
3.  Firmware creates **EFI Configuration Table** entry
4.  OS finds it via System Table: `ST->ConfigurationTable[i].VendorGuid == ACPI_20_TABLE_GUID`

### SMBIOS

**Purpose**: Hardware inventory (for OS, apps)

**Examples**:
*   Type 0: BIOS Information (Vendor, Version, Release Date)
*   Type 1: System Information (Manufacturer, Product Name, UUID)
*   Type 4: Processor Information (Family, Frequency, Cores)
*   Type 17: Memory Device (Size, Speed, Manufacturer)

**In Linux**: `dmidecode` reads SMBIOS tables

---

## Summary: The Complete Flow

### Visual Overview

![UEFI Boot Flow Diagram](uefi_boot_flow_chart.png)

### Step-by-Step Flow

```
1. POWER ON
   ↓
2. SEC: Reset Vector → Protected Mode → Cache-as-RAM → Find PEI
   ↓
3. PEI: Dispatch PEIMs → Initialize DRAM → Create HOBs → Find DXE
   ↓
4. DXE: Load Drivers → Install Protocols → Produce Boot & Runtime Services → Wait for APs
   ↓
5. BDS: Connect consoles → Load boot options → Show UI (optional) → Load OS Loader
   ↓
6. TSL: OS Loader runs → Uses Boot Services → Loads kernel → Calls ExitBootServices()
   ↓
7. RT: OS Kernel runs → Only Runtime Services available → SetVirtualAddressMap()
   ↓
8. AL: OS shuts down → ResetSystem() → Back to step 1 (or S5 power off)
```

**Key Takeaway**: UEFI is a **layered, modular, protocol-based** architecture that separates responsibilities across distinct phases, enabling a clean transition from hardware initialization to OS control.

---

## 11. Common Confusions Clarified (FAQ)

This section addresses common misconceptions, especially for those coming from a Linux kernel driver development background.

### Q1: Are `.efi` Drivers the Same as Linux Drivers?

**Answer**: Conceptually similar, architecturally VERY different.

#### Similarities:
| Concept | Linux Driver | UEFI Driver |
|---------|--------------|-------------|
| **Purpose** | Manage hardware device | Manage hardware device |
| **Abstraction** | Provides interface to kernel | Provides Protocol to firmware |
| **Binding** | Probes device on bus | `Supported()` checks device |
| **Lifecycle** | `probe()`, `remove()` | `Start()`, `Stop()` |

#### Critical Differences:

**1. No Interrupts in UEFI**
```c
// Linux Driver - Interrupt-driven
static int my_probe(struct pci_dev *dev) {
    request_irq(dev->irq, my_handler, ...);  // ← Interrupt handling
    return 0;
}

// UEFI Driver - Polling only!
EFI_STATUS MyDriverStart(...) {
    // NO interrupts! Must poll the device
    while (!DataReady()) {  // Busy-wait loop
        gBS->Stall(10);     // 10 microsecond delay
    }
}
```

**2. Lifetime**
*   **Linux drivers**: Stay resident while OS runs
*   **UEFI drivers**: Most are **unloaded** after `ExitBootServices()`
    - Exception: Runtime drivers (for Variable/RTC services)

**3. Binary Format**
*   **Linux**: `.ko` files (ELF relocatable objects)
*   **UEFI**: `.efi` files (PE/COFF executables - Windows format!)

**4. Services Available**
*   **Linux**: Full kernel API (`kmalloc()`, `printk()`, `dma_alloc_coherent()`)
*   **UEFI**: Only Boot Services (`AllocatePool()`, `Print()`, no DMA abstraction)

**Conclusion**: UEFI drivers are "bootstrap drivers" - their job is to help the system boot, then get out of the way. Linux drivers are permanent system components.

---

### Q2: Why Are Software Interfaces Called "Protocols"?

**Answer**: Poor terminology choice by the UEFI specification authors.

#### Traditional Protocol (Wire/Communication Rules):
```
TCP/IP Protocol = Rules for transmitting packets over network
I2C Protocol    = Clock/Data line timing specification
SPI Protocol    = Master/Slave communication format (MISO/MOSI)
USB Protocol    = Electrical signaling + packet structure
```
These are **physical/logical communication rules**.

#### UEFI "Protocol" (Software Interface):
```c
// This is called a "Protocol" in UEFI
// But it's just a struct of function pointers!
typedef struct _EFI_USB_IO_PROTOCOL {
  EFI_USB_IO_CONTROL_TRANSFER        UsbControlTransfer;
  EFI_USB_IO_BULK_TRANSFER           UsbBulkTransfer;
  EFI_USB_IO_ASYNC_INTERRUPT_TRANSFER UsbAsyncInterruptTransfer;
  // ... more function pointers
} EFI_USB_IO_PROTOCOL;
```

**In other programming contexts, this would be called**:
*   **Interface** (Java, Go, TypeScript)
*   **Abstract Base Class** (C++)
*   **Trait** (Rust)
*   **COM Interface** (Windows)
*   **vtable** (C internals)

**Why "Protocol"?**
The UEFI specification wanted a formal-sounding name for "standardized API contract". Unfortunately, this creates confusion with hardware protocols.

**UEFI Terminology Translation**:
*   **Protocol** → Software API/Interface (struct with function pointers + GUID)
*   **Driver** → Code that *produces* (implements) a Protocol
*   **Consumer** → Code that *uses* a Protocol

**Example**:
```c
// A driver "produces" the USB I/O Protocol
gBS->InstallProtocolInterface(&Handle, 
                               &gEfiUsbIoProtocolGuid,  // ← GUID identifies the "interface"
                               &MyUsbIoInstance);        // ← Function pointers

// Another driver "consumes" it
EFI_USB_IO_PROTOCOL *UsbIo;
gBS->LocateProtocol(&gEfiUsbIoProtocolGuid, NULL, &UsbIo);
UsbIo->UsbBulkTransfer(...);  // Call the function pointer
```

---

### Q3: Are There Hidden Phases Beyond the 7 Main Phases?

**Answer**: Yes! There are pre-SEC hardware phases and parallel execution contexts.

#### Before SEC (Platform Hardware Sequencing):

```
0. Power-On Sequencing (Hardware)
   - VRMs bring up voltage rails (1.8V, 3.3V, 5V, 12V)
   - Crystal oscillators stabilize
   - Platform Controller Hub (PCH) initializes
   ↓
1. ME/CSME Boot (Intel Management Engine)
   - **Runs BEFORE main CPU**
   - Independent microcontroller on PCH
   - Verifies Boot Guard ACM (Authenticated Code Module)
   - Configures PCH, early GPIO
   - De-asserts CPURST# (releases main CPU from reset)
   ↓
2. Microcode Load (sometimes hardware-assisted)
   - CPU's internal ROM loader reads FIT (Firmware Interface Table)
   - Loads microcode patch from SPI flash
   ↓
3. ACM (Authenticated Code Module) - if Boot Guard enabled
   - Hardware TPM measures SEC phase code
   - Verifies signature using eFuses (hardware public key)
   ↓
Then SEC phase starts on main CPU...
```

#### Parallel Execution: SMM (System Management Mode)

**SMM** is a **hidden execution context** that runs alongside the normal boot phases:

```
Normal Flow:  SEC → PEI → DXE → BDS → TSL → RT (OS Running)
                              ↓
SMM Flow:              [SMM Drivers Load in DXE]
                              ↓
                       [SMM runs when SMI triggered]
                              ↓
                       [Continues to run even after OS boots]
```

**What is SMM?**
*   **CPU Mode**: x86 CPUs have a special mode (entered via SMI - System Management Interrupt)
*   **Isolated Memory**: SMRAM (not visible to OS)
*   **Purpose**: Handle platform events invisible to OS
    - USB legacy support (PS/2 emulation)
    - Thermal management (fan control)
    - Power button handling
    - Firmware variable storage backend

**Why it's "hidden"**:
*   The OS has **no visibility** into SMM execution
*   SMM code can access OS memory, but OS cannot access SMRAM
*   Security-critical: SMM bugs = full system compromise

#### Complete Boot Layering:

```
ME/CSME Boot (Coprocessor)
    ↓
Microcode Patch Load
    ↓
[Boot Guard ACM]
    ↓
SEC (Main CPU starts)
    ↓
PEI
    ↓
DXE
    ├─→ [SMM Drivers Load in Parallel]
    ↓
BDS → TSL → RT (OS)
    ↓
[SMM still running, invisible to OS]
```

---

### Q4: How Can PEI Initialize the CPU if the CPU is Running PEI?

**Answer**: The CPU is already running in a minimal state. "Initialize CPU" means configure advanced features, not "turn on the CPU".

#### The Bootstrap Reality:

```
Power On
    ↓
[Hardware] CPU Core 0 boots with MINIMAL capabilities:
           - 16-bit Real Mode (legacy 8086 compatibility)
           - Running from Reset Vector at 0xFFFFFFF0
           - NO microcode updates applied yet
           - Cache DISABLED or in default mode
           - All other cores (1, 2, 3...) are SLEEPING (in HALT state)
    ↓
[SEC] Minimal CPU setup:
      - Switch to 32-bit Protected Mode
      - Enable Cache-as-RAM (NEM - No Eviction Mode)
      - Provide a stack for C code
    ↓
[PEI] "Initialize CPU" actually means:
      CPU Core 0:
        ✓ Load microcode updates (patch silicon bugs)
        ✓ Configure MTRRs (Memory Type Range Registers - cache policy)
        ✓ Enable CPU features (SSE, AVX, Turbo Boost, C-states)
        ✓ Set up performance monitoring counters
      
      CPU Cores 1-N (Application Processors):
        ✓ Send INIT-SIPI-SIPI sequence (wake-up IPIs)
        ✓ Each core runs startup code
        ✓ Synchronize configuration with Core 0 (BSP)
    ↓
[DXE/BDS] CPU is now "fully initialized"
          - All cores active and configured
          - Optimized for OS workloads
```

#### Analogy: Starting a Car

| Phase | CPU State | Car Analogy |
|-------|-----------|-------------|
| **Power On** | CPU exists, but idle | Car engine exists |
| **Reset Vector** | CPU fetches first instruction | Starter motor cranks engine |
| **SEC** | CPU in minimal state (16-bit, no features) | Engine running, but in neutral, no accessories |
| **PEI "CPU Init"** | Load microcode, wake other cores, enable features | Shift into drive, turn on A/C, enable cruise control |
| **DXE/OS** | CPU fully operational | Car driving at highway speed |

**The key insight**: The CPU was never "off". It just started in a very limited state (like DOS mode on a modern PC). PEI's job is to unlock the advanced features and wake up the sleeping cores.

#### What PEI CPU Init Does NOT Do:
❌ Turn on the CPU (it's already on)  
❌ Start the clock (already started by hardware)  
❌ Initialize silicon transistors (that's physics/hardware)

#### What PEI CPU Init DOES Do:
✅ Patch CPU microcode (software updates to hardware)  
✅ Wake up secondary cores (Core 1, 2, 3... via SIPI interrupts)  
✅ Configure cache policies (via MTRRs)  
✅ Enable advanced instruction sets (SSE4, AVX2, etc.)  
✅ Set power management features (Turbo, C-states)

---

### Summary Table: Linux vs UEFI Terminology

| Concept | Linux Kernel | UEFI Firmware |
|---------|--------------|---------------|
| **Driver binary** | `.ko` (ELF) | `.efi` (PE/COFF) |
| **Interface/API** | `struct file_operations`, `struct device_driver` | **Protocol** (struct with GUIDs) |
| **Communication** | Interrupts, DMA | **Polling only** (no interrupts) |
| **Service layer** | Kernel APIs (`kmalloc`, `printk`) | **Boot Services** (`AllocatePool`, `Print`) |
| **Lifecycle** | Resident in memory | **Temporary** (unloaded after boot) |
| **Wire protocol** | TCP, I2C, SPI (actual protocols) | N/A (UEFI "protocol" ≠ wire protocol) |

**Key Insight for Linux Developers**: Think of UEFI as a "mini OS" that only exists to bootstrap the real OS. Drivers are temporary helpers, Protocols are function-pointer interfaces, and everything is polled (no interrupts).

