obj-m+=dynamic.o
CROSS_COMPILE = x86_64-linux-gnu-
CC := $(CROSS_COMPILE)gcc-12
KERNELDIR := /lib/modules/$(shell uname -r)/build
EXTRA_CFLAGS += -U__STRICT_ANSI__
PWD := $(shell pwd)
all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=x86_64 CROSS_COMPILE=$(CROSS_COMPILE) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	
