#include "epoch_impl.h"
/*
	creates a page buffer with a certain number of preallocated free entries
*/
active_page_table_t* create_active_page_table() {
	unsigned num_elements = DEFAULT_PAGE_BUFFER_SIZE;

	active_page_table_t* new_buffer = NULL;

	new_buffer = (active_page_table_t*)ZeroedEpochMallocNoAlign(sizeof(active_page_table_t)); //zeroed allocation
	//assume this is persisted in the allocator ???
	new_buffer->page_size = PAGE_SIZE;
	new_buffer->current_size = 0;

#ifdef BUFFERING_ON
	new_buffer->shared_flush_buffer = NULL;
#endif
	write_data_nowait(new_buffer, 1);

	/*
		FIXME this buffer should be allocated and linked atomically; the persistent memory allocator should provide this by default
	*/

	if (num_elements == 0) {
		return new_buffer;

	}
	unsigned num_entries = (num_elements -1) / (WORDS_PER_CACHE_LINE - 1) + 1;

	unsigned i;
	active_page_table_entry_t* new_entry = NULL;
	active_page_table_entry_t* prev = NULL;
	

	for (i = 0; i < num_entries; i++) {
		new_entry = (active_page_table_entry_t*)ZeroedEpochMallocNoAlign(sizeof(active_page_table_entry_t)); //zeroed alloc
		write_data_nowait(new_entry, 2);
		if (i == 0) {
			new_buffer->pages = new_entry;
			write_data_nowait(&new_buffer->pages, 1);
		}
		else {
			prev->next = new_entry;
			write_data_nowait(&prev->next, 1);
		}

		/* FIXME: should be an atomic alloc and link */
		prev = new_entry;

	}
	wait_writes();
	return new_buffer;
}

/*
	frees all the memory associated with a page buffer
*/
void destroy_active_page_table(active_page_table_t* active_page_table) {
	
	active_page_table_entry_t* current;
	active_page_table_entry_t* next;

	current = active_page_table->pages;
	while (current != NULL) {
		next = current->next;
		EpochFreeNoAlign(current);
		current = next;
	}
	
	EpochFreeNoAlign(active_page_table);

	//TODO make this pmem aware? what if there's a crash while I'm destroying the buffer (although the buffer should exists for the lifetime of a thread accessing the data structure)
}

/*
	clears all the entries in the buffer;
*/
void clear_buffer(active_page_table_t* buffer, EpochTsVal cleanTs, EpochTsVal currTs) {
	active_page_table_entry_t* current = buffer->pages;

#ifdef BUFFERING_ON
	if (buffer->shared_flush_buffer != NULL) {
		//fprintf(stderr, "clearing page buffer\n");
		buffer_flush_all_buckets(buffer->shared_flush_buffer);
	}
#endif

	int i;

	while (current != NULL) {
		for (i = 0; i < WORDS_PER_CACHE_LINE - 1; i++) {
			if ((current->pages[i].page!=NULL) && ((current->pages[i].lastTsAccess < cleanTs) || (current->pages[i].lastTsAccess ==0)) && ((current->pages[i].lastTsIns < currTs) || (current->pages[i].lastTsIns == 0))) {
				current->pages[i].page = NULL;
				current->pages[i].lastTsAccess = EPOCH_FIRST_EPOCH;
				buffer->current_size--;
			}
		}
		current = current->next;
	}

	buffer->clear_all = 0;
	// no need to persist this now
}

/*
	mark a page as having data that was either allocated or freed in the current epoch
*/

void mark_page(active_page_table_t* pages, void* ptr,  int allocation_size, EpochTsVal currentTs, EpochTsVal collectTs, int isRemove) {

#ifdef DO_STATS
	pages->num_marks++;
#endif
	if ((pages->clear_all) || (pages->current_size > CLEAN_THRESHOLD)) {
		//fprintf(stderr, "clear all size before %u curr ts %u collect ts %u\n", pages->current_size, currentTs, collectTs);
		clear_buffer(pages, collectTs, currentTs);
		//fprintf(stderr, "clear all size after %u\n", pages->current_size);
	}

	void * address = ptr;
	if (address == NULL) {
		address = GetNextNodeAddress(allocation_size);
	}

	void* page = get_page_start_address(address);

	int i;

	active_page_table_entry_t* current = pages->pages;
	active_page_table_entry_t* prev = NULL;

	page_descriptor_t* first_empty = NULL;

	while (current != NULL) {

		for (i = 0; i < WORDS_PER_CACHE_LINE - 1; i++) {
			if (current->pages[i].page == page) {
				//page already present, nothing to add, can return
#ifdef DO_STATS
				pages->hits++;
#endif
				if (isRemove) {
					if (current->pages[i].lastTsAccess < currentTs) {
						current->pages[i].lastTsAccess = currentTs;
						//no need to persist this, the timestamps are not important for recovery
					}
				}
				else {
					if (current->pages[i].lastTsIns < currentTs) {
						current->pages[i].lastTsIns = currentTs;
						//no need to persist this, the timestamps are not important for recovery
					}
				}
				return;
			}

			if (current->pages[i].page == NULL) {
				if (first_empty == NULL) first_empty = &(current->pages[i]);
			}
		}

		prev = current;
		current = current->next;
	}

	if (first_empty != NULL){ 
		first_empty->page = page;
		if (isRemove) {
			first_empty->lastTsAccess = currentTs;
			first_empty->lastTsIns = 0;
		}
		else {
			first_empty->lastTsAccess = 0;
			first_empty->lastTsIns = currentTs;
		}
		pages->current_size++;
		
		write_data_wait(first_empty, 1);
		return;
	}


	// page has not been found, and no empty entry in the buffer, that means we need to create a new entry and link it

	active_page_table_entry_t* new_entry = (active_page_table_entry_t*) ZeroedEpochMallocNoAlign(sizeof(active_page_table_entry_t));
	// the new entry is assumed to be zeroed - works right now
	// the new is also assumed to be persisted

	new_entry->pages[0].page = page;
	if (isRemove) {
		new_entry->pages[0].lastTsAccess = currentTs;
		new_entry->pages[0].lastTsIns = 0;
	}
	else {
		new_entry->pages[0].lastTsAccess = 0;
		new_entry->pages[0].lastTsIns = currentTs;
	}

	write_data_nowait(new_entry, 1);

	if (pages->pages == NULL) {
		pages->pages = new_entry;
		write_data_nowait(pages->pages, 1);
	}
	else {
		prev->next = new_entry;
		write_data_nowait(&(prev->next), 1);
	}


	wait_writes();

	pages->current_size++;

	/* FIXME: for the insertion of a new page buffer entry:
	it's not completely ok right now, since we have two separate steps
	we should assume the presence of a persistent allocator, 
	where you can call "alloc and link"; 
	need not be the same allocator type as for the structure nodes */
}
