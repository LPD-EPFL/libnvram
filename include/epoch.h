#ifndef _EPOCH_H_
#define _EPOCH_H_

#include "epoch_impl.h"
#include "active-page-table.h"

// Use 64 bit timestamps.
// This could lead to high space requirements just for storing the
// timestamps, as we will need one of these for each thread in the
// system in each bucket. With 512 threads, we would end up using
// 4kB just for storing epoch timestamps in each bucket.
typedef UINT64 EpochTsVal;

// opaque type to store epoch data
typedef void *EpochThread;

// cleanup function for the pointers
typedef void (*EpochFinalizeFun)(void *object, void *context, void *tls);

// initialize and cleanup epoch system
void EpochGlobalInit();
void EpochGlobalShutdown();

// print all stats
void EpochPrintStats();

// initialize and cleanup epoch-related thread data
EpochThread EpochThreadInit();
void EpochThreadShutdown(EpochThread epoch);

void EpochUnsafeFinalizeAll(EpochThread epoch);

void EpochSetFlushBuffer(flushbuffer_t* buffer_ptr);

void EpochGlobalInit(flushbuffer_t* buffer_ptr);

// start and end epochs
void EpochStart(EpochThread epoch);
void EpochEnd(EpochThread epoch);
void EpochEndIfStarted(EpochThread epoch);

void* EpochAllocNode(EpochThread epoch, size_t size);
void EpochDeclareUnlinkNode(EpochThread epoch, void* ptr, size_t size);
void EpochFreeNode(void* ptr);
void* GetOpaquePageBuffer(EpochThread opaqueEpoch);
void  SetOpaquePageBuffer(EpochThread opaqueEpoch, void* pb);


// pass a pointer to the epoch system to remove when safe
void EpochReclaimObject(
		EpochThread opaqueEpoch,
		void *ptr,
		void *context,
		void *tls,
		EpochFinalizeFun finalizeFun);

// get information about the number of objects waiting to be reclaimed
// in the current thread
ULONG EpochGetGarbageCount(EpochThread epoch);

// functions to force memory cleanup
void EpochFlush(EpochThread opaqueEpoch);
void EpochScan(EpochThread opaqueEpoch);


//---------------------------------------------------------------------
// Constants
//

// ADL: taken from other parts of code, should reuse from there not
// define again in the end (equal to MAX_CPUS from baseutil.h)
const ULONG EPOCH_MAX_CPUS = 1024;

// Maximum number of nodes to deallocate per thread. This is about the
// same number as with guards. This is not actually the maximum to avoid
// livelocks, but a threshold which shouldn't be passed.
const ULONG EPOCH_MAX_NODES_PER_THREAD = EPOCH_MAX_CPUS / 2;

// How many epoch nodes we want to group in one generation. Arbitrarily
// set.
// Consider that one vector of timestamps will be allocated per generation.
// Also consider that nodes in the generation will deallocated only when
// it is completely full and when no nodes from the generation are visible.
// The higher this number, more memory is kept around unnecessarily, but also
// the performance is better.
const ULONG EPOCH_NODES_IN_GENERATION = 64;

// How many epoch generations are reserved for each thread.
const ULONG EPOCH_GENERATIONS_PER_THREAD =
	EPOCH_MAX_NODES_PER_THREAD / EPOCH_NODES_IN_GENERATION;

// Initial number of timestamps in each generation timestamp.
const ULONG EPOCH_INITIAL_EPOCH_VECTOR_SIZE = 16;

// We start counting from 0. No need to reserve any values here.
const UINT64 EPOCH_FIRST_EPOCH = 0;
const UINT64 EPOCH_LAST_EPOCH = 0xffffffffffffffff;

//---------------------------------------------------------------------
// Type declarations
//


static flushbuffer_t* link_flush_buffer;

// one pointer to deallocate
struct EpochNode
{
	void *ptr;
	void *context;
	void *tls;
	EpochFinalizeFun finalizeFun;
};

// general dynamic vector data structure
template<typename T, ULONG INITIAL_CAPACITY>
class EpochDynamicVector
{
public:
	void Init();
	void Uninit();

	T& operator[](ULONG idx);

	ULONG size;

private:
	T *data;
	ULONG capacity;
};


// Vector of timestamps.
typedef EpochDynamicVector<
		EpochTsVal,
		EPOCH_INITIAL_EPOCH_VECTOR_SIZE> EpochTimestampVector;

// An array of epoch nodes with their timestamp.
struct EpochGeneration
{
	// Init / Uninit.
	void Init();
	void Uninit();

	// Finalize all. It was determined that it is safe to do so.
	void FinalizeAll();

	// prepare to reuse
	void Clean();

	// nodes
	EpochNode nodes[EPOCH_NODES_IN_GENERATION];

	// how many nodes are used
	ULONG usedNodes;

	// timestamp vector
	EpochTimestampVector vectorTs;

	// for linking generations in various lists
	EpochGeneration *next;

	// for linking generations in the list of all generations
	// which is used for cleanup later
	EpochGeneration *nextAllGenerations;
};

// Main thread epoch data structure.
struct EpochThreadData
{
	// Init / Uninit.
	void Init();
	void Uninit();

	// Free all data that this thread deallocated.
	// This is not thread safe and is used during shutdown of the
	// thread, when no other threads are running.
	void UnsafeFinalizeAll();

	// This is current timestamp for the thread. Read shared with
	// other threads.
	union
	{
		volatile EpochTsVal ts;
		UINT8 pad_ts[EPOCH_CACHE_LINE_SIZE];
	};

	union
	{
		volatile EpochTsVal largestCollectedTs;
		UINT8 pad_ts[EPOCH_CACHE_LINE_SIZE];
	};

	active_page_table_t* active_page_table;

	// For chaining all epochs.
	// Read shared by other threads during normal execution.
	// Write shared during initialization.
	union {
		volatile EpochThreadData *next;
		UINT8 pad_next[EPOCH_CACHE_LINE_SIZE];
	};

	// The following is thread local data.

	// All generations assigned to this thread.
	EpochGeneration *allGenerations;

	// Current generation.
	EpochGeneration *current;

	// All generations that can be used when the current one is filled up.
	EpochGeneration *free;

	// The oldest generation that has been used already.
	EpochGeneration *oldestUsed;

	ULONG oldestUsedCount;

	// Epoch vector tiemstamp used while collecting data.
	EpochTimestampVector vectorTsBuf;

	// This is the head of the global list of all epoch threads,
	EpochThreadData * volatile * epochListHead;

	// epoch stats
	EpochStats stats;
};

// When current generation becomes full, change the generation.
void EpochChangeGeneration(EpochThreadData *epoch);

//---------------------------------------------------------------------
// Type definitions
//

// EpochDynamicVector.
//
template<typename T, ULONG INITIAL_CAPACITY>
inline void EpochDynamicVector<T, INITIAL_CAPACITY>::Init() {
	capacity = INITIAL_CAPACITY;
	data = (T *)EpochCacheAlignedCacheSizeAlloc(sizeof(T) * capacity);
}

template<typename T, ULONG INITIAL_CAPACITY>
inline void EpochDynamicVector<T, INITIAL_CAPACITY>::Uninit() {
	EpochFreeAligned(data);
}

template<typename T, ULONG INITIAL_CAPACITY>
inline T& EpochDynamicVector<T, INITIAL_CAPACITY>::operator[](ULONG idx) {
	if(idx < capacity) {
		return data[idx];
	}

	// if there is not enough space in the array, allocate a new one
	// with double size
	ULONG newCapacity = capacity * 2;
	T *newData = (T *)EpochCacheAlignedCacheSizeAlloc(
		sizeof(T) * newCapacity);

	// copy data to new array
	memcpy(newData, data, capacity * sizeof(T));

	// free old array
	EpochFreeAligned(data);

	// make old array current
	data = newData;
	capacity = newCapacity;

	// and now return the element in the array
	return data[idx];
}

// EpochGeneration.
//
inline void EpochGeneration::Init() {
	usedNodes = 0;
	vectorTs.Init();
	next = NULL;
}


inline void EpochGeneration::Uninit() {
	vectorTs.Uninit();
}

inline void EpochGeneration::FinalizeAll() {
	for(UINT i = 0;i < usedNodes;i++) {
		nodes[i].finalizeFun(nodes[i].ptr, nodes[i].context, nodes[i].tls);
	}

	usedNodes = 0;
}

inline void EpochGeneration::Clean() {
	usedNodes = 0;
}


// EpochThreadData.
//

inline void EpochThreadData::Init() {
	// initialize this thread's epoch appropriately
	ts = EPOCH_FIRST_EPOCH;

	largestCollectedTs = EPOCH_FIRST_EPOCH;

	// initialize all generations
	allGenerations = NULL;
	free = NULL;

	for(UINT i = 0;i < EPOCH_GENERATIONS_PER_THREAD;i++) {
		EpochGeneration *newGen =
				(EpochGeneration *)EpochCacheAlignedCacheSizeAlloc(
				sizeof(EpochGeneration));
		newGen->Init();

		// link into free list
		newGen->next = free;
		free = newGen;

		// link into list of all generations
		newGen->nextAllGenerations = allGenerations;
		allGenerations = newGen;
	}

	// take the first generation and prepare it for use
	current = free;
	free = current->next;

	// nothing has been used so far
	oldestUsed = NULL;
	oldestUsedCount = 0;

	// not linked
	next = NULL;

	// initialize the buffer
	vectorTsBuf.Init();

	// stats
	stats.Init();

	//init the page buffer
	active_page_table = create_active_page_table();
#ifdef BUFFERING_ON
	active_page_table->shared_flush_buffer = link_flush_buffer;
#endif
}

inline void EpochThreadData::Uninit() {
	// free all generations
	EpochGeneration *curr = allGenerations;
	EpochGeneration *prev;

	while(curr != NULL) {
		prev = curr;
		curr = curr->nextAllGenerations;

		prev->Uninit();
		EpochFreeAligned(prev);
	}

	// uninit the used vector buffer
	vectorTsBuf.Uninit();
#ifndef ESTIMATE_RECOVERY
	destroy_active_page_table(active_page_table);
#endif
}

// This is not thread safe and is used during shutdown.
inline void EpochThreadData::UnsafeFinalizeAll() {
	EpochGeneration *curr = allGenerations;

	while(curr != NULL) {
		curr->FinalizeAll();
		curr = curr->nextAllGenerations;
	}
}

inline bool EpochIsStarted(EpochThreadData *epoch) {
	return (epoch->ts % 2) == 1;
}

//---------------------------------------------------------------------
// Interface implementation.
//

// We just increment the epoch timestamp to start and end the epochs.
// AD: we could also perform some checks here if we want to be more
//     aggresive about reclaiming memory (and are ready to pay higher
//     price for doing so).
inline void EpochStart(EpochThread opaqueEpoch) {
	EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;

	assert(!EpochIsStarted(epoch));

	// ImpliedAcquireBarrier();
	epoch->ts++;
	// ImpliedReleaseBarrier();
}

inline void EpochEnd(EpochThread opaqueEpoch) {
	EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;

	assert(EpochIsStarted(epoch));

	// ImpliedAcquireBarrier();
	epoch->ts++;
	// ImpliedReleaseBarrier();
}

inline void EpochEndIfStarted(EpochThread opaqueEpoch) {
	EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;

	if(EpochIsStarted(epoch)) {
		epoch->ts++;
	}
}

// pass a pointer to the epoch system to remove when safe
inline void EpochReclaimObject(
		EpochThread opaqueEpoch,
		void *ptr,
		void *context,
		void *tls,
		EpochFinalizeFun finalizeFun) {
	EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;

	// add node to current generation
	ULONG usedNodes = epoch->current->usedNodes;
	EpochNode *node = epoch->current->nodes + usedNodes;
	node->ptr = ptr;
	node->context = context;
	node->tls = tls;
	node->finalizeFun = finalizeFun;
#ifdef SIMULATE_NAIVE_IMPLEMENTATION
	write_data_wait(ptr, 1);
#endif
	usedNodes++;
	//fprintf(stderr, "used nodes is %u\n", usedNodes);
	epoch->current->usedNodes = usedNodes;

	epoch->stats.Increment(EpochStatsEnum::DEALLOCATION_COUNT);

	// if current is full, then we need to process generation change
	if(usedNodes == EPOCH_NODES_IN_GENERATION) {
		EpochChangeGeneration(epoch);
	}
}

inline void* EpochAllocNode(EpochThread opaqueEpoch, size_t size) {
	EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;
#ifdef SIMULATE_NAIVE_IMPLEMENTATION
	write_data_wait(NULL, 1);
#else
	mark_page(epoch->active_page_table, NULL, size, epoch->ts, epoch->largestCollectedTs, 0);
#endif
	return AllocNode(size);
}

inline void EpochDeclareUnlinkNode(EpochThread opaqueEpoch, void * ptr, size_t size) {
	EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;
#ifdef SIMULATE_NAIVE_IMPLEMENTATION
	write_data_wait(ptr, 1);
#else
	mark_page(epoch->active_page_table, ptr, size, epoch->ts, epoch->largestCollectedTs, 1);
#endif
}

inline void EpochFreeNode( void* ptr) {
	//EpochThreadData *epoch = (EpochThreadData *)opaqueEpoch;
	//mark_page(epoch->active_page_table, size);
	FreeNode(ptr);
}

#endif
