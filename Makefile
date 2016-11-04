SRC = src
INCLUDE = include
BENCH = benchmarks
PROF = prof

CC ?= gcc

CFLAGS = -O3 -Wall
LDFLAGS = -lm -lrt -lpthread -lnvmem
VER_FLAGS = -D_GNU_SOURCE

MEASUREMENTS = 0

ifeq ($(VERSION),DEBUG) 
CFLAGS = -O0 -ggdb -Wall -g -fno-inline
VER_FLAGS += -DDEBUG
endif

UNAME := $(shell uname -n)

all: libnvmem.a nvmem_test

default: nvmem_test

nvmem.o: $(SRC)/nvmem.c 
	$(CC) $(VER_FLAGS) -c $(SRC)/nvmem.c $(CFLAGS) -I./$(INCLUDE)

ifeq ($(MEASUREMENTS),1)
VER_FLAGS += -DDO_TIMINGS
MEASUREMENTS_FILES += measurements.o
endif

libnvmem.a: nvmem.o $(INCLUDE)/nvmem.h $(MEASUREMENTS_FILES)
	@echo Archive name = libnvmem.a
	ar -r libnvmem.a nvmem.o $(MEASUREMENTS_FILES)
	rm -f *.o	

nvmem_test.o: $(SRC)/nvmem_test.c libnvmem.a
	$(CC) $(VER_FLAGS) -c $(SRC)/nvmem_test.c $(CFLAGS) -I./$(INCLUDE)

nvmem_test: libnvmem.a nvmem_test.o
	$(CC) $(VER_FLAGS) -o nvmem_test nvmem_test.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ 


clean:
	rm -f *.o *.a nvmem_test
