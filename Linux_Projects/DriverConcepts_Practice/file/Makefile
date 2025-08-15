obj-m += dynamic_file.o

# Use CROSS_COMPILE variable
CROSS_COMPILE = x86_64-linux-gnu-
CC := $(CROSS_COMPILE)gcc-12

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

