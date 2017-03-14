//#include "epoch_impl.h"

#include "active-page-table.h"

__thread char path[32];

static __thread PMEMobjpool *pop;

active_page_table_t* allocate_apt(UINT32 id) {

    //char path[32];
    sprintf(path, "/tmp/thread_%u", id); //thread id as file name

    //remove file if it exists
    //TODO might want to remove this instruction in the future
    remove(path);

	pop = NULL;

	if (access(path, F_OK) != 0) {
        if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(apt),
            APT_POOL_SIZE, S_IWUSR | S_IRUSR)) == NULL) {
            printf("failed to create pool1 wiht name %s\n", path);
            return NULL;
        }
    } else {
        if ((pop = pmemobj_open(path, LAYOUT_NAME)) == NULL) {
            printf("failed to open pool with name %s\n", path);
            return NULL;
        }
    }
    
	//zeroed allocation if the object does not exist yet
    TOID(active_page_table_t) apt = POBJ_ROOT(pop, active_page_table_t);

	//I should now return a pointer to this 
	return D_RW(apt);
}

/*
	creates a page buffer with a certain number of preallocated free entries
*/
active_page_table_t* create_active_page_table(UINT32 id) {

	active_page_table_t* new_buffer = NULL;

	new_buffer = allocate_apt(id); //zeroed allocation

	new_buffer->page_size = PAGE_SIZE;
	new_buffer->current_size = 0;

#ifdef BUFFERING_ON
	new_buffer->shared_flush_buffer = NULL;
#endif

	new_buffer->last_in_use = DEFAULT_PAGE_BUFFER_SIZE;
	write_data_nowait(new_buffer, 1);

	wait_writes();
	return new_buffer;
}

/*
	frees all the memory associated with a page buffer
*/
void destroy_active_page_table(active_page_table_t* active_page_table) {
	
	pmemobj_close(pop);

    remove(path);
}

/*
	clears all the entries in the buffer;
*/
void clear_buffer(active_page_table_t* buffer, EpochTsVal cleanTs, EpochTsVal currTs) {

    size_t max_seen = 0;
#ifdef BUFFERING_ON
	if (buffer->shared_flush_buffer != NULL) {
		//fprintf(stderr, "clearing page buffer\n");
		cache_wb_all_buckets(buffer->shared_flush_buffer);
	}
#endif

	size_t i;


    for (i = 0; i < buffer->last_in_use; i++) {
        if ((buffer->pages[i].page!=NULL) && ((buffer->pages[i].lastTsAccess < cleanTs) || (buffer->pages[i].lastTsAccess ==0)) && ((buffer->pages[i].lastTsIns < currTs) || (buffer->pages[i].lastTsIns == 0))) {
            buffer->pages[i].page = NULL;
            buffer->pages[i].lastTsAccess = EPOCH_FIRST_EPOCH;
            buffer->current_size--;
        }
        if ((buffer->pages[i].page != NULL) && (i > max_seen)) {
            max_seen = i;
        }   
    }

    //decrease search size if last half is empty
    size_t half = buffer->last_in_use/2;
    if ((max_seen < half) && (half > DEFAULT_PAGE_BUFFER_SIZE)) {
       buffer->last_in_use = half;
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

	size_t i;

    size_t first_empty = SIZE_MAX;


    for (i = 0; i < pages->last_in_use; i++) {
			if (pages->pages[i].page == page) {
				//page already present, nothing to add, can return
#ifdef DO_STATS
				pages->hits++;
#endif
				if (isRemove) {
					if (pages->pages[i].lastTsAccess < currentTs) {
						pages->pages[i].lastTsAccess = currentTs;
						//no need to persist this, the timestamps are not important for recovery
					}
				}
				else {
					if (pages->pages[i].lastTsIns < currentTs) {
						pages->pages[i].lastTsIns = currentTs;
						//no need to persist this, the timestamps are not important for recovery
					}
				}
				return;
			}

			if (pages->pages[i].page == NULL) {
				if (first_empty == SIZE_MAX) first_empty = i;
			}
	}

	if (first_empty != SIZE_MAX){ 
		pages->pages[first_empty].page = page;
		if (isRemove) {
			pages->pages[first_empty].lastTsAccess = currentTs;
			pages->pages[first_empty].lastTsIns = 0;
		}
		else {
			pages->pages[first_empty].lastTsAccess = 0;
			pages->pages[first_empty].lastTsIns = currentTs;
		}
		pages->current_size++;
		
		write_data_wait(&(pages->pages[first_empty]), 1);
		return;
	}


	// page has not been found, and no empty entry in the buffer, up to last_in_use, means we need to try to expand our search space
    size_t twice = pages->last_in_use*2; 

    if (twice >= MAX_NUM_PAGES) {
        fprintf(stderr, "PAGE_BUFFER_SIZE_EXCEEDED!\n");
        return;
    }

    size_t old = pages->last_in_use;

    pages->last_in_use = twice;
    write_data_wait(pages,1); //need to make sure this is persisted before writing after the marker

    assert(pages->pages[old].page == NULL); //we just expanded; this means the newly enabled page entries should be null

	pages->pages[old].page = page;
	if (isRemove) {
		pages->pages[old].lastTsAccess = currentTs;
		pages->pages[old].lastTsIns = 0;
	}
	else {
		pages->pages[old].lastTsAccess = 0;
		pages->pages[old].lastTsIns = currentTs;
	}

	write_data_nowait(&(pages->pages[old]), 1);

	wait_writes();

	pages->current_size++;

}
