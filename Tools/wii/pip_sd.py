#!/usr/bin/env python3
"""
Populate wii-folder/python/ with pip and stdlib files for the Wii SD card.

Layout produced:
  wii-folder/
    python/
      pip/          <- pip 26.0.1 from the bundled wheel (one .py per module)
      <stdlib>      <- Lib/ Python files, excluding GUI / test / cache dirs

Copy wii-folder/python/ to sd:/python/ on your SD card.
sys.path already contains sd:/python, so all files are importable directly.

Usage:
  python3 Tools/wii/pip_sd.py
"""

import os
import shutil
import zipfile

# Paths relative to the CPython root (two levels up from this script)
_HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(_HERE))

PIP_WHEEL   = os.path.join(ROOT, "Lib", "ensurepip", "_bundled",
                           "pip-26.0.1-py3-none-any.whl")
WII_FOLDER  = os.path.join(ROOT, "wii-folder")
WII_PY      = os.path.join(WII_FOLDER, "python")
LIB_DIR     = os.path.join(ROOT, "Lib")

# Directories inside Lib/ that are not useful on the Wii
EXCLUDE_DIRS = {
    "test", "tests",       # CPython test suite
    "tkinter",             # GUI toolkit
    "idlelib",             # IDLE editor
    "turtledemo",          # turtle demo
    "__pycache__",         # compiled bytecode
    "_bundled",            # ensurepip bundled wheels (large)
}


def extract_pip():
    print("Extracting pip from bundled wheel...")
    if not os.path.isfile(PIP_WHEEL):
        print(f"  ERROR: wheel not found: {PIP_WHEEL}")
        return

    out_dir = WII_PY
    extracted = 0
    with zipfile.ZipFile(PIP_WHEEL) as z:
        for name in sorted(z.namelist()):
            if (name.startswith("pip/")
                    and name.endswith(".py")
                    and "__pycache__" not in name):
                target = os.path.join(out_dir, name)
                os.makedirs(os.path.dirname(target), exist_ok=True)
                with open(target, "wb") as f:
                    f.write(z.read(name))
                extracted += 1

    print(f"  pip: {extracted} files -> {os.path.join(WII_PY, 'pip')}")


def copy_stdlib():
    print("Copying Lib/ stdlib files...")
    copied = skipped = 0

    for dirpath, dirnames, filenames in os.walk(LIB_DIR):
        # Prune unwanted subdirectories in-place so os.walk skips them
        dirnames[:] = [
            d for d in dirnames
            if d not in EXCLUDE_DIRS and not d.startswith(".")
        ]

        for fname in filenames:
            if not fname.endswith(".py"):
                continue
            src = os.path.join(dirpath, fname)
            rel = os.path.relpath(src, LIB_DIR)
            dst = os.path.join(WII_PY, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            # Skip if destination is already up-to-date
            if (os.path.exists(dst)
                    and os.path.getmtime(src) <= os.path.getmtime(dst)):
                skipped += 1
                continue
            shutil.copy2(src, dst)
            copied += 1

    print(f"  stdlib: {copied} files copied, {skipped} already up-to-date")


# Wii-specific patches applied after extraction.
# Each entry: (relative path inside wii-folder/python/, old_text, new_text)
WII_PATCHES = [
    # platformdirs: os.getuid does not exist on Wii/libogc
    (
        os.path.join("pip", "_vendor", "platformdirs", "unix.py"),
        "else:\n    from os import getuid\n",
        (
            "else:\n"
            "    try:\n"
            "        from os import getuid\n"
            "    except ImportError:\n"
            "        def getuid() -> int:  # type: ignore[misc]\n"
            "            return 0  # no user IDs on embedded targets (Wii/libogc)\n"
        ),
    ),
]


def apply_wii_patches():
    print("Applying Wii-specific patches...")
    patched = 0
    for rel, old, new in WII_PATCHES:
        path = os.path.join(WII_PY, rel)
        if not os.path.isfile(path):
            print(f"  SKIP (not found): {rel}")
            continue
        with open(path, "r", encoding="utf-8") as f:
            src = f.read()
        if old not in src:
            print(f"  SKIP (already patched): {rel}")
            continue
        with open(path, "w", encoding="utf-8") as f:
            f.write(src.replace(old, new, 1))
        print(f"  patched: {rel}")
        patched += 1
    print(f"  {patched} file(s) patched")


def main():
    os.makedirs(WII_PY, exist_ok=True)
    extract_pip()
    copy_stdlib()
    apply_wii_patches()
    print()
    print(f"Done!  wii-folder/ is ready.")
    print(f"  Copy wii-folder/python/ -> sd:/python/ on your SD card.")
    print(f"  'import pip' and full stdlib will work out of the box.")


if __name__ == "__main__":
    main()
