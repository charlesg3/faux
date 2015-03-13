# To build external modules, you must have a prebuilt kernel available
# that contains the configuration and header files used in the build.
# go in the kernel directory and do a
#	make oldconfig; make scripts; make prepare

CONFIG_NETMAP=m

# To build the module as in-tree, the following two lines would be sufficient

netmap_lin-objs := netmap.o # netmap_linux.o
obj-$(CONFIG_NETMAP) += netmap_lin.o

# The following commands are needed to build the modules as out-of-tree, in
# fact the kernel sources path must be specified.

# If the headers that you need are not present in the kernel sources, you must
# specify them.
#
#EXTRA_CFLAGS += -I$(PWD)/../../include
#EXTRA_CFLAGS += -DCONFIG_NETMAP=m

# The following variable is normally set inside the kernel configuration file
# and is visible to all specific module Makefiles. If you want to build your
# module without modifying the kernel configuration, than you must simulate it
# with a local variable.
#

KSRC = /usr/src/linux-3.0
KSRC = /usr/src/linux-source-3.0.0/linux-source-3.0.0
KSRC = /usr/src/linux-headers-$(shell uname -r)

#all:
#	make -C $(KSRC) M=$(PWD) modules
clean:
	make -C $(KSRC) M=$(PWD) clean
	-rm pkt-gen bridge libnetmap.so send_test recv_test txring

#test:
#	echo build in $(KSRC)

apps= pkt-gen libnetmap.so # txring
apps : $(apps)

$(apps): CFLAGS= -I../../include -O2 -Wall -Werror
pkt-gen: pkt-gen.c
	$(CC) $(CFLAGS) -o pkt-gen pkt-gen.c -L/usr/lib/i386-linux-gnu -lpcap -lpthread
libnetmap.so:	pcap.c
	$(CC) -I../../include  -fpic -c ${<}
	$(CC) -shared -o $@ pcap.o

txring: packet-tx-ring.c
	$(CC) $(CFLAGS) -o txring packet-tx-ring.c
