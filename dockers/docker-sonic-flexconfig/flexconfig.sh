#!/usr/bin/env bash

FLEXCONFIG_ARGS=" --config /usr/local/bin/config_test.json --schema /usr/local/bin/schema_test.json --address localhost --port 8001"

logger "FLEXCONFIG_ARGS: $FLEXCONFIG_ARGS"
exec /usr/local/bin/FlexConfig ${FLEXCONFIG_ARGS}
