Python for the Wii
=====================================


Copyright © 2001 Python Software Foundation.  All rights reserved.

See the end of this file for further copyright and license information.



General Information
-------------------

__This is a port of Python for the Nintendo Wii.__



__Included components:__

- wiitoolsmodule: Provides Python bindings for many libogc functions.
- libcurl: Available through wiitoolsmodule for network and HTTP/HTTPS operations.
- LodePNG: Available through wiitoolsmodule for PNG image loading and saving.



__Python:__

- Website: https://www.python.org
- Source code: https://github.com/lberwa/wii-cpython
- Issue tracker: https://github.com/lberwa/wii-cpython/issues
- Documentation: https://docs.python.org
- Developer's Guide: https://devguide.python.org/



Build Instructions
-----------------------

__PC-build-tool:__

```bash
make build-host -j$(nproc)
```

This builds a Python toolchain on the PC to create frozen Python files for the Wii.



__Build:__

```bash
make py -j$(nproc)
```

This will make a cpython.a.



__or if you want test it:__

```bash
make -j$(nproc)
```

This will make a cpython.a and a boot.dol in wiitest/.



__Install:__

```bash
sudo make install DEVKITPRO="/PATH/devkitpro" DEVKITPPC="/PATH/devkitpro/devkitPPC"

```

This will copy your libarys.a to DEVKITPRO/portlibs/ppc/lib and *.h to include



__Clean:__

```bash
make clean
```

This will cleaning



## Tests/Examples:

In __[./wiitest](./wiitest)__ you will find examples how to use it in your main.c and how you make your Makefile.



## Changes

- Modified Python to support compilation and execution on the Nintendo Wii.
- Added Wii-specific build configuration and platform support.



Copyright and License Information
---------------------------------


Copyright © 2001 Python Software Foundation.  All rights reserved.

Copyright © 2000 BeOpen.com.  All rights reserved.

Copyright © 1995-2001 Corporation for National Research Initiatives.  All
rights reserved.

Copyright © 1991-1995 Stichting Mathematisch Centrum.  All rights reserved.

See the  [LICENSE](./LICENSE) for information on the history of this software, terms & conditions for usage, and a
DISCLAIMER OF ALL WARRANTIES.

This Python distribution contains *no* GNU General Public License (GPL) code,
so it may be used in proprietary projects.  There are interfaces to some GNU
code but these are entirely optional.

All trademarks referenced herein are property of their respective holders.
