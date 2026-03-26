#!/bin/sh
VENV_PY="/home/lew/.python-venv/bin/python"
VENV_SITE="/home/lew/.python-venv/lib/python3.11/site-packages"
if [ -n "$PYTHONPATH" ]; then
  export PYTHONPATH="$VENV_SITE:$PYTHONPATH"
else
  export PYTHONPATH="$VENV_SITE"
fi
exec "$VENV_PY" "$@"
