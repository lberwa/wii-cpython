/*
 cache.c
 The cache is not visible to the user. It should be flushed
 when any file is closed or changes are made to the filesystem.

 This cache implements a least-used-page replacement policy. This will
 distribute sectors evenly over the pages, so if less than the maximum
 pages are used at once, they should all eventually remain in the cache.
 This also has the benefit of throwing out old sectors, so as not to keep
 too many stale pages around.

 Copyright (c) 2006 Michael "Chishm" Chisholm

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __wii__
#define __wii__
#endif
#ifndef __WII__
#define __WII__
#endif

#include <string.h>
#include <limits.h>

#include "common.h"
#include "cache.h"
#include "disc.h"

#include "mem_allocate.h"
#include "bit_ops.h"
#include "file_allocation_table.h"

#define CACHE_FREE UINT_MAX

CACHE* _FAT_cache_constructor (unsigned int numberOfPages, unsigned int sectorsPerPage, const DISC_INTERFACE* discInterface, sec_t endOfPartition, unsigned int bytesPerSector) {
	CACHE* cache;
	unsigned int i;
	CACHE_ENTRY* cacheEntries;

	if (numberOfPages < 2) {
		numberOfPages = 2;
	}

	if (sectorsPerPage < 8) {
		sectorsPerPage = 8;
	}

	cache = (CACHE*) _FAT_mem_allocate (sizeof(CACHE));
	if (cache == NULL) {
		return NULL;
	}

	cache->disc = discInterface;
	cache->endOfPartition = endOfPartition;
	cache->numberOfPages = numberOfPages;
	cache->sectorsPerPage = sectorsPerPage;
	cache->bytesPerSector = bytesPerSector;


	cacheEntries = (CACHE_ENTRY*) _FAT_mem_allocate ( sizeof(CACHE_ENTRY) * numberOfPages);
	if (cacheEntries == NULL) {
		_FAT_mem_free (cache);
		return NULL;
	}

	for (i = 0; i < numberOfPages; i++) {
		cacheEntries[i].sector = CACHE_FREE;
		cacheEntries[i].count = 0;
		cacheEntries[i].last_access = 0;
		cacheEntries[i].dirty = false;
		cacheEntries[i].cache = (uint8_t*) _FAT_mem_align ( sectorsPerPage * bytesPerSector );
		if (cacheEntries[i].cache == NULL) {
			unsigned int j;
			for (j = 0; j < i; j++) {
				_FAT_mem_free (cacheEntries[j].cache);
			}
			_FAT_mem_free (cacheEntries);
			_FAT_mem_free (cache);
			return NULL;
		}
	}

	cache->cacheEntries = cacheEntries;

	return cache;
}

void _FAT_cache_destructor (CACHE* cache) {
	unsigned int i;
	// Clear out cache before destroying it
	_FAT_cache_flush(cache);

	// Free memory in reverse allocation order
	for (i = 0; i < cache->numberOfPages; i++) {
		_FAT_mem_free (cache->cacheEntries[i].cache);
	}
	_FAT_mem_free (cache->cacheEntries);
	_FAT_mem_free (cache);
}


static u32 accessCounter = 0;

static u32 accessTime(){
	accessCounter++;
	return accessCounter;
}


static CACHE_ENTRY* _FAT_cache_getPage(CACHE *cache,sec_t sector)
{
	unsigned int i;
	CACHE_ENTRY* cacheEntries = cache->cacheEntries;
	unsigned int numberOfPages = cache->numberOfPages;
	unsigned int sectorsPerPage = cache->sectorsPerPage;

	bool foundFree = false;
	unsigned int oldUsed = 0;
	unsigned int oldAccess = UINT_MAX;

	for(i=0;i<numberOfPages;i++) {
		if(sector>=cacheEntries[i].sector && sector<(cacheEntries[i].sector + cacheEntries[i].count)) {
			cacheEntries[i].last_access = accessTime();
			return &(cacheEntries[i]);
		}

		if(foundFree==false && (cacheEntries[i].sector==CACHE_FREE || cacheEntries[i].last_access<oldAccess)) {
			if(cacheEntries[i].sector==CACHE_FREE) foundFree = true;
			oldUsed = i;
			oldAccess = cacheEntries[i].last_access;
		}
	}

	if(foundFree==false && cacheEntries[oldUsed].dirty==true) {
		if(!_FAT_disc_writeSectors(cache->disc,cacheEntries[oldUsed].sector,cacheEntries[oldUsed].count,cacheEntries[oldUsed].cache)) return NULL;
		cacheEntries[oldUsed].dirty = false;
	}

	if (cacheEntries[oldUsed].cache == NULL) {
		return NULL;
	}

	sector = (sector/sectorsPerPage)*sectorsPerPage; // align base sector to page size
	sec_t next_page = sector + sectorsPerPage;
	if(next_page > cache->endOfPartition)	next_page = cache->endOfPartition;

	if(!_FAT_disc_readSectors(cache->disc,sector,next_page-sector,cacheEntries[oldUsed].cache)) return NULL;

	cacheEntries[oldUsed].sector = sector;
	cacheEntries[oldUsed].count = next_page-sector;
	cacheEntries[oldUsed].last_access = accessTime();

	return &(cacheEntries[oldUsed]);
}

/*bool _FAT_cache_readSectors(CACHE *cache,sec_t sector,sec_t numSectors,void *buffer)
{
	sec_t sec;
	sec_t secs_to_read;
	CACHE_ENTRY *entry;
	uint8_t *dest = (uint8_t *)buffer;

	while(numSectors>0) {
		entry = _FAT_cache_getPage(cache,sector);
		if(entry==NULL) return false;

		sec = sector - entry->sector;
		if (sec >= entry->count) return false;
		secs_to_read = ememcpyntry->count - sec;
		if(secs_to_read>numSectors) secs_to_read = numSectors;

		memcpy(dest,entry->cache + (sec*cache->bytesPerSector),(secs_to_read*cache->bytesPerSector));

		dest += (secs_to_read*cache->bytesPerSector);
		sector += secs_to_read;
		numSectors -= secs_to_read;
	}

	return true;
}*/

bool _FAT_cache_readSectors(CACHE *cache,
                            sec_t sector,
                            sec_t numSectors,
                            void *buffer)
{
    // HARD SAFETY: Check parameters immediately
    if (numSectors <= 0 || numSectors > 1000000) {
        return false;
    }
    
    if (!cache || !buffer) {
        return false;
    }

    // HARD SAFETY: Validate buffer pointer before use
    // Check it's not in impossible memory ranges
    if ((uintptr_t)buffer > 0x93400000) {
        return false;
    }
    
    // Check alignment if it matters for this platform
    if (((uintptr_t)buffer & 0x3) != 0) {
        return false;
    }

    if (cache->bytesPerSector == 0) {
        return false;
    }

    uint8_t *dest = (uint8_t *)buffer;
    
    // HARD SAFETY: Check destination pointer is sane
    // On Wii, heap/stack typically in lower memory ranges
    if ((uintptr_t)dest > 0x90FFFFFF) {
        return false;
    }

    while (numSectors > 0) {

        // HARD SAFETY: sector muss gültig bleiben
        if (sector == (sec_t)-1) {
            return false;
        }

        CACHE_ENTRY *entry = _FAT_cache_getPage(cache, sector);

        if (!entry || !entry->cache) {
            return false;
        }

        // zusätzliche Schutzschicht gegen kaputte entries
        if (entry->count == 0) {
            return false;
        }

        if (sector < entry->sector) {
            return false; // Underflow verhindert
        }

        sec_t sec = sector - entry->sector;

        if (sec >= entry->count) {
            return false;
        }

        sec_t secs_to_read = entry->count - sec;

        if (secs_to_read > numSectors) {
            secs_to_read = numSectors;
        }

        // Check multiplication overflow: sec * bytesPerSector
        if (cache->bytesPerSector > 0 && sec > (SIZE_MAX / cache->bytesPerSector)) {
            return false;
        }
        size_t offset = (size_t)sec * cache->bytesPerSector;

        // Check multiplication overflow: secs_to_read * bytesPerSector  
        if (cache->bytesPerSector > 0 && secs_to_read > (SIZE_MAX / cache->bytesPerSector)) {
            return false;
        }
        size_t size = (size_t)secs_to_read * cache->bytesPerSector;

        // Check addition overflow: offset + size
        if (offset > (SIZE_MAX - size)) {
            return false;
        }

        // Verify access is within allocated cache buffer
        // entry->cache is allocated as: sectorsPerPage * bytesPerSector
        // Check sectorsPerPage * bytesPerSector multiplication
        if (cache->bytesPerSector > 0 && cache->sectorsPerPage > (SIZE_MAX / cache->bytesPerSector)) {
            return false;
        }
        size_t max_cache_size = (size_t)cache->sectorsPerPage * cache->bytesPerSector;
        
        if ((offset + size) > max_cache_size) {
            return false;
        }

        memcpy(dest,
               entry->cache + offset,
               size);

        dest       += size;
        sector     += secs_to_read;
        numSectors -= secs_to_read;
    }

    return true;
}

/*
Reads some data from a cache page, determined by the sector number
*/
bool _FAT_cache_readPartialSector (CACHE* cache, void* buffer, sec_t sector, unsigned int offset, size_t size)
{
	sec_t sec;
	CACHE_ENTRY *entry;

	if (cache == NULL || buffer == NULL) return false;
	if (cache->bytesPerSector == 0 || cache->bytesPerSector > MAX_SECTOR_SIZE) return false;
	if (offset + size > cache->bytesPerSector) return false;

#if defined(__WII__) || defined(__wii__)
	/*
	 * Defensive: bypass cache on Wii for partial reads to avoid crashes if
	 * cache pages are corrupted. Read directly from disc into a temp buffer.
	 */
	if (cache->disc == NULL) return false;
	if (cache->bytesPerSector < MIN_SECTOR_SIZE) return false;
	{
		uint8_t *tmp = (uint8_t*)_FAT_mem_align(cache->bytesPerSector);
		if (tmp == NULL) return false;
		if (!_FAT_disc_readSectors(cache->disc, sector, 1, tmp)) {
			_FAT_mem_free(tmp);
			return false;
		}
		memcpy(buffer, tmp + offset, size);
		_FAT_mem_free(tmp);
		return true;
	}
#else

	entry = _FAT_cache_getPage(cache,sector);
	if(entry==NULL) return false;
	if (entry->cache == NULL) return false;

	sec = sector - entry->sector;
	if (sec >= entry->count) return false;
	{
		uint64_t bps = (uint64_t)cache->bytesPerSector;
		uint64_t start = (uint64_t)sec * bps + (uint64_t)offset;
		uint64_t end = start + (uint64_t)size;
		uint64_t limit = (uint64_t)entry->count * bps;
		if (end > limit) return false;
	}
	memcpy(buffer,entry->cache + ((sec*cache->bytesPerSector) + offset),size);

	return true;
#endif
}

bool _FAT_cache_readLittleEndianValue (CACHE* cache, uint32_t *value, sec_t sector, unsigned int offset, int num_bytes) {
  uint8_t buf[4];
  if (!_FAT_cache_readPartialSector(cache, buf, sector, offset, num_bytes)) return false;

  switch(num_bytes) {
  case 1: *value = buf[0]; break;
  case 2: *value = u8array_to_u16(buf,0); break;
  case 4: *value = u8array_to_u32(buf,0); break;
  default: return false;
  }
  return true;
}

/*
Writes some data to a cache page, making sure it is loaded into memory first.
*/
bool _FAT_cache_writePartialSector (CACHE* cache, const void* buffer, sec_t sector, unsigned int offset, size_t size)
{
	sec_t sec;
	CACHE_ENTRY *entry;

	if (offset + size > cache->bytesPerSector) return false;

	entry = _FAT_cache_getPage(cache,sector);
	if(entry==NULL) return false;

	sec = sector - entry->sector;
	
	// Bounds check: verify sector is within entry
	if(sec >= entry->count) return false;
	
	// Check multiplication overflow: sec * bytesPerSector
	if (cache->bytesPerSector > 0 && sec > (SIZE_MAX / cache->bytesPerSector)) {
		return false;
	}
	size_t offset_bytes = ((size_t)sec * cache->bytesPerSector) + offset;
	
	// Check overflow in offset addition
	if (((size_t)sec * cache->bytesPerSector) > (SIZE_MAX - offset)) {
		return false;
	}
	
	// Check sectorsPerPage * bytesPerSector multiplication
	if (cache->bytesPerSector > 0 && cache->sectorsPerPage > (SIZE_MAX / cache->bytesPerSector)) {
		return false;
	}
	size_t max_cache_size = (size_t)cache->sectorsPerPage * cache->bytesPerSector;
	
	// Check write doesn't exceed buffer
	if((offset_bytes + size) > max_cache_size) {
		return false;
	}
	
	memcpy(entry->cache + ((sec*cache->bytesPerSector) + offset),buffer,size);

	entry->dirty = true;
	return true;
}

bool _FAT_cache_writeLittleEndianValue (CACHE* cache, const uint32_t value, sec_t sector, unsigned int offset, int size) {
  uint8_t buf[4] = {0, 0, 0, 0};

  switch(size) {
  case 1: buf[0] = value; break;
  case 2: u16_to_u8array(buf, 0, value); break;
  case 4: u32_to_u8array(buf, 0, value); break;
  default: return false;
  }

  return _FAT_cache_writePartialSector(cache, buf, sector, offset, size);
}

/*
Writes some data to a cache page, zeroing out the page first
*/
bool _FAT_cache_eraseWritePartialSector (CACHE* cache, const void* buffer, sec_t sector, unsigned int offset, size_t size)
{
	sec_t sec;
	CACHE_ENTRY *entry;

	if (offset + size > cache->bytesPerSector) return false;

	entry = _FAT_cache_getPage(cache,sector);
	if(entry==NULL) return false;

	sec = sector - entry->sector;
	
	// Check multiplication overflow: sec * bytesPerSector (for memset)
	if (cache->bytesPerSector > 0 && sec > (SIZE_MAX / cache->bytesPerSector)) {
		return false;
	}
	
	// Check multiplication overflow: sec * bytesPerSector (for memcpy)
	if (cache->bytesPerSector > 0 && sec > (SIZE_MAX / cache->bytesPerSector)) {
		return false;
	}
	
	// Check secotrsPerPage bounds for memset/memcpy
	if (cache->bytesPerSector > 0 && cache->sectorsPerPage > (SIZE_MAX / cache->bytesPerSector)) {
		return false;
	}
	size_t sector_offset = (size_t)sec * cache->bytesPerSector;
	size_t max_cache_size = (size_t)cache->sectorsPerPage * cache->bytesPerSector;
	
	if (sector_offset >= max_cache_size) {
		return false;
	}
	
	memset(entry->cache + sector_offset, 0, cache->bytesPerSector);
	
	// Check memcpy offset doesn't overflow
	if (sector_offset > (SIZE_MAX - offset)) {
		return false;
	}
	if ((sector_offset + offset + size) > max_cache_size) {
		return false;
	}
	
	memcpy(entry->cache + (sector_offset + offset), buffer, size);

	entry->dirty = true;
	return true;
}


bool _FAT_cache_writeSectors (CACHE* cache, sec_t sector, sec_t numSectors, const void* buffer)
{
	sec_t sec;
	sec_t secs_to_write;
	CACHE_ENTRY* entry;
	const uint8_t *src = (const uint8_t *)buffer;

	while(numSectors>0)
	{
		entry = _FAT_cache_getPage(cache,sector);
		if(entry==NULL) return false;

		sec = sector - entry->sector;
		
		// Bounds check: sec must be within the entry
		if(sec >= entry->count) return false;
		
		secs_to_write = entry->count - sec;
		if(secs_to_write>numSectors) secs_to_write = numSectors;

		// Check multiplication overflow: sec * bytesPerSector
		if (cache->bytesPerSector > 0 && sec > (SIZE_MAX / cache->bytesPerSector)) {
			return false;
		}
		size_t offset_bytes = (size_t)sec * cache->bytesPerSector;
		
		// Check multiplication overflow: secs_to_write * bytesPerSector
		if (cache->bytesPerSector > 0 && secs_to_write > (SIZE_MAX / cache->bytesPerSector)) {
			return false;
		}
		size_t write_bytes = (size_t)secs_to_write * cache->bytesPerSector;
		
		// Check sectorsPerPage * bytesPerSector multiplication
		if (cache->bytesPerSector > 0 && cache->sectorsPerPage > (SIZE_MAX / cache->bytesPerSector)) {
			return false;
		}
		size_t max_cache_size = (size_t)cache->sectorsPerPage * cache->bytesPerSector;
		
		if((offset_bytes + write_bytes) > max_cache_size) {
			return false;
		}

		memcpy(entry->cache + offset_bytes, src, write_bytes);

		src += (secs_to_write*cache->bytesPerSector);
		sector += secs_to_write;
		numSectors -= secs_to_write;

		entry->dirty = true;
	}
	return true;
}

/*
Flushes all dirty pages to disc, clearing the dirty flag.
*/
bool _FAT_cache_flush (CACHE* cache) {
	unsigned int i;

	for (i = 0; i < cache->numberOfPages; i++) {
		if (cache->cacheEntries[i].dirty) {
			if (!_FAT_disc_writeSectors (cache->disc, cache->cacheEntries[i].sector, cache->cacheEntries[i].count, cache->cacheEntries[i].cache)) {
				return false;
			}
		}
		cache->cacheEntries[i].dirty = false;
	}

	return true;
}

void _FAT_cache_invalidate (CACHE* cache) {
	unsigned int i;
	_FAT_cache_flush(cache);
	for (i = 0; i < cache->numberOfPages; i++) {
		cache->cacheEntries[i].sector = CACHE_FREE;
		cache->cacheEntries[i].last_access = 0;
		cache->cacheEntries[i].count = 0;
		cache->cacheEntries[i].dirty = false;
	}
}
