# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2026 Saúl Fernández García <https://github.com/saulfernandezgarcia>
#

PWD := $(CURDIR)
MID := sfg		#Module Installation Directory. They will be installed at /lib/modules/<kernelversion>/sfg/
KVER := $(shell uname -r)
KDIR := /lib/modules/$(KVER)/build


.PHONY: all clean install uninstall reinstall mic minstall


all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) INSTALL_MOD_DIR=$(MID) modules_install
	depmod -a

mic: all install clean

minstall: all install

reinstall: clean all install


####################################################################

# Experimental

uninstall:
	- modprobe -r check_a
	- modprobe -r check_b
	- modprobe -r sfgcore
	rm -rf /lib/modules/$(KVER)/$(MID)
	depmod -a

#

load:
	modprobe sfgcore
	modprobe check_a
	modprobe check_b

# Unloading must reverse the order of loading. At the very least, sfgcore must be the first to load and the last to unload.

unload:
	- modprobe -r check_a
	- modprobe -r check_b
	- modprobe -r sfgcore