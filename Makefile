.PHONY:all clean link-cache_test

SRC = src
INCLUDE = include
BENCH = benchmarks
PROF = prof

CC ?= gcc

CFLAGS = -O3 -Wall
LDFLAGS = -lm -lrt -lpthread
VER_FLAGS = -D_GNU_SOURCE

MEASUREMENTS = 0

ifndef PC_NAME
	PC_NAME = $(shell uname -n)
endif

ifeq ($(PC_NAME), lpdquad)
    CFLAGS += -DTSX_ENABLED
    CFLAGS += -mrtm
endif

ifeq ($(PC_NAME), lpdpc34)
    CFLAGS += -DTSX_ENABLED
    CFLAGS += -mrtm
endif

ifeq ($(VERSION),DEBUG) 
CFLAGS = -O0 -ggdb -Wall -g -fno-inline
VER_FLAGS += -DDEBUG
endif

ifeq ($(PROFILE),1) 
CFLAGS += -DDO_PROFILE
endif

ifeq ($(TSX),1) 
CFLAGS += -DTSX_ENABLED
endif

UNAME := $(shell uname -n)

all: link-cache_test

default: link-cache_test

ifeq ($(MEASUREMENTS),1)
VER_FLAGS += -DDO_PROFILE
#MEASUREMENTS_FILES += measurements.o
endif

link-cache.o: $(SRC)/link-cache.c $(INCLUDE)/link-cache.h $(INCLUDE)/nv_memory.h $(INCLUDE)/utils.h
	$(CC) $(VER_FLAGS) -c $(SRC)/link-cache.c $(CFLAGS) -I./$(INCLUDE)

active-page-table.o: $(SRC)/active-page-table.cpp $(INCLUDE)/link-cache.h $(INCLUDE)/nv_memory.h $(INCLUDE)/utils.h $(INCLUDE)/active-page-table.h $(INCLUDE)/epoch_common.h
	$(CC) $(VER_FLAGS) -c $(SRC)/active-page-table.cpp $(CFLAGS) -I./$(INCLUDE) -I${NVML_PATH}/include -L${NVML_PATH}/lib -I${JEMALLOC_PATH}/include -L${JEMALLOC_PATH}/lib -Wl,-rpath,${JEMALLOC_PATH}/lib -ljemalloc -lpmemobj -lpmem


epoch.o: $(SRC)/epoch.cpp $(INCLUDE)/link-cache.h $(INCLUDE)/nv_memory.h $(INCLUDE)/utils.h $(INCLUDE)/active-page-table.h $(INCLUDE)/epoch_common.h $(INCLUDE)/epoch.h $(INCLUDE)/epochstats.h  $(INCLUDE)/epoch_impl.h
	$(CC) $(VER_FLAGS) -c $(SRC)/epoch.cpp $(CFLAGS) -I./$(INCLUDE) -I${NVML_PATH}/include -L${NVML_PATH}/lib -I${JEMALLOC_PATH}/include -L${JEMALLOC_PATH}/lib -Wl,-rpath,${JEMALLOC_PATH}/lib -ljemalloc -lpmemobj -lpmem

link-cache_test: link-cache.o $(SRC)/link-cache_test.c $(INCLUDE)/random.h
	$(CC) $(VER_FLAGS) $(SRC)/link-cache_test.c link-cache.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ -o link-cache_test

#epoch.o: $(SRC)/active-page-table.cpp $(SRC)/epoch.cpp $(SRC)/ $(INCLUDE)/active-page-table.h $(INCLUDE)/nv_memory.h $(INCLUDE)/utils.h $(INCLUDE)/epoch.h $(INCLUDE)/epochalloc.h $(INCLUDE)/epoch_impl.h $(INCLUDE)/epoch_pmem.h
	#$(CC) $(VER_FLAGS) -c $(SRC)/link-cache.c $(CFLAGS) -I./$(INCLUDE)

clean:
	rm -f *.o *.a link-cache_test
