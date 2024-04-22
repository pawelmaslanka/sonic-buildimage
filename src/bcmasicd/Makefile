platform=x86-smp_generic_64-2_6
TARGET=unix-user

SDKLT=/sonic/src/OpenBCM/sdk-6.5.21/libs/sdklt
SDK=/sonic/src/OpenBCM/sdk-6.5.21

MA_DIR ?= /usr/share/modass
KVERSION ?= 5.10.0-23-2-amd64
KERNVERSION ?= 5.10.0-23-2

# load generic variable handling
-include $(MA_DIR)/include/generic.make
# load default rules, including kdist, kdist_image, ...
-include $(MA_DIR)/include/common-rules.make

all: sdk_libs sdklt_libs system_libs

include $(SDK)/make/Make.config

system_libs:
	LINUX_UAPI_SPLIT=1 DEBIAN_LINUX_HEADER=1 BUILD_KNET_CB=1 \
	BUILD_SDKLT=1 BUILD_AAPL=1 LTSW_CHIPS=1 SYSOK=1 \
	KERNDIR=/usr/src/linux-headers-$(KERNVERSION)-common \
	KERNEL_SRC=/usr/src/linux-headers-$(KERNVERSION)-amd64 \
	KDIR=/usr/src/linux-headers-$(KERNVERSION)-common \
	$(MAKE) -C $(SDKLT)/linux/bde SDK=$(SDKLT)

ifdef BUILD_KNET
	LINUX_UAPI_SPLIT=1 DEBIAN_LINUX_HEADER=1 BUILD_KNET_CB=1 \
	BUILD_SDKLT=1 BUILD_AAPL=1 LTSW_CHIPS=1 SYSOK=1 \
	KERNDIR=/usr/src/linux-headers-$(KERNVERSION)-common \
	KERNEL_SRC=/usr/src/linux-headers-$(KERNVERSION)-amd64 \
	KDIR=/usr/src/linux-headers-$(KERNVERSION)-common \
	$(MAKE) -C $(SDKLT)/linux/knet SDK=$(SDKLT)
endif

LINUX_UAPI_SPLIT=1 DEBIAN_LINUX_HEADER=1 BUILD_KNET_CB=1 \
BUILD_SDKLT=1 BUILD_AAPL=1 LTSW_CHIPS=1 SYSOK=1 \
KERNDIR=/usr/src/linux-headers-$(KERNVERSION)-common \
KERNEL_SRC=/usr/src/linux-headers-$(KERNVERSION)-amd64 \
KDIR=/usr/src/linux-headers-$(KERNVERSION)-common \
$(MAKE) -C $(SDK)/systems/bde/linux/user CFLAGS="$(CFLAGS) -I$(SDK)/system/bde/linux/include"

sdk_libs:
	LINUX_UAPI_SPLIT=1 DEBIAN_LINUX_HEADER=1 BUILD_KNET_CB=1 \
	BUILD_SDKLT=1 BUILD_AAPL=1 LTSW_CHIPS=1 SYSOK=1 \
	KERNDIR=/usr/src/linux-headers-$(KERNVERSION)-common \
	KERNEL_SRC=/usr/src/linux-headers-$(KERNVERSION)-amd64 \
	KDIR=/usr/src/linux-headers-$(KERNVERSION)-common \
	$(MAKE) -C $(SDK) TARGET=$(TARGET) platform=$(platform)

sdklt_libs:
	LINUX_UAPI_SPLIT=1 BUILD_AAPL=1 DEBIAN_LINUX_HEADER=1 BUILD_KNET_CB=1 \
	BUILD_SDKLT=1 LTSW_CHIPS=1 SYSOK=1 \
	KERNDIR=/usr/src/linux-headers-$(KERNVERSION)-common \
	KERNEL_SRC=/usr/src/linux-headers-$(KERNVERSION)-amd64 \
	KDIR=/usr/src/linux-headers-$(KERNVERSION)-common \
	$(MAKE) -C $(SDKLT) SDK=$(SDKLT)
