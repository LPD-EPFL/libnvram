#include "link_cache.h"

#ifdef DO_PROFILE
extern __thread uint64_t inserts;
extern __thread uint64_t removes;
#endif

linkcache_t* cache_create() {
	int i;

#ifdef DO_PROFILE
    inserts = 0;
    removes = 0;
#endif

	linkcache_t* new_cache = (linkcache_t*)_aligned_malloc(sizeof(linkcache_t), CACHE_LINE_SIZE);

	for (i = 0; i < NUM_BUCKETS; i++) {
		new_cache->buckets[i] = (bucket_t*)_aligned_malloc(sizeof(bucket_t), CACHE_LINE_SIZE);
		new_cache->buckets[i]->header.all = 0;
	}

	MemoryBarrier();
	return new_cache;
}

void cache_destroy(linkcache_t* cache) {
	int i;

	for (i = 0; i < NUM_BUCKETS; i++) {
		_aligned_free(cache->buckets[i]);
	}
	_aligned_free(cache);
}


/*
	make sure everything is flushed
*/
void cache_wb_all_buckets(linkcache_t* cache) {
	//fprintf(stderr, "flushing all\n");
	// is there a cheaper way of doing this?
	int i;
	char flushed[NUM_BUCKETS];
	for (i = 0; i < NUM_BUCKETS; i++) {
		flushed[i] = 0;
	}
	int total_flushed = 0;
	while (total_flushed < NUM_BUCKETS) {
		for (i = 0; i < NUM_BUCKETS; i++) {
			if (flushed[i] == 0) {
				if (bucket_wb(cache, i)) {
					flushed[i] = 1;
					total_flushed++;
				}
			}
		}
		_mm_pause();
	}
}

int bucket_wb(linkcache_t* cache, int bucket_num) {
	//fprintf(stderr, "flushing %d\n",bucket_num);
	int i;
	UINT16 state, new_state;
	UINT16 already_flushed = 0;

	bucket_t* current_bucket = cache->buckets[bucket_num];
	//if someone else flushing, return failure
	if (current_bucket->header.write_back_lock) {
		return 0;
	}
	//Step 1: try acquire the flush lock (contention should be unlikely for this CAS, so the acquisition should ususally succeed)
	if (CAS_U16((volatile short*)&current_bucket->header.write_back_lock, 0, 1) != 0) {
		return 0;
	}

	do {

		//Step 2: get the bitmap of the state of the addresses in the bucket
		state = current_bucket->header.local_flags;

		//Step 3: flush those which had the flags set to busy (just once)
		for (i = 0; i < NUM_ENTRIES_PER_BUCKET; i++) {
			if (is_busy(state, i) && (is_free(already_flushed, i))) {
				write_data_nowait((void*)current_bucket->addresses[i], 1);
			}
		}

		//Step 4: try to reset all the flags of the entries which have been flushed
		new_state = state;
		for (i = 0; i < NUM_ENTRIES_PER_BUCKET; i++) {
			if (is_busy(state, i)) {
				new_state = mark_free(new_state, i);
#ifdef DO_PROFILE
                removes++;
#endif
				already_flushed = mark_busy(already_flushed, i);
			}
		}

	} while (CAS_U16((volatile short*)&current_bucket->header.local_flags, state, new_state) != state);

	//Step 5: make sure the write-backs which I have issued are written to persistent memory
	wait_writes();

	//Step 6: release the flushing lock
	current_bucket->header.write_back_lock = 0;
	_mm_sfence();

	return 1;
}

/*int cache_add(linkcache_t* cache, UINT64 key, void* addr) {
	int bucket_num = get_bucket(key);
	UINT16 hash = get_hash(key);

	bucket_t* bucket = cache->buckets[bucket_num];
retry:
	UINT16 state = bucket->header.local_flags;
	
	if (all_busy(state)) {
		if (bucket_wb(cache, bucket_num)) {
			goto retry;
		}
		return 0;
	}

	int i = find_free_index(state);

	UINT16 new_state = state;
	new_state = mark_pending(new_state, i);

	if (InterlockedCompareExchange16((volatile short*) &bucket->header.local_flags, new_state, state) != state) {
		goto retry;
	}

	bucket->addresses[i] = addr;
	bucket->hashes[i] = hash;

	state = bucket->header.local_flags;
	new_state = state;
	new_state = mark_busy(new_state, i);

	while (InterlockedCompareExchange16((volatile short*)&bucket->header.local_flags, new_state, state) != state) {
		_mm_pause();
	}
	return 1;

} */

int cache_try_link_and_add(linkcache_t* cache, UINT64 key, volatile void** target, volatile void* oldvalue, volatile void* value) {
	
	unsigned bucket_num = get_bucket(key);
	//fprintf(stderr, "adding %d\n", bucket_num);
	UINT16 hash = get_hash(key);

	bucket_t* bucket = cache->buckets[bucket_num];

#ifdef TSX_ENABLED
	//first try to do the linking and insertion through a TSX transaction
	unsigned int status = _xbegin();
	if (status == _XBEGIN_STARTED) {
		UINT16 state = bucket->header.local_flags;
		if (no_free_index(state)) {
			_xabort(0);
		}
		else {
			int i = find_free_index(state);
			UINT16 new_state = state;
			new_state = mark_busy(new_state, i);
			bucket->header.local_flags = new_state;
			bucket->addresses[i] = target;
			bucket->hashes[i] = hash;
			if (*target == oldvalue) {
				*target = value;
#ifdef DO_PROFILE
                inserts++;
#endif
				_xend();
				return 1;
			}
			else {
				_xabort(0);
				return 0;
			}
		}
	}
#endif
	//if we do not manage to to execute the tsx transaction, try through successive CASes
retry:
	UINT16 state = bucket->header.local_flags;

	if (no_free_index(state)) {
		if (!(no_completed_entries(state)) &&  (bucket_wb(cache, bucket_num))) {
			goto retry;
		}
		return 0;
	}

	int i = find_free_index(state);

	UINT16 new_state = state;
	new_state = mark_pending(new_state, i);

	if (CAS_U16((volatile short*)&bucket->header.local_flags, state, new_state) != state) {
		goto retry;
	}

	bucket->addresses[i] = target; //FIXME is this right ???
	bucket->hashes[i] = hash;

	if (CAS_PTR((volatile PVOID*)target, (PVOID) oldvalue, (PVOID)mark_ptr_cache((UINT_PTR)value)) != oldvalue) {
		//abort
		state = bucket->header.local_flags;
		new_state = state;
		new_state = mark_free(new_state, i);

		while (CAS_U16((volatile short*)&bucket->header.local_flags, state, new_state) != state) {
			_mm_pause();
		}
		return 0;
	}

	state = bucket->header.local_flags;
	new_state = state;
	new_state = mark_busy(new_state, i);

	while (CAS_U16((volatile short*)&bucket->header.local_flags, state, new_state) != state) {
		_mm_pause();
	}


	CAS_PTR((volatile PVOID*)target,(PVOID)mark_ptr_cache((UINT_PTR) value), (PVOID) value);

#ifdef DO_PROFILE
    inserts++;
#endif
	return 1;

}

int cache_scan(linkcache_t* cache, UINT64 key) {
	unsigned bucket_num = get_bucket(key);
	UINT16 hash = get_hash(key);

	bucket_t* to_search = cache->buckets[bucket_num];
	if (all_free(to_search->header.local_flags)) {
		return 0;
	}
	int i;
	for (i = 0; i < NUM_ENTRIES_PER_BUCKET; i++) {
		if ((to_search->hashes[i] == hash)) {
			if (is_busy(to_search->header.local_flags, i)) {
				return bucket_wb(cache, bucket_num);
			}
			//if an entry for the key is there, it is pending, and the pointer has been marked (meaning the link happened), then I shpuld flush the pointer before I return
			else if ((is_pending(to_search->header.local_flags, i)) && (is_marked_ptr_cache((UINT_PTR)*(void**)(to_search->addresses[i])))){
				void* val = *(void**)(to_search->addresses[i]);
				write_data_wait((void*)unmark_ptr_cache((UINT_PTR)val), 1);
			}
		}
	}
	return 0;
}

int cache_size(linkcache_t* cache) {
    int size = 0;

    return size;
}
