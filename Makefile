PWD := $(CURDIR)
MID := sfg		#Module Installation Directory
# they will be installed at /lib/modules/<kernelversion>/sfg/

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	$(MAKE) INSTALL_MOD_DIR=$(MID) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules_install
	depmod -a

# ^^ include in "install" function also a "depmod -a"?

minstall: all install

mic: all install clean

uninstall:
	-modprobe -r core
	-rrmod plugin_a
	rm -r /lib/modules/$(shell uname -r)/sfg
	depmod -a

# insert (with modprobe)?
# remove?