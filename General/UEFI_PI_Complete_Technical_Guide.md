# UEFI Platform Initialization: Complete Technical Guide
## Comprehensive Coverage of On-Line Web Training Materials

---

## Table of Contents
1. [UEFI & Platform Initialization (PI) Overview](#1-uefi--platform-initialization-pi-overview)
2. [PEI, DXE, and Firmware Volumes Deep Dive](#2-pei-dxe-and-firmware-volumes-deep-dive)
3. [UEFI Driver Model](#3-uefi-driver-model)
4. [User Interface: BDS and HII](#4-user-interface-bds-and-hii)

---

## 1. UEFI & Platform Initialization (PI) Overview

### 1.1 What is Platform Initialization?

**Platform Initialization (PI)** is the specification that defines how a **platform** (motherboard + chipset + CPU) boots from power-on to the point where an OS loader can run.

**Key Distinction**:
*   **UEFI Specification**: Defines the **interface between firmware and OS**
*   **PI Specification**: Defines the **internal architecture of the firmware itself**

Think of it as:  
`UEFI = API for OS`  
`PI = Internal implementation details`

### 1.2 The PI Architecture Layers

PI defines a layered architecture with clear separation of concerns. The architecture has evolved with **modern implementations using FSP (Firmware Support Package)**.

#### Legacy Model (Pre-FSP):

```
┌─────────────────────────────────────┐
│        Platform Specific            │  ← Board-specific code (GPIO, clocks)
├─────────────────────────────────────┤
│   Silicon/Chipset Code (Source)    │  ← Vendor provides C source code
├─────────────────────────────────────┤
│        PI Framework (Core)          │  ← PEI Core, DXE Core (generic)
├─────────────────────────────────────┤
│     CPU Architectural Code          │  ← x86, ARM, RISC-V specific
└─────────────────────────────────────┘
```

#### Modern Model (FSP-based) - Intel Platforms:

```
┌─────────────────────────────────────────────────┐
│         Platform Specific (Open Source)         │
│      Board Init, GPIO, Platform Policy          │
├─────────────────────────────────────────────────┤
│              PI Framework (Core)                │
│         PEI Core, DXE Core (EDK II)             │
├─────────────────────────────────────────────────┤
│  ┌───────────────────────────────────────────┐  │
│  │    FSP (Firmware Support Package)        │  │
│  │         Intel Binary Blob                 │  │
├───────────────────────────────────────────┤  │
│  │ FSP-T: Early init (Cache-as-RAM)         │  │
│  │ FSP-M: Memory init (MRC)                 │  │
│  │ FSP-S: Silicon init (PCH, chipset)       │  │
│  └───────────────────────────────────────────┘  │
├─────────────────────────────────────────────────┤
│         CPU Architectural Code (EDK II)         │
│            x86-64, IA32, ARM                    │
└─────────────────────────────────────────────────┘
```

#### How FSP Integration Works:

**FSP is NOT a layer you build on top of—it's a component you CALL INTO:**

```
Your PEI Code (Open Source)
    ↓
    Calls FSP-T API: TempRamInit()
    ↓
    ← FSP-T sets up Cache-as-RAM ←
    ↓
Your PEI Code continues
    ↓
    Calls FSP-M API: FspMemoryInit()
    ↓
    ← FSP-M trains memory (MRC) ←
    ↓
Your DXE Code runs
    ↓
    Calls FSP-S API: FspSiliconInit()
    ↓
    ← FSP-S initializes PCH, USB, SATA, etc. ←
```

**Benefits of Modern FSP Model**:
*   **Portability**: Core framework (EDK II) runs on any platform
*   **Binary Distribution**: Intel ships FSP as pre-compiled binary
    - Silicon initialization code remains proprietary
    - Faster updates (no source code needed)
*   **Modularity**: Replace only platform-specific parts
*   **Reusability**: Same FSP binary works across different boards with same chipset
*   **Security**: FSP is signed and measured (Boot Guard)

**AMD Equivalent**: AGESA (AMD Generic Encapsulated Software Architecture) - similar concept, binary blob for memory/silicon init

### 1.3 The Boot Phases (Revisited with PI Context)

```
Power → SEC → PEI → DXE → BDS → TSL → RT → AL
        ↓     ↓     ↓     ↓
        |     |     |     └─ Boot Device Selection
        |     |     └─ Driver Execution Environment
        |     └─ Pre-EFI Initialization
        └─ Security
```

**PI Focus**: SEC, PEI, and DXE are defined by the **PI Specification**.  
**UEFI Focus**: BDS, TSL, RT are defined by the **UEFI Specification**.

### 1.4 Foundation vs Modules

PI introduces the concept of **Foundations** (cores):

| Phase | Foundation | Modules |
|-------|-----------|---------|
| PEI | **PEI Foundation** (PeiCore) | PEIMs (Pre-EFI Initialization Modules) |
| DXE | **DXE Foundation** (DxeCore) | DXE Drivers |

**Foundation's Role**:
*   Dispatcher (load modules in correct order)
*   Service provider (memory allocation, protocol database)
*   Phase orchestrator (transitions to next phase)

**Module's Role**:
*   Do specific tasks (initialize USB, configure PCI, train memory)
*   Depend on foundation services
*   Communicate via PPIs (PEI) or Protocols (DXE)

---

## 2. PEI, DXE, and Firmware Volumes Deep Dive

### 2.1 Firmware Volumes (FV): The Storage Container

#### What is a Firmware Volume?

A **Firmware Volume** is a **logical file system** within the firmware binary. It's like a "file system in flash memory".

**Structure**:
```
┌───────────────────────────────────┐
│ FV Header (Signature, Size, GUID)│
├───────────────────────────────────┤
│ FFS File 0: SEC Core (RAW)        │
├───────────────────────────────────┤
│ FFS File 1: PEI Core (TE format)  │
├───────────────────────────────────┤
│ FFS File 2: PEIM (CPU Init)       │
│   ├─ DEPEX Section                │
│   ├─ PE32 Section (code)          │
│   └─ UI Section (name string)     │
├───────────────────────────────────┤
│ FFS File 3: PEIM (Memory Init)    │
├───────────────────────────────────┤
│ ...                               │
└───────────────────────────────────┘
```

#### Firmware File System (FFS)

**File Types**:
*   `EFI_FV_FILETYPE_RAW` - Raw data (no parsing)
*   `EFI_FV_FILETYPE_SEC_CORE` - SEC phase code
*   `EFI_FV_FILETYPE_PEI_CORE` - PEI Foundation
*   `EFI_FV_FILETYPE_PEIM` - PEI Module
*   `EFI_FV_FILETYPE_DXE_CORE` - DXE Foundation
*   `EFI_FV_FILE TYPE_DRIVER` - DXE Driver
*   `EFI_FV_FILETYPE_APPLICATION` - UEFI Application

**Section Types** (within a file):
*   `EFI_SECTION_PE32` - Executable code (PE/COFF format)
*   `EFI_SECTION_TE` - Terse Executable (compressed PE)
*   `EFI_SECTION_GUID_DEFINED` - Compressed/Encrypted data
*   `EFI_SECTION_DXE_DEPEX` - Dependency Expression
*   `EFI_SECTION_USER_INTERFACE` - String name (for debugging)

#### Multiple Firmware Volumes

A typical flash image contains **multiple FVs**:

```
┌─────────────────────────┐
│ FV_RECOVERY (fallback)  │  ← 0xFF800000
├─────────────────────────┤
│ FV_MAIN (SEC+PEI)       │  ← 0xFFC00000
├─────────────────────────┤
│ FV_DXE (DXE drivers)    │  ← 0xFFE00000
├─────────────────────────┤
│ FV_NV_VARIABLE (NVRAM)  │  ← 0xFFF00000 (top of flash)
└─────────────────────────┘
```

**Why Multiple FVs?**
*   **Size optimization**: DXE code can be compressed (needs memory to decompress)
*   **Recovery**: Keep SEC+PEI uncompressed for disaster recovery
*   **Security**: Different FVs can have different signatures

### 2.2 PEI Phase Deep Dive

#### PEI's Mission

**Primary Goal**: Get memory working (DRAM training).  
**Secondary Goals**:
*   Minimal CPU init (enable features)
*   Discover firmware volumes
*   Build HOB list for DXE

#### PEI Foundation Services

The PEI Foundation provides a **table of function pointers** (PEI Services Table):

```c
typedef struct _EFI_PEI_SERVICES {
  EFI_TABLE_HEADER                Hdr;
  
  // PPI Services
  EFI_PEI_INSTALL_PPI             InstallPpi;
  EFI_PEI_REINSTALL_PPI           ReInstallPpi;
  EFI_PEI_LOCATE_PPI              LocatePpi;
  EFI_PEI_NOTIFY_PPI              NotifyPpi;
  
  // Boot Mode Services
  EFI_PEI_GET_BOOT_MODE           GetBootMode;
  EFI_PEI_SET_BOOT_MODE           SetBootMode;
  
  // HOB Services
  EFI_PEI_GET_HOB_LIST            GetHobList;
  EFI_PEI_CREATE_HOB              CreateHob;
  
  // Firmware Volume Services
  EFI_PEI_FFS_FIND_NEXT_VOLUME    FfsFindNextVolume;
  EFI_PEI_FFS_FIND_NEXT_FILE      FfsFindNextFile;
  EFI_PEI_FFS_FIND_SECTION_DATA   FfsFindSectionData;
  
  // Memory Services
  EFI_PEI_INSTALL_PEI_MEMORY      InstallPeiMemory;
  EFI_PEI_ALLOCATE_PAGES          AllocatePages;
  EFI_PEI_ALLOCATE_POOL           AllocatePool;
  
  // ...
} EFI_PEI_SERVICES;
```

**Usage**:
```c
// Inside a PEIM
EFI_STATUS MyPeimEntry(
  IN EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES **PeiServices  // ← Pointer to service table
) {
  VOID *Buffer;
  (*PeiServices)->AllocatePool(PeiServices, 1024, &Buffer);
  // ...
}
```

#### PPIs (PEIM-to-PEIM Interfaces)

**What are PPIs?**
Lightweight "protocols" for PEI phase. Since we're in a resource-constrained environment, PPIs are simpler than DXE protocols.

**PPI Structure**:
```c
typedef struct {
  EFI_GUID  *Guid;   // Interface ID
  VOID      *Ppi;    // Pointer to function table
} EFI_PEI_PPI_DESCRIPTOR;
```

**Common PPIs**:
*   `gEfiPeiCpuIoPpiGuid` - Read/Write I/O ports and MMIO
*   `gEfiPeiMemoryDiscoveredPpiGuid` - Signals "RAM is ready"
*   `gEfiPeiStallPpiGuid` - Microsecond delay service
*   `gEfiPeiReadOnlyVariable2PpiGuid` - Read UEFI variables

**Example**: Installing a PPI
```c
EFI_PEI_PPI_DESCRIPTOR mMyPpiList = {
  (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gMyCustomPpiGuid,
  &mMyPpiInstance
};

(*PeiServices)->InstallPpi(PeiServices, &mMyPpiList);
```

#### Memory Initialization (MRC) Process

**Step-by-Step**:

1. **SPD Detection** (Serial Presence Detect)
   ```
   - PEIM uses I2C/SMBus to read SPD chips on DIMMs
   - SPD contains: DDR type, speed, timings, capacity, rank info
   ```

2. **Memory Controller Configuration**
   ```
   - Program DRAM controller registers
   - Set frequency, voltage, timings (CAS latency, RAS, etc.)
   ```

3. **Training** (The most complex part)
   ```
   Write Leveling:
     - Align DQ signals with DQS clocks (write timing)
     - Iterate through delays to find optimal settings
   
   Read Training:
     - Find the center of the "eye" (data valid window)
     - Test multiple read delays to find stable region
   
   Command/Address Training:
     - Similar process for CMD/ADDR bus
   ```

4. **Memory Test**
   ```
   - Quick pattern write/read (0xAA55, 0x55AA, walking 1s/0s)
   - Verify basic functionality
   ```

5. **Create HOBs**
   ```c
   BuildResourceDescriptorHob(
     EFI_RESOURCE_SYSTEM_MEMORY,
     EFI_RESOURCE_ATTRIBUTE_PRESENT |
     EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
     EFI_RESOURCE_ATTRIBUTE_TESTED,
     0x00000000,  // Base
     0x80000000   // 2GB length
   );
   ```

6. **Install "Memory Discovered" PPI**
   ```c
   (*PeiServices)->InstallPpi(PeiServices, &mMemoryDiscoveredPpi);
   ```

**Result**: All PEIMs waiting for memory can now execute.

#### HOBs (Hand-Off Blocks) Detailed

**HOB Types**:

| Type | Purpose | Example |
|------|---------|---------|
| **Resource Descriptor** | Describe memory regions | "0x0-0x80000000 is System Memory" |
| **Memory Allocation** | Reserved memory regions | "0x100000-0x200000 reserved for DXE" |
| **Firmware Volume** | Location of FVs | "FV_DXE is at 0xFFE00000" |
| **CPU** | CPU information | "4 cores, 2.4GHz" |
| **GUID Extension** | Custom platform data | Platform-specific config |

**HOB List Structure**:
```
HOB[0] → HOB[1] → HOB[2] → ... → End-of-HOB-List
  ↓        ↓        ↓
 Type=    Type=    Type=
Resource  FV      CPU
```

**Creating a HOB**:
```c
// In a PEIM
BuildGuidDataHob(
  &gMyPlatformDataGuid,
  &MyPlatformData,
  sizeof(MY_PLATFORM_DATA)
);
```

**Consuming HOBs** (in DXE):
```c
// In DXE
EFI_HOB_GUID_TYPE *GuidHob;
GuidHob = GetFirstGuidHob(&gMyPlatformDataGuid);
if (GuidHob != NULL) {
  MY_PLATFORM_DATA *Data = GET_GUID_HOB_DATA(GuidHob);
  // Use Data->...
}
```

### 2.3 DXE Phase Deep Dive

#### DXE Foundation Architecture

```
┌─────────────────────────────────────┐
│         Application Layer           │  ← UEFI Apps, OS Loaders
├─────────────────────────────────────┤
│      UEFI Boot Services             │  ← gBS->AllocatePages()
│      UEFI Runtime Services          │  ← gRT->GetVariable()
├─────────────────────────────────────┤
│        DXE Foundation               │
│  ┌──────────────────────────────┐   │
│  │  Event Manager               │   │
│  │  Memory Manager (GCD)        │   │
│  │  Protocol Database           │   │
│  │  DXE Dispatcher              │   │
│  └──────────────────────────────┘   │
├─────────────────────────────────────┤
│       DXE Drivers (Modules)         │
└─────────────────────────────────────┘
```

#### DXE Dispatcher: How Drivers Load

**Dispatcher Algorithm**:
```
1. Build a list of all DXE drivers from firmware volumes
2. For each driver:
   a. Check if DepEx (dependencies) are satisfied
   b. If yes, load driver into memory
   c. Call driver's entry point
   d. Mark as "Dispatched"
3. Repeat until no more drivers can be dispatched
4. If "Architectural Protocols" are all present, signal EndOfDxe
```

**Dependency Expression (DepEx) Examples**:

```
// Simple: "I need Protocol A"
DEPEX = gEfiPciIoProtocolGuid

// AND: "I need both A and B"
DEPEX = gEfiPciIoProtocolGuid AND gEfiDevicePathProtocolGuid

// OR: "I need either A or B"
DEPEX = gEfiSerialIoProtocolGuid OR gEfiSioProtocolGuid

// Complex: "I need (A AND B) OR C"
DEPEX = (gProtocolAGuid AND gProtocolBGuid) OR gProtocolCGuid
```

**DepEx in Binary** (stored in firmware volume):
Uses **Reverse Polish Notation (RPN)**:
```
Example: A AND B
Binary: [GUID_A] [PUSH] [GUID_B] [PUSH] [AND] [END]
```

#### Global Coherency Domain (GCD)

**What is GCD?**
The **memory map manager** for DXE. It tracks all memory and MMIO regions.

**GCD Regions**:
*   **System Memory** (DRAM)
*   **Memory-Mapped I/O** (MMIO - PCI BARs, LAPIC, etc.)
*   **Persistent Memory** (NVDIMM, optane)

**GCD Services**:
```c
// Add a new region
gDS->AddMemorySpace(
  EfiGcdMemoryTypeMemoryMappedIo,
  0xFED00000,  // HPET base
  0x1000,      // 4KB
  EFI_MEMORY_UC  // Uncacheable
);

// Allocate from a region
gDS->AllocateMemorySpace(
  EfiGcdAllocateAddress,
  EfiGcdMemoryTypeSystemMemory,
  12,  // 4KB aligned (2^12)
  0x1000,
  &MyAddress,
  ImageHandle,
  NULL
);
```

**GCD vs Boot Services Memory**:
*   **GCD**: Tracks physical address space (low-level)
*   **gBS->AllocatePages()**: High-level memory allocation (uses GCD internally)

#### Architectural Protocols

These are the **"must-have" protocols** that DXE Core waits for before proceeding to BDS:

```c
// Defined in PI Specification
EFI_SECURITY_ARCH_PROTOCOL_GUID
EFI_CPU_ARCH_PROTOCOL_GUID
EFI_METRONOME_ARCH_PROTOCOL_GUID
EFI_TIMER_ARCH_PROTOCOL_GUID
EFI_BDS_ARCH_PROTOCOL_GUID
EFI_WATCHDOG_TIMER_ARCH_PROTOCOL_GUID
EFI_RUNTIME_ARCH_PROTOCOL_GUID
EFI_VARIABLE_ARCH_PROTOCOL_GUID
EFI_VARIABLE_WRITE_ARCH_PROTOCOL_GUID
EFI_CAPSULE_ARCH_PROTOCOL_GUID
EFI_MONOTONIC_COUNTER_ARCH_PROTOCOL_GUID
EFI_RESET_ARCH_PROTOCOL_GUID
EFI_RTC_ARCH_PROTOCOL_GUID
```

**Why are they "Architectural"?**
Because DXE Core itself **depends on them** to provide UEFI services.

**Example**: `EFI_TIMER_ARCH_PROTOCOL`
```c
struct _EFI_TIMER_ARCH_PROTOCOL {
  EFI_TIMER_REGISTER_HANDLER       RegisterHandler;
  EFI_TIMER_SET_TIMER_PERIOD       SetTimerPeriod;
  EFI_TIMER_GET_TIMER_PERIOD       GetTimerPeriod;
  EFI_TIMER_GENERATE_SOFT_INTERRUPT GenerateSoftInterrupt;
};
```

DXE Core uses this to implement:
```c
gBS->SetTimer()  // Uses EFI_TIMER_ARCH internally
```

---

## 3. UEFI Driver Model

### 3.1 Why a Driver Model?

**Problem**: How do we write **portable** drivers that work across different platforms?

**Solution**: Define a **standard interface** (Driver Binding Protocol) that all drivers follow.

### 3.2 Driver Binding Protocol

**The Core of the UEFI Driver Model**:

```c
typedef struct _EFI_DRIVER_BINDING_PROTOCOL {
  EFI_DRIVER_BINDING_SUPPORTED  Supported;
  EFI_DRIVER_BINDING_START      Start;
  EFI_DRIVER_BINDING_STOP       Stop;
  UINT32                        Version;
  EFI_HANDLE                    ImageHandle;
  EFI_HANDLE                    DriverBindingHandle;
} EFI_DRIVER_BINDING_PROTOCOL;
```

**Three Key Functions**:

#### `Supported()`
```c
EFI_STATUS Supported(
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
);
```

**Purpose**: "Can I drive this device?"

**Implementation Example** (USB driver):
```c
EFI_STATUS MyUsbDriverSupported(...) {
  EFI_PCI_IO_PROTOCOL *PciIo;
  
  // Check if PCI I/O is on this handle
  Status = gBS->OpenProtocol(
    ControllerHandle,
    &gEfiPciIoProtocolGuid,
    &PciIo,
    This->DriverBindingHandle,
    ControllerHandle,
    EFI_OPEN_PROTOCOL_BY_DRIVER
  );
  
  if (EFI_ERROR(Status)) {
    return EFI_UNSUPPORTED;  // Not a PCI device
  }
  
  // Check if it's a USB controller
  PciIo->Pci.Read(PciIo, ..., &ClassCode);
  if (ClassCode != PCI_CLASS_SERIAL_USB) {
    gBS->CloseProtocol(...);
    return EFI_UNSUPPORTED;
  }
  
  gBS->CloseProtocol(...);
  return EFI_SUCCESS;  // Yes, I can drive this!
}
```

#### `Start()`
```c
EFI_STATUS Start(
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
);
```

**Purpose**: "Start managing this device"

**Implementation**:
```c
EFI_STATUS MyUsbDriverStart(...) {
  MY_USB_CONTROLLER *Controller;
  
  // Allocate private context
  Controller = AllocateZeroPool(sizeof(MY_USB_CONTROLLER));
  
  // Open PCI I/O Protocol
  gBS->OpenProtocol(..., &Controller->PciIo, ..., EFI_OPEN_PROTOCOL_BY_DRIVER);
  
  // Initialize hardware
  Controller->PciIo->Mem.Write(..., USB_CMD_REG, USB_CMD_RUN);
  
  // Install USB HC Protocol (so other drivers can use it)
  Controller->UsbHc.Reset = MyUsbReset;
  Controller->UsbHc.GetState = MyUsbGetState;
  // ... fill in function pointers
  
  gBS->InstallProtocolInterface(
    &ControllerHandle,
    &gEfiUsb2HcProtocolGuid,
    EFI_NATIVE_INTERFACE,
    &Controller->UsbHc
  );
  
  return EFI_SUCCESS;
}
```

#### `Stop()`
```c
EFI_STATUS Stop(
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
);
```

**Purpose**: "Stop managing this device and clean up"

**Implementation**:
```c
EFI_STATUS MyUsbDriverStop(...) {
  MY_USB_CONTROLLER *Controller;
  
  // Retrieve our context
  gBS->OpenProtocol(..., &UsbHc, ..., EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  Controller = USB_CONTROLLER_FROM_THIS(UsbHc);
  
  // Uninstall protocol
  gBS->UninstallProtocolInterface(..., &gEfiUsb2HcProtocolGuid, &UsbHc);
  
  // Stop hardware
  Controller->PciIo->Mem.Write(..., USB_CMD_REG, USB_CMD_STOP);
  
  // Close protocols we opened
  gBS->CloseProtocol(..., &gEfiPciIoProtocolGuid, ...);
  
  // Free context
  FreePool(Controller);
  
  return EFI_SUCCESS;
}
```

### 3.3 Device Path Protocol

**Purpose**: Universal way to identify hardware location.

**Device Path = Chain of Nodes**

```
Example: USB Keyboard on Port 3

PciRoot(0x0)               ← PCI Root Bridge
  /Pci(0x1D,0x0)           ← PCI Device (Bus 0, Dev 29, Func 0)
    /USB(0x3,0)            ← USB Port 3
      /USB(0x0,0)          ← USB Interface 0
        /Messaging(HID)    ← HID Keyboard endpoint
```

**Device Path Nodes**:
```c
typedef struct {
  UINT8  Type;     // HARDWARE, ACPI, MESSAGING, MEDIA, BIOS_BOOT
  UINT8  SubType;  // Specific within type
  UINT16 Length;   // Size of this node
  // ... node-specific data follows
} EFI_DEVICE_PATH_PROTOCOL;
```

**Common Node Types**:
*   **HARDWARE**: PCI, memory-mapped, vendor-specific
*   **ACPI**: `_HID`, `_UID` from ACPI namespace
*   **MESSAGING**: USB, SATA, SCSI, network MAC address
*   **MEDIA**: Hard disk partition, file path, RAM disk

**Example**: Creating a Device Path
```c
HDD_DEVICE_PATH HdNode = {
  {
    MEDIA_DEVICE_PATH,
    MEDIA_HARDDRIVE_DP,
    sizeof(HARDDRIVE_DEVICE_PATH)
  },
  1,                     // Partition Number
  0x800,                 // Start sector
  0x100000,              // Size (sectors)
  {/* GPT GUID */},      // Signature
  MBR_TYPE_EFI_PARTITION_TABLE_HEADER,
  SIGNATURE_TYPE_GUID
};
```

### 3.4 Driver Types

#### Bus Driver
*   **Purpose**: Enumerate child devices
*   **Example**: PCI Bus Driver, USB Bus Driver
*   **Creates**: Child handles for each device found
*   **Produces**: Bus-specific protocol (e.g., `EFI_PCI_IO_PROTOCOL`)

**Flow**:
```
1. Bus Driver's Start() called
2. Scan bus for devices (PCI config space enumeration)
3. For each device:
   a. Create new handle (gBS->InstallMultipleProtocolInterfaces)
   b. Attach Device Path
   c. Install PCI I/O Protocol
4. Return control to dispatcher
5. Dispatcher connects device drivers to children
```

#### Device Driver
*   **Purpose**: Control a specific hardware device
*   **Example**: Network card driver, USB keyboard driver
*   **Consumes**: Bus protocol (PCI I/O, USB I/O)
*   **Produces**: High-level protocol (Simple Network, Simple Text Input)

#### Hybrid Driver
*   Acts as both bus and device driver
*   **Example**: SATA controller driver (bus) that produces Block I/O (device)

### 3.5 Connect Controller Flow

**User calls**:
```c
gBS->ConnectController(HandleBuffer[i], NULL, NULL, TRUE);
```

**What happens**:
```
1. DXE Core iterates through all drivers
2. For each driver:
   a. Call driver->Supported(ControllerHandle)
   b. If returns SUCCESS:
      - Call driver->Start(ControllerHandle)
3. If driver creates child handles:
   - Recursively call ConnectController() on children
```

**Example**: Connecting a USB keyboard

```
1. ConnectController(PciUsbControllerHandle)
   → UsbBusDxe->Supported() → SUCCESS
   → UsbBusDxe->Start()
       ↓ Creates UsbPortHandle[0..3]
   
2. ConnectController(UsbPortHandle[2])  // Keyboard plugged into port 2
   → UsbKbDxe->Supported() → SUCCESS
   → UsbKbDxe->Start()
       ↓ Installs SimpleTextIn Protocol
   
3. Now console can use the keyboard!
```

---

## 4. User Interface: BDS and HII

### 4.1 BDS (Boot Device Selection) Phase

#### BDS Responsibilities

```
┌────────────────────────────────────┐
│ 1. Platform Policy Execution       │  ← Platform-specific setup
├────────────────────────────────────┤
│ 2. Console Connection              │  ← GOP, keyboard, mouse
├────────────────────────────────────┤
│ 3. Device Connection               │  ← Storage, network
├────────────────────────────────────┤
│ 4. Boot Manager                    │  ← UI (HII forms)
├────────────────────────────────────┤
│ 5. Boot Device Enumeration         │  ← Read BootOrder
├────────────────────────────────────┤
│ 6. Load OS Loader                  │  ← LoadImage() + StartImage()
└────────────────────────────────────┘
```

#### Boot Options

**Stored in NVRAM as UEFI variables**:
```
Boot0000: Description="Windows Boot Manager"
          DevicePath=HD(1,GPT,...)/\EFI\Microsoft\Boot\bootmgfw.efi
          
Boot0001: Description="Ubuntu"
          DevicePath=HD(1,GPT,...)/\EFI\ubuntu\grubx64.efi
          
BootOrder: 0001,0000  (Try Ubuntu first, then Windows)
```

**Variable Format**:
```c
typedef struct {
  UINT32               Attributes;
  UINT16               FilePathListLength;
  CHAR16               Description[];  // Null-terminated
  EFI_DEVICE_PATH      FilePathList[];
  UINT8                OptionalData[];  // Kernel command line, etc.
} EFI_LOAD_OPTION;
```

#### Boot Process

```c
// Pseudo-code of BDS
for (i = 0; i < BootOrderCount; i++) {
  BootOption = BootOrder[i];  // e.g., Boot0001
  LoadOption = ReadVariable(BootOption);
  
  Status = gBS->LoadImage(
    FALSE,
    ImageHandle,
    LoadOption->FilePathList,
    NULL,
    0,
    &BootImageHandle
  );
  
  if (!EFI_ERROR(Status)) {
    Status = gBS->StartImage(BootImageHandle, NULL, NULL);
    
    if (!EFI_ERROR(Status)) {
      // OS loader took over, we won't reach here
      break;
    }
  }
  
  // Failed, try next boot option
}

// If we get here, all boot options failed
// Show error or enter Setup
```

### 4.2 HII (Human Interface Infrastructure)

#### What is HII?

**HII** is a framework for creating **graphical setup utilities** (BIOS Setup).

**Key Components**:
```
┌──────────────────────────────────┐
│    Forms Browser (Renderer)      │  ← Displays UI
├──────────────────────────────────┤
│      HII Database                │  ← Storage for forms, strings
├──────────────────────────────────┤
│  Form Definitions (IFR)          │  ← Layout & controls
├──────────────────────────────────┤
│  String Database (UNI)           │  ← Multi-language text
├──────────────────────────────────┤
│  Configuration Storage           │  ← NVRAM, setup variables
└──────────────────────────────────┘
```

#### IFR (Internal Forms Representation)

**IFR** is a **binary encoding** of form definitions (like HTML for BIOS setup).

**Visual Form Example**:
```
┌─────────────────────────────────────┐
│ Main Setup                          │
├─────────────────────────────────────┤
│ [✓] Secure Boot                     │  ← Checkbox
│ Boot Mode: [ UEFI        ▼ ]        │  ← Dropdown
│ Timeout:   [5] seconds              │  ← Numeric input
│                                     │
│ [ Advanced... ]                     │  ← Link to another form
│                                     │
│ [Save & Exit]  [Discard & Exit]    │  ← Action buttons
└─────────────────────────────────────┘
```

**IFR Code** (pseudo-VFR - Visual Forms Representation):
```
formset
  guid = SETUP_FORMSET_GUID
  title = STRING_TOKEN(STR_FORMSET_TITLE);
  
  form
    formid = 1,
    title = STRING_TOKEN(STR_MAIN_FORM_TITLE);
    
    checkbox
      varid = Setup.SecureBoot,
      prompt = STRING_TOKEN(STR_SECURE_BOOT),
      help = STRING_TOKEN(STR_SECURE_BOOT_HELP),
      default = TRUE
    endcheckbox;
    
    oneof
      varid = Setup.BootMode,
      prompt = STRING_TOKEN(STR_BOOT_MODE),
      option text = STRING_TOKEN(STR_UEFI_MODE), value = 0, flags = DEFAULT;
      option text = STRING_TOKEN(STR_LEGACY_MODE), value = 1;
    endoneof;
    
    numeric
      varid = Setup.BootTimeout,
      prompt = STRING_TOKEN(STR_TIMEOUT),
      minimum = 0,
      maximum = 65535,
      default = 5
    endnumeric;
    
  endform;
endformset;
```

**Compiled to Binary IFR** (OpCodes):
```
FORMSET_OP (GUID, Title)
  FORM_OP (ID=1, Title)
    CHECKBOX_OP (VarStoreId, Offset, Prompt, Default)
    ONEOF_OP (...)
      OPTION_OP (Text, Value, Flags)
      OPTION_OP (Text, Value)
    END_ONEOF_OP
    NUMERIC_OP (...)
  END_FORM_OP
END_FORMSET_OP
```

#### HII Database

**Protocols**:
*   `EFI_HII_DATABASE_PROTOCOL` - Register/retrieve packages
*   `EFI_HII_STRING_PROTOCOL` - Get/set strings (multi-language)
*   `EFI_HII_FONT_PROTOCOL` - Render text
*   `EFI_HII_IMAGE_PROTOCOL` - Display images
*   `EFI_HII_CONFIG_ACCESS_PROTOCOL` - Read/write configuration

**Package Types**:
*   **Form Package** - IFR opcodes
*   **String Package** - UTF-16 strings with language codes
*   **Font Package** - Glyph data
*   **Image Package** - JPEG/BMP images
*   **Simple Font Package** - 8x19 bitmap fonts

#### Configuration Storage

**Two storage types**:

**1. EFI Variable Storage** (NVRAM):
```c
gRT->SetVariable(
  L"Setup",
  &gSetupVariableGuid,
  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
  sizeof(SETUP_DATA),
  &SetupData
);
```

**2. Buffer Storage** (Driver-managed):
```c
// Driver maintains configuration in its own memory
typedef struct {
  UINT8 SecureBoot;
  UINT8 BootMode;
  UINT16 BootTimeout;
  // ... more settings
} MY_DRIVER_CONFIG;

MY_DRIVER_CONFIG gConfig;  // Driver's private storage
```

#### Forms Browser

**User Interaction Flow**:
```
1. User presses F2 during boot
   ↓
2. BDS calls SetupBrowser
   ↓
3. Browser queries HII Database for all forms
   ↓
4. Render main menu
   ↓
5. User navigates/edits values
   ↓
6. On "Save":
   a. Browser calls ConfigAccess->RouteConfig()
   b. Driver receives new values
   c. Driver validates and writes to NVRAM
   ↓
7. Exit browser, resume boot
```

**Browser Protocol**:
```c
typedef struct {
  EFI_SEND_FORM  SendForm;
  EFI_BROWSER_CALLBACK BrowserCallback;
} EFI_FORM_BROWSER2_PROTOCOL;
```

**Invoking the Browser**:
```c
EFI_FORM_BROWSER2_PROTOCOL *FormBrowser;
gBS->LocateProtocol(&gEfiFormBrowser2ProtocolGuid, NULL, &FormBrowser);

FormBrowser->SendForm(
  FormBrowser,
  &MyFormsetGuid,  // Which formset to display
  1,
  NULL,
  0
);
```

### 4.3 Configuration Language

HII uses a **query string format** for reading/writing configuration:

**Request String** (read config):
```
GUID=32412222-1234&NAME=0044&PATH=0104140600...&OFFSET=0&WIDTH=1
```

**Config Response** (data returned):
```
GUID=32412222-1234&NAME=0044&PATH=0104140600...&OFFSET=0&WIDTH=1&VALUE=01
```

**Format**: `GUID=...&NAME=...&OFFSET=...&WIDTH=...&VALUE=...`

**HII Config Routing Protocol** handles this:
```c
EFI_STATUS ExtractConfig(
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL *This,
  IN CONST EFI_STRING Request,
  OUT EFI_STRING *Progress,
  OUT EFI_STRING *Results
);
```

---

## Summary

This comprehensive guide covered all four On-Line Web presentations:

### 1. UEFI & PI Overview
*   Platform Initialization layered architecture
*   Distinction between UEFI (OS interface) and PI (firmware internals)
*   Foundation vs Module concept

### 2. PEI, DXE, and Firmware Volumes
*   Firmware Volume structure (FFS file system)
*   PEI Foundation services and PPIs
*   Memory Reference Code (MRC) training process
*   HOB creation and consumption
*   DXE Dispatcher and dependency expressions
*   Global Coherency Domain (memory mapping)
*   Architectural Protocols

### 3. UEFI Driver Model
*   Driver Binding Protocol (Supported/Start/Stop)
*   Device Path Protocol (hardware location)
*   Bus vs Device vs Hybrid drivers
*   ConnectController flow

### 4. BDS and HII
*   Boot Device Selection responsibilities
*   Boot Options and BootOrder variables
*   Human Interface Infrastructure framework
*   IFR (Internal Forms Representation)
*   Forms Browser and user interaction
*   Configuration storage (NVRAM vs buffer)

**Key Insight**: The PI/UEFI architecture is a **highly modular, protocol-driven system** that separates hardware initialization (SEC/PEI), service production (DXE), user interaction (BDS/HII), and OS interface (TSL/RT) into distinct, well-defined phases.

---

## Common Confusions Clarified

### Confusion: Is PI a Stage Before UEFI?

**Answer**: No! PI and UEFI are **complementary specifications**, not sequential stages.

#### The Relationship

```
┌─────────────────────────────────────────────────┐
│         THE COMPLETE FIRMWARE                   │
├─────────────────────────────────────────────────┤
│                                                 │
│  ┌──────────────────┐  ┌───────────────────┐   │
│  │ PI Specification │  │ UEFI Specification│   │
│  │ (Internal)       │  │ (External)        │   │
│  └──────────────────┘  └───────────────────┘   │
│          ↓                      ↓               │
│     Defines how            Defines how          │
│   firmware boots           firmware talks       │
│      itself                  to OS              │
│                                                 │
└─────────────────────────────────────────────────┘
```

#### Think of it This Way:

**PI Specification** = **Backend** (Internal plumbing)
- How does the CPU wake up?
- How do we initialize RAM?
- How do drivers load?
- **Defines**: SEC, PEI, DXE phases

**UEFI Specification** = **Frontend** (Public API)
- What services does firmware provide to OS Loader?
- How does firmware load and start the OS Loader?
- How does OS Loader use firmware services (Boot Services)?
- How does OS get memory map from firmware?
- **Defines**: Boot Services, Runtime Services, BDS protocols

**Key Flow**:
```
Firmware (BDS) ──LoadImage()──> OS Loader (GRUB, bootmgfw.efi)
                                     ↓
OS Loader <──Uses Boot Services── Firmware
              (AllocatePages,    (Still providing
               LocateProtocol)     services)
```

#### The Boot Stages with Spec Ownership

```
Power On
    ↓
SEC  ──────┐
    ↓      │ Defined by PI Spec (Platform Initialization)
PEI  ──────┤ = "How firmware initializes the platform"
    ↓      │
DXE  ──────┘ (Produces UEFI Boot Services here)
    ↓
BDS  ──────┐
    ↓      │ Defined by UEFI Spec
TSL  ──────┤ = "Interface between firmware and OS"
    ↓      │
RT   ──────┘
    ↓
AL   ────── UEFI Spec
```

#### What the UEFI Spec Actually Defines

According to the **UEFI Specification**, there are only **two main phases**:

1. **Boot Services Phase** (Pre-OS)
   - Includes: DXE (from PI) + BDS + TSL
   - `ExitBootServices()` has NOT been called yet
   - OS Loader can use Boot Services
   - Examples: `AllocatePages()`, `LoadImage()`, `LocateProtocol()`

2. **Runtime Services Phase** (Post-OS)
   - After `ExitBootServices()` is called
   - Only Runtime Services available
   - OS kernel is running
   - Examples: `GetVariable()`, `SetTime()`, `ResetSystem()`

#### Specification Boundaries

| Specification | What It Defines | Audience | Example Content |
|---------------|------------------|----------|-----------------|
| **PI** | How firmware boots itself (SEC→PEI→DXE) | **Firmware developers** (platform vendors) | HOBs, PPIs, DepEx, Dispatcher, FV format |
| **UEFI** | How OS interacts with firmware | **OS developers** (Linux, Windows) | Boot Services, Runtime Services, Protocols, System Table |

#### Real-World Analogy

Imagine a restaurant:

**PI Specification** = Kitchen operations manual
- How to turn on the stove
- How to prep ingredients
- How to cook the food
- (Internal process - customers don't see this)

**UEFI Specification** = Customer menu and service protocol
- What dishes are available
- How to order
- How to pay
- (External interface - what customers interact with)

**The customer (OS)** only sees the UEFI menu. They don't care about the kitchen (PI).

**The chef (firmware developer)** needs both:
- Kitchen manual (PI) to cook
- Menu (UEFI) to know what to serve

#### Summary Table

| Question | Answer |
|----------|--------|
| Does PI come before UEFI? | **No**. They are parallel specifications covering different aspects |
| What are the "stages" in UEFI? | From UEFI spec view: **Boot Services Phase** and **Runtime Services Phase** |
| What are the "stages" from boot flow? | SEC, PEI, DXE (PI) → BDS, TSL (UEFI using Boot Services) → RT, AL (UEFI Runtime Services) |
| Who implements PI? | Firmware/BIOS vendors (Intel, AMI, Insyde) |
| Who consumes UEFI? | OS Loaders (GRUB, Windows Boot Manager) and OS kernels |

#### The Key Insight

**PI + UEFI Together = Complete Firmware**

- **PI** = "How it's built" (the construction manual)
- **UEFI** = "What it provides to the OS" (the service contract)

When you boot a system:
1. PI phases (SEC/PEI/DXE) **build** the infrastructure
2. DXE **produces** UEFI Boot Services and Runtime Services
3. BDS/TSL **use** those services to boot the OS
4. OS **consumes** UEFI services via the defined API

**Both specifications are active simultaneously during DXE/BDS**, which is why the confusion exists. DXE is defined by PI (how drivers are dispatched), but it produces services defined by UEFI (Boot/Runtime Services).

