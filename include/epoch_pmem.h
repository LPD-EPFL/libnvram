#pragma once
// opaque type to store epoch data
typedef void *EpochThread;

// cleanup function for the pointers
typedef void(*EpochFinalizeFun)(void *object, void *context, void *tls);

// initialize and cleanup epoch system
void EpochGlobalInit();
void EpochGlobalShutdown();

// print all stats
void EpochPrintStats();

// initialize and cleanup epoch-related thread data
EpochThread EpochThreadInit();
void EpochThreadShutdown(EpochThread epoch);

void EpochUnsafeFinalizeAll(EpochThread epoch);

// start and end epochs
void EpochStart(EpochThread epoch);
void EpochEnd(EpochThread epoch);
void EpochEndIfStarted(EpochThread epoch);

// pass a pointer to the epoch system to remove when safe
void EpochReclaimObject(
	EpochThread opaqueEpoch,
	void *ptr,
	void *context,
	void *tls,
	EpochFinalizeFun finalizeFun);

// get information about the number of objects waiting to be reclaimed
// in the current thread
ULONG EpochGetGarbageCount(EpochThread epoch);

// functions to force memory cleanup
void EpochFlush(EpochThread opaqueEpoch);
void EpochScan(EpochThread opaqueEpoch);