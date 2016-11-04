#pragma once

#include <Windows.h>

#define SIMULATE_LATENCIES 1

#define WAIT_WRITES_DELAY 370
#define WRITE_DATA_WAIT_DELAY 370
#define WRITE_DATA_NOWAIT_DELAY 9


/* contains functions for working with non-volatile memory */

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#define CACHE_ALIGNED __declspec(align(CACHE_LINE_SIZE)) 
// TODO add operations to do moves from volatile memory to persistent memeory addresses using non-temporal stores
// for examples, see https://github.com/pmem/nvml/blob/master/src/libpmem/pmem.c



inline size_t num_cache_lines(size_t size_bytes) {
	if (size_bytes == 0) return 0;
	return (((size_bytes - 1)/ CACHE_LINE_SIZE) + 1);
}

#ifdef NEW_INTEL_INSTRUCTIONS

#define _mm_clflushopt(addr) asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)addr));
#define _mm_clwb(addr) asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)addr));

inline void wait_writes() {
	_mm_sfence();
}

inline void write_data_nowait(void* addr, size_t sz) {
	UINT_PTR p;

	for (p = (UINT_PTR)adddr & ~(CACHE_LINE_SIZE - 1; p < (UINT_PTR)addr + sz; p += CACHE_LINE_SIZE) {
		_mm_clwb((void*)p);
	}
}

inline void write_data_wait(void* addr, size_t sz) {
	UINT_PTR p;

		for (p = (UINT_PTR)adddr & ~(CACHE_LINE_SIZE - 1; p < (UINT_PTR)addr + sz; p += CACHE_LINE_SIZE) {
			_mm_clwb((void*)p);
		}
	_mm_sfence();
}

#else

#define _mm_clflushopt(addr) _mm_clflush(addr)
#define _mm_clwb(addr) _mm_clflush(addr)


inline void wait_writes() {
#ifdef SIMULATE_LATENCIES
	ULONG64 startCycles = __rdtsc();
	ULONG64 endCycles = startCycles + WAIT_WRITES_DELAY;
	ULONG64 cycles = startCycles;

	while (cycles < endCycles) {
		cycles = __rdtsc();
	}
	_mm_sfence();
#else
	//_mm_sfence();
#endif
}

//size is in terms of number of cache lines
inline void write_data_wait(void* addr, size_t sz) {
#ifdef SIMULATE_LATENCIES
	ULONG64 startCycles = __rdtsc();
	ULONG64 endCycles = startCycles + WRITE_DATA_WAIT_DELAY;
	ULONG64 cycles = startCycles;

	while (cycles < endCycles) {
		cycles = __rdtsc();
	}
	_mm_sfence();
#else
	UINT_PTR p;

	for (p = (UINT_PTR)addr & ~(CACHE_LINE_SIZE - 1); p < (UINT_PTR)addr + sz; p += CACHE_LINE_SIZE) {
		_mm_clflush((void*)p);
	}
#endif
}

inline void write_data_nowait(void* addr, size_t sz) {
#ifdef SIMULATE_LATENCIES
	//this might just take a few cycles if we are not waiting for the op to complete
	//just reading rdtsc may take a few tens of cycles
	//so perhaps better to just do a pause
	if (WRITE_DATA_NOWAIT_DELAY < 10) {
		_mm_pause();
	}
	else {
		ULONG64 startCycles = __rdtsc();
		ULONG64 endCycles = startCycles + WRITE_DATA_NOWAIT_DELAY;
		ULONG64 cycles = startCycles;

		while (cycles < endCycles) {
			cycles = __rdtsc();
		}
	}
#else
	write_data_wait(addr, sz);
#endif
}

#endif