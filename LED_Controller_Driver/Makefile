obj-m += led_controller.o

CROSS_COMPILE ?= /usr/bin/aarch64-linux-gnu-
KDIR := /linux_kernel/linux/
PWD := $(shell pwd)

all:
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean
