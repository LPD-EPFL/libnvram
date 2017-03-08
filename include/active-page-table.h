#ifndef _ACTIVE_PAGE_TABLE_H_
#define _ACTIVE_PAGE_TABLE_H_

#define DEFAULT_PAGE_BUFFER_SIZE 32
#define CLEAN_THRESHOLD 16

#define WORDS_PER_CACHE_LINE 8
#define PAGE_SIZE  65536//could have larger granularity pages as well - that would reduce the number of page numbers we need to store


//#define DO_STATS 1
/*
	the page buffer is NOT thread safe;
	we assume such a buffer for each individual thread
*/


typedef struct page_descriptor_t {
	void* page;
	EpochTsVal lastTsAccess;
	EpochTsVal lastTsIns;
}page_descriptor_t;

//an entry should take two cache lines
typedef struct active_page_table_entry_t{
	page_descriptor_t pages[WORDS_PER_CACHE_LINE-1];
	active_page_table_entry_t* next;
	BYTE padding[8];
} active_page_table_entry_t;


typedef struct active_page_table_t {
	unsigned page_size; //TODO what if I want to add a larger page?
	unsigned current_size;
	BYTE clear_all; // if flag set, I must clear the page buffer before accessing it again
	active_page_table_entry_t* pages; // pages from which frees and allocs just happened
#ifdef BUFFERING_ON
	flushbuffer_t* shared_flush_buffer;
#endif
#ifdef DO_STATS
	UINT64 num_marks;
	UINT64 hits;
#endif
} active_page_table_t;


//given an address, get the start memory location of the page it belongs to
inline void* get_page_start_address(void* address) {
	return (void*) ((UINT_PTR)address & ~(PAGE_SIZE - 1));
}

//allocate a oage buffer already containing space for a predefined number of elements
active_page_table_t* create_active_page_table();

//deallocate the bage buffer and entries
void destroy_active_page_table(active_page_table_t* to_delete);

//if a page is not present, add it to the buffer and persist the addition
void mark_page(active_page_table_t* pages, void* ptr, int allocation_size, EpochTsVal currentTs, EpochTsVal collectTs, int isRemove);

//clear all the pages in the buffer
void clear_buffer(active_page_table_t* buffer, EpochTsVal cleanTs, EpochTsVal currTs);

#endif
