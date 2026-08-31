#!/bin/sh
# Portable wrapper: use a local venv if present, otherwise fall back to system python3.
# cmake passes this script as PYTHON_EXECUTABLE so mbedTLS code-gen runs on any machine.

for candidate in \
    "/home/lew/.python-venv/bin/python" \
    "/home/server/.python-venv/bin/python" \
    "$(dirname "$0")/../.python-venv/bin/python" \
    "$(which python3 2>/dev/null)" \
    "$(which python 2>/dev/null)"; do
    [ -x "$candidate" ] && exec "$candidate" "$@"
done

echo "venv-python.sh: kein Python gefunden" >&2
exit 1
