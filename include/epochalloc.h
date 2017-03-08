#pragma once

/*TODO these two functions should use a fully-functional non-volatile memory allocator */
#define nvalloc malloc
#define nvalloc_zero(sz) calloc(1, sz)

// Allocate block of memory that is aligned to a given boundary.
// Boundary has to be equal to some power of two.
template<size_t BOUNDARY>
void *EpochMallocAligned(size_t size) {
	size_t actualSize = size + BOUNDARY + sizeof(void *);
	//UINT_PTR memBlock = (UINT_PTR)malloc(actualSize);
	UINT_PTR memBlock = (UINT_PTR)nvalloc(actualSize);
	UINT_PTR ret = ((memBlock + sizeof(void *) + BOUNDARY)
		& ~(BOUNDARY - 1));
	void **backPtr = (void **)(ret - sizeof(void *));
	*backPtr = (void *)memBlock;
	return (void *)ret;		
}


inline void EpochFreeAligned(void *ptr) {
	void **backPtr = (void **)((UINT_PTR)ptr - sizeof(void *));
	//free(*backPtr);
	nvfree(*backPtr);
}


inline void *ZeroedEpochMallocNoAlign(size_t size) {
	return nvalloc_zero(size);
}

inline void *EpochMallocNoAlign(size_t size) {
	return nvalloc(size);
}


inline void EpochFreeNoAlign(void *ptr) {

	nvfree(ptr);
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

// nodes larger than one cache line are naturally alligned to cache line boundaries in Rockall
inline void *AllocNode(size_t size) {
	return mallocx(size,0);
}

inline void* GetNextNodeAddress(size_t size) {
	return nextx(size,0);
}

inline void FreeNode(void *ptr) {
	dallocx(ptr,0);
}

inline int NodeMemoryIsFree(void *ptr) {
	if (malloc_usable_size(ptr) != 0) {
		return 0;
	}
	return 1;
}

inline void MarkNodeMemoryAsFree(void * ptr) {
	node_heap.Delete(ptr);
}
