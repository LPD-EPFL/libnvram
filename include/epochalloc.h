#ifndef _EPOCHALLOC_H_
#define _EPOCHALLOC_H_

#include <stdlib.h>
#include <jemalloc/jemalloc.h>

/*
 *  Volatile memory allocations
 */

// Allocate block of memory that is aligned to a given boundary.
// Boundary has to be equal to some power of two.
template<size_t BOUNDARY>
void *EpochMallocAligned(size_t size) {
	size_t actualSize = size + BOUNDARY + sizeof(void *);
    UINT_PTR memBlock = (UINT_PTR)malloc(actualSize);
	UINT_PTR ret = ((memBlock + sizeof(void *) + BOUNDARY)
		& ~(BOUNDARY - 1));
	void **backPtr = (void **)(ret - sizeof(void *));
	*backPtr = (void *)memBlock;
	return (void *)ret;		
}


inline void EpochFreeAligned(void *ptr) {
	void **backPtr = (void **)((UINT_PTR)ptr - sizeof(void *));
	free(*backPtr);
}


inline void *ZeroedEpochMallocNoAlign(size_t size) {
	return calloc(1,size);
}

inline void *EpochMallocNoAlign(size_t size) {
	return malloc(size);
}


inline void EpochFreeNoAlign(void *ptr) {
	free(ptr);
}


// Allocate a block of memory aligned to cache line boundary.
inline void *EpochCacheAlignedAlloc(size_t size) {
	return EpochMallocAligned<EPOCH_CACHE_LINE_SIZE>(size);
}

// Allocate a block of memory aligned to cache line boundary and
// having size that is a multiple of cache line size.
inline void *EpochCacheAlignedCacheSizeAlloc(size_t size) {
	size_t newSize = (size + EPOCH_CACHE_LINE_SIZE - 1)
		& ~(EPOCH_CACHE_LINE_SIZE - 1);
	return EpochMallocAligned<EPOCH_CACHE_LINE_SIZE>(newSize);
}

// Free a cache aligned block of memory.
inline void EpochCacheAlignedFree(void *ptr) {
	return EpochFreeAligned(ptr);
}

/*
 *  Data structure node specific functions
 */

// nodes larger than one cache line are naturally alligned to cache line boundaries in Rockall
inline void *AllocNode(size_t size) {
	return nv_mallocx(size,0);
}

inline void* GetNextNodeAddress(size_t size) {
	return nv_nextx(size,0);
}

inline void FreeNode(void *ptr) {
	nv_dallocx(ptr,0);
}

inline int DSNodeMemoryIsFree(void *ptr, uint64_t all_size) {
  uint64_t sa = nv_sallocx(ptr, 0);
  if ((sa != all_size) && (sa!=0)) {
	  fprintf(stderr,"Non-standard allocation size: %u\n", sa);
  }
  if ((sa!=0)){
    if (sa <= all_size) {
      return 0;
      //if reported allocation size larger than all_size, it means we probably hit an internal large
      //chunk allocation for which the initial address was not assigned to a
      //data structure node
    }
  }
  return 1;
}

inline int NodeMemoryIsFree(void *ptr) {
  uint64_t sa = nv_sallocx(ptr, 0);
  if ((sa!=0)){
    return 0;
  }
  return 1;
}

inline void FlushThread() {
    nv_mallctl("thread.tcache.flush", NULL, NULL, NULL, 0);
    bool en = false;
    nv_mallctl("thread.tcache.enable", NULL, NULL, &en, sizeof(en));
}

inline void MarkNodeMemoryAsFree(void * ptr) {
	nv_dallocx(ptr,0);
}

#endif
