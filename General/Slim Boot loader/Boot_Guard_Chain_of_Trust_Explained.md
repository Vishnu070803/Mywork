# Intel Boot Guard Chain of Trust - Deep Technical Explanation

> [!IMPORTANT]
> This document explains **how each stage in Intel Boot Guard validates the next stage**, answering the fundamental question: "On what basis does each component perform verification?"

---

## Table of Contents

1. [The Chain of Trust Overview](#the-chain-of-trust-overview)
2. [Stage-by-Stage Trust Verification](#stage-by-stage-trust-verification)
3. [Cryptographic Trust Flow](#cryptographic-trust-flow)
4. [Technical Implementation Details](#technical-implementation-details)
5. [Common Confusions Clarified](#common-confusions-clarified)

---

## The Chain of Trust Overview

### The Core Principle: Each Stage Validates the Next

```mermaid
graph TD
    A[Power On] --> B[CSME Boot ROM]
    B -->|Hardware Path| C[CSME Firmware]
    C -->|PCH Init| D[Clocks, PMC, GPIO, SPI Init]
    D -->|Release| E[CPU Reset Released]
    E --> F[CPU Reset Vector]
    F --> G[FIT Table Read]
    G --> H[Microcode Loaded]
    H --> I[ACM Loaded]
    I --> J[ACM Signature Verified]
    J --> K[ACM Executes in ACRAM]
    K --> L[ACM Reads FPF Fuses]
    L --> M[ACM Validates KM]
    M --> N[ACM Validates BPM]
    N --> O[ACM Verifies IBB Hash]
    O --> P[IBB (Stage1A) Executes]
    
    style B fill:#ff6b6b
    style D fill:#4ecdc4
    style J fill:#ffe66d
    style N fill:#95e1d3
```

### Trust Anchors in the Chain

| Component | Trust Anchor | What Validates It | What It Validates |
|-----------|--------------|-------------------|-------------------|
| **CSME ROM** | Immutable ROM in PCH | Hardware logic | CSME Firmware (Intel Key) |
| **CSME Firmware** | RSA signature (Intel) | CSME ROM | PCH Hardware (Clocks, PMC) |
| **FPF Fuses** | Hardware fuses (OTP) | Physically burned (EoM) | Nothing (stores Trust Anchor) |
| **PMC Patch**| Signed binary | CSME Firmware | CPU Power Sequencing |
| **ACM** | RSA signature | CPU (Intel key in fuses) | KM/BPM/IBB chain |
| **KM (Key Manifest)** | RSA signature (OEM) | ACM (using FPF hash) | BPM signature |
| **BPM (Boot Policy)** | RSA signature (OEM) | ACM (using KM key) | IBB hash |
| **IBB (Stage1A)** | SHA256/384 hash | ACM (using BPM hash) | Next stages (SBL) |

---

## Stage-by-Stage Trust Verification

### Stage 1: CSME ROM Execution

**What:** CSME ROM code executes first when power is applied

**Trust Anchor:** Immutable ROM code baked into the PCH silicon

**How It Works:**
```
Power Applied
  → PCH power rails stabilize
  → CSME microcontroller releases from reset
  → CSME begins executing from ROM address 0x0
  → ROM code is PHYSICALLY IMMUTABLE (mask ROM or OTP)
```

**Key Point:** This is the **hardware root of trust** - the ROM cannot be modified after manufacturing.

---

### Stage 2: CSME Authenticates CSME Firmware

**Question:** On what basis does CSME authenticate its own firmware?

**Answer:** Using **RSA signature verification** with Intel's public key embedded in CSME ROM

#### Technical Process:

```
CSME ROM contains Intel's public key (hardcoded)
  ↓
CSME ROM reads CSME firmware from SPI Flash (ME region in IFWI)
  ↓
ME region contains:
  - Firmware code (encrypted/compressed)
  - RSA signature (signed by Intel's private key)
  - Manifest with metadata
  ↓
CSME ROM verifies:
  1. Calculate SHA256 hash of firmware code
  2. Decrypt signature using Intel public key from ROM
  3. Compare calculated hash with decrypted signature
  ↓
If match: firmware is authentic → load and execute
If mismatch: HALT (platform won't boot)
```

#### Code Flow (Conceptual):

```c
// Inside CSME ROM (immutable)
const uint8_t INTEL_PUBLIC_KEY[256] = { /* RSA public key */ };

bool authenticate_csme_firmware() {
    // Read CSME firmware from SPI flash
    csme_fw_t *fw = read_spi_flash(ME_REGION_BASE);
    
    // Extract signature from firmware
    rsa_sig_t *signature = &fw->manifest.signature;
    
    // Calculate hash of firmware code
    uint8_t calculated_hash[32];
    sha256(fw->code, fw->code_size, calculated_hash);
    
    // Decrypt signature using Intel public key
    uint8_t expected_hash[32];
    rsa_verify(signature, INTEL_PUBLIC_KEY, expected_hash);
    
    // Compare
    if (memcmp(calculated_hash, expected_hash, 32) == 0) {
        return true;  // Signature valid
    }
    return false;  // Signature invalid - HALT
}
```

**Trust Chain:** CSME ROM (immutable) → Intel Public Key (embedded) → CSME Firmware Signature

---

### Stage 3: CSME Reads Boot Guard Fuses (FPF)

**Question:** Why does CSME read fuses?

**Answer:** To determine **Boot Guard policy** and retrieve the **OEM Root Key Hash** (trust anchor for manifests)

#### What's Stored in FPF Fuses:

The Field Programmable Fuses (FPF) in the PCH contain:

1. **Boot Guard Enable Bit** - Is Boot Guard active?
2. **Boot Guard Policy:**
   - Verified Boot: ACM verifies IBB before execution
   - Measured Boot: ACM measures IBB into TPM
   - Both: Verify AND measure
3. **OEM Root Key Hash (SHA256)** - Hash of OEM's public key
4. **Boot Guard Profile** - Legacy vs. CBnT
5. **Error Policy** - Shutdown vs. ignore on verification failure

#### Why CSME Reads Fuses:

```
CSME reads FPF to answer:
  1. "Is Boot Guard enabled?"
  2. "What policy mode (Verify/Measure)?"
  3. "What is the OEM's trusted root key hash?"
  4. "What should I do if verification fails?"
```

#### Technical Process:

```c
// CSME firmware reads fuses
boot_guard_config_t config;

// Read fuses from PCH hardware
config.enabled = read_fuse(FPF_BOOT_GUARD_ENABLE);
config.policy = read_fuse(FPF_BOOT_GUARD_POLICY);  // Verify/Measure
config.oem_key_hash[32] = read_fuse_array(FPF_OEM_KEY_HASH, 32);
config.error_policy = read_fuse(FPF_ERROR_ENFORCEMENT);

// If Boot Guard disabled, skip manifest checks
if (!config.enabled) {
    release_cpu_reset();  // Traditional boot
    return;
}

// Otherwise, enforce Boot Guard
enforce_boot_guard(&config);
```

**Key Point:** Fuses are **One-Time Programmable (OTP)** - set once at factory/ODM, cannot be changed. This makes them a **hardware root of trust**.

---

### Stage 4: CSME Validates Key Manifest (KM)

**Question:** How does CSME validate KM, and on what basis?

**Answer:** CSME validates the KM signature using the **OEM Root Key Hash stored in FPF fuses**

#### Key Manifest (KM) Structure:

```
Key Manifest {
    Version
    KM_SVN (Security Version Number)
    KM_ID
    OEM_Public_Key       ← The actual RSA public key
    KM_Hash (self-hash)
    RSA_Signature        ← Signed by OEM's private key
}
```

#### Validation Process:

```
Step 1: Read KM from SPI flash
  → KM is stored in Boot Guard Data Section of IFWI
  
Step 2: Extract OEM public key from KM structure
  → oem_pubkey = KM.OEM_Public_Key

Step 3: Calculate hash of OEM public key
  → calculated_hash = SHA256(oem_pubkey)

Step 4: Compare with hash burned in fuses
  → fused_hash = read_fuse(FPF_OEM_KEY_HASH)
  → if (calculated_hash == fused_hash) → KEY IS TRUSTED
  → if (calculated_hash != fused_hash) → HALT

Step 5: Verify KM signature using the now-trusted OEM public key
  → km_hash = SHA256(KM_body)
  → expected_hash = RSA_decrypt(KM.Signature, oem_pubkey)
  → if (km_hash == expected_hash) → KM IS AUTHENTIC
```

#### Code Flow (Conceptual):

```c
bool validate_key_manifest(key_manifest_t *km, uint8_t fused_hash[32]) {
    // Step 1: Extract OEM public key from KM
    rsa_pubkey_t *oem_key = &km->oem_public_key;
    
    // Step 2: Hash the OEM public key
    uint8_t calculated_hash[32];
    sha256(oem_key, sizeof(*oem_key), calculated_hash);
    
    // Step 3: Compare with fused hash (TRUST ANCHOR)
    if (memcmp(calculated_hash, fused_hash, 32) != 0) {
        return false;  // Key doesn't match fuse - HALT
    }
    
    // Step 4: Now that we trust the key, verify KM signature
    uint8_t km_body_hash[32];
    sha256(&km->body, km->body_size, km_body_hash);
    
    uint8_t expected_hash[32];
    rsa_verify(&km->signature, oem_key, expected_hash);
    
    return (memcmp(km_body_hash, expected_hash, 32) == 0);
}
```

**Trust Chain:** FPF OEM Key Hash (fused) → KM Public Key (validated) → KM Signature (verified)

---

### Stage 5: CSME Validates Boot Policy Manifest (BPM)

**Question:** How does CSME validate BPM, and on what basis?

**Answer:** CSME validates the BPM signature using the **OEM Public Key from the already-validated KM**

#### Boot Policy Manifest (BPM) Structure:

```
Boot Policy Manifest {
    Version
    BPM_SVN (Security Version Number)
    ACM_SVN_Minimum
    IBB_Segments[] {          ← List of IBB regions
        base_address
        size
        flags
    }
    IBB_Digest[]              ← SHA256/384 hashes of each IBB segment
    Platform_Manufacturer_Data
    RSA_Signature             ← Signed by same OEM key as KM
}
```

#### Validation Process:

```
Step 1: KM has already been validated (previous stage)
  → We now TRUST the OEM public key from KM

Step 2: Read BPM from SPI flash
  → BPM is stored next to KM in Boot Guard Data Section

Step 3: Calculate hash of BPM body
  → bpm_hash = SHA256(BPM_body)

Step 4: Verify BPM signature using trusted OEM public key from KM
  → expected_hash = RSA_decrypt(BPM.Signature, KM.OEM_Public_Key)
  → if (bpm_hash == expected_hash) → BPM IS AUTHENTIC
  → if (bpm_hash != expected_hash) → HALT
```

#### Code Flow (Conceptual):

```c
bool validate_boot_policy_manifest(
    boot_policy_manifest_t *bpm,
    key_manifest_t *validated_km  // Already validated!
) {
    // Step 1: Extract the trusted OEM key from validated KM
    rsa_pubkey_t *oem_key = &validated_km->oem_public_key;
    
    // Step 2: Hash the BPM body
    uint8_t bpm_hash[32];
    sha256(&bpm->body, bpm->body_size, bpm_hash);
    
    // Step 3: Decrypt signature using OEM key
    uint8_t expected_hash[32];
    rsa_verify(&bpm->signature, oem_key, expected_hash);
    
    // Step 4: Compare
    return (memcmp(bpm_hash, expected_hash, 32) == 0);
}
```

**Trust Chain:** KM.OEM_Public_Key (already validated) → BPM Signature (verified)

---

### Stage 6: CSME Releases CPU from Reset

**What Happens:** After validating KM and BPM, CSME allows the CPU to start executing

**Technical Process:**

```c
// CSME has now validated:
// ✅ CSME firmware (via ROM)
// ✅ Key Manifest (via FPF hash)
// ✅ Boot Policy Manifest (via KM key)

// CSME configures platform
configure_platform_security(km, bpm);

// Release CPU from reset state
write_register(PCH_RESET_CONTROL, CPU_RESET_RELEASE);

// CPU now starts executing at reset vector
```

**Key Point:** This is a **hardware handoff** - CSME physically releases the CPU reset signal.

---

### Stage 7: CPU Executes Reset Vector

**What:** CPU begins executing at address `0xFFFFFFF0` (top 16 bytes of 4GB address space)

**How It's Coded:**

The SPI flash is memory-mapped into the CPU's address space:

```
Physical Memory Map (reset state):
  0x00000000 - 0xFFFFFFFF: Various regions
  0xFF000000 - 0xFFFFFFFF: SPI Flash mapped here (16MB region)
  0xFFFFFFF0: Reset vector (16 bytes from top)
```

At address `0xFFFFFFF0`, there's a **JMP instruction** to the real startup code:

```assembly
; At 0xFFFFFFF0 (Reset Vector)
jmp far 0xF000:0xFFF0    ; Jump to real 16-bit startup code

; Real startup code location
startup_code:
    cli                   ; Disable interrupts
    cld                   ; Clear direction flag
    
    ; Load GDT for protected mode
    lgdt [gdt_descriptor]
    
    ; Find FIT pointer
    mov eax, 0xFFFFFFC0   ; FIT pointer location
    mov esi, [eax]        ; ESI = FIT address
    
    ; Continue...
```

**Key Point:** The reset vector is **hard-wired in the CPU** - it always starts at `0xFFFFFFF0`.

---

### Stage 8: CPU Loads Microcode via FIT

**Question:** How does CPU load microcode? How does FIT help? How does CPU know?

**Answer:** CPU uses the **Firmware Interface Table (FIT)** - a standardized structure that points to microcode and ACM locations

#### FIT (Firmware Interface Table) Structure:

The FIT is a table stored in SPI flash at a **fixed location**: `0xFFFFFFC0` (64 bytes before top of flash)

```
FIT Pointer (at 0xFFFFFFC0):
  → Points to FIT Table base address

FIT Table Structure:
Entry 0: FIT Header
  - Type: 0x00 (Header)
  - Address: FIT table address
  - Size: Number of entries
  - Version: 0x0100

Entry 1: Microcode Update
  - Type: 0x01
  - Address: 0xFF800000 (example)
  - Size: 0x20000 (128KB)
  - Checksum

Entry 2: Startup ACM
  - Type: 0x02
  - Address: 0xFF780000 (example)
  - Size: 0x40000 (256KB)
  - Checksum

Entry 3: BIOS Startup Module
  - Type: 0x07
  - Address: 0xFFFF0000
  - ...
```

#### How CPU Finds and Loads Microcode:

```c
// CPU microcode (simplified representation)
void cpu_load_microcode() {
    // Step 1: Read FIT pointer at fixed location
    uint32_t *fit_pointer = (uint32_t *)0xFFFFFFC0;
    fit_entry_t *fit_table = (fit_entry_t *)(*fit_pointer);
    
    // Step 2: Parse FIT entries
    uint32_t num_entries = fit_table[0].size;  // Header entry
    
    for (int i = 1; i < num_entries; i++) {
        fit_entry_t *entry = &fit_table[i];
        
        // Step 3: Find microcode entry
        if (entry->type == FIT_MICROCODE_UPDATE) {
            // Step 4: Load microcode from specified address
            void *ucode = (void *)entry->address;
            uint32_t ucode_size = entry->size * 16;  // Size in 16-byte units
            
            // Step 5: Verify microcode signature (Intel-signed)
            if (verify_microcode_signature(ucode)) {
                // Step 6: Apply microcode patch using MSR
                apply_microcode(ucode);
            }
        }
    }
}

void apply_microcode(void *ucode) {
    // Write microcode to special CPU register
    uint64_t ucode_addr = (uint64_t)ucode;
    wrmsr(MSR_IA32_BIOS_UPDT_TRIG, ucode_addr);
    
    // CPU hardware loads and validates microcode internally
}
```

**Key Points:**
- FIT location (`0xFFFFFFC0`) is **hard-coded in CPU logic**
- CPU **automatically** reads FIT after reset
- Microcode has **Intel RSA signature** verified by CPU hardware
- Trust anchor: Intel public key **fused into CPU silicon**

---

### Stage 9: CPU Loads ACM via FIT

**Question:** How does CPU load ACM? Similar to microcode?

**Answer:** **Yes**, very similar process using FIT, but ACM is executable code (not just CPU patches)

#### ACM Loading Process:

```c
void cpu_load_acm() {
    fit_entry_t *fit_table = get_fit_table();
    
    // Step 1: Find ACM entry in FIT
    for (int i = 0; i < fit_table[0].size; i++) {
        fit_entry_t *entry = &fit_table[i];
        
        // Step 2: Check for Startup ACM
        if (entry->type == FIT_STARTUP_ACM) {
            // Step 3: Load ACM from address specified in FIT
            acm_header_t *acm = (acm_header_t *)entry->address;
            
            // Step 4: Verify ACM (next stage - see below)
            if (verify_acm_signature(acm)) {
                // Step 5: Execute ACM in secure mode
                execute_acm(acm);
            } else {
                halt_platform();  // ACM signature invalid
            }
        }
    }
}
```

**Difference from Microcode:**
- **Microcode**: CPU patches (modify CPU behavior)
- **ACM**: Executable program (runs security checks)

---

### Stage 10: CPU Verifies ACM Signature

**Question:** How does CPU verify ACM signature? On what basis?

**Answer:** Using **Intel's public key fused into the CPU silicon** during manufacturing

#### ACM Signature Verification:

```
ACM Structure in Flash:
  ACM_Header {
      module_type
      header_len
      module_vendor (0x8086 = Intel)
      date
      size
      ...
      rsa_public_key     ← Intel's public key
      rsa_signature      ← Signature of ACM code
  }
  ACM_Code {
      ... actual ACM executable code ...
  }
```

#### Verification Process:

```
Step 1: CPU extracts Intel public key from ACM header
  → acm_pubkey = ACM.rsa_public_key

Step 2: CPU calculates hash of this public key
  → calculated_hash = SHA256(acm_pubkey)

Step 3: CPU compares with hash FUSED IN CPU DIE
  → cpu_fused_hash = read_cpu_fuse(INTEL_KEY_HASH)
  → if (calculated_hash == cpu_fused_hash) → KEY IS TRUSTED
  → if not: IMMEDIATE HALT (security violation)

Step 4: Using the now-trusted key, verify ACM code signature
  → acm_code_hash = SHA256(ACM_Code)
  → expected_hash = RSA_decrypt(ACM.Signature, acm_pubkey)
  → if (acm_code_hash == expected_hash) → ACM CODE IS AUTHENTIC
```

#### Code Flow (Conceptual):

```c
bool verify_acm_signature(acm_header_t *acm) {
    // Step 1: Extract public key from ACM
    rsa_pubkey_t *acm_key = &acm->rsa_public_key;
    
    // Step 2: Hash the public key
    uint8_t calculated_key_hash[32];
    sha256(acm_key, sizeof(*acm_key), calculated_key_hash);
    
    // Step 3: Read Intel key hash from CPU fuses
    uint8_t cpu_fused_hash[32];
    read_cpu_fuse(INTEL_ACM_KEY_HASH_FUSE, cpu_fused_hash, 32);
    
    // Step 4: Compare (TRUST ANCHOR CHECK)
    if (memcmp(calculated_key_hash, cpu_fused_hash, 32) != 0) {
        // Key doesn't match CPU fuse - CRITICAL SECURITY VIOLATION
        trigger_machine_check(); // CPU halt
        return false;
    }
    
    // Step 5: Key is trusted, now verify ACM code signature
    void *acm_code = (void *)((uint8_t *)acm + acm->header_len);
    uint32_t acm_code_size = acm->size - acm->header_len;
    
    uint8_t acm_code_hash[32];
    sha256(acm_code, acm_code_size, acm_code_hash);
    
    uint8_t expected_hash[32];
    rsa_verify(&acm->signature, acm_key, expected_hash);
    
    return (memcmp(acm_code_hash, expected_hash, 32) == 0);
}
```

**Trust Chain:** CPU Fused Intel Key Hash (hardware) → ACM Public Key (validated) → ACM Code Signature (verified)

**Key Point:** This verification happens **entirely in CPU hardware** - no software involvement possible.

---

### Stage 11: ACM Executes in Authenticated Code RAM

**What:** ACM runs in a special secure CPU environment

**Technical Details:**

```
Authenticated Code RAM (AC RAM):
  - Special region of CPU cache (typically L3)
  - Isolated from normal execution
  - Cannot be accessed by other code
  - No external bus snooping allowed
  - CPU enters special execution mode
```

#### How ACM Execution Works:

```c
void execute_acm(acm_header_t *acm) {
    // Step 1: Setup Authenticated Code RAM
    // - Configure L3 cache as secure execution space
    // - Lock out all DMA/external access
    setup_ac_ram();
    
    // Step 2: Copy ACM code to AC RAM
    void *acm_code = get_acm_code(acm);
    memcpy(AC_RAM_BASE, acm_code, acm->code_size);
    
    // Step 3: Enter Authenticated Code Execution Mode
    // - Set ACMOD CPU mode flag
    // - All caches locked to AC RAM only
    // - External memory access blocked
    enter_ac_mode();
    
    // Step 4: Jump to ACM entry point
    acm_entry_point_t entry = (acm_entry_point_t)AC_RAM_BASE;
    entry();  // Execute ACM code
    
    // ACM returns here after completing verification
    exit_ac_mode();
}
```

**Key Point:** AC RAM is **hardware-enforced isolation** - ACM cannot be tampered with while running.

---

### Stage 12: ACM Authoritative Manifest Validation

**Question:** Does ACM just trust the manifests CSME provided?

**Answer:** **No.** ACM does not trust CSME. It independently re-reads the FPF fuses and performs a cryptographic validation of the entire manifest chain.

#### The Authoritative Chain of Trust (Line 371):

1. **Read FPF Fuses**: ACM reads the `BP.KEY` (OEM Root Key Hash) directly from the PCH fuses.
2. **Authorize KM**: ACM hashes the public key in the **Key Manifest (KM)** and compares it to the `BP.KEY` fuse.
3. **Verify KM Signature**: ACM verifies the RSA signature of the KM to ensure it hasn't been tampered with.
4. **Authorize BPM**: ACM hashes the public key in the **Boot Policy Manifest (BPM)** and compares it to the hash stored in the (now-trusted) KM.
5. **Verify BPM Signature**: ACM verifies the RSA signature of the BPM using the KM's public key.
6. **Determine IBB Hash**: ACM extracts the authoritative IBB hash from the verified BPM.

---

### Stage 13: ACM Verifies IBB (Stage1A) Hash

**Question:** How does ACM verify IBB?

**Answer:** ACM reads the **IBB hash(es) from the authoritatively validated BPM** and compares them with the **actual hash** of firmware in flash.

#### IBB Verification Process:

```
ACM has access to:
  - Key Manifest (KM)
  - Boot Policy Manifest (BPM)
  - FPF fuses (read directly)

BPM defines:
  IBB_Element[] {
      base_address: 0xFFFF0000
      size: 0x10000 (64KB)
      flags: 0x0
  }
  IBB_Digest[] {
      hash_alg: SHA256
      digest: [32 bytes of SHA256 hash]
  }
```

#### ACM IBB Verification Code (Conceptual):

```c
// Runs inside ACM (in AC RAM)
bool acm_verify_ibb(boot_policy_manifest_t *bpm) {
    // Step 1: Extract IBB regions from BPM
    uint32_t num_ibb_elements = bpm->num_ibb_elements;
    
    for (int i = 0; i < num_ibb_elements; i++) {
        ibb_element_t *ibb = &bpm->ibb_elements[i];
        ibb_digest_t *expected_digest = &bpm->ibb_digests[i];
        
        // Step 2: Read actual IBB code from SPI flash
        void *ibb_code = (void *)ibb->base_address;
        uint32_t ibb_size = ibb->size;
        
        // Step 3: Calculate hash of actual IBB code
        uint8_t calculated_hash[32];
        if (expected_digest->hash_alg == HASH_ALG_SHA256) {
            sha256(ibb_code, ibb_size, calculated_hash);
        } else if (expected_digest->hash_alg == HASH_ALG_SHA384) {
            sha384(ibb_code, ibb_size, calculated_hash);
        }
        
        // Step 4: Compare with expected hash from BPM
        if (memcmp(calculated_hash, expected_digest->digest, 32) != 0) {
            // IBB hash mismatch!
            handle_boot_guard_failure(bpm);
            return false;  // Will halt or continue based on policy
        }
    }
    
    // Step 5: All IBB segments verified
    // If measured boot enabled, measure into TPM
    if (bpm->policy & BOOT_GUARD_MEASURED) {
        tpm_extend_pcr(0, ibb_hash);
    }
    
    return true;  // IBB verified successfully
}

void handle_boot_guard_failure(boot_policy_manifest_t *bpm) {
    // Check error enforcement policy
    if (bpm->error_enforcement == ENFORCE_POLICY_HALT) {
        // Critical failure - HALT system
        cpu_halt();
    } else {
        // Log error but continue boot
        log_boot_guard_error();
    }
}
```

**Trust Chain:** FPF Fuses (Hardwired) → KM (Authoritative) → BPM (Authoritative) → Actual IBB Code (calculated hash).

**Key Point:** ACM performs the "Heavy Lifting" of cryptographic validation. CSME's earlier checks are for platform gating, but ACM's check is the **definitive security auditor**.

---

### Stage 14: Control Transfers to Stage1A

**What Happens:** After successful verification, ACM jumps to Stage1A code

```c
// ACM final step
void acm_transfer_to_ibb() {
    // Step 1: Verify IBB (done above)
    if (!acm_verify_ibb(bpm)) {
        halt();  // Verification failed
    }
    
    // Step 2: Setup environment for IBB
    // - Exit AC mode
    // - Setup initial CPU state
    exit_ac_mode();
    setup_cpu_for_ibb();
    
    // Step 3: Get IBB entry point from BPM
    uint32_t ibb_entry = bpm->ibb_elements[0].base_address;
    
    // Step 4: Jump to IBB (Stage1A)
    void (*stage1a_entry)(void) = (void (*)(void))ibb_entry;
    stage1a_entry();  // Start Stage1A execution
    
    // Never returns
}
```

**Key Point:** By the time control reaches Stage1A, there has been a complete **cryptographic chain of trust** from hardware fuses all the way through every code stage.

---

## Cryptographic Trust Flow

### Complete Chain Summary

```
Hardware Root (Immutable)
  ├─ CSME ROM (immutable silicon)
  │    └─ Contains: Intel Public Key (hardcoded)
  │         └─ Validates: CSME Firmware Signature
  │              └─ Result: CSME Firmware Trusted
  │
  ├─ PCH FPF Fuses (one-time programmable)
  │    └─ Contains: OEM Root Key Hash (SHA256)
  │         └─ Validates: Key Manifest Public Key
  │              └─ Result: OEM Public Key Trusted
  │
  └─ CPU Silicon Fuses (factory programmed)
       └─ Contains: Intel ACM Key Hash
            └─ Validates: ACM Public Key
                 └─ Result: ACM Code Trusted

Software Chain (Validated)
  ├─ CSME Firmware (validated by ROM)
  │    └─ Reads: FPF Fuses
  │    └─ Validates: Key Manifest (using FPF hash)
  │         └─ Result: KM.OEM_Public_Key Trusted
  │
  ├─ Key Manifest (validated by FPF)
  │    └─ Contains: OEM Public Key
  │    └─ Validates: Boot Policy Manifest Signature
  │         └─ Result: BPM Trusted
  │
  ├─ Boot Policy Manifest (validated by KM)
  │    └─ Contains: IBB Hash(es)
  │    └─ Used by: ACM for IBB verification
  │
  ├─ ACM (validated by CPU)
  │    └─ Reads: BPM IBB Digests
  │    └─ Calculates: Actual IBB Hash
  │    └─ Compares: Expected vs Actual
  │         └─ Result: IBB (Stage1A) Trusted
  │
  └─ IBB / Stage1A (validated by ACM)
       └─ Can now verify: Stage1B, Stage2, etc.
            └─ Chain continues...
```

### Cryptographic Operations Summary

| Stage | Operation | Input | Trust Anchor | Output |
|-------|-----------|-------|--------------|--------|
| **CSME ROM** | RSA Verify | CSME FW + Signature | Intel Public Key (ROM) | Trusted CSME FW |
| **CSME FW** | SHA256 Compare | KM Public Key | OEM Key Hash (FPF) | Trusted OEM Key |
| **CSME FW** | RSA Verify | KM Body + Signature | Trusted OEM Key | Trusted KM |
| **CSME FW** | RSA Verify | BPM Body + Signature | Trusted OEM Key (from KM) | Trusted BPM |
| **CPU** | SHA256 Compare | ACM Public Key | Intel Key Hash (CPU Fuse) | Trusted ACM Key |
| **CPU** | RSA Verify | ACM Code + Signature | Trusted ACM Key | Trusted ACM Code |
| **ACM** | SHA256 Compare | IBB Code | IBB Hash (from BPM) | Trusted IBB |

---

## Technical Implementation Details

### How FIT Works in Hardware

```
CPU Reset Vector Logic (Silicon):

1. Power-on / Reset
   - Set instruction pointer to 0xFFFFFFF0
   - Memory controller maps SPI flash to 0xFF000000

2. Execute first instruction at 0xFFFFFFF0
   - Usually: JMP to real startup code

3. CPU Microcode scans for FIT
   - Read pointer at 0xFFFFFFC0
   - Validate FIT signature "_FIT_   "
   - Parse FIT entries

4. Process Microcode Entries
   - Find type 0x01 entries
   - Load each microcode module
   - Apply via MSR 0x79

5. Process ACM Entry
   - Find type 0x02 entry
   - Verify signature (Intel key in CPU)
   - Load into AC RAM
   - Execute
```

### How ACM Gets Data from CSME

```
Memory-Mapped Communication:

CSME prepares data before releasing CPU:
  1. CSME validates KM and BPM
  2. CSME copies KM, BPM to special memory region
  3. CSME sets up Boot Guard Configuration MSRs
  4. CSME releases CPU reset

CPU/ACM reads data:
  1. ACM reads Boot Guard MSRs
  2. ACM finds KM/BPM in memory region
  3. ACM re-validates (doesn't trust CSME blindly!)
  4. ACM uses BPM IBB info for verification
```

**Key Point:** ACM **doesn't trust CSME** - it **re-validates** the manifests independently using the same FPF fuses!

### SPI Flash Layout

```
SPI Flash (16MB example):
  0xFF000000 - 0xFF0FFFFF: Descriptor Region (4KB)
  0xFF100000 - 0xFF6FFFFF: ME Region (CSME Firmware)
  0xFF700000 - 0xFF71FFFF: Boot Guard Data (KM + BPM)
  0xFF720000 - 0xFF77FFFF: ACM
  0xFF780000 - 0xFF7FFFFF: Microcode
  0xFF800000 - 0xFFFFEFFF: BIOS Region
      0xFFFF0000 - 0xFFFFEFFF: Stage1A (IBB) ← Verified by ACM
      0xFFFFB000 - 0xFFFFBFFF: FIT Table
      0xFFFFFFC0: FIT Pointer
      0xFFFFFFF0: Reset Vector
```

---

## Common Confusions Clarified

### Confusion 1: "CSME validates everything"

**❌ Wrong:** CSME is not a cryptographic co-verifier for ACM or IBB

**✅ Correct:**
- CSME validates **its own firmware** and the **manifest chain** (KM/BPM)
- CSME **enforces policy** (reads fuses, releases CPU)
- **ACM independently re-validates** and verifies IBB
- CSME and ACM use the **same trust anchors** (FPF fuses) but work independently

### Confusion 2: "CPU trusts ACM because CSME said so"

**❌ Wrong:** CPU doesn't ask CSME for permission

**✅ Correct:**
- CPU has **Intel's public key hash fused into silicon**
- CPU **independently verifies** ACM signature
- CSME releasing CPU reset and CPU verifying ACM are **separate, independent operations**

### Confusion 3: "ACM trusts CSME's validation of BPM"

**❌ Wrong:** ACM doesn't trust anyone

**✅ Correct:**
- CSME validates KM/BPM and puts them in memory
- ACM **reads the same FPF fuses** and **re-validates KM/BPM** itself
- ACM uses validated BPM to get IBB hash
- ACM **calculates actual IBB hash** and compares

### Confusion 4: "How does CPU know to use FIT?"

**✅ Answer:** 
- FIT pointer location (`0xFFFFFFC0`) is **part of the Intel CPU specification**
- CPU **microcode** has built-in logic to read FIT after reset
- This is **hard-coded in the CPU silicon** - not configurable

### Confusion 5: "Each stage helps the next"

**✅ Correct Interpretation:**

| Stage | Helps Next Stage By... |
|-------|------------------------|
| **CSME ROM** | Validates CSME FW so it can run |
| **CSME FW** | Validates KM/BPM, releases CPU so it can boot |
| **CPU** | Loads microcode (enables features), loads and verifies ACM |
| **Microcode** | Enables CPU features needed by ACM |
| **ACM** | Verifies IBB hash, enables IBB to execute |
| **IBB (Stage1A)** | Establishes CAR, loads Stage1B |

### Confusion 6: "Why so many stages?"

**✅ Answer:** **Defense in Depth**

```
Each stage has a LIMITED scope of trust:

CSME ROM → Only needs to trust: Immutable ROM
CSME FW  → Only needs to trust: ROM + Intel Key
CPU      → Only needs to trust: CPU Fuses + FIT spec
ACM      → Only needs to trust: FPF Fuses + BPM
IBB      → Only needs to trust: ACM verification result

This creates a "trust chain" where:
- Each stage is SMALL and SIMPLE
- Each stage validates ONLY the next stage
- Compromise of one stage doesn't compromise earlier stages
- Hardware roots (ROM, Fuses) cannot be modified
```

---

## Summary: The Complete Trust Flow

```
POWER ON
  ↓
[CSME ROM] ← IMMUTABLE SILICON
  ↓ (validates using Intel Public Key embedded in ROM)
[CSME Firmware] ← RSA SIGNATURE CHECK
  ↓ (reads FPF fuses for OEM Key Hash)
[Key Manifest] ← SHA256 KEY HASH + RSA SIGNATURE
  ↓ (provides OEM Public Key to validate)
[Boot Policy Manifest] ← RSA SIGNATURE (using OEM key from KM)
  ↓ (releases CPU reset)
[CPU Reset Vector]
  ↓ (reads FIT at 0xFFFFFFC0)
[Microcode] ← RSA SIGNATURE (Intel key in CPU fuse)
  ↓ (enables CPU features)
[ACM] ← RSA SIGNATURE (Intel key in CPU fuse)
  ↓ (re-validates KM/BPM, reads IBB hash from BPM)
[IBB Hash Verification] ← SHA256 HASH COMPARISON
  ↓ (hash matches!)
[Stage1A Executes] ← VERIFIED CODE
  ↓
[Continue SBL boot flow...]
```

### Three Trust Anchors

1. **CSME ROM** (immutable silicon) → validates CSME firmware
2. **FPF Fuses** (OTP hardware) → validates OEM manifests
3. **CPU Fuses** (factory programmed) → validates Intel ACM

### Three Validation Techniques

1. **RSA Signature Verification**
   - Used for: CSME FW, KM, BPM, ACM
   - Process: Hash content → Decrypt signature → Compare

2. **SHA256 Hash Comparison**
   - Used for: Public keys (KM, ACM), IBB code
   - Process: Hash content → Compare with trusted hash

3. **Hardware Enforcement**
   - Used for: Fuses (FPF, CPU silicon)
   - Process: Read-only hardware values set at factory

---

## Conclusion

**Every stage has a clear trust anchor and validation mechanism:**

- Nothing is "trusted by default"
- Everything traced back to immutable hardware (ROM/Fuses)
- Each stage does **independent cryptographic verification**
- CSME doesn't "help" ACM verify - they work in parallel with same trust anchors
- CPU doesn't "ask" CSME - it has its own fused trust anchor

This creates a **cryptographically verifiable chain of trust** from power-on to OS, where **every stage is mathematically proven to be authentic** before execution.
