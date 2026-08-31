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
freeze xml                    Lib/xml/__init__.py
freeze 'xml.etree'            Lib/xml/etree/__init__.py
freeze 'xml.etree.ElementPath' Lib/xml/etree/ElementPath.py
freeze sysconfig              Lib/sysconfig/__init__.py
freeze struct                 Lib/struct.py
freeze zoneinfo               Lib/zoneinfo/__init__.py
freeze 'zoneinfo._tzpath'     Lib/zoneinfo/_tzpath.py
freeze 'zoneinfo._common'     Lib/zoneinfo/_common.py
