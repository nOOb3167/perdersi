#!/usr/bin/env bash

set -e

WEBPATH=/usr/local/perdersi/deploy/web

cd "$WEBPATH"
PYTHONPATH="$WEBPATH" python3 -m startup
