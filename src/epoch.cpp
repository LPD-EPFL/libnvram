#include "epoch.h"

// Linked epoch threads.
static union {
	volatile EpochThreadData *head;
	UINT8 pad[EPOCH_CACHE_LINE_SIZE];
} EpochThreadList;



// Epoch stats names.
const char *EpochStatsEnum::Names[] = {
	"NewGenerationsAdded",
	"CollectCount",
	"CollectCountSuccess",
	"CollectCountFai",
	"DeallocationCount"
};

// Free generations that were used up.
void FreeUsedGenerations(EpochThreadData *epoch);

//---------------------------------------------------------------------
// Interface implementation.
//

void EpochGlobalInit() {
	EpochThreadList.head = NULL;
	link_flush_buffer = NULL;
}

void EpochGlobalInit(linkcache_t* buffer_ptr) {
	EpochThreadList.head = NULL;
	link_flush_buffer = buffer_ptr;
}


void EpochSetFlushBuffer(linkcache_t* buffer_ptr) {
	link_flush_buffer = buffer_ptr;
}

void* GetOpaquePageBuffer(EpochThread opaqueEpoch) {
	EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;
	return (void*)epoch->active_page_table;
}

void SetOpaquePageBuffer(EpochThread opaqueEpoch, void* pb) {
	EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;
	epoch->active_page_table = (active_page_table_t*) pb;
}


// Everything is stopped, so we can cleanly deallocate all data.
void EpochGlobalShutdown() {
	EpochThreadData *curr = (EpochThreadData *)EpochThreadList.head;
	EpochThreadData *prev;

	while(curr != NULL) {
		prev = curr;
		curr = (EpochThreadData *)curr->next;

		// only deallocate the epoch descriptor, everything else was
		// done before at ThreadShutdown time.
		EpochFreeAligned(prev);
	}
}

// initialize and cleanup epoch-related thread data
EpochThread EpochThreadInit(UINT32 id) {
	EpochThreadData *epoch = (EpochThreadData *)EpochCacheAlignedCacheSizeAlloc(
			sizeof(EpochThreadData));
	epoch->Init(id);

	// Now link the new epoch thread at the end of the list. We must make
	// sure that the positions of epochs in the list are never changed
	// and that is why we link it at the end.
	// This makes it incorrect to unlink anything from the list (because
	// thread leaves the system for example).
	//
	// We could solve this if needed by assigning indices to epoch threads.
	EpochThreadData * volatile *prev = (EpochThreadData **)&EpochThreadList.head;
	EpochThreadData *curr = (EpochThreadData *)EpochThreadList.head;

	while(true) {
		while(curr != NULL) {
			prev = (EpochThreadData * volatile *)&(curr->next);
			curr = (EpochThreadData *)curr->next;
		}

		// we are at the current end of the list
		curr = (EpochThreadData *)CAS_PTR(
				(volatile PVOID *)prev,
				NULL,
				epoch);

		// if CAS was successful we are done
		if(curr == NULL) {
			break;
		}

		// otherwise keep looping from the current element
		prev = (EpochThreadData * volatile *)&(curr->next);
		curr = (EpochThreadData *)curr->next;
	}

	// initialize pointer to the global head
	epoch->epochListHead = (EpochThreadData * volatile *)&EpochThreadList.head;

	

	return (EpochThread)epoch;
}

// Cleanup all pointers in an unsafe way. Should be called at the end of
// parallel work to perform cleanup.
//
void EpochUnsafeFinalizeAll(EpochThread opaqueEpoch) {
	EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;
	epoch->UnsafeFinalizeAll();
}

// All threads have stopped execution, so we can clean stuff in an
// unsafe way.
void EpochThreadShutdown(EpochThread opaqueEpoch) {
	EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;
	epoch->UnsafeFinalizeAll();
	epoch->Uninit();
}

// Get count of all garbage that is waiting to be reclaimed.
// This is useful for debugging and getting statistics about the
// epoch system.
ULONG EpochGetGarbageCount(EpochThread opaqueEpoch) {
	EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;

	ULONG count = epoch->current->usedNodes;

	EpochGeneration *gen = epoch->oldestUsed;

	while(gen != NULL) {
		count += gen->usedNodes;
		gen = gen->next;
	}

	return count;
}

void EpochScan(EpochThread opaqueEpoch) {
	EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;
	FreeUsedGenerations(epoch);
}

// Move partly populated current generation to list of used generations.
// This should be done when trying to free all memory held by this thread.
void EpochFlush(EpochThread opaqueEpoch) {
	EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;
    //fprintf(stderr, "epoch flush\n");

	// flush only if there is something to be flushed
	if(epoch->current->usedNodes != 0) {
		EpochChangeGeneration(epoch);
	}
}

//---------------------------------------------------------------------
// Memory management happens here.
//

// collect timestamps from all registered threads
static void CollectTimestampVector(
		EpochThreadData *collector,
		EpochTimestampVector *vectorTs) {
	EpochThreadData *curr = (EpochThreadData *)*collector->epochListHead;
	ULONG size = 0;

	assert(curr != NULL);

	while(curr != NULL) {
		if(curr == collector) { 
			// we can set timestamp for the current one to 0 safely
			// this enables us to immediately reclaim memory, without
			// waiting for current thread to wait for epoch end
			if (curr->ts < 2) {
				(*vectorTs)[size++] = EPOCH_FIRST_EPOCH;
			}
			else {
				(*vectorTs)[size++] = curr->ts - 2;
			}
		} else {
			(*vectorTs)[size++] = curr->ts;
		}
        
        //fprintf(stderr, "collect %lu %lu\n", (*vectorTs)[size-1], curr->ts);
		curr = (EpochThreadData *)curr->next;
	}

	vectorTs->size = size;
}

static void CollectTimestampVectorAll(
	EpochThreadData *collector,
	EpochTimestampVector *vectorTs) {
	EpochThreadData *curr = (EpochThreadData *)*collector->epochListHead;
	ULONG size = 0;

	assert(curr != NULL);

	while (curr != NULL) {
	
			(*vectorTs)[size++] = curr->ts;
		

		curr = (EpochThreadData *)curr->next;
	}

	vectorTs->size = size;
}


static void MarkCollectedTimestampVector(
	EpochThreadData *collector,
	EpochTimestampVector *vectorTs) {
	EpochThreadData *curr = (EpochThreadData *)*collector->epochListHead;
	ULONG size = 0;

	assert(curr != NULL);

   while (curr != collector) {
		    curr = (EpochThreadData *)curr->next;
        size++;
   }

   if ((*vectorTs)[size] > (curr->largestCollectedTs)) {
     curr->largestCollectedTs = (*vectorTs)[size];
     //fprintf(stderr, "curr->largestCollectedTs\n");
   }

	assert(curr != NULL);

//	while (curr != NULL) {
        //fprintf(stderr, "mark %lu\n",(*vectorTs)[size]);
//		if ((*vectorTs)[size] > (curr->largestCollectedTs)) {
//			curr->largestCollectedTs = (*vectorTs)[size];
            //fprintf(stderr, "curr->largestCollectedTs\n");
//		}
        //fprintf(stderr, "%d %lu\n", size, curr->largestCollectedTs);
//		curr = (EpochThreadData *)curr->next;
 //       size++;
	//}
    //fprintf(stderr, "\n");

//	vectorTs->size = size;
}

// Check whether new timestamp dominates old timestamp.
static bool IsTimestampVectorDominated(
		EpochTimestampVector *tsNew,
		EpochTimestampVector *tsOld) {
	// old timestamp is shorter or equal size to the new timestamp
	ULONG size = tsOld->size;

	for(ULONG idx = 0;idx < size;idx++) {
		EpochTsVal newVal = (*tsNew)[idx];

		// if thread is currently not using data, it doesn't prevent
		// us from deallocating memory
		if(newVal % 2 == 0) {
			continue;
		}

		// if thread moved to a new state, it doesn't prevent
		// us from deallocating memory
		EpochTsVal oldVal = (*tsOld)[idx];

		if(newVal > oldVal) {
			continue;
		}

		// if we are here, that means that the thread has not yet finished
		// its operation on the memory and we cannot deallocate memory
		return false;
	}

	return true;
}

// Free used generations starting from a vector that was already collected.
static void FreeUsedGenerations(
		EpochThreadData *epoch,
		EpochTimestampVector *newTs) {
	EpochGeneration *curr = epoch->oldestUsed;

	// keep stats about this collection
	epoch->stats.Increment(EpochStatsEnum::COLLECT_COUNT);
	bool success =  false;

    //fprintf(stderr, "free used gens\n");
	while(curr != NULL) {
		if(IsTimestampVectorDominated(newTs, &curr->vectorTs)) {
			// free memory and prepare generation for reuse
            //fprintf(stderr, "free\n");
			
			//buffer_flush_all_buckets(link_flush_buffer);
			curr->FinalizeAll();


			//MarkCollectedTimestampVector(epoch, &curr->vectorTs);
			curr->Clean();

			// prepare to move the generation in the new list
			EpochGeneration *free = curr;

			// move to the next newer generation
			curr = curr->next;

			// link at the head of free list
			free->next = epoch->free;
			epoch->free = free;

			epoch->oldestUsedCount--;

			success = true;
		} else {
			// we can break here, because all subsequent generations 
			// are newer and so cannot be dominated by the newTs it this
			// generation is not dominated
			break;
		}
	}

	// make the current generation the oldest
	epoch->oldestUsed = curr;

	// increment stats about this collect
	if(success) {
		epoch->stats.Increment(EpochStatsEnum::COLLECT_COUNT_SUCCESS);
	} else {
		epoch->stats.Increment(EpochStatsEnum::COLLECT_COUNT_FAIL);
	}
}

// Try to free any of the used generations:
// 1. Collect the timestamp, using the buffer in the epoch.
// 2. Traverse epochs and free all that are dominated by the
//    timestamp collected in step 1.
void FreeUsedGenerations(EpochThreadData *epoch) {
    //fprintf(stderr, "free used generatiosn\n");
	CollectTimestampVector(epoch, &epoch->vectorTsBuf);
    //fprintf(stderr, "after coll %lu %lu\n", (&epoch->vectorTsBuf)[0], (&epoch->vectorTsBuf[1]));
	FreeUsedGenerations(epoch, &epoch->vectorTsBuf);
}

// To change epoch generation we do the following:
// 1. Collect the current timestamp in the system.
// 2. Link the epoch at the end of the list of used epochs.
// 3. Use the epoch collected in step 1. to try to free some data.
//    Alternatively, we could postpone this until there are no more
//    free generations to use.
// 4. If there are no free generations, try to free some generations.
// 5. If there are still no free generations, allocate more space for
//    generations. Set one of the free generations to be used as current.
//
// The new generation is freed immediatelly in case when none
// of the threads is in the middle of a memory operation (which is
// always true if we have only one thread in the system).
//
void EpochChangeGeneration(EpochThreadData *epoch) {
    //fprintf(stderr, "epoch cahnge ge\n");
	// 1. Collect the current timestamp.
	CollectTimestampVector(epoch, &epoch->current->vectorTs);
	EpochTimestampVector *currentVectorTs = &epoch->current->vectorTs;


	// 2. Link the current generation at the end of used generations list.
	EpochGeneration *curr = epoch->oldestUsed;

	if(curr != NULL) {
		while(curr->next != NULL) {
			curr = curr->next;
		}

		curr->next = epoch->current;
	} else {
		epoch->oldestUsed = epoch->current;
	}

	epoch->current->next = NULL;
	epoch->oldestUsedCount++;

	// 3. See if we can free something from used generations.
#ifdef BUFFERING_ON
	//fprintf(stderr, "chaning generations\n");
	cache_wb_all_buckets(link_flush_buffer);
#endif

    //fprintf(stderr, "after coll %lu %lu\n", (*currentVectorTs)[0], (*currentVectorTs)[1]);
	FreeUsedGenerations(epoch, currentVectorTs);

	// 4. Make sure there are some free generations for reuse.
	//    We could spin here for a while when there is too much garbage.
	if(epoch->free == NULL ||
			epoch->oldestUsedCount > EPOCH_MAX_NODES_PER_THREAD) {
		FreeUsedGenerations(epoch);
	}

	// 5. If there are still no free generations allocate one and make
	// it current.
	if(epoch->free == NULL) {
		EpochGeneration *newGen =
				(EpochGeneration *)EpochCacheAlignedCacheSizeAlloc(
				sizeof(EpochGeneration));

		newGen->Init();

		// link into free list
		newGen->next = NULL;
		epoch->current = newGen;

		// link into list of all generations
		newGen->nextAllGenerations = epoch->allGenerations;
		epoch->allGenerations = newGen;

		epoch->stats.Increment(EpochStatsEnum::NEW_GENERATIONS_ADDED);
	} else {
		epoch->current = epoch->free;
		epoch->free = epoch->free->next;
	}
}

// Print stats for all epochs in the system.
void EpochPrintStats() {
#ifdef EPOCH_KEEP_STATS
	// Don't print anything if there were no threads running.
	if(EpochThreadList.head == NULL) {
		return;
	}

	UINT indent = 0;
	EpochStats totalStats;
	totalStats.Init();

	printf("Epoch stats:\n");
	printf("------------\n");

	// print stats for each thread
	EpochThreadData *curr = (EpochThreadData *)EpochThreadList.head;

	while(curr != NULL) {
		printf("Thread 0x%x:\n", (UINT_PTR)curr);
		indent += 1;
		curr->stats.Print(indent);
		indent -= 1;

		// accumulate stats
		EpochStats::Accumulate(&totalStats, &curr->stats);

		curr = (EpochThreadData *)curr->next;
	}

	// print total stats
	printf("Totals:\n", curr);
	indent += 1;
	totalStats.Print(indent);
	indent -= 1;
#endif /* EPOCH_KEEP_STATS */
}
