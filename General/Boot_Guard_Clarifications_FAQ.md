# Intel Boot Guard - Critical Clarifications FAQ

> [!IMPORTANT]
> This document answers specific questions about Boot Guard applicability to different bootloaders, the necessity of manifest chains, CPU execution order, and ACM independent validation.

---

## Table of Contents

1. [Does Boot Guard Apply to Slim Bootloader (SBL)?](#1-does-boot-guard-apply-to-slim-bootloader-sbl)
2. [Why Do We Need KM? Why Not Directly Validate IBBM/BPM?](#2-why-do-we-need-km-why-not-directly-validate-ibbmbpm)
3. [CPU Execution Sequence: Reset Vector → FIT → ACM → IBB](#3-cpu-execution-sequence-reset-vector--fit--acm--ibb)
4. [How ACM Validates Independently of CSME](#4-how-acm-validates-independently-of-csme)

---

## 1. Does Boot Guard Apply to Slim Bootloader (SBL)?

### Short Answer: **YES, Absolutely!**

Boot Guard is **platform-level hardware security**, not bootloader-specific. It operates **below and before** any bootloader (SBL, UEFI, coreboot, etc.).

### Technical Explanation:

```
Boot Guard Layer: HARDWARE + FIRMWARE (CSME + ACM)
  ├─ Platform: Intel PCH + CPU
  ├─ Technology: Fuses, CSME, ACM
  ├─ Purpose: Verify FIRST CODE that runs on CPU
  └─ Validates: IBB (Initial Boot Block)

IBB can be:
  ├─ SBL Stage1A ← YES, Boot Guard applies!
  ├─ UEFI SEC phase ← YES, Boot Guard applies!
  ├─ coreboot bootblock ← YES, Boot Guard applies!
  └─ Any other firmware's first stage
```

### What Changes Between SBL and UEFI?

| Aspect | SBL | UEFI | Boot Guard Impact |
|--------|-----|------|-------------------|
| **Platform Security** | Boot Guard | Boot Guard | **IDENTICAL** |
| **ACM Validation** | ACM validates Stage1A hash | ACM validates SEC hash | **IDENTICAL** |
| **IBB Definition** | Stage1A FV | SEC/PEI Core FV | Different firmware, **same validation** |
| **FPF Fuses** | OEM Key Hash | OEM Key Hash | **IDENTICAL** |
| **Manifest Chain** | KM → IBBM → Stage1A | KM → IBBM → SEC | **IDENTICAL** |

### Visual Comparison:

```
┌─────────────────────────────────────────┐
│   Common Boot Guard Layer (Hardware)   │
│  CSME → FPF Fuses → ACM → KM → IBBM    │
└─────────────────┬───────────────────────┘
                  │ validates IBB
                  ├──────────┬──────────┐
                  ▼          ▼          ▼
            ┌──────────┐ ┌─────────┐ ┌──────────┐
            │   SBL    │ │  UEFI   │ │ coreboot │
            │ Stage1A  │ │   SEC   │ │bootblock │
            └──────────┘ └─────────┘ └──────────┘
```

### Key Insight:

The Medium article uses UEFI terminology (SEC/PEI) **only because UEFI is the most common bootloader**, but the Boot Guard mechanism itself is **bootloader-agnostic**.

**For SBL:**
- **IBB** = Stage1A Firmware Volume (FV)
- **ACM validates:** Stage1A hash from IBBM
- **After ACM:** Control transfers to Stage1A entry point
- **Everything else:** Identical to UEFI's Boot Guard flow

> [!NOTE]
> Boot Guard cares about **"what is the first code the CPU executes?"** - it doesn't care if that first code is SBL, UEFI, or anything else. It just validates its hash.

---

## 2. Why Do We Need KM? Why Not Directly Validate IBBM/BPM?

### The Core Question:

"If we just want to validate IBB, why not store the IBB hash (or IBBM key hash) directly in FPF fuses?"

### Answer: **Flexibility, Key Revocation, and Security Updates**

FPF fuses are **One-Time Programmable (OTP)** - once burned, they can **NEVER** be changed. This creates a critical design challenge.

### Scenario Analysis:

#### ❌ Bad Design: Direct IBBM Hash in Fuses

```
FPF Fuses (OTP - cannot change)
  └─ IBBM Public Key Hash (burned once)
       └─ IBBM (in flash)
            └─ IBB hashes
```

**Problem scenarios:**

1. **Key Compromise:**
   ```
   IBBM private key leaked
   → Need to revoke and use new key
   → But IBBM key hash is in FUSES
   → FUSES CANNOT BE CHANGED
   → Platform is PERMANENTLY compromised! ❌
   ```

2. **Product Line Variants:**
   ```
   OEM has 3 product lines: Laptop, Desktop, Server
   → Each needs different firmware
   → Each has different IBB
   → Need 3 different IBBM keys
   → But can only burn ONE hash in fuses
   → Either:
     a) Use same key for all (security risk)
     b) Different fuses for each SKU (manufacturing nightmare)
   ```

3. **Firmware Updates:**
   ```
   New CPU generation with security fixes
   → New IBB code required
   → New IBBM needed
   → Fuses already burned
   → CANNOT UPDATE ❌
   ```

#### ✅ Good Design: KM → IBBM Chain

```
FPF Fuses (OTP - burned once, never changes)
  └─ OEM Root Key Hash (KM key)
       └─ KM (in flash, updateable, signed by Root Key)
            └─ IBBM Public Key Hash
                 └─ IBBM (in flash, updateable, signed by IBBM key)
                      └─ IBB hashes
```

**How this solves problems:**

1. **Key Compromise (IBBM) - Recoverable:**
   ```
   IBBM key leaked
   → Generate new IBBM key pair
   → Create new KM signed by (still-secure) Root Key
            └─ Contains hash of NEW IBBM key
   → Update flash with new KM + new IBBM
   → Fuses unchanged!
   → System recovered ✅
   ```

2. **Product Line Variants - Easy:**
   ```
   KM for Laptops:
      └─ Hash of Laptop_IBBM_Key
   
   KM for Desktops:
      └─ Hash of Desktop_IBBM_Key
   
   KM for Servers:
      └─ Hash of Server_IBBM_Key
   
   All signed by SAME Root Key
   → Same fuses for all product lines
   → Different KMs in their respective flash images ✅
   ```

3. **Security Updates - Possible:**
   ```
   New vulnerability discovered
   → Need new IBB
   → Create new IBBM with new IBB hashes
   → Update KM to reference new IBBM key (if needed)
   → Update flash
   → Fuses unchanged ✅
   ```

### Security Version Numbers (SVN)

Both KM and IBBM have **SVN (Security Version Number)** fields:

```c
Key_Manifest {
    KM_SVN: 5        // Security version
    IBBM_Key_Hash: [...]
    Signature: [...]
}

IBB_Manifest {
    IBBM_SVN: 12     // Security version
    IBB_Hashes: [...]
    Signature: [...]
}
```

**SVN Anti-Rollback:**
- Platform stores "minimum acceptable SVN" in protected storage
- ACM checks: `if (manifest.SVN < minimum_SVN) → REJECT`
- Prevents attacker from using old, vulnerable firmware
- Can update minimum SVN without touching fuses

### The Indirection Benefit:

```
Fuses (Immutable) → KM (Updatable) → IBBM (Updatable) → IBB (Updatable)

Each level provides:
  Level 1 (Fuses): Hardware root - never changes
  Level 2 (KM): Policy/key flexibility - can change with Root Key signature
  Level 3 (IBBM): Firmware flexibility - can change with IBBM Key signature
  Level 4 (IBB): Code updates - can change with new hash in IBBM
```

### Real-World Analogy:

```
Your House Security:

❌ Bad: Write your house key's serial number in concrete
    → If key lost, can't change locks (concrete is permanent)

✅ Good: Write your MASTER KEY's serial in concrete
    → Master key unlocks "key cabinet" in your house
    → Key cabinet contains actual house keys
    → If house key lost, use master key to update cabinet
    → Concrete unchanged, security maintained
```

In Boot Guard:
- **Fuses** = Concrete (permanent)
- **Root Key** = Master Key serial in concrete
- **KM** = Key cabinet (updateable with master key)
- **IBBM** = House keys (updateable via key cabinet)

> [!IMPORTANT]
> **The manifest chain exists to provide security update capability while maintaining an immutable hardware root of trust.**

---

## 3. CPU Execution Sequence: Reset Vector → FIT → ACM → IBB

### The Confusion:

You mentioned: "Reset vector jumps to SEC phase where SBL/UEFI starts, FIT is below reset vector"

### Clarification Needed:

The **reset vector** does NOT directly jump to SEC/Stage1A. There are several steps in between.

### Detailed CPU Execution Sequence:

```
┌─────────────────────────────────────────────────────────────┐
│ Step 0: Power On                                            │
├─────────────────────────────────────────────────────────────┤
│ PCH powers on → CSME starts → CPU held in reset            │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 1: CSME Validates Manifests (Parallel to CPU)         │
├─────────────────────────────────────────────────────────────┤
│ CSME ROM → CSME FW → Read FPF → Validate KM → Validate IBBM│
│ CSME copies KM + IBBM to special memory region              │
│ CSME configures Boot Guard MSRs                             │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 2: CSME Releases CPU Reset                             │
├─────────────────────────────────────────────────────────────┤
│ PCH hardware signal: CPU_RESET deasserted                   │
│ CPU begins execution                                        │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 3: CPU Reset Vector (0xFFFFFFF0)                       │
├─────────────────────────────────────────────────────────────┤
│ CPU IP (Instruction Pointer) = 0xFFFFFFF0                   │
│ This address is in SPI flash (memory-mapped)                │
│ Contains: JMP instruction to startup code                   │
│                                                              │
│ Assembly at 0xFFFFFFF0:                                     │
│   JMP FAR 0xF000:0xFFF0   ; Jump to real startup code      │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 4: Early Startup Code (Still 16-bit Real Mode)        │
├─────────────────────────────────────────────────────────────┤
│ Basic CPU initialization:                                   │
│   - Disable interrupts (CLI)                                │
│   - Clear direction flag (CLD)                              │
│   - Setup temporary stack                                   │
│                                                              │
│ ⚠️ CRITICAL: Boot Guard check happens HERE via CPU ucode   │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 5: CPU Microcode Reads FIT (Automatic, in Hardware)   │
├─────────────────────────────────────────────────────────────┤
│ CPU microcode (hardcoded logic):                            │
│   1. Read FIT pointer at 0xFFFFFFC0                         │
│   2. Parse FIT table entries                                │
│   3. Find microcode update entries (Type 0x01)              │
│   4. Find ACM entry (Type 0x02)                             │
│                                                              │
│ FIT memory layout:                                          │
│   0xFFFFFFF0: [JMP instruction] ← Reset vector              │
│   0xFFFFFFC0: [FIT Pointer] ← Points to FIT table           │
│   0xFFFFB000: [FIT Table Start]                             │
│       Entry 0: FIT Header                                   │
│       Entry 1: Microcode (addr: 0xFF780000)                 │
│       Entry 2: Startup ACM (addr: 0xFF700000)               │
│       Entry 3: ...                                          │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 6: CPU Loads and Applies Microcode                    │
├─────────────────────────────────────────────────────────────┤
│ For each microcode entry in FIT:                            │
│   - Read microcode from address specified in FIT            │
│   - Verify microcode signature (Intel key in CPU)           │
│   - Load via MSR: WRMSR(0x79, microcode_address)           │
│   - CPU hardware applies patches                            │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 7: CPU Loads ACM from FIT                              │
├─────────────────────────────────────────────────────────────┤
│ Read ACM entry from FIT (Type 0x02)                         │
│ Load ACM from address: e.g., 0xFF700000                     │
│ ACM structure:                                              │
│   - ACM Header (with Intel public key)                      │
│   - ACM Code (signed by Intel)                              │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 8: CPU Verifies ACM Signature (Hardware)              │
├─────────────────────────────────────────────────────────────┤
│ CPU reads Intel public key from ACM header                  │
│ CPU hashes this public key                                  │
│ CPU reads Intel key hash from CPU FUSES                     │
│ Compare: calculated hash vs fused hash                      │
│ If MATCH → ACM key is trusted                               │
│ Then verify ACM code signature using this trusted key       │
│ If valid → Proceed to execute ACM                           │
│ If invalid → HALT (Machine Check Exception)                 │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 9: CPU Executes ACM in AC RAM (Authenticated Code)    │
├─────────────────────────────────────────────────────────────┤
│ CPU enters special execution mode:                          │
│   - Configure L3 cache as Authenticated Code RAM (AC RAM)   │
│   - Lock memory access (no DMA, no snooping)                │
│   - Copy ACM code to AC RAM                                 │
│   - Jump to ACM entry point                                 │
│                                                              │
│ ACM now running in isolated, secure environment             │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 10: ACM Re-Validates KM and IBBM (Independent Check)  │
├─────────────────────────────────────────────────────────────┤
│ ACM reads KM/IBBM from memory region (CSME provided)        │
│                                                              │
│ ACM validation (INDEPENDENT of CSME):                       │
│   1. Read FPF fuses (same fuses CSME read)                  │
│   2. Re-validate KM public key hash against fuse            │
│   3. Re-validate KM signature                               │
│   4. Re-validate IBBM public key hash against KM            │
│   5. Re-validate IBBM signature                             │
│                                                              │
│ ⚠️ ACM doesn't TRUST CSME - it RE-DOES all checks!         │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 11: ACM Verifies IBB Hash                              │
├─────────────────────────────────────────────────────────────┤
│ ACM extracts IBB region info from (now-trusted) IBBM:       │
│   - IBB base address (e.g., 0xFFFF0000)                     │
│   - IBB size (e.g., 0x10000 = 64KB)                         │
│   - Expected hash (SHA256/384)                              │
│                                                              │
│ ACM reads actual IBB code from flash                        │
│ ACM calculates: hash = SHA256(IBB_code)                     │
│ ACM compares: calculated_hash vs expected_hash              │
│ If MATCH → IBB is trusted                                   │
│ If MISMATCH → Check error policy:                           │
│   - If ENFORCEMENT=HALT → Platform halts                    │
│   - If ENFORCEMENT=LOG → Log error, continue                │
│                                                              │
│ If Measured Boot enabled:                                   │
│   - Extend IBB hash into TPM PCR 0                          │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 12: ACM Transfers Control to IBB                       │
├─────────────────────────────────────────────────────────────┤
│ ACM completes validation                                    │
│ ACM exits AC RAM mode                                       │
│ ACM sets up CPU state for IBB entry                         │
│ ACM jumps to IBB entry point:                               │
│                                                              │
│   For SBL:  Jump to Stage1A entry (IBB base address)        │
│   For UEFI: Jump to SEC Core entry point                    │
│                                                              │
│ ✅ VERIFIED CODE NOW EXECUTING                              │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ Step 13: IBB (Stage1A or SEC) Begins Execution             │
├─────────────────────────────────────────────────────────────┤
│ SBL Path:                                                   │
│   → Stage1A → Stage1B → Stage2 → Payload → OS              │
│                                                              │
│ UEFI Path:                                                  │
│   → SEC → PEI → DXE → BDS → OS Loader → OS                 │
└─────────────────────────────────────────────────────────────┘
```

### Key Insights:

1. **Reset vector is just the starting point** - it's a JMP instruction, not the actual firmware
2. **FIT processing happens automatically** - CPU microcode handles this
3. **ACM validation is mandatory** - can't skip to IBB without ACM verification
4. **IBB only executes AFTER** complete validation chain

### Memory Map at Reset:

```
0xFFFFFFFF ┐
           │ [unused]
0xFFFFFFF0 ├─ Reset Vector (JMP instruction) ← CPU starts HERE
           │
0xFFFFFFC0 ├─ FIT Pointer ← CPU microcode reads THIS
           │
           │ [BIOS Code Region]
           │
0xFFFFB000 ├─ FIT Table (example location)
           │   Entry 0: Header
           │   Entry 1: Microcode @ 0xFF780000
           │   Entry 2: ACM @ 0xFF700000
           │   Entry 3: IBB entry
           │
           │ [More BIOS]
           │
0xFFFF0000 ├─ IBB Start (Stage1A or SEC) ← ACM validates, then jumps HERE
           │
           │ [IBB Code - 64KB example]
           │
0xFFFC0000 ├─ [Rest of BIOS]
           │
           ⋮
0xFF800000 ├─ Microcode region
           │
0xFF700000 ├─ ACM region
           │
           ⋮
0xFF000000 └─ SPI Flash base (memory-mapped)
```

---

## 4. How ACM Validates Independently of CSME

### The Confusion:

"You said ACM doesn't believe CSME and revalidates BPM/IBBM, but IBBM hash needs KM, KM needs CSME validation..."

### Critical Understanding: **Two Independent Validators**

Both CSME and ACM perform **the same validation** using **the same trust anchors**, but they don't trust each other's results.

### The Actual Flow:

```
┌─────────────────────────────────────────────────────────────┐
│                    PARALLEL VALIDATION                       │
│  (Both use SAME fuses, both do SAME crypto, independent)    │
├────────────────────────┬────────────────────────────────────┤
│   CSME Path            │   ACM Path (later)                 │
├────────────────────────┼────────────────────────────────────┤
│ 1. CSME reads FPF      │ 1. ACM reads FPF                   │
│    fuses               │    fuses (SAME fuses!)             │
│                        │                                    │
│ 2. CSME validates KM:  │ 2. ACM validates KM:               │
│    • Read KM from      │    • Read KM from memory           │
│      flash            │      (CSME put it there)           │
│    • Hash KM pubkey    │    • Hash KM pubkey                │
│    • Compare with      │    • Compare with                  │
│      FPF hash          │      FPF hash                      │
│    • Verify KM sig     │    • Verify KM sig                 │
│    ✅ KM trusted       │    ✅ KM trusted                   │
│                        │                                    │
│ 3. CSME validates      │ 3. ACM validates                   │
│    IBBM:               │    IBBM:                           │
│    • Read IBBM         │    • Read IBBM from memory         │
│    • Hash IBBM pubkey  │    • Hash IBBM pubkey              │
│    • Compare with      │    • Compare with                  │
│      hash in KM        │      hash in KM                    │
│    • Verify IBBM sig   │    • Verify IBBM sig               │
│    ✅ IBBM trusted     │    ✅ IBBM trusted                 │
│                        │                                    │
│ 4. CSME prepares:      │ 4. ACM validates IBB:              │
│    • Copy KM to memory │    • Read IBB hash from IBBM       │
│    • Copy IBBM to      │    • Calculate actual IBB hash     │
│      memory            │    • Compare                       │
│    • Set Boot Guard    │    ✅ IBB trusted                  │
│      MSRs              │                                    │
│    • Release CPU       │ 5. ACM jumps to IBB                │
└────────────────────────┴────────────────────────────────────┘
```

### Why CSME Validates First:

```c
// CSME validation purpose:
// 1. Determine if Boot Guard is enabled (from fuses)
// 2. Validate that manifests EXIST and are CORRECTLY SIGNED
// 3. Prepare platform for ACM execution
// 4. Put manifests in memory for ACM to access
// 5. Release CPU reset so ACM can run

// ACM validation purpose:
// 1. DON'T TRUST CSME (could be compromised)
// 2. RE-VALIDATE everything independently
// 3. Provide CRYPTOGRAPHIC PROOF of IBB integrity
// 4. Transfer control only if ALL checks pass
```

### The Trust Model:

```
Trust Anchors (Shared by both CSME and ACM):
  ├─ FPF Fuses (OEM Root Key Hash)
  ├─ CSME ROM (Intel key for ME FW)
  └─ CPU Fuses (Intel key for ACM)

CSME Trust Chain:
  CSME ROM → CSME FW → FPF Fuses → KM → IBBM
  Purpose: Platform setup, manifest availability

ACM Trust Chain:
  CPU Fuses → ACM → FPF Fuses → KM → IBBM → IBB
  Purpose: Cryptographic verification before execution
  
❗ ACM does NOT trust CSME's validation results
❗ ACM DOES trust the same fuses CSME used
```

### Code Example - ACM Validation Logic:

```c
// ACM entry point (simplified)
void acm_main() {
    // Step 1: Read Boot Guard configuration
    // (CSME set these MSRs, but we'll verify independently)
    boot_guard_config_t *config = read_boot_guard_msrs();
    
    // Step 2: Read same FPF fuses that CSME read
    uint8_t oem_key_hash[32];
    read_fpf_fuse(FPF_OEM_ROOT_KEY_HASH, oem_key_hash, 32);
    
    // Step 3: Get KM from memory (CSME put it there)
    // BUT we don't trust CSME's validation!
    key_manifest_t *km = (key_manifest_t *)config->km_address;
    
    // Step 4: INDEPENDENTLY validate KM
    if (!acm_validate_km(km, oem_key_hash)) {
        // KM invalid - HALT regardless of what CSME said
        boot_guard_failure("KM validation failed");
        halt();
    }
    
    // Step 5: Get IBBM from memory
    ibb_manifest_t *ibbm = (ibb_manifest_t *)config->ibbm_address;
    
    // Step 6: INDEPENDENTLY validate IBBM using KM
    if (!acm_validate_ibbm(ibbm, km)) {
        // IBBM invalid - HALT
        boot_guard_failure("IBBM validation failed");
        halt();
    }
    
    // Step 7: Validate actual IBB code
    if (!acm_validate_ibb_code(ibbm)) {
        // IBB code doesn't match IBBM hash - HALT
        boot_guard_failure("IBB hash mismatch");
        halt();
    }
    
    // Step 8: Everything validated, safe to execute
    transfer_control_to_ibb(ibbm->ibb_entry_point);
}

// ACM validates KM (independent from CSME)
bool acm_validate_km(key_manifest_t *km, uint8_t fused_hash[32]) {
    // Same crypto CSME did, but WE do it ourselves
    
    // 1. Hash the KM public key
    uint8_t calculated_hash[32];
    sha256(&km->oem_public_key, sizeof(km->oem_public_key), calculated_hash);
    
    // 2. Compare with fuse (same fuse CSME read)
    if (memcmp(calculated_hash, fused_hash, 32) != 0) {
        return false;  // Key doesn't match fuse
    }
    
    // 3. Verify KM signature
    uint8_t km_body_hash[32];
    sha256(&km->body, km->body_size, km_body_hash);
    
    uint8_t expected_hash[32];
    rsa_verify(&km->signature, &km->oem_public_key, expected_hash);
    
    if (memcmp(km_body_hash, expected_hash, 32) != 0) {
        return false;  // Signature invalid
    }
    
    // 4. Check SVN (anti-rollback)
    if (km->svn < get_minimum_km_svn()) {
        return false;  // Rollback attack detected
    }
    
    return true;  // KM is valid
}
```

### Why ACM Doesn't Just Trust CSME:

**Defense in Depth / Zero Trust Principle:**

1. **CSME could be compromised:**
   - ME firmware vulnerability exploited
   - Attacker could put fake manifests in memory
   - ACM must verify independently

2. **Different threat models:**
   - CSME: Platform management, broad attack surface
   - ACM: Minimal code, single purpose, harder to compromise

3. **Hardware separation:**
   - CSME runs on separate processor (PCH)
   - ACM runs on main CPU in isolated mode
   - They don't share code or trust

4. **Complementary validation:**
   - CSME ensures manifests are available
   - ACM ensures they're cryptographically valid
   - Both use same trust anchor (fuses) but independent validation

### Analogy:

```
Airport Security (CSME) vs Pilot Pre-Flight Check (ACM):

Security checkpoint (CSME):
  - Checks ticket, ID, scans bags
  - Clears passenger for boarding
  - Puts passenger info in system

Pilot pre-flight (ACM):
  - Doesn't trust security blindly
  - Does own safety checklist
  - Verifies flight plan independently
  - Checks weight/balance independently
  - Only then takes off

Both use same source of truth:
  - Passenger manifest (like manifests in flash)
  - FAA regulations (like fuse policy)
  
But pilot doesn't say "security checked, so I won't"
```

### The Critical Insight:

```
CSME validates: "These manifests are AVAILABLE and SIGNED"
              → Prepares platform
              → Provides manifest LOCATION to ACM

ACM validates:  "I will INDEPENDENTLY VERIFY these manifests"
              → Re-does all crypto checks
              → Uses same fuses, same algorithms
              → Doesn't trust CSME's judgment
              → Only executes IBB if validation succeeds
```

**CSME and ACM are COOPERATIVE but NOT TRUSTING:**
- CSME: "Here's where the manifests are"
- ACM: "Thanks, but I'll verify them myself before executing anything"

---

## Summary

### Question 1: SBL Applicability
✅ **YES** - Boot Guard is platform-level, applies identically to SBL, UEFI, or any bootloader

### Question 2: Why KM Instead of Direct IBBM?
✅ **Flexibility** - Allows key revocation, product variants, and security updates without re-fusing hardware

### Question 3: CPU Execution Sequence
✅ **Order:**
```
Reset Vector (0xFFFFFFF0)
  → CPU microcode reads FIT
  → Load microcode
  → Load ACM
  → Verify ACM (Intel key in CPU fuse)
  → Execute ACM
  → ACM validates KM/IBBM/IBB
  → ACM jumps to IBB (Stage1A/SEC)
```

### Question 4: ACM Independent Validation
✅ **ACM re-validates everything:**
- CSME provides manifest **LOCATION**
- ACM uses **SAME FUSES** to re-validate
- ACM doesn't trust CSME's validation **RESULTS**
- Both do identical crypto, independently
- Defense in depth / zero trust

---

> [!TIP]
> **Mental Model:** Think of Boot Guard as a relay race where each runner (CSME, ACM) independently verifies the baton is genuine before running their leg, but they all use the same rulebook (fuses).
