#ifndef _EPOCH_COMMON_H_
#define _EPOCH_COMMON_H_

#include "utils.h"

// Use 64 bit timestamps.
// This could lead to high space requirements just for storing the
// timestamps, as we will need one of these for each thread in the
// system in each bucket. With 512 threads, we would end up using
// 4kB just for storing epoch timestamps in each bucket.
typedef UINT64 EpochTsVal;


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

#endif
