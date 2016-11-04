
#pragma once

#ifdef _WIN32

#include <immintrin.h>
#include <Windows.h>

#define CAS_U16(a,b,c) InterlockedCompareExchange16(a,c,b)
#define CAS_U32(a,b,c) InterlockedCompareExchange32(a,c,b)
#define CAS_U64(a,b,c) InterlockedCompareExchange64(a,c,b)
#define CAS_PTR(a,b,c) InterlockedCompareExchangePointer(a,c,b)

#else 
#ifdef(__SSE__)
#include <xmmintrin.h>
#endif

typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;

#define void* PVOID
#define uintptr_t UINT_PTR

#define CAS_U16(a,b,c) __sync_val_compare_and_swap(a,b,c)
#define CAS_U32(a,b,c) __sync_val_compare_and_swap(a,b,c)
#define CAS_U64(a,b,c) __sync_val_compare_and_swap(a,b,c)
#define CAS_PTR(a,b,c) __sync_val_compare_and_swap(a,b,c)

#endif


