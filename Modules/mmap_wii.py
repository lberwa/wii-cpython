# mmap.py — Wii shim.  libogc has no mmap() syscall, so the real C _mmap
# module is unavailable.  This provides the minimal subset used by pip's
# vendored cachecontrol (pip/_vendor/cachecontrol/filewrapper.py):
#
#     result = memoryview(mmap.mmap(fp.fileno(), 0, access=mmap.ACCESS_READ))
#
# We simply read the file's bytes into a bytearray, which supports the buffer
# protocol, so memoryview(...) works and cachecontrol's CallbackFileWrapper is
# happy.  This is NOT a real memory map (no lazy paging), but on the Wii files
# are small and short-lived, so reading them fully is fine.
import os as _os

# Access flags (values match CPython's mmap module).
ACCESS_DEFAULT = 0
ACCESS_READ = 1
ACCESS_WRITE = 2
ACCESS_COPY = 3

# prot / flags constants some code imports (unused here, provided for safety).
PROT_READ = 1
PROT_WRITE = 2
PROT_EXEC = 4
MAP_SHARED = 1
MAP_PRIVATE = 2

PAGESIZE = 4096
ALLOCATIONGRANULARITY = 4096


def mmap(fileno, length, *args, **kwargs):
    """Minimal mmap replacement: return a bytearray of the file's contents.

    length == 0 means 'the whole file' (as in the real mmap).  The file
    position is preserved.  access/prot/flags arguments are accepted and
    ignored (we only support read-style access via the buffer protocol).
    """
    pos = _os.lseek(fileno, 0, 1)          # SEEK_CUR: remember position
    try:
        _os.lseek(fileno, 0, 0)            # SEEK_SET: rewind
        if length and length > 0:
            data = bytearray(_os.read(fileno, length))
        else:
            data = bytearray()
            while True:
                chunk = _os.read(fileno, 65536)
                if not chunk:
                    break
                data += chunk
    finally:
        _os.lseek(fileno, pos, 0)          # restore original position
    return data
