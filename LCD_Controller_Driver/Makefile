KDIR := /linux_kernel/out/
CROSS_COMPILE := /usr/bin/aarch64-linux-gnu-
ARCH := arm64
MODULE_NAME := lcd_driver
obj-m := $(MODULE_NAME).o

all:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) clean
