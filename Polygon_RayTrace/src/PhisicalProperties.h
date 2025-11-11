/*
 * Polygon_RayTrace
 * Copylight (C) 2023 mocchi
 * mocchi_2003@yahoo.co.jp
 * License: Boost ver.1
 */

#ifndef PHISICAL_PROPERTIES_H_
#define PHISICAL_PROPERTIES_H_

#include "opennurbs.h"
#include "nlohmann/json.hpp"
 
struct xorshift_rnd_32bit;

struct LightSources{
	struct Impl;
	friend struct Impl;
	Impl *pimpl;
	LightSources(nlohmann::json &lsv);
	~LightSources();
	int LSCount() const;               ///< 光源の数
	int64_t RayCount() const;          ///< 全光源の合計光線数
	int64_t RayCount(int lsidx) const; ///< 各光源の光線数
	bool Get(int64_t idx, int &blockseed, int &lsidx, int64_t &rayidx, ON_3dRay &ray, double &flux, double &wavelength) const;
};

enum class FaceNormalDirectionMode {
	OUTER, INNER, AUTO
};

struct Materials{
	struct Impl;
	friend struct Impl;
	Impl *pimpl;
	Materials(nlohmann::json &mtv, nlohmann::json &shv, ON_SimpleArray<int> &shape2matidx);
	~Materials();
	int Count() const; ///< 定義された材質の数
	bool VolumeAttenuate(int midx, int power_count, double *power, double length) const;
	// 入力時の nrm の向きは任意、処理終了時に入射の反対向きにして返す。
	bool CalcBSDF(int midx, FaceNormalDirectionMode fndm, ON_3dVector &nrm, const ON_3dVector &incident_dir, bool &in_medium, xorshift_rnd_32bit &rnd, int power_count, double *power, ON_3dVector &emit_dir) const;
};

#endif // PHISICAL_PROPERTIES_H_
