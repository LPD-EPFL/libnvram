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

link-cache_test: link-cache.o $(SRC)/link-cache_test.c $(INCLUDE)/random.h
	$(CC) $(VER_FLAGS) $(SRC)/link-cache_test.c link-cache.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ -o link-cache_test


clean:
	rm -f *.o *.a link-cache_test
