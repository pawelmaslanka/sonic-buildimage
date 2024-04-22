#!/usr/bin/env bash

FLEXCONFIG_ARGS=" --config /usr/bin/config_test.json --schema /usr/bin/schema_test.json --address localhost --port 8001"

logger "NET_MIDDLEWARE_ARGS: $FLEXCONFIG_ARGS"
exec /usr/bin/NetMiddleware ${FLEXCONFIG_ARGS}
