#ifndef CALC_DURATION_H_
#define CALC_DURATION_H_

#include <windows.h>

struct calc_duration {
	struct item {
		uint64_t *duration, *count;
		LARGE_INTEGER c1;
		void toc() {
			if (!duration || !count) return;
			LARGE_INTEGER c2;
			::QueryPerformanceCounter(&c2);
			*duration += c2.QuadPart - c1.QuadPart;
			++(*count);
			duration = 0;
		}
		~item() {
			toc();
		}
	};
	static item tic(uint64_t *duration, uint64_t *count, int idx, DWORD thread_id) {
		item itm = { 0 };
		if (thread_id != 0 && thread_id != ::GetCurrentThreadId()) return itm;
		itm.duration = duration + idx;
		itm.count = count + idx;
		::QueryPerformanceCounter(&itm.c1);
		return itm;
	}
};

#endif // CALC_DURATION_H_
