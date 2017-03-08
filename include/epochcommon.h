
#pragma once

// size of cache line in bytes
#define EPOCH_CACHE_LINE_SIZE 64

#if defined(_MSC_VER)
#define EPOCH_CACHE_ALIGNED __declspec(align(EPOCH_CACHE_LINE_SIZE))
#else
#if defined(__GNUC__)
#define EPOCH_CACHE_ALIGNED __attribute__ ((aligned(EPOCH_CACHE_LINE_SIZE)))
#endif
#endif
//see http://stackoverflow.com/a/12654801
