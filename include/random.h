#ifndef _H_RANDOM_
#define _H_RANDOM_

#include <malloc.h>
#include <stdio.h>
#include <math.h>

#define LOCAL_RAND

#if defined(LOCAL_RAND)
extern __thread unsigned long* seeds; 
#endif


#define mrand(x) xorshf96(&x[0], &x[1], &x[2])
#define my_random xorshf96

//fast but weak random number generator for the sparc machine
static inline uint32_t
fast_rand() 
{
  return ((nv_getticks()&4294967295UL)>>4);
}


static inline unsigned long* 
seed_rand() 
{
  unsigned long* seeds;
  seeds = (unsigned long*) memalign(64, 64);
  seeds[0] = nv_getticks() % 123456789;
  seeds[1] = nv_getticks() % 362436069;
  seeds[2] = nv_getticks() % 521288629;
  return seeds;
}

//Marsaglia's xorshf generator
static inline unsigned long
xorshf96(unsigned long* x, unsigned long* y, unsigned long* z)  //period 2^96-1
{
  unsigned long t;
  (*x) ^= (*x) << 16;
  (*x) ^= (*x) >> 5;
  (*x) ^= (*x) << 1;

  t = *x;
  (*x) = *y;
  (*y) = *z;
  (*z) = t ^ (*x) ^ (*y);

  return *z;
}

static inline long
rand_range(long r) 
{
#if defined(LOCAL_RAND)
  long v = xorshf96(seeds, seeds + 1, seeds + 2) % r;
  v++;
#else
  int m = RAND_MAX;
  long d, v = 0;
	
  do {
    d = (m > r ? r : m);
    v += 1 + (long)(d * ((double)rand()/((double)(m)+1.0)));
    r -= m;
  } while (r > 0);
#endif
  return v;
}

/* Re-entrant version of rand_range(r) */
static inline long
rand_range_re(unsigned int *seed, long r) 
{
#if defined(LOCAL_RAND)
  long v = xorshf96(seeds, seeds + 1, seeds + 2) % r;
  v++;
#else
  int m = RAND_MAX;
  long d, v = 0;
	
  do {
    d = (m > r ? r : m);		
    v += 1 + (long)(d * ((double)rand_r(seed)/((double)(m)+1.0)));
    r -= m;
  } while (r > 0);
#endif
  return v;
}


#endif
