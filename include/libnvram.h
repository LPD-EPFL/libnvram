#ifndef EPOCH_IMPL_H_
#define EPOCH_IMPL_H_

#define BUFFERING_ON 1
//#define SIMULATE_NAIVE_IMPLEMENTATION 1
#define ESTIMATE_RECOVERY 1

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
//#include <intrin.h>
#include <cassert>
#include <sys/stat.h>
#include <sys/types.h>
#include <stddef.h>
#include <libpmem.h>
#include <libpmemobj.h>

#include "utils.h"
#include "nv_memory.h"
#include "link-cache.h"
#include "epoch_common.h"
#include "active-page-table.h"
#include "epochalloc.h"
#include "epochstats.h"
#include "epoch.h"

#endif
