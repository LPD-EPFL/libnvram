.PHONY:all clean link-cache_test

SRC = src
INCLUDE = include
BENCH = benchmarks
PROF = prof

CC = g++

CFLAGS = -O3 -Wall
LDFLAGS = -lm -lrt -lpthread
VER_FLAGS = -D_GNU_SOURCE

MEASUREMENTS = 0

ifndef PC_NAME
	PC_NAME = $(shell uname -n)
endif

ifndef DESTDIR
	DESTDIR = $(HOME)/local
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

all: link-cache_test libnvram.a

default: link-cache_test libnvram.a

ifeq ($(MEASUREMENTS),1)
VER_FLAGS += -DDO_PROFILE
endif

link-cache.o: $(SRC)/link-cache.c $(INCLUDE)/link-cache.h $(INCLUDE)/nv_memory.h $(INCLUDE)/nv_utils.h
	$(CC) $(VER_FLAGS) -c $(SRC)/link-cache.c $(CFLAGS) -I./$(INCLUDE)

active-page-table.o: $(SRC)/active-page-table.cpp $(INCLUDE)/link-cache.h $(INCLUDE)/nv_memory.h $(INCLUDE)/nv_utils.h $(INCLUDE)/active-page-table.h $(INCLUDE)/epoch_common.h
	$(CC) $(VER_FLAGS) -c $(SRC)/active-page-table.cpp $(CFLAGS) -I./$(INCLUDE) -I${NVML_PATH}/include -I${JEMALLOC_PATH}/include 

epoch.o: $(SRC)/epoch.cpp $(INCLUDE)/link-cache.h $(INCLUDE)/nv_memory.h $(INCLUDE)/nv_utils.h $(INCLUDE)/active-page-table.h $(INCLUDE)/epoch_common.h $(INCLUDE)/epoch.h $(INCLUDE)/epochstats.h 
	$(CC) $(VER_FLAGS) -c $(SRC)/epoch.cpp $(CFLAGS) -I./$(INCLUDE) -I${NVML_PATH}/include -I${JEMALLOC_PATH}/include 

link-cache_test: link-cache.o $(SRC)/link-cache_test.c $(INCLUDE)/random.h
	$(CC) $(VER_FLAGS) $(SRC)/link-cache_test.c link-cache.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ -o link-cache_test

libnvram.a: link-cache.o active-page-table.o epoch.o $(INCLUDE)/link-cache.h $(INCLUDE)/nv_memory.h $(INCLUDE)/nv_utils.h $(INCLUDE)/active-page-table.h $(INCLUDE)/epoch_common.h $(INCLUDE)/epoch.h $(INCLUDE)/epochstats.h 
	@echo Archive name = libnvram.a
	ar -r libnvram.a link-cache.o active-page-table.o epoch.o
	rm -f *.o

libnvram_test.o: $(SRC)/libnvram_test.c libnvram.a
	$(CC) $(VER_FLAGS) -c $(SRC)/libnvram_test.c $(CFLAGS) -I./$(INCLUDE) -I${NVML_PATH}/include -L${NVML_PATH}/lib -I${JEMALLOC_PATH}/include

libnvram_test: libnvram.a libnvram_test.o
	$(CC) $(VER_FLAGS) -o libnvram_test libnvram_test.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ -I${NVML_PATH}/include -L${NVML_PATH}/lib -I${JEMALLOC_PATH}/include -L${JEMALLOC_PATH}/lib -Wl,-rpath,${JEMALLOC_PATH}/lib -ljemalloc -lpmemobj -lpmem -lnvram

clean:
	rm -f *.o *.a link-cache_test

install: libnvram.a
	cp libnvram.a $(DESTDIR)/lib
	mkdir -p $(DESTDIR)/include/libnvram
	cp $(INCLUDE)/*.h $(DESTDIR)/include/libnvram
