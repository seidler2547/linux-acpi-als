obj-m := drivers/acpi/ drivers/als/

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

install: drivers/acpi/als.ko drivers/als/als_sys.ko
	install -D 

uninstall:
	/bin/bash restore.sh
clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

