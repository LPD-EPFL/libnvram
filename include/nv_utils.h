#ifndef _UTILS_H_
#define _UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#define CACHE_ALIGNED __attribute__ ((aligned(CACHE_LINE_SIZE)))


#ifndef EPOCH_CACHE_LINE_SIZE
#define EPOCH_CACHE_LINE_SIZE 64
#endif

#define EPOCH_CACHE_ALIGNED __attribute__ ((aligned(EPOCH_CACHE_LINE_SIZE)))

#if !defined(UNUSED)
#define UNUSED __attribute__ ((unused))
#endif

#ifdef __SSE__
#include <xmmintrin.h>
#endif

#include <stdint.h>

typedef uint8_t BYTE;
typedef uint8_t UINT8;
typedef unsigned long ULONG;
typedef uint16_t UINT16;
typedef unsigned int UINT;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef uint64_t ULONG64;;
typedef void* PVOID;
typedef uintptr_t UINT_PTR;

//#define PVOID void*
//#define UINT_PTR uintptr_t

#define CAS_U16(a,b,c) __sync_val_compare_and_swap(a,b,c)
#define CAS_U32(a,b,c) __sync_val_compare_and_swap(a,b,c)
#define CAS_U64(a,b,c) __sync_val_compare_and_swap(a,b,c)
#define CAS_PTR(a,b,c) __sync_val_compare_and_swap(a,b,c)

typedef uint64_t ticks;

#if defined(__i386__)
static inline ticks 
nv_getticks(void) 
{
  ticks ret;

  __asm__ __volatile__("rdtsc" : "=A" (ret));
  return ret;
}
#elif defined(__x86_64__)
static inline ticks
nv_getticks(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#elif defined(__sparc__)
static inline ticks 
nv_getticks()
{
  ticks ret = 0;
  __asm__ __volatile__ ("rd %%tick, %0" : "=r" (ret) : "0" (ret)); 
  return ret;
}
#elif defined(__tile__)
#  include <arch/cycle.h>
static inline ticks
nv_getticks()
{
  return get_cycle_count();
}
#endif

#ifdef __cplusplus
}
#endif

#endif
