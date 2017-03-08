#ifndef EPOCH_IMPL_H_
#define EPOCH_IMPL_H_

#define BUFFERING_ON 1
//#define SIMULATE_NAIVE_IMPLEMENTATION 1
#define ESTIMATE_RECOVERY 1

#include <intrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <cassert>

#include <Interface\SmpHeap.hpp>
#include <Interface\NVSmpHeap.hpp>

#include <nv-memory\nv_memory.h>

#include <nv-memory\FlushBuffer_one_cl.h>

#include "epochcommon.h"
#include "epochalloc.h"

#include "epochstats.h"
#include "epoch.h"

#include "active-page-table.h"

#endif
