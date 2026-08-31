Python for the Wii
=====================================


Copyright © 2001 Python Software Foundation.  All rights reserved.

See the end of this file for further copyright and license information.



General Information
-------------------

__This is a port of Python for the Nintendo Wii.__


__Version:__

  Python 3.15.0a7


__Included components:__

- wiitoolsmodule: Provides Python bindings for many libogc functions.
- libcurl: Available through wiitoolsmodule for network and HTTP/HTTPS operations.
- LodePNG: Available through wiitoolsmodule for PNG image loading and saving.
- compatible with *.so files which were generated as shown in the [example](#build-so-files)

__Python:__

- Website: https://www.python.org
- Source code: https://github.com/lberwa/wii-cpython
- Issue tracker: https://github.com/lberwa/wii-cpython/issues
- Documentation: https://docs.python.org
- Developer's Guide: https://devguide.python.org/



Build Instructions
-----------------------

__First, install the dependencies__
```bash
sudo apt install libssl-dev cmake
dkp-pacman install wii-dev ppc-zlib ppc-bzip2
```

__PC-build-tool:__
```bash
dkp-pacman -S libogc2
```
if libogc2 is not avaiable: see [here](#install-libogc2)


__Then, build the PC-build-tool:__
```bash
make build-host -j$(nproc)
```
This builds a Python toolchain on the PC to create frozen Python files for the Wii.



__Build:__
```bash
make py -j$(nproc)
```

This will make a cpython.a.



__or if you want to test it:__
```bash
make -j$(nproc)
```

This will make a cpython.a and a boot.dol in wiitest/.



__Install:__
```bash
sudo make install DEVKITPRO="/PATH/devkitpro" DEVKITPPC="/PATH/devkitpro/devkitPPC"

```

This will copy your library .a files to DEVKITPRO/portlibs/ppc/lib and the *.h headers to the include directory.



__Clean:__
```bash
make clean
```

This will clean the build.



```bash
rm -rf build-host/
```

This will remove the pc-host .



## Tests/Examples:

### Build/main.c

In __[wiitest/](./wiitest)__ you will find examples of how to use it in your __main.c__ and how to write your __Makefile__.



__Example:__

```c
#include <Python.h>
// #include ...
// ...

void init_wii_system() {
	VIDEO_Init();
	//...
	//...
}

int main() {
    //################
    //INIT SYSTEHM
    //################
	init_wii_system();
	
    //################
    //INIT PYTHON
    //################
	size_t count = 2;
	
	// if you don't want import paths
	//Py_Initialize_Custom(NULL, NULL); 
	
    // you can now import from "sd:" and sd:/python
    PyStatus status = Py_Init_Custom((const char*[]){ "sd:/", "sd:/python"}, &count);
    
    // return if failed to initialize Python
    if (status._type != _PyStatus_TYPE_OK) { 
        return status.exitcode;  
    }
    
    
    //################
    //RUN PYTHON
    //################
    
    // run your file
    int rc = PyRun_SimpleString("import runpy\n" // runpy runs your file
                                "runpy.run('sd:/test.py')"); // path=sd:/test.py
    
    //################
    //FINALIZE AND RETURN
    //################
    
    Py_Finalize();
    
    if (rc == 0) {/* script ran successfully */}
    else {/* script returned an error */}
    
    return rc;
}
```

-----------



### Python

#### WiiToolsModule

##### Initializing

First, import wiitools:

```python
import wiitools
```

Then initialize the Wii system:

```python
wiitools.init() # init WPAD, PAD, Network, mount sd/usb and Video
```

If you want to see Python's print() output, initialize it:

```python
wiitools.terminal_init()
```

Otherwise, if you want to see what you render (e.g. wiitools.render_text(...)), initialize this:

```python
wiitools.rendering_init()
```

--------

If you want to use CPython in a game that is already initialized in C, you can put this C code in your C game:

```c
set_main_global("mode_pointer",
					PyLong_FromUnsignedLong((unsigned long)gfx_wii_screenmode()));
set_main_global("framebuffer_pointer",
					PyLong_FromUnsignedLong((unsigned long)gfx_wii_backbuffer())); 
// put pointer into python
```

and run the script with runpy and these arguments:

```c
PyRun_SimpleString(
        "runpy.run_path(_hbc_path,\n"
		"init_globals={'mode_ptr': mode_pointer, 'fb_ptr': framebuffer_pointer},\n"
		"run_name='__main__')\n"
                   );
```

init in python:

```python
wiitools.rendering_adopt(mode_ptr, fb_ptr)
```

----------



##### Game loop

The Python functions are mostly named like in C:

```python
import wiitools as wt

wt.init()

game = True

while game: # Game loop
    if wt.WPAD_ButtonsDown(wt.WPAD_BUTTON_HOME, 0): # key, channel
        game = False
       
    if wt.WPAD_ButtonsHeld(wt.WPAD_BUTTON_A, 0):
        wt.render_text(10, 10, "Player 1 pressed Button A!", 2,   True,
                     # x   y              text              size shadow
                      (255, 255, 255, 255),         0)
                     #  r    g    b    a   angle: 0°↑

    wt.update() # update WPAD, PAD, rendering and VIDEO_WaitVSync: Next frame
```



You can copy [wiitools.pyi](./wiitools.pyi) to your workspace so your IDE knows the wiitools functions.

In wiitools.pyi you can also find all the wiitools functions you can use.

------------



#### Other modules

You can copy the contents of the Lib folder to `sd/usb:/python/*`

and if your `main.c` has the import path `sd:/python`, you can import these modules.

--------------



### Build *.so files

build:

```bash
powerpc-eabi-gcc \
    -fPIC -fno-plt -shared -nostdlib \
    -o out.so     test.c
#  the .so file | the .c file
```



usage:

```python
import out
```

------



## Install libogc2

add this line to your *pacman.conf*
```properties
[libogc2-devkitpro]
Server = https://packages.libogc2.org/devkitpro/linux/$arch
```

```bash
sudo nano /opt/devkitpro/pacman/etc/pacman.conf
```


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
