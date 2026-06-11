/*
	pyfat.h - libfat public API for the CPython/Wii fat fork.
	Mirrors <fat.h> without pulling in the system header.
*/

#ifndef _PYFAT_H
#define _PYFAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#if defined(__gamecube__) || defined(__wii__)
#  include <ogc/disc_io.h>
#else
#  include <disc_io.h>
#endif

extern bool fatInit(uint32_t cacheSize, bool setAsDefaultDevice);
extern bool fatInitDefault(void);
extern bool fatMountSimple(const char *name, const DISC_INTERFACE *interface);
extern bool fatMount(const char *name, const DISC_INTERFACE *interface, sec_t startSector, uint32_t cacheSize, uint32_t sectorsPerPage);
extern void fatUnmount(const char *name);
extern void fatGetVolumeLabel(const char *name, char *label);

#define ATTR_ARCHIVE   0x20
#define ATTR_DIRECTORY 0x10
#define ATTR_VOLUME    0x08
#define ATTR_SYSTEM    0x04
#define ATTR_HIDDEN    0x02
#define ATTR_READONLY  0x01

int FAT_getAttr(const char *file);
int FAT_setAttr(const char *file, uint8_t attr);

#ifdef __cplusplus
}
#endif

#endif /* _PYFAT_H */
