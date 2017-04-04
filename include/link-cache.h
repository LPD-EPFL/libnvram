#ifndef _LINK_CACHE_H_
#define _LINK_CACHE_H_

#include <malloc.h>
#include <immintrin.h>

#include "nv_utils.h"
#include "nv_memory.h"

/* this is basically a hash table that should be kept in
volatile memory which stores the cache lines that need to be persistenly
written to non-volatile memory*/

#define NUM_BUCKETS 32
#define KEY_TO_HASH_MOD 65536
#define NUM_ENTRIES_PER_BUCKET 6

typedef union header_t {
	struct {
		volatile UINT16 write_back_lock;
		volatile UINT16 local_flags;
	};
	volatile UINT32 all;
}header_t;

typedef CACHE_ALIGNED struct bucket_t {
	volatile header_t header;
	volatile UINT16 hashes[6];
	volatile void* addresses[6];
} bucket_t;

typedef CACHE_ALIGNED struct linkcache_t {
	bucket_t* buckets[NUM_BUCKETS];
} linkcache_t;


//create a new cache
linkcache_t* cache_create();

//free a cache
void cache_destroy(linkcache_t* cache);

//adds a new entry to the cache
//int cache_add(linkcache_t* cache, UINT64 key, void* addr);

//could have this not be public
//i.e., it can be called internally in case of an add that fills the cache
//or if a search finds the key
int bucket_wb(linkcache_t* cache, int bucket_num);

int cache_scan(linkcache_t* cache, UINT64 key);

int cache_try_link_and_add(linkcache_t* cache, UINT64 key, volatile void** target, volatile void* oldvalue, volatile void* value);

void cache_wb_all_buckets(linkcache_t* cache);

int cache_size(linkcache_t* cache); //not thread-safe!

//00 - free
//01 - pending
//10 - busy

#define STATE_FREE 0x0
#define STATE_PENDING 0x1
#define STATE_BUSY 0x2

static inline UINT32 mark_pending(UINT16 bmap, int pos) {
	return (bmap & ~(0x00000003 << (2 * pos))) | (0x1 << (2 * pos));
}

static inline UINT32 mark_busy(UINT16 bmap, int pos) {
	return (bmap & ~(0x00000003 << (2 * pos))) | (0x2 << (2 * pos));
}

static inline UINT32 mark_free(UINT16 bmap, int pos) {
	return bmap & ~(0x00000003 << (2*pos));
}

static inline int is_free(UINT16 bmap, int pos) {
	return (bmap & (0x03 << (2 * pos))) == 0;
}

static inline int is_pending(UINT16 bmap, int pos) {
	return (bmap & (0x03 << (2 * pos))) == 1;
}

static inline int is_busy(UINT16 bmap, int pos) {
	return (bmap & (0x02 << (2*pos))) != 0;
}

static inline int no_completed_entries(UINT16 bmap) {
	return (bmap | 0xaaaa) == 0;
}

static inline int all_free(UINT16 bmap) {
	return (bmap == 0);
}

static inline UINT16 get_hash(UINT64 key) {
	return (key / NUM_BUCKETS) % KEY_TO_HASH_MOD;
}

static inline unsigned get_bucket(UINT64 key) {
	return key % NUM_BUCKETS;
}

static inline int find_free_index(UINT16 bmap) {
	int i = 0;
	while ((bmap & 0x03) != 0) {
		bmap = bmap >> 2;
		i++;
		if (i == NUM_ENTRIES_PER_BUCKET) {
			return -1;
		}
	}
	return i;
}

static inline int no_free_index(UINT16 bmap) {
	int i = 0;
	while ((bmap & 0x03) != 0) {
		bmap = bmap >> 2;
		i++;
		if (i == NUM_ENTRIES_PER_BUCKET) {
			return 1;
		}
	}
	return 0;
}

static inline UINT_PTR mark_ptr_cache(UINT_PTR p) {
	return (p | (UINT_PTR)0x04);
}

static inline UINT_PTR unmark_ptr_cache(UINT_PTR p) {
	return(p & ~(UINT_PTR)0x04);
}
static inline int is_marked_ptr_cache(UINT_PTR p) {
	return (int)(p & (UINT_PTR)0x04);
}

#endif
