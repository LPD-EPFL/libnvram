#ifndef _ACTIVE_PAGE_TABLE_H_
#define _ACTIVE_PAGE_TABLE_H_


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stddef.h>
#include <libpmem.h>
#include <assert.h>
#include <libpmemobj.h>

#include "nv_memory.h"
#include "link-cache.h"
#include "epochalloc.h"
#include "nv_utils.h"
#include "epoch_common.h"

#define APT_POOL_SIZE    (10 * 1024 * 1024) /* 1 MB */


#define LAYOUT_NAME "apt"

#define DEFAULT_PAGE_BUFFER_SIZE 32
#define CLEAN_THRESHOLD 16

//#define WORDS_PER_CACHE_LINE 8
#define PAGE_SIZE  65536//could have larger granularity pages as well - that would reduce the number of page numbers we need to store


//#define DO_STATS 1
/*
	the page buffer is NOT thread safe;
	we assume such a buffer for each individual thread
*/

#define MAX_NUM_PAGES 8192

typedef struct page_descriptor_t {
	void* page;
	EpochTsVal lastTsAccess;
	EpochTsVal lastTsIns;
}page_descriptor_t;

typedef struct active_page_table_t {
	size_t page_size; //TODO what if I want to add a larger page?
	size_t current_size;
    size_t last_in_use;
	BYTE clear_all; // if flag set, I must clear the page buffer before accessing it again
#ifdef BUFFERING_ON
	linkcache_t* shared_flush_buffer;
#endif
#ifdef DO_STATS
	UINT64 num_marks;
	UINT64 hits;
#endif
	page_descriptor_t pages[MAX_NUM_PAGES]; // pages from which frees and allocs just happened
} active_page_table_t;


POBJ_LAYOUT_BEGIN(apt);
POBJ_LAYOUT_ROOT(apt, active_page_table_t);
POBJ_LAYOUT_END(apt);

//given an address, get the start memory location of the page it belongs to
inline void* get_page_start_address(void* address) {
	return (void*) ((UINT_PTR)address & ~(PAGE_SIZE - 1));
}

//allocate a oage buffer already containing space for a predefined number of elements
active_page_table_t* create_active_page_table(UINT32 id);

//deallocate the bage buffer and entries
void destroy_active_page_table(active_page_table_t* to_delete);

//if a page is not present, add it to the buffer and persist the addition
void mark_page(active_page_table_t* pages, void* ptr, int allocation_size, EpochTsVal currentTs, EpochTsVal collectTs, int isRemove);

//clear all the pages in the buffer
void clear_buffer(active_page_table_t* buffer, EpochTsVal cleanTs, EpochTsVal currTs);

#endif
