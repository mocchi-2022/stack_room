#if 1
#include "simdxorshift128plus.h"

struct xorshift_rnd_32bit {
	avx_xorshift128plus_key_t key;
	int cnt;
	__m256i rnd;
	void init(uint64_t key1, uint64_t key2) {
		avx_xorshift128plus_init(key1, key2, &key);
		cnt = 8;
	}
	double operator()() {
		if (cnt == 8) cnt = 0, rnd = avx_xorshift128plus(&key);
		return static_cast<double>(rnd.m256i_u32[cnt++]) / (4294967295.0);
	}
};
#else
#include "xorshift128plus.h"

struct xorshift_rnd_32bit {
	xorshift128plus_key_t key;
	int cnt;
	uint64_t rnd;
	void init(uint64_t key1, uint64_t key2) {
		xorshift128plus_init(key1, key2, &key);
		cnt = 2;
	}
	double operator()() {
		if (cnt == 2) {
			cnt = 1;
			rnd = xorshift128plus(&key);
			return (rnd & 4294967295) / (4294967295.0);
		} else {
			cnt = 2;
			return ((rnd >> 32) & 4294967295) / (4294967295.0);
		}
	}
};
#endif