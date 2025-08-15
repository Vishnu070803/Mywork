# USB 2.0 Driver Work

This work focuses on USB 2.0 **HCI** driver-level operations, particularly in the **HID (Keyboard)** and **Storage Class (Bulk-Only Transport - BOT)** areas.

## Structure
- **storage/** → Default Linux USB storage drivers.
- **USB_STORAGE_CLASS/** → Custom storage class driver code.  
  - Currently working.  
  - Replace the **compatible ID** as needed for your target device.

## Storage Class Details
- Protocol: **Bulk-Only Transport (BOT)**.
- Recommendations:
  - Study BOT protocol rules for correct driver implementation.
  - Explore **SCSI command set** and understand how it links to the **Linux block layer**.
  - Refer to:
    - https://docs.kernel.org/driver-api/usb/writing_usb_driver.html
    - https://www.infineon.com/assets/row/public/documents/cross-divisions/42/infineon-an57294-usb-101-an-introduction-to-universal-serial-bus-2.0-applicationnotes-en.pdf?fileId=8ac78c8c7cdc391c017d072d8e8e5256
    - 

## HID Class Details
- Focus: **Keyboard**.
- Most of the code is Linux built-in.
- You can directly explore the default HID driver implementation for understanding.

---
**Note:** While the HID portion primarily leverages built-in Linux code, the storage class driver involves custom handling, making protocol knowledge (BOT & SCSI) essential.

