# Makefile to build the netmap modules

CONFIG_NETMAP:=m
CONFIG_R8169:=n
CONFIG_E1000:=m
CONFIG_E1000E:=m
CONFIG_IXGBE:=m
CONFIG_IGB:=m
CONFIG_FORCEDETH:=n

obj-$(CONFIG_NETMAP) += netmap/
obj-$(CONFIG_E1000) += e1000/
obj-$(CONFIG_E1000E) += e1000e/
obj-$(CONFIG_IXGBE) += ixgbe/
obj-$(CONFIG_IGB) += igb/
obj-$(CONFIG_R8169) += r8169.o
obj-$(CONFIG_FORCEDETH) += forcedeth.o

NETMAP_OPTIONS += 

#r8169-objs := r8169.o

# this is needed to access symbols exported by netmap module. It wouldn't be
# necessary if the module was built in-tree.
KBUILD_EXTRA_SYMBOLS=$(PWD)/../netmap/Module.symvers

KSRC ?= /usr/src/linux-headers-$(shell uname -r)
#KSRC ?= /usr/src/linux-3.0.3
EXTRA_CFLAGS += -I$(PWD) -I$(PWD)/../include
EXTRA_CFLAGS += -DCONFIG_NETMAP=m -Werror -Wall 
#-DCONFIG_NETMAP_DEBUG

all:
	make -C $(KSRC) M=$(PWD) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules

clean:
	make -C $(KSRC) M=$(PWD) clean
	(cd netmap && make clean)
	rm -rf */Module.symvers */.tmp_versions

bridge:
	(cd netmap; make bridge)
pkt-gen:
	(cd netmap; make bridge)

addmods: e1000-r-$(CONFIG_E1000) ixgbe-r-$(CONFIG_IXGBE) e1000e-r-$(CONFIG_E1000E) netmap_addmod e1000e-a-$(CONFIG_E1000E) e1000-a-$(CONFIG_E1000) ixgbe-a-$(CONFIG_IXGBE) 


scaleback-%:
	sudo ifconfig $* down
	sudo ethtool -g $*
	sudo ethtool -G $* rx 64
	sudo ethtool -G $* tx 64
	sudo ethtool -g $*
	sudo ifconfig $* promisc
	sudo ifconfig $* up

netmap_addmod:
	sudo rmmod netmap_lin || true
	sudo insmod netmap/netmap_lin.ko $(NETMAP_OPTIONS) || true

%-a-n:
	@true 

%-r-n:
	@true

%-r-m:
	sudo rmmod $* || true

%-a-m:
	sudo insmod $*/$*.ko || true
