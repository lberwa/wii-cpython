#!/bin/sh
# Freeze WII_BUILD-only Python modules that regen-importlib doesn't handle.
PYTHON="$1"
FREEZE_PY="$2"
SRCDIR="$3"
OUTDIR="$4"

freeze() {
    mod="$1"; src="$2"
    out="$OUTDIR/${mod}.h"
    [ -f "$out" ] && return 0
    echo "Freezing $mod"
    "$PYTHON" "$FREEZE_PY" "$mod" "$SRCDIR/$src" "$out"
}

freeze pkgutil                Lib/pkgutil.py
freeze enum                   Lib/enum.py
freeze keyword                Lib/keyword.py
freeze operator               Lib/operator.py
freeze copyreg                Lib/copyreg.py
freeze reprlib                Lib/reprlib.py
freeze warnings               Lib/warnings.py
freeze _py_warnings           Lib/_py_warnings.py
freeze threading              Lib/threading.py
freeze weakref                Lib/weakref.py
freeze _weakrefset            Lib/_weakrefset.py
freeze copy                   Lib/copy.py
freeze _compat_pickle         Lib/_compat_pickle.py
freeze struct                 Lib/struct.py
freeze zoneinfo               Lib/zoneinfo/__init__.py
freeze 'zoneinfo._tzpath'     Lib/zoneinfo/_tzpath.py
freeze 'zoneinfo._common'     Lib/zoneinfo/_common.py

freeze __future__             Lib/__future__.py

# asyncio and networking dependencies
freeze signal                 Lib/signal.py
freeze base64                 Lib/base64.py
freeze socket                 Lib/socket.py
freeze ssl                    Lib/ssl.py
freeze selectors              Lib/selectors.py
freeze locale                 Lib/locale.py
freeze queue                  Lib/queue.py
freeze contextlib             Lib/contextlib.py
freeze string                 Lib/string/__init__.py
freeze 'string.templatelib'   Lib/string/templatelib.py

# --- hashlib, json ---
freeze hashlib                Lib/hashlib.py
freeze json                   Lib/json/__init__.py
freeze 'json.decoder'         Lib/json/decoder.py
freeze 'json.encoder'         Lib/json/encoder.py
freeze 'json.scanner'         Lib/json/scanner.py

# --- ipaddress, bisect ---
freeze ipaddress              Lib/ipaddress.py
freeze bisect                 Lib/bisect.py

# --- file/dir utilities ---
freeze tempfile               Lib/tempfile.py
freeze shutil                 Lib/shutil.py
freeze fnmatch                Lib/fnmatch.py
freeze glob                   Lib/glob.py

# --- pathlib package ---
freeze pathlib                Lib/pathlib/__init__.py
freeze 'pathlib._local'       Lib/pathlib/_local.py
freeze 'pathlib._os'          Lib/pathlib/_os.py
freeze 'pathlib.types'        Lib/pathlib/types.py

# --- data types / utilities ---
freeze random                 Lib/random.py
freeze datetime               Lib/datetime.py

# pip is no longer frozen into libpython — it is shipped as individual .py files
# on the SD card via the wiiload payload (wiitest/wiiload/SD/python/pip/).
# Use: make pip-sd   (in the top-level Makefile) to extract pip from the wheel.
