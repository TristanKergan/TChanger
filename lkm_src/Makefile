obj-m += hide_tracer.o

KDIR   ?= /lib/modules/$(shell uname -r)/build
PWD    := $(shell pwd)

ccflags-y := -Wall -Wextra -O2

.PHONY: all clean

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
