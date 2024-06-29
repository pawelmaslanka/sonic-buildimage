#!/usr/bin/env bash

BCM_ASIC_SERVICE_ARGS=" --config /usr/bin/config_test.json --schema /usr/bin/schema_test.json --address localhost --port 8001"

logger "BCM_ASIC_SERVICE_ARGS: $BCM_ASIC_SERVICE_ARGS"
exec /usr/bin/BcmAsicService ${BCM_ASIC_SERVICE_ARGS}
