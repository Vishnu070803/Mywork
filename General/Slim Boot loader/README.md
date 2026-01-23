# Intel Boot Flow & Security Documentation: Getting Started

Welcome. This workspace contains a curated set of technical documents detailing the **Intel Arrow Lake** boot flow and the **Intel Boot Guard (CBnT)** security architecture. 

All documentation is verified against the **October 2022 Intel CSME Security White Paper** and **TianoCore Specifications**.

---

## 🧭 Suggested Learning Path

If you are new to Intel Silicon Security or the x86 boot process, follow this order:

### 1. The Big Picture (Start Here)
*   **[Narrated Speaker Guide](file:///home/admin1/Intel/review/Arrow_Lake_Boot_Flow_Narrated_Speaker_Guide.md)**
    *   **Perspective**: Educational / Narrative.
    *   **Goal**: Understand the "Story" of the silicon. It explains long journeys like "The Starter Motor" (CSME) and "The Auditor" (ACM) using engaging analogies.
    *   **Original Source**: Expanded from `Arrow_Lake_Boot_Flow_Text.txt`.

### 2. The Core Security Domains (Master Guides)
Once you have the big picture, dive into the authoritative technical mechanisms:
*   **[CSME Security Master Guide](file:///home/admin1/Intel/review/Intel_CSME_Security_Role_Master_Guide_VERIFIED.md)**
    *   **Focus**: What happens *before* the CPU wakes up. Covers PMC patches, hardwired SPI DMA, and platform gating.
    *   **Consolidated from**: `CSME_Role_Comparative_Analysis.md` and `CSME_Security_Role_Explained.md`.
*   **[Boot Guard Chain of Trust & FAQ](file:///home/admin1/Intel/review/Intel_Boot_Guard_Chain_of_Trust_Master_FAQ_VERIFIED.md)**
    *   **Focus**: The cryptographic relay. Covers the 4-step ACM audit, ACRAM "Vault" isolation, and flexibility of Key Manifests.
    *   **Consolidated from**: `Boot_Guard_Chain_of_Trust_Explained.md` and `Boot_Guard_Clarifications_FAQ.md`.

### 3. The Technical Reference (Deep Dive)
*   **[Complete Arrow Lake Technical Guide](file:///home/admin1/Intel/review/Complete_Arrow_Lake_Boot_Flow_Technical_Guide.md)**
    *   **Focus**: The massive, 1000+ line reference.
    *   **Goal**: Comparison between x86 (SBL), ARM (TF-A), and UEFI flows. Includes mermaid diagrams for every stage.

---

## 🛠 Repository Structure

| File | Type | Description |
| :--- | :--- | :--- |
| `Intel_..._Master_Guide_VERIFIED.md` | **Authority** | The most up-to-date, consolidated, and technically precise documents. |
| `Official_Intel_Sources_Analysis.md` | **Evidence** | The raw research log connecting claims to specific line numbers in Intel white papers. |
| `Boot_Guard_Clarifications_FAQ.md` | **Reference** | Answers to specific technical questions like "SBL vs UEFI" and "Why KM?". |
| `*.txt` | **Raw Text** | Plaintext versions of the boot flows for low-overhead reference. |

---

## 📜 Key Technical Terms to Know

*   **CSME**: Converged Security and Management Engine. Initializes the hardware.
*   **ACM**: Authenticated Code Module. The authoritative security auditor.
*   **FIT**: Firmware Interface Table. The "map" the CPU uses to find security modules.
*   **IBB**: Initial Boot Block. The first stage of your BIOS/SBL (e.g., Stage 1A).
*   **ACRAM**: Authenticated Code RAM. A locked-down cache region where checks are performed.
*   **PMC Patch**: A mandatory update CSME applies to the Power Controller to allow the CPU to start.

---

**Last Synchronized**: January 23, 2026  
**Source of Truth**: [Intel-CSME-Security-White-Paper](file:///home/admin1/Intel/review/intel-csme-security-white-paper-text.txt) (Oct 2022)
