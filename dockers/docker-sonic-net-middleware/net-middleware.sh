#!/usr/bin/env bash

NET_MIDDLEWARE_ARGS=" --config /usr/bin/config_test.json --schema /usr/bin/schema_test.json --address localhost --port 8001"

logger "NET_MIDDLEWARE_ARGS: $NET_MIDDLEWARE_ARGS"
exec /usr/bin/NetMiddleware ${NET_MIDDLEWARE_ARGS}
