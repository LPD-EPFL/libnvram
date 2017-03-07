.PHONY: all

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

UNAME := $(shell uname -n)

all: link-cache_test

default: link-cache_test

ifeq ($(MEASUREMENTS),1)
VER_FLAGS += -DDO_PROFILE
#MEASUREMENTS_FILES += measurements.o
endif

link-cache.o: $(SRC)/link-cache.c
	$(CC) $(VER_FLAGS) -c $(SRC)/link-cache.c $(CFLAGS) -I./$(INCLUDE)

link-cache_test: link-cache.o $(SRC)/link-cache_test.c
	$(CC) $(VER_FLAGS) $(SRC)/link-cache_test.c link-cache.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ -o link-cache_test


clean:
	rm -f *.o *.a link-cache_test
