obj-m += Untitled.o

CROSS_COMPILE = x86_64-linux-gnu-

CC := $(CROSS_COMPILE)gcc-12

#EXTRA_CFLAGS += -U__STRICT_ANSI__

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

