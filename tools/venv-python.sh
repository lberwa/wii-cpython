#!/bin/sh
VENV_ROOT="/opt/devkitpro/extras/cpython/.python-venv"
VENV_PY="$VENV_ROOT/bin/python"
VENV_SITE="$VENV_ROOT/lib/python3.11/site-packages"
SYS_SITE1="/usr/local/lib/python3.11/dist-packages"
SYS_SITE2="/usr/lib/python3/dist-packages"
SYS_SITE3="/usr/lib/python3.11/dist-packages"

PY_PATHS="$VENV_SITE:$SYS_SITE1:$SYS_SITE2:$SYS_SITE3"
if [ -n "$PYTHONPATH" ]; then
  export PYTHONPATH="$PY_PATHS:$PYTHONPATH"
else
  export PYTHONPATH="$PY_PATHS"
fi
exec "$VENV_PY" "$@"
