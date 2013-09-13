
#--------------------------------------------------------------
# Just run 'make menuconfig', configure stuff, then run 'make'.
# You shouldn't need to mess with anything beyond this point...
#--------------------------------------------------------------
TOPDIR=$(shell pwd)
Q :=@

KTREE = $(TOPDIR)/src/kern-ucore
ifndef O
OBJPATH_ROOT := $(TOPDIR)/obj
else
OBJPATH_ROOT := $(abspath $(O))
endif
export TOPDIR KTREE OBJPATH_ROOT

CONFIG = package/config
CONFIG_SCRIPT = $(OBJPATH_ROOT)/.config
DEFCONFIG_SCRIPT = $(OBJPATH_ROOT)/.defconfig
export KCONFIG_CONFIG=$(CONFIG_SCRIPT)
CONFIG_DIR := $(OBJPATH_ROOT)/config
KCONFIG_AUTOCONFIG=$(CONFIG_DIR)/auto.conf
KCONFIG_AUTOHEADER=$(CONFIG_DIR)/autoconf.h

CONFIG_SHELL:=$(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	else if [ -x /bin/bash ]; then echo /bin/bash; \
	else echo sh; fi; fi)

MAKEFLAGS += -rR --no-print-directory

-include $(KCONFIG_AUTOCONFIG)

#### CROSS COMPILE HERE ####
ARCH ?= $(patsubst "%",%,$(UCONFIG_ARCH))
ARCH ?= amd64
BOARD ?= default
CROSS_COMPILE ?= $(UCONFIG_CROSS_COMPILE)

CONFIG_CONFIG_IN = $(KTREE)/arch/$(ARCH)/Kconfig
CONFIG_DEFCONFIG = $(KTREE)/arch/$(ARCH)/configs/$(BOARD)_defconfig

export CONFIG_SHELL quiet Q KBUILD_VERBOSE
export ARCH CROSS_COMPILE
export KCONFIG_AUTOHEADER KCONFIG_AUTOCONFIG


TARGET_CC := $(CROSS_COMPILE)gcc
TARGET_LD := $(CROSS_COMPILE)ld
TARGET_AR := $(CROSS_COMPILE)ar
TARGET_STRIP := $(CROSS_COMPILE)strip
TARGET_OBJCOPY := $(CROSS_COMPILE)objcopy

export TARGET_CC TARGET_LD TARGET_AR TARGET_LD TARGET_STRIP TARGET_OBJCOPY


ifndef HOSTAR
HOSTAR:=ar
endif
ifndef HOSTAS
HOSTAS:=as
endif
ifndef HOSTCC
HOSTCC:=gcc
else
endif
ifndef HOSTCXX
HOSTCXX:=g++
endif
ifndef HOSTLD
HOSTLD:=ld
endif
ifndef HOSTLN
HOSTLN:=ln
endif
HOSTAR:=$(shell $(CONFIG_SHELL) -c "which $(HOSTAR)" || type -p $(HOSTAR) || echo ar)
HOSTAS:=$(shell $(CONFIG_SHELL) -c "which $(HOSTAS)" || type -p $(HOSTAS) || echo as)
HOSTCC:=$(shell $(CONFIG_SHELL) -c "which $(HOSTCC)" || type -p $(HOSTCC) || echo gcc)
HOSTCXX:=$(shell $(CONFIG_SHELL) -c "which $(HOSTCXX)" || type -p $(HOSTCXX) || echo g++)
HOSTLD:=$(shell $(CONFIG_SHELL) -c "which $(HOSTLD)" || type -p $(HOSTLD) || echo ld)
HOSTLN:=$(shell $(CONFIG_SHELL) -c "which $(HOSTLN)" || type -p $(HOSTLN) || echo ln)
ifndef CFLAGS_FOR_BUILD
CFLAGS_FOR_BUILD:=-g -O2
endif
export HOSTAR HOSTAS HOSTCC HOSTCXX HOSTLD

HOSTCFLAGS=$(CFLAGS_FOR_BUILD)
export HOSTCFLAGS

PHONY+=defconfig menuconfig clean all tags amd64tags run

all: kernel

$(CONFIG_SCRIPT): $(DEFCONFIG_SCRIPT) | $(CONFIG_DIR)
	@cp $(CONFIG_DEFCONFIG) $(CONFIG_SCRIPT);

$(DEFCONFIG_SCRIPT): | $(CONFIG_DIR)
	@if [ -e $(CONFIG_DEFCONFIG) ]; then \
		cp $(CONFIG_DEFCONFIG) $(DEFCONFIG_SCRIPT); \
	else \
		echo No defconfig found for ARCH \"$(ARCH)\" and BOARD \"$(BOARD)\"!; \
		exit 1; \
	fi

defconfig: $(DEFCONFIG_SCRIPT) $(CONFIG)/conf | $(CONFIG_DIR)
	@rm -f $(KCONFIG_AUTOCONFIG) $(KCONFIG_AUTOHEADER)
	@cp $(DEFCONFIG_SCRIPT) $(CONFIG_SCRIPT)
	@$(CONFIG)/conf -D $(CONFIG_SCRIPT) $(CONFIG_CONFIG_IN)

$(CONFIG)/conf:
	$(MAKE) CC="$(HOSTCC)" -C $(CONFIG) conf

$(CONFIG)/mconf:
	$(MAKE) CC="$(HOSTCC)" -C $(CONFIG) mconf

menuconfig: $(CONFIG_DIR) $(CONFIG)/mconf $(CONFIG_SCRIPT)
	rm -f $(KCONFIG_AUTOCONFIG) $(KCONFIG_AUTOHEADER)
	@if ! $(CONFIG)/mconf $(CONFIG_CONFIG_IN); then \
		test -f .config.cmd || rm -f $(CONFIG_SCRIPT); \
	fi

PHONY += kernel userlib userapp

kernel: $(OBJPATH_ROOT) $(KCONFIG_AUTOHEADER) $(KCONFIG_AUTOCONFIG)
	$(Q)$(MAKE)  -C $(KTREE) -f $(KTREE)/Makefile.build

userlib: $(OBJPATH_ROOT) $(KCONFIG_AUTOCONFIG)
	$(Q)$(MAKE) -f $(TOPDIR)/src/libs-user-ucore/Makefile -C $(TOPDIR)/src/libs-user-ucore  all

userapp: $(OBJPATH_ROOT) $(KCONFIG_AUTOCONFIG)
	$(Q)$(MAKE) -f $(TOPDIR)/src/user-ucore/Makefile -C $(TOPDIR)/src/user-ucore  all

## TOOLS 

ifdef UCONFIG_HAVE_SFS
TOOLS_MKSFS_DIR := $(TOPDIR)/src/ht-mksfs
TOOLS_MKSFS := $(OBJPATH_ROOT)/mksfs
$(TOOLS_MKSFS): | $(OBJPATH_ROOT)
	$(Q)$(MAKE) CC=$(HOSTCC) -f $(TOOLS_MKSFS_DIR)/Makefile -C $(TOOLS_MKSFS_DIR) all

## image
FATIMG_FILE := $(OBJPATH_ROOT)/fat.img
SFSIMG_LINK := $(OBJPATH_ROOT)/sfs.img
SFSIMG_FILE := $(OBJPATH_ROOT)/sfs-orig.img
TMPSFS := $(OBJPATH_ROOT)/.tmpsfs
sfsimg: $(SFSIMG_LINK)

fatimg: | $(OBJPATH_ROOT)
	@@rm -f $(FATIMG_FILE)
	@dd if=/dev/zero of=$(FATIMG_FILE) bs=1M count=128 >/dev/null 2>&1
	@mkfs -t vfat $(FATIMG_FILE)

$(SFSIMG_LINK): $(SFSIMG_FILE)
	@ln -sf sfs-orig.img $@

$(SFSIMG_FILE): $(TOOLS_MKSFS) userlib userapp FORCE | $(OBJPATH_ROOT)
	@echo Making $@
	@rm -rf $(TMPSFS)
	@mkdir -p $(TMPSFS)
	@mkdir -p $(TMPSFS)/lib/modules
	@cp -r $(OBJPATH_ROOT)/modules $(TMPSFS)/lib
	@cp -r $(OBJPATH_ROOT)/user-ucore/bin $(TMPSFS)
ifneq ($(UCORE_TEST),)
	@cp -r $(OBJPATH_ROOT)/user-ucore/testbin $(TMPSFS)
endif
	@$(Q)$(MAKE) -f $(TOPDIR)/src/user-ucore/Makefile -C $(TOPDIR)/src/user-ucore initial_dir
	-cp -r $(TOPDIR)/src/user-ucore/_initial/* $(TMPSFS)
	@rm -f $@
	@dd if=/dev/zero of=$@ bs=1M count=$(UCONFIG_SFS_IMAGE_SIZE) >/dev/null 2>&1
	@$(TOOLS_MKSFS) $@ $(TMPSFS)
	@rm -rf $(TMPSFS)

endif

ifdef UCONFIG_SWAP
SWAPFS_FILE := $(OBJPATH_ROOT)/swap.img
swapimg: $(SWAPFS_FILE)

$(SWAPFS_FILE): | $(OBJPATH_ROOT)
	@echo Making $@
	$(Q)dd if=/dev/zero of=$@ bs=1M count=128 >/dev/null 2>&1
endif

$(OBJPATH_ROOT):
	-mkdir -p $@

$(CONFIG_DIR):
	-mkdir -p $@

#	$(Q)rm -f $(KCONFIG_AUTOCONFIG) $(KCONFIG_AUTOHEADER)
clean:
	@echo CLEAN ALL
	$(Q)rm -f  $(CONFIG_SCRIPT).old config.cmd .tmpconfig.h
	$(Q)rm -f $(SFSIMG_FILE)
	$(Q)rm -rf $(OBJPATH_ROOT)
	-$(Q)$(MAKE) -C $(CONFIG) clean
	$(Q)$(MAKE) -C $(KTREE) -f Makefile.build clean
	$(Q)$(MAKE) -f $(TOPDIR)/src/libs-user-ucore/Makefile -C $(TOPDIR)/src/libs-user-ucore  clean
	$(Q)$(MAKE) -f $(TOPDIR)/src/user-ucore/Makefile -C $(TOPDIR)/src/user-ucore  clean
	$(Q)rm -f tags

indent:
	$(Q)find $(TOPDIR)/src -name *.c -or -name *.h | grep -vf $(TOPDIR)/misc/indent-whitelist | xargs $(TOPDIR)/misc/Lindent
	$(Q)rm -rf `find $(TOPDIR)/src -name *~`

tags:
	@echo TAGS ALL
	$(Q)rm -f cscope.files cscope.in.out cscope.out cscope.po.out tags
	$(Q)ctags -R .
	$(Q)find . -type f -name "*.[chS]" >cscope.files
	$(Q)cscope -bq 

amd64tags:
	$(Q)./buildamd64tags.sh
runamd64: kernel sfsimg
	qemu-system-x86_64  -serial mon:stdio -nographic -m 512m -hda obj/kernel.img -drive file=obj/sfs.img,media=disk,cache=writeback,index=2 -s 
runamd64smp: kernel sfsimg
	qemu-system-x86_64  -serial mon:stdio -nographic -m 1g -hda obj/kernel.img -smp 16 -numa node -numa node  -drive file=obj/sfs.img,media=disk,cache=writeback,index=2 -s 
kvmamd64: kernel sfsimg
	qemu-kvm  -serial mon:stdio -nographic -m 512m -hda obj/kernel.img -drive file=obj/sfs.img,media=disk,cache=writeback,index=2 -s 
kvmamd64smp: kernel sfsimg
	qemu-kvm  -serial mon:stdio -nographic -m 1g -hda obj/kernel.img -smp 16 -numa node -numa node  -drive file=obj/sfs.img,media=disk,cache=writeback,index=2 -s 

FORCE:

PHONY += FORCE

.PHONY: $(PHONY)
