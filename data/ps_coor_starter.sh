#!/usr/bin/env bash

set -e

VENPATH=/usr/local/perdersi/venv/bin
WEBPATH=/usr/local/perdersi/deploy/web

cd "$WEBPATH"
PYTHONPATH="$WEBPATH" $VENPATH/python3 -m startup server_coor
