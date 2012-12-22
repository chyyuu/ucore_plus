ARCH_INLUCDES:=debug driver include libs mm process sync trap syscall module

### DEFINE THE BOARD MACROS ###
ifdef UCONFIG_ARM_BOARD_GOLDFISH
ARCH_INLUCDES += mach-goldfish
PLATFORM_DEF := -DPLATFORM_GOLDFISH
PLATFORM_DEF += -DCONFIG_MACH_GOLDFISH_ARMV7 -DCONFIG_ARCH_GOLDFISH

ifdef UCONFIG_HAVE_LINUX_DDE_BASE
PLATFORM_DEF += -include $(KTREE)/module/include/mach-goldfish/autoconf.h
endif

endif

ifdef UCONFIG_ARM_BOARD_PANDABOARD
ARCH_INLUCDES += mach-pandaboard
PLATFORM_DEF := -DPLATFORM_PANDABOARD -D__MACH_ARM_CORTEX_A9
PLATFORM_DEF += -DCONFIG_ARCH_OMAP4 -DCONFIG_OMAP_MUX -DCONFIG_GENERIC_GPIO
PLATFORM_DEF += -DCONFIG_GPIOLIB -DCONFIG_ARCH_NR_GPIO=512

# use goldfish linux header
ifdef UCONFIG_HAVE_LINUX_DDE_BASE
PLATFORM_DEF += -include $(KTREE)/module/include/mach-goldfish/autoconf.h
endif

endif


ifdef UCONFIG_ARM_CPU_V7
MACH_MACRO := -D__MACH_ARM_ARMV7 	-D__LINUX_ARM_ARCH__=7
PLATFORM_DEF += -march=armv7-a
endif

ifdef UCONFIG_ARM_CPU_V5
MACH_MACRO := -D__MACH_ARM_ARMV5	-D__LINUX_ARM_ARCH__=5
PLATFORM_DEF += -march=armv5
endif

MACH_MACRO += -DDEBUG -D__ARM_EABI__ -DARCH_ARM

ARCH_CFLAGS := $(MACH_MACRO) $(PLATFORM_DEF)
