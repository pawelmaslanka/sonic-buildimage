# openbcm package

OPENBCM_VERSION = 6.5.21

export OPENBCM_VERSION

OPENBCM = openbcm_$(OPENBCM_VERSION)_$(CONFIGURED_ARCH).deb
$(OPENBCM)_DEPENDS += $(LINUX_HEADERS) $(LINUX_HEADERS_COMMON)
$(OPENBCM)_RDEPENDS += $(LINUX_KERNEL)
$(OPENBCM)_SRC_PATH = $(SRC_PATH)/openbcm

SONIC_DPKG_DEBS += $(OPENBCM)

$(OPENBCM)_BASE_IMAGE_FILES += bcmcmd:/usr/bin/bcmcmd

export OPENBCM