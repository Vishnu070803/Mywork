# Complete Boot Flow Reference: Intel x86, ARM TF-A, and UEFI/PI
## Verified Technical Documentation

> [!IMPORTANT]
> This document consolidates boot flow information for three major firmware architectures, verified against official specifications and documentation from Intel, TianoCore, ARM Trusted Firmware, and Slim Bootloader projects.

---

## Executive Summary

Modern computing platforms employ secure boot flows with hardware roots of trust to ensure integrity from power-on through OS loading. This document compares three major boot architectures:

1. **Intel x86 with Slim Bootloader (SBL)** - Arrow Lake/newer platforms
2. **ARM Trusted Firmware-A (TF-A) + U-Boot**
3. **UEFI/PI firmware stack** (EDK II style)

All architectures solve the same core problem—securely initialize hardware and load an OS—but differ in:
- **Boot ownership:** CPU vs secure co-processor
- **Trust anchors:** Hardware roots of trust (fuses, ROM)
- **Handoff mechanisms:** HOBs, Device Trees, or protocol-based

---

## Table of Contents

1. [Intel x86 Boot Flow (Arrow Lake + Slim Bootloader)](#1-intel-x86-boot-flow-arrow-lake--slim-bootloader)
2. [ARM Boot Flow (TF-A + U-Boot)](#2-arm-boot-flow-tf-a--u-boot)
3. [UEFI/PI Boot Flow](#3-uefipi-boot-flow)
4. [Architecture Comparison](#4-architecture-comparison)
5. [References](#5-references)

---

## 1. Intel x86 Boot Flow (Arrow Lake + Slim Bootloader)

Arrow Lake platforms use **Intel Boot Guard / Intel CBnT** (Converged Boot Guard and TXT) for hardware-rooted secure boot. CSME participates in policy enablement and manifest enforcement, while the CPU/ACM implement the measured/verified chain execution.

### 1.1 Boot Guard / CBnT Trust Model

The Boot Guard architecture establishes a hardware root of trust using the following components:

**Trust Chain:**
```
Power On
  → CSME Boot ROM initializes PCH Clocks and GPIO (Line 276)
  → CSME Firmware loads and authenticates itself from SPI
  → CSME applies mandatory PMC patch to allow CPU power-up (Line 705)
  → CSME releases CPU reset once platform is prepared
  → CPU loads microcode + ACM pointer via FIT
  → CPU verifies ACM signature using CPU-rooted Intel trust anchor
  → ACM independently validates KM/BPM/IBB chain using FPF fuses
  → Control transfers to verified IBB (Stage1A)
```

> [!WARNING]
> **Common Misconceptions About Arrow Lake Boot Guard:**
> 
> The following claims are **NOT** supported by official Intel documentation:
> - ❌ "CPU queries CSME for ACM validation"
> - ❌ "CSME assists ACM verification"
> - ❌ "CSME and ACM jointly verify IBB hash"
> 
> **Correct Model:** Arrow Lake continues the CBnT/Boot Guard model where CSME enforces fused security policy and manifest authorization, while CPU verifies ACM, and ACM verifies IBB. CSME may log events/enforce locks, but it is **not a "cryptographic co-verifier"** in the ACM runtime chain.

### 1.2 Component Roles: CSME vs CPU vs ACM

#### ✅ CSME (Converged Security and Management Engine) Responsibilities

CSME is a microcontroller inside the PCH (Platform Controller Hub) that executes first:

- Boots from **immutable ROM** inside PCH
- Authenticates its own firmware stored in SPI flash (ME region inside IFWI)
- Reads Boot Guard configuration from **PCH Field Programmable Fuses (FPF)**
- Enforces policy mode: **Verified / Measured / Both**
- Authorizes required manifests and initial platform security state
- Controls reset gating / boot progress gating according to policy

**Key Point:** CSME enables storing Boot Guard policy in PCH FPF fuses.

#### ✅ CPU Responsibilities (Boot Guard perspective)

- Executes reset vector at `0xFFFFFFF0` (top of flash mapped into memory space)
- Uses **FIT (Firmware Interface Table)** to locate ACM/microcode
- Verifies microcode signatures
- Verifies **ACM signature** using Intel-rooted trust fused into CPU

#### ✅ ACM (Authenticated Code Module) Responsibilities

ACM is an Intel-signed module executed in a secure environment:

- Loaded into **Authenticated Code RAM** (secure cache region, commonly L3-based)
- Enforces Boot Guard policy (verified/measured path)
- Validates the first boot firmware region **IBB (Initial Boot Block)**
- Measures into TPM if Measured Boot is active

### 1.3 Manifest Chain: KM/BPM/IBB

**Trust Anchor Structure:**

```
FPF Fuses (trust anchor + Boot Guard mode)
  → ACM validates KM (Key Manifest)
  → KM provides key(s) to validate BPM (Boot Policy Manifest)
  → BPM defines IBB region(s) + hashes
  → ACM verifies IBB (Stage1A)
```

**Detailed Components:**

- **FIT (Firmware Interface Table):** Points to ACM/microcode entries as part of BIOS layout
- **KM (Key Manifest):** Signed structure stored in flash containing OEM public keys
- **BPM (Boot Policy Manifest):** Signed structure defining IBB regions and security policy
- **FPF (Field Programmable Fuses):** Hardware fuses containing the trust anchor hash

> [!NOTE]
> **OEM Root Key Hash** is provisioned into FPF fuses and used as the Boot Guard trust anchor. ACM validation relies on **fused trust**, not on runtime "CSME key service calls."

### 1.4 Slim Bootloader Boot Flow

Slim Bootloader (SBL) follows a modular, staged execution flow designed for efficiency and simplicity. It integrates closely with the **Intel Firmware Support Package (FSP)** to handle silicon-specific initialization.

#### Boot Stages Overview

```
CSME Init → CPU Reset Vector + ACM → Stage1A → Stage1B → Stage2 → Payload → OS
```

#### 1.4.1 Power-On / Reset

```
Power applied
  → PCH power rails stable
  → CSME comes out of reset (inside PCH)
```

#### 1.4.2 CSME Initialization (inside PCH)

```
CSME ROM executes
  → Uses hardware-default SPI logic to load CSME Firmware (Line 483)
  → Initializes PCH Clocks, GPIO, and power sequencing (Line 276)
  → Applies mandatory PMC firmware patch (Line 705)
  → Reads Boot Guard policy fuses (FPF) and sets hardware gates
  → Releases CPU reset
```

#### 1.4.3 CPU Reset Vector + FIT + ACM

```
CPU begins at 0xFFFFFFF0
  → Uses FIT near top of BIOS region to locate microcode + ACM pointer
  → Applies microcode patches
  → Loads ACM
  → CPU verifies ACM signature (Intel trust in CPU)
  → ACM runs in Authenticated Code RAM
  → ACM validates IBB (Stage1A) using KM/BPM chain
  → Jump to verified Stage1A
```

### 1.5 Slim Bootloader Stages (Detailed)

#### Stage 1A: Pre-Memory Initialization

**Entry Point:** Executes directly from Flash (XIP - eXecute In Place)

**Key Actions:**
- **Reset Vector:** Located in the `Vtf0` file at the top of the Stage 1A Firmware Volume
- **Mode Transition:** Switches from real mode → protected mode (GDT + CR0.PE)
- **FSP-T Integration:** Calls `TempRamInit` to establish **Cache-as-RAM (CAR)**
- **LdrGlobal Creation:** Initializes the Loader Global Data structure in CAR
- **Debug Setup:** Sets up debug ports (UART if available)
- **IDTR Trick:** Uses IDTR to retain pointer for early boot state
- **Load Stage1B:** Loads Stage1B into CAR/RAM

**FSP Integration:** `FSP-T TempRamInit()`

> [!NOTE]
> Stage1A FV contains FIT Table, HashStore, and other security structures required by Boot Guard.

#### Stage 1B: Memory Initialization

**Entry Point:** Loaded into CAR by Stage1A

**Key Actions:**
- **Configuration Loading:** Loads and parses **CfgData** (YAML-generated configuration)
- **Config Database:** Creates configuration database for platform-specific settings
- **FSP-M Integration:** Builds FSP-M UPD (Update Data) from board config
- **Memory Training:** Calls `FspMemoryInit()` - DRAM training occurs (dominant boot time)
- **HOB Creation:** Builds HOBs (Hand-Off Blocks) describing actual RAM discovered
- **Migration:** Migrates LdrGlobal and stack from CAR to DRAM
- **CAR Teardown:** Calls `TempRamExit()` to restore cache to normal operation
- **Load Stage2:** Loads Stage2 into DRAM and jumps to it

**FSP Integration:** `FSP-M FspMemoryInit()` and `TempRamExit()`

**Data Structures:**
- **LdrGlobal (Loader Global Data):** Central structure used by SBL stages (1A through 2) to maintain state across transitions. **Not passed to payload.**
- **CfgData:** YAML-based configuration with "Internal" defaults compiled in, and "External" DLT (delta) files for board-specific customization

#### Stage 2: Silicon Initialization

**Entry Point:** Loaded into DRAM by Stage1B

**Key Actions:**
- **FSP-S Integration:** Builds FSP-S UPD and calls `FspSiliconInit()` for PCH/SoC controllers
- **Multi-Processor Init:** Initializes all CPU cores (MP init)
- **PCI Enumeration:** Enumerates and allocates resources for PCI/PCIe devices
- **ACPI Table Generation:** Generates ACPI tables (derived from memory/IO HOBs)
- **FSP Notifications:** Calls FSP-S `FspNotifyPhase` APIs (e.g., `AfterPciEnumeration`)
- **Payload Loading:** Loads and verifies payload
- **HOB Handoff:** Passes HOB list pointer to payload

**FSP Integration:** `FSP-S FspSiliconInit()` and `FspNotifyPhase()`

### 1.6 Payload + OS Handoff

#### Linux Boot (Direct OS Loader Payload)

```
Payload loads kernel bzImage
  → Builds boot_params structure:
    - E820 memory map
    - ACPI RSDP address
    - initrd location + cmdline
  → Jumps to kernel entry point
```

#### Windows Boot (UEFI Payload)

```
Windows requires UEFI environment:
  → Use UEFI payload (SBL provides UEFI compatibility layer)
  → Chain: bootmgfw.efi → winload.efi → ntoskrnl.exe
  → ACPI tables required for device enumeration
```

### 1.7 Complete Data Flow Summary

```
CSME FW from IFWI ME region
  → Boot Guard fuses (FPF) define policy
  → FIT points to ACM/microcode
  → ACM validates KM/BPM structures in flash
  → ACM verifies IBB (Stage1A)
  → Stage1A creates LdrGlobal + starts CAR via FSP-T
  → Stage1B loads CfgData + DDR init via FSP-M, creates memory HOBs
  → Stage2 does silicon init via FSP-S, creates ACPI tables
  → Payload consumes HOBs + ACPI pointers
  → OS boots
```

---

## 2. ARM Boot Flow (TF-A + U-Boot)

The ARM Trusted Firmware-A (TF-A) boot flow is a multi-stage process designed to establish a Secure World execution environment and hand off control to a Normal World bootloader or OS.

### 2.1 ARM Exception Levels

ARM architecture defines privilege levels called **Exception Levels (EL)**:

| Exception Level | Privilege | Usage |
|----------------|-----------|-------|
| **EL0** | Lowest | User space applications (Secure & Non-secure) |
| **EL1** | OS Kernel | Operating System kernel |
| **EL2** | Hypervisor | Virtualization (optional) |
| **EL3** | Highest | Secure Monitor (TF-A BL31) |

Additionally, ARM has two execution states:
- **Secure World:** Access to all system resources
- **Non-Secure/Normal World:** Limited access, isolated from Secure World

### 2.2 Boot Level (BL) Stages

#### BL1: AP Trusted ROM

**Execution Level:** EL3 (Secure Monitor)

**Responsibilities:**
- Starts from platform's reset vector in **Trusted ROM** (immutable)
- Performs minimal architectural initialization (CPU reset, exception vectors)
- Determines boot path (cold vs. warm boot)
- On cold boot:
  - Initializes console for debug output
  - Sets up MMU for Trusted SRAM
  - Initializes platform storage (to load BL2)
- Loads BL2 image into Trusted SRAM
- Passes control to BL2 at **Secure-EL1**

**Trust Anchor:** Trusted ROM is the hardware root of trust

#### BL2: Trusted Boot Firmware

**Execution Level:** Secure-EL1

**Responsibilities:**
- Performs further architectural and platform initialization
- Security setup and memory reservation
- **Primary Role:** Load subsequent stages:
  - BL31 (EL3 Runtime)
  - BL32 (Secure-EL1 Payload, optional)
  - BL33 (Non-trusted Firmware)
  - SCP_BL2 (System Control Processor, optional)
- Once all images loaded, raises **SMC (Secure Monitor Call)** to BL1
- BL1 then transitions execution to BL31 at **EL3**

**Handoff Mechanism:** SMC `BL1_SMC_RUN_IMAGE`

#### BL31: EL3 Runtime Software

**Execution Level:** EL3 (Secure Monitor)

**Responsibilities:**
- Resides permanently in Trusted SRAM
- Provides **Runtime Services:**
  - **PSCI (Power State Coordination Interface)** - power management
  - Secure service dispatch via SMC handling
- Replaces BL1 exception vectors with its own
- Handles transitions between Secure and Non-Secure worlds
- Initializes BL32 (if present) before passing control to BL33
- Transitions processor from EL3 to highest available Normal World EL

**Key Point:** BL31 remains resident and handles all SMC calls from Normal World OS

#### BL32: Secure-EL1 Payload (Optional)

**Execution Level:** Secure-EL1

**Type:** Typically a **Trusted OS** (e.g., OP-TEE)

**Responsibilities:**
- Runs in Secure World
- Manages **Trusted Applications (TAs)**
- Provides secure services to Normal World applications via SMC interface

**Handoff:** Initialized by BL31 before control passes to Normal World

#### BL33: Non-trusted Firmware

**Execution Level:** EL2 (if available) or EL1

**Type:** Normal World bootloader, typically:
- **U-Boot**
- **UEFI (EDK2)**

**Responsibilities:**
- Initialize remaining platform hardware
- Load operating system
- Provide boot services to OS

**Handoff from BL31:** BL31 transitions from EL3 to EL2/EL1 and passes:
- **X0:** Base address of **Flattened Device Tree (DTB)**
- **X1:** Often **Transfer List (TL)** pointer (Firmware Handoff spec) or reserved
- **X2-X3:** Reserved (set to 0)

### 2.3 Secure Monitor Calls (SMC)

SMCs are the standard mechanism for requesting services from the Secure World:

**During Boot:**
- BL2 uses SMC `BL1_SMC_RUN_IMAGE` to request BL1 jump to BL31

**Runtime:**
- Normal World OS uses SMCs for:
  - Power management operations (PSCI): `CPU_ON`, `CPU_OFF`, `CPU_SUSPEND`
  - Trusted Application calls (if BL32/OP-TEE present)
  - Platform-specific services

### 2.4 Complete ARM TF-A Boot Flow

```
Power-On / Reset
  ↓
BL1 (EL3 - Trusted ROM)
  → Platform initialization
  → Load BL2 from storage
  ↓
BL2 (Secure-EL1)
  → Load BL31, BL32, BL33 into memory
  → Verify signatures (if Trusted Boot enabled)
  → SMC to BL1: run BL31
  ↓
BL1 (EL3)
  → Jump to BL31
  ↓
BL31 (EL3 - Runtime Resident)
  → Initialize Secure Monitor
  → Setup PSCI services
  → Initialize BL32 (Trusted OS) if present
  → Prepare Normal World context
  → Drop to EL2/EL1 and jump to BL33
  → (X0 = DTB address)
  ↓
BL33 (EL2/EL1 - U-Boot/UEFI)
  → Parse DTB from X0
  → Initialize hardware
  → Load OS kernel
  → Jump to OS
  ↓
OS Kernel (EL1)
  → Can call PSCI via SMC to BL31 for power management
```

### 2.5 U-Boot Integration

When used as BL33, **U-Boot**:
- Receives DTB pointer in register **X0**
- Parses Device Tree for hardware configuration
- Is typically compiled as `u-boot.bin` and packaged into **FIP (Firmware Image Package)** alongside TF-A stages
- Provides boot services and can load Linux kernel with:
  - Device Tree Blob (DTB) for hardware description
  - Or ACPI tables (on ARM server platforms that support it)

### 2.6 Device Tree vs ACPI on ARM

**Device Tree (DTB):**
- **Dominant** on embedded ARM systems
- FDT (Flattened Device Tree) passed from TF-A → U-Boot → OS kernel
- Describes hardware topology, interrupt routing, device properties
- OS uses DTB to enumerate and configure devices

**ACPI:**
- Used on **ARM server platforms** (e.g., ARM ServerReady compliance)
- TF-A provides secure foundation
- UEFI (as BL33) generates/provides ACPI tables to OS
- Windows and some enterprise Linux distros on ARM require ACPI

---

## 3. UEFI/PI Boot Flow

The **UEFI Platform Initialization (PI)** specification defines a modular firmware architecture. The boot process is divided into distinct phases, each with specific responsibilities.

### 3.1 UEFI/PI Boot Phases

```
Power-On / Reset
  ↓
SEC (Security)
  ↓
PEI (Pre-EFI Initialization)
  ↓
DXE (Driver Execution Environment)
  ↓
BDS (Boot Device Selection)
  ↓
TSL (Transient System Load)
  ↓
ExitBootServices()
  ↓
RT (Runtime) + OS Kernel
```

#### Phase 1: SEC (Security)

**Responsibilities:**
- First phase to execute at the platform reset vector
- Handles all platform reset events (Power-on, Warm reset, etc.)

**Key Functions:**
- **Temporary RAM:** Establishes temporary memory environment using **Cache As RAM (CAR)**
  - DRAM not yet initialized
  - Uses CPU cache as temporary writable memory
- **Root of Trust:** Acts as system's root of trust
  - On Intel platforms: integrates with Boot Guard ACM
- **Handoff:** Passes HOB list pointer and system state to PEI Foundation

**Transition:** → PEI once temporary environment ready

#### Phase 2: PEI (Pre-EFI Initialization)

**Responsibilities:**
- Primary goal: Initialize permanent system memory (DRAM)
- Initialize critical early-start hardware

**Key Components:**

**PEIMs (PEI Modules):**
- Tiny, focused modules that perform initialization tasks
- Often run directly from Flash (Execute-In-Place / XIP) before RAM available
- Examples: Memory initialization PEIM, CPU PEIM, Recovery PEIM

**PPIs (PEIM-to-PEIM Interfaces):**
- Communication mechanism between PEIMs
- Lightweight services for pre-memory environment

**HOB Creation:**
- PEI creates **Hand-Off Blocks (HOBs)** describing:
  - Discovered memory ranges
  - Firmware volumes
  - Platform configuration
  - Resource allocations

**Transition:** Concludes with **DXE IPL (Initial Program Load)**
- Uses HOB list to locate DXE Core
- Passes control to DXE Foundation

> [!NOTE]
> **HOBs are the bridge between PEI and DXE**, allowing information to flow across the "memory initialization boundary."

#### Phase 3: DXE (Driver Execution Environment)

**Responsibilities:**
- Majority of system initialization
- Full-featured environment with DRAM available

**Key Components:**

**DXE Core:**
- Provides execution environment for DXE Drivers
- Manages **Handle Database** and **Protocols**
- Dispatches drivers based on dependencies

**HOB Consumption:**
- DXE Core consumes HOB list from PEI
- Understands system memory map and hardware status
- Builds UEFI memory map from HOB memory descriptors

**DXE Drivers:**
- More complex than PEIMs, run from RAM
- Initialize platform hardware:
  - Chipset initialization
  - PCI/PCIe enumeration
  - USB, SATA, network controllers
  - Graphics initialization
- Produce **Protocols** (standardized interfaces)

**Services Initialized:**
- **UEFI Boot Services:** Memory allocation, event/timer services, driver/image services
- **UEFI Runtime Services:** Variable storage, time services, reset/power management

**ACPI/SMBIOS:**
- DXE drivers publish ACPI tables
- Publish SMBIOS tables for hardware inventory

**Transition:** → BDS after DXE Dispatcher finishes loading all available drivers

#### Phase 4: BDS (Boot Device Selection)

**Responsibilities:**
- Implements platform-specific boot policy
- User interaction for boot configuration

**Key Functions:**
- **Console Setup:** Connect console devices (keyboard, display)
- **Boot Policy:** Interprets UEFI variables:
  - `BootOrder`: Order of boot options to try
  - `Boot####`: Individual boot option descriptions
  - `BootNext`: One-time boot override
- **Device Connection:** Connects drivers to selected boot device
- **Boot Attempt:** Loads and executes boot application from selected device

**Transition:** → TSL once valid boot application executed

#### Phase 5: TSL (Transient System Load)

**Responsibilities:**
- OS loader is executing (e.g., GRUB, systemd-boot, Windows Boot Manager)

**Available Services:**
- **UEFI Boot Services:** Full access
  - Load kernel/drivers into memory
  - Allocate memory
  - Access file systems via Simple File System Protocol
- **UEFI Runtime Services:** Available
- **Protocols:** Access to all published protocols

**OS Loader Actions:**
1. Use Boot Services to:
   - Load OS kernel and initrd/drivers
   - Allocate memory for kernel
   - Query system configuration
   - Read ACPI tables
2. Prepare to transfer control to kernel
3. Call `ExitBootServices()`

**Transition:** → Runtime when `ExitBootServices()` called

#### Phase 6: Runtime

**Entry Point:** `ExitBootServices()` function

**Critical Transition:**
- OS loader calls `ExitBootServices()`
- Firmware tears down:
  - **Boot Services** and their memory
  - All boot-time drivers and protocols
  - Temporary data structures
- **Irreversible:** Cannot return to Boot Services

**Remaining Services:**
- **UEFI Runtime Services** only:
  - `GetVariable` / `SetVariable`: NVRAM access for UEFI variables
  - `GetTime` / `SetTime`: System time
  - `ResetSystem`: System reset/shutdown
  - `UpdateCapsule`: Firmware updates (platform-specific)

**OS Kernel:**
- Takes full ownership of system
- Can call Runtime Services throughout its operation
- ACPI tables remain available (static data)
- SMM (System Management Mode) remains resident for platform-specific tasks

### 3.2 Key UEFI Concepts

#### Hand-Off Blocks (HOBs)

**Purpose:** Pass information across the PEI → DXE boundary

**HOB Types:**
- **Resource Descriptor HOB:** Describes memory/IO resources
- **Memory Allocation HOB:** Describes allocated memory regions
- **Firmware Volume HOB:** Describes firmware volume locations
- **CPU HOB:** Describes CPU characteristics
- **Module HOB:** Contains data from specific PEIMs

**Flow:**
- **Producer:** PEI creates HOBs
- **Consumer:** DXE Core consumes HOBs

#### PEIMs vs DXE Drivers

| Aspect | PEIMs | DXE Drivers |
|--------|-------|-------------|
| **Phase** | PEI | DXE |
| **Environment** | Minimal, often XIP from flash | Full DRAM available |
| **Complexity** | Small, focused | Feature-rich, complex |
| **Communication** | PPIs (PEIM-to-PEIM Interfaces) | Protocols (via Handle Database) |
| **Memory** | Temporary (CAR or early DRAM) | Permanent (DRAM) |
| **Purpose** | Initialize memory & critical HW | Initialize all other platform HW |

#### Boot Services vs Runtime Services

| Service Type | Availability | Purpose | Examples |
|--------------|--------------|---------|----------|
| **Boot Services** | Before `ExitBootServices()` | Platform initialization, boot support | Memory allocation, Protocol access, File I/O |
| **Runtime Services** | Before and after `ExitBootServices()` | OS runtime support | Variable access, Time, Reset |

### 3.3 UEFI Secure Boot(Optional Security Feature)

If enabled, UEFI Secure Boot adds verification:

```
BDS Phase
  → Attempts to load boot application
  → Checks signature against:
    - db (signature database - allowed signers)
    - dbx (forbidden signature database)
  → If signature invalid/revoked: boot blocked
  → If valid: TSL proceeds
```

**Trust Anchor:** Platform Key (PK) stored in firmware NVRAM

---

## 4. Architecture Comparison

### 4.1 Boot Phase Mapping

| Intel x86 SBL | ARM TF-A | UEFI/PI | Primary Function |
|---------------|----------|---------|------------------|
| CSME Init | BL1 (Trusted ROM) | SEC | Hardware root of trust, platform initialization |
| ACM | BL1 (ROM verification) | SEC (ACM on Intel) | Verify next stage integrity |
| Stage1A + FSP-T | BL2 | PEI (early) | Establish temporary memory (CAR) |
| Stage1B + FSP-M | BL2 | PEI | Initialize permanent memory (DRAM) |
| Stage2 + FSP-S | BL2 (loading) / BL31 (runtime) | DXE | Silicon/device initialization |
| Payload | BL33 (U-Boot/UEFI) | BDS/TSL | Select and load OS |
| OS | OS (with PSCI calls to BL31) | RT | Operating system execution |

### 4.2 Trust Anchor Comparison

| Architecture | Root of Trust | Location | Verification Mechanism |
|--------------|---------------|----------|------------------------|
| **Intel x86 CBnT** | OEM Key Hash | PCH Field Programmable Fuses (FPF) | ACM validates KM/BPM/IBB chain |
| **ARM TF-A** | BL1 Code | Trusted ROM (immutable) | BL1 verifies BL2, BL2 verifies BL31/32/33 |
| **UEFI Secure Boot** | Platform Key (PK) | Firmware NVRAM | Signature verification against db/dbx |

### 4.3 Handoff Mechanism Comparison

| Architecture | Handoff Mechanism | Data Passed | Consumer |
|--------------|-------------------|-------------|----------|
| **Intel x86 SBL** | HOBs (Hand-Off Blocks) | Memory map, ACPI RSDP, Platform info | Payload (OS Loader) |
| **ARM TF-A** | Registers (X0-X3) | X0 = DTB address, X1 = TL/reserved | BL33 (U-Boot/UEFI) |
| **UEFI/PI** | HOBs (PEI→DXE), then Protocols (DXE→BDS→TSL) | System state, Configuration tables | DXE Core, then Boot Services consumers |

### 4.4 Security Feature Comparison

| Feature | Intel x86 SBL | ARM TF-A | UEFI/PI |
|---------|---------------|----------|---------|
| **Hardware Root of Trust** | ✅ Boot Guard (FPF fuses) | ✅ Trusted ROM (BL1) | ⚠️ Platform-dependent (can use Boot Guard or Trusted ROM) |
| **Verified Boot** | ✅ ACM verifies IBB, chain of trust | ✅ BL1→BL2→BL31/32/33 verification | ✅ Secure Boot (signature verification) |
| **Measured Boot** | ✅ TPM measurements via ACM | ✅ TPM measurements in BL stages | ✅ TPM measurements in PEI/DXE |
| **Secure Execution Environment** | ❌ (uses SMM for runtime) | ✅ BL31 at EL3, BL32 Trusted OS | ❌ (uses SMM for runtime) |
| **Runtime Security Services** | SMM (System Management Mode) | BL31 + PSCI + Trusted OS (OP-TEE) | SMM + Runtime Services |

### 4.5 Ecosystem Comparison

| Aspect | Intel x86 SBL | ARM TF-A | UEFI/PI (EDK II) |
|--------|---------------|----------|------------------|
| **Primary Use Case** | Embedded x86, IoT, Client | ARM servers, embedded, mobile | Desktops, laptops, servers (x86 & ARM) |
| **Code Size** | ⬇️ Small (optimized) | ⬇️ Small | ⬆️ Large (feature-rich) |
| **Boot Time** | ⚡ Fast | ⚡ Fast | 🐌 Slower (more initialization) |
| **Flexibility** | ⚠️ Limited (YAML config) | ✅ High (modular BL stages) | ✅ Very High (driver-based) |
| **OS Compatibility** | Linux, Windows (via UEFI payload) | Linux, Android, RTOS | Windows, Linux, BSD, etc. |
| **Silicon Vendor Support** | Intel FSP | ARM IP + Vendor BL2 | Broad (Intel, AMD, ARM, RISC-V) |

---

## 5. References

### Official Documentation Sources

#### Intel x86 / Boot Guard / CBnT
- [Intel Boot Guard Technology](https://edc.intel.com/) - Intel Embedded Design Center
- [Intel Converged Boot Guard and Intel TXT](https://edc.intel.com/content/www/us/en/design/products/platforms/details/arrow-lake/) - Arrow Lake S Datasheet
- [Introduction to Key Usage in Integrated Firmware Images](https://www.intel.com/content/www/us/en/developer/articles/technical/software-security-guidance/secure-coding/introduction-to-key-usage-in-integrated-firmware-images.html)

#### Slim Bootloader
- [Slim Bootloader Official Documentation](https://slimbootloader.github.io/)
- [SBL Boot Flow](https://slimbootloader.github.io/developer-guides/boot-flow.html)
- [SBL Supported Hardware](https://slimbootloader.github.io/supported-hardware/index.html)
- [SBL Configuration Guide](https://slimbootloader.github.io/developer-guides/configuration.html)

#### ARM Trusted Firmware-A
- [ARM Trusted Firmware-A Documentation](https://trustedfirmware-a.readthedocs.io/)
- [TF-A Firmware Design](https://trustedfirmware-a.readthedocs.io/en/latest/design/firmware-design.html)
- [TF-A Boot Flow](https://trustedfirmware-a.readthedocs.io/en/latest/getting_started/index.html)

#### UEFI/PI Specifications
- [UEFI Specifications](https://uefi.org/specifications)
- [TianoCore EDK II Documentation](https://github.com/tianocore/tianocore.github.io/wiki)
- [PI Boot Flow](https://github.com/tianocore/tianocore.github.io/wiki/PI-Boot-Flow)
- [HOB Design Discussion](https://github.com/tianocore/tianocore.github.io/wiki)

### Key Terms Glossary

| Term | Full Name | Description |
|------|-----------|-------------|
| **ACM** | Authenticated Code Module | Intel-signed code module that verifies boot firmware |
| **BDS** | Boot Device Selection | UEFI/PI phase for selecting boot device |
| **BL** | Boot Level | ARM TF-A boot stage (BL1, BL2, BL31, etc.) |
| **BPM** | Boot Policy Manifest | Defines IBB regions and Boot Guard policy |
| **CAR** | Cache-as-RAM | Using CPU cache as temporary RAM before DRAM init |
| **CBnT** | Converged Boot Guard and TXT | Intel hardware security technology |
| **CSME** | Converged Security and Management Engine | Microcontroller in PCH, first to execute |
| **DTB/FDT** | Device Tree Blob / Flattened Device Tree | Hardware description format (ARM) |
| **DXE** | Driver Execution Environment | UEFI/PI phase for driver loading |
| **EL** | Exception Level | ARM privilege level (EL0-EL3) |
| **FIP** | Firmware Image Package | ARM TF-A package format |
| **FIT** | Firmware Interface Table | Points to ACM/microcode locations |
| **FPF** | Field Programmable Fuses | Hardware fuses storing Boot Guard trust anchor |
| **FSP** | Firmware Support Package | Intel silicon initialization binary |
| **HOB** | Hand-Off Block | Data structure passing info between boot phases |
| **IBB** | Initial Boot Block | First verified boot firmware (Stage1A in SBL) |
| **IFWI** | Integrated Firmware Image | Complete firmware image (BIOS + ME + others) |
| **KM** | Key Manifest | Contains OEM public keys for Boot Guard |
| **PCH** | Platform Controller Hub | Intel chipset component |
| **PEI** | Pre-EFI Initialization | UEFI/PI phase for early initialization |
| **PEIM** | PEI Module | Module executing in PEI phase |
| **PPI** | PEIM-to-PEIM Interface | Service interface in PEI phase |
| **PSCI** | Power State Coordination Interface | ARM power management standard |
| **SBL** | Slim Bootloader | Lightweight Intel x86 bootloader |
| **SEC** | Security | First UEFI/PI phase |
| **SMC** | Secure Monitor Call | ARM instruction for Secure World services |
| **SMM** | System Management Mode | x86 high-privilege execution mode |
| **TF-A** | Trusted Firmware-A | ARM secure firmware reference implementation |
| **TPM** | Trusted Platform Module | Hardware security chip for measurements |
| **TSL** | Transient System Load | UEFI/PI phase where OS loader runs |
| **XIP** | eXecute In Place | Running code directly from flash without copying to RAM |

---

## Document Verification

This document was created by cross-referencing:

✅ **Official Intel documentation** for Boot Guard, CBnT, and Arrow Lake architecture  
✅ **Official Slim Bootloader documentation** from slimbootloader.github.io  
✅ **Official ARM TF-A documentation** from trustedfirmware-a.readthedocs.io  
✅ **Official UEFI/PI specifications** and TianoCore EDK II documentation  

**Verification Date:** January 23, 2026  
**Verified Against:** Current official documentation as of Q1 2026

> [!TIP]
> For the most up-to-date information, always consult the official documentation sources listed in the References section.

---

**Document Version:** 1.0  
**Last Updated:** 2026-01-23  
**Status:** ✅ Verified against official sources
