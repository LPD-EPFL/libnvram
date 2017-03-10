
#pragma once

struct EpochStatsEnum {
	enum Key {
		NEW_GENERATIONS_ADDED = 0,
		COLLECT_COUNT,
		COLLECT_COUNT_SUCCESS,
		COLLECT_COUNT_FAIL,
		DEALLOCATION_COUNT,
		STATS_COUNT
	};

	static const char *Names[STATS_COUNT];
};

struct EpochStats {
	void Init();

	void Increment(EpochStatsEnum::Key key, UINT64 inc = 1);

	void Print(ULONG indent);

	static void Accumulate(EpochStats *result, EpochStats *acc);

	UINT64 stats[EpochStatsEnum::STATS_COUNT];
};


inline void EpochStats::Init() {
	memset(stats, 0, sizeof(stats));
}

inline static void EpochStats::Increment(
		EpochStatsEnum::Key key,
		UINT64 inc) {
#ifdef EPOCH_KEEP_STATS
	stats[key] += inc;
#endif /* EPOCH_KEEP_STATS */
}

static const char *EPOCH_TAB_STRING = "    ";

inline static void EpochPrintIndent(ULONG indent) {
	for(ULONG i = 0;i < indent;i++) {
		printf("%s", EPOCH_TAB_STRING);
	}
}

inline void EpochStats::Print(ULONG indent) {
	for(ULONG idx = 0;idx < EpochStatsEnum::STATS_COUNT;idx++) {
		if(stats[idx] != 0) {
			EpochPrintIndent(indent);
			printf("%s: %llu\n", EpochStatsEnum::Names[idx], stats[idx]);
		}
	}
}

inline void EpochStats::Accumulate(EpochStats *result, EpochStats *acc) {
	for(ULONG idx = 0;idx < EpochStatsEnum::STATS_COUNT;idx++) {
		result->stats[idx] += acc->stats[idx];
	}
}
