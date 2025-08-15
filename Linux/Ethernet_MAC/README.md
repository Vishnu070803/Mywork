# BL702 Ethernet MAC Driver

This driver is developed for the **Ethernet MAC IP** in the **BL702 Board**.

## Current Status
- The driver successfully **registers and links** with the PHY.
- **Transmit (`xmit`) function** is implemented, but **not tested** due to hardware issues.
- **Receive function** is partially implemented.
- The driver currently under development is available in:  
  `Currently_working.c`

## Recommended Exploration
To better understand the driver implementation, explore the documentation and pay special attention to:
- Functionality of **MII bus (MDIO)**
- **PHY linking** process
- **Transmit (`xmit`) function**
- **NAPI** concepts
- Linking of **NAPI receive functionality**

Additionally:
- Search on GitHub for **BL702 EMAC baremetal code** to reference register-level operations.
- Refer to the **BL702 User Manual** for detailed hardware behavior and register descriptions.

---
**Note:** This driver is still in active development and may require hardware verification before production use.

