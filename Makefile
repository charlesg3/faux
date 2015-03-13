fos_src_root := $(shell realpath .)
fos_bld_root := $(shell realpath ./build)

ARCH := $(shell getconf LONG_BIT)
HARE_ROOT := $(shell realpath $(fos_src_root))
HARE_CONTRIB := $(shell realpath $(fos_src_root)/../hare-contrib)
HARE_INSTALL := /afs/csail/u/c/cg3/public_html

CHROOT_DIR :=
HARE_RUNNING := $(shell ps ax | grep faux.py | grep -v grep | wc -l)

FAUX_OPT += -ggdb3
FAUX_OPT += -O3 -mtune=native -msse2
#FAUX_OPT += -DNDEBUG

FAUX_CFLAGS = -pedantic -Wall -Werror -Wshadow -Wno-strict-aliasing -Wno-vla -Wno-write-strings -Wno-sequence-point
FAUX_CFLAGS += $(FAUX_OPT)
FAUX_CFLAGS += -I$(fos_bld_root)/include/ -fPIC -lpthread -march=native

ifeq ($(ARCH),32)
FAUX_CFLAGS += -DVDSOWRAP
endif

FAUX_CXXFLAGS := $(FAUX_CFLAGS) -std=gnu++0x
FAUX_CFLAGS += -std=gnu99

# FAUX_CXXFLAGS += -DNDEBUG
# FAUX_CFLAGS += -DNDEBUG

GDB := gdb

# preload stuffs
ifeq ($(ARCH),64)
LIBFAUX := $(shell realpath $(fos_bld_root))/libs/basicp/libfaux32.so
else
LIBFAUX :=$(shell realpath $(fos_bld_root))/libs/basicp/libfaux.so
endif
LIBVDSO := $(shell realpath $(fos_bld_root))/libs/vdsowrap/libvdso-32.so
PRELOAD_LIBS_32 := $(LIBFAUX) $(LIBVDSO)
PRELOAD_LIBS_64 := $(LIBFAUX) $(LIBVDSO)
PRELOAD_LIBS := $(PRELOAD_LIBS_$(ARCH))
PRELOAD := LD_PRELOAD="$(PRELOAD_LIBS)" 

ifeq ($(ARCH),64)
PATH_=$(CHROOT_DIR)/usr/local/bin:$(CHROOT_DIR)/bin:$(CHROOT_DIR)/usr/bin:
LD_PATH=$(CHROOT_DIR)/lib:$(CHROOT_DIR)/usr/lib:$(CHROOT_DIR)/lib/i386-linux-gnu:$(CHROOT_DIR)/usr/lib/i386-linux-gnu
PYTHONPATH=$(CHROOT_DIR)/usr/lib/python2.7
HARE_SERVER_ENV := LD_LIBRARY_PATH=$(LD_PATH) PYTHONPATH=$(PYTHONPATH) HARE_ROOT=$(HARE_ROOT) HARE_CONTRIB=$(HARE_CONTRIB) HARE_BUILD=$(shell realpath $(fos_bld_root))
HARE_ENV := $(PRELOAD) PATH=$(PATH_) $(HARE_SERVER_ENV)
else
PATH_=/usr/local/bin:/usr/bin:/bin
HARE_SERVER_ENV := HARE_ROOT=$(HARE_ROOT) HARE_CONTRIB=$(HARE_CONTRIB) HARE_BUILD=$(shell realpath $(fos_bld_root))
HARE_ENV := $(PRELOAD) PATH=$(PATH_) $(HARE_SERVER_ENV)
endif

HARE_DEPS := $(PRELOAD_LIBS) hare

include $(fos_src_root)/common/makefiles/Makefile.faux
include $(fos_src_root)/Makefile.fos.in

DEPS := $(fos_bld_root)/sys/server/server $(fos_bld_root)/name/nameserver $(LIBFAUX)

include $(fos_src_root)/Makefile.fs_tests

.PHONY: hare
hare: common/scripts/faux.py ${DEPS}
ifeq (,$(findstring local,$(MODE)))
ifeq ($(HARE_RUNNING),0) # not running? run it
	@$(HARE_SERVER_ENV) $< nameserver server="" &
else # if it is running and we'll clean then we can run it as well
ifeq (,$(findstring clean,$(MAKECMDGOALS)))
else
	@$(HARE_SERVER_ENV) $< nameserver server="" &
endif
endif
	@sleep 0.5
endif

.PHONY: cleanup
cleanup:
	-@killall -q -TERM faux.py
	-@sleep 0.5
	-@killall -q -KILL server
	-@killall -q -KILL nameserver
	-@sleep 0.5
	-@rm -f /dev/shm/faux-shm-*
	-@rm -f $(HARE_BUILD)/fifo.*


