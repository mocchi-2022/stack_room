/*
 * Polygon_RayTrace
 * Copylight (C) 2013 mocchi
 * mocchi_2003@yahoo.co.jp
 * License: Boost ver.1
 */

#include "MonteCarlo.h"

#include <cstdlib>

double CalcHaltonSequence(int64_t index, int base){
	double result = 0, f = 1.0;
	while(index > 0){
		f /= static_cast<double>(base);
		div_t dv = std::div(static_cast<int32_t>(index), base);
		result += f * dv.rem;
		index = dv.quot;
	}
	return result;
}

