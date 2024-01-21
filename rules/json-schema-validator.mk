# json-schema-validator package
# This version has to be reflected to debian/changelog
JSON_SCHEMA_VALIDATOR_VERSION = 0.1

export JSON_SCHEMA_VALIDATOR_VERSION

JSON_SCHEMA_VALIDATOR = json-schema-validator_$(JSON_SCHEMA_VALIDATOR_VERSION)_$(CONFIGURED_ARCH).deb
$(JSON_SCHEMA_VALIDATOR)_DEPENDS += $(LIBNLOHMANN_JSON_SCHEMA_VALIDATOR) $(LIBNLOHMANN_JSON_SCHEMA_VALIDATOR_DEV)
$(JSON_SCHEMA_VALIDATOR)_RDEPENDS += $(LIBNLOHMANN_JSON_SCHEMA_VALIDATOR)
$(JSON_SCHEMA_VALIDATOR)_UNINSTALLS = $(LIBNLOHMANN_JSON_SCHEMA_VALIDATOR)
$(JSON_SCHEMA_VALIDATOR)_SRC_PATH = $(SRC_PATH)/json-schema-validator
# SONIC_MAKE_DEBS += $(JSON_SCHEMA_VALIDATOR)

SONIC_DPKG_DEBS += $(JSON_SCHEMA_VALIDATOR)

export JSON_SCHEMA_VALIDATOR
