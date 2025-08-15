# SPDX-License-Identifier: GPL-2.0
#
# Custom Makefile for building a standalone USB Mass Storage kernel module
#

# Compiler flags (same as original)
ccflags-y := -I$(PWD)/../scsi
ccflags-y += -DDEFAULT_SYMBOL_NAMESPACE='"USB_STORAGE"'

# Define the name of the output module
obj-m := usb_storage_custom.o

# Object files to include in the module build
usb_storage_custom-objs := scsiglue.o protocol.o transport.o usb.o
usb_storage_custom-objs += initializers.o sierra_ms.o option_ms.o
usb_storage_custom-objs += usual-tables.o

# Path to the kernel build directory
KDIR := /lib/modules/$(shell uname -r)/build

# Current working directory
PWD := $(shell pwd)

# Default target
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean target
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

