/*
 * Polygon_RayTrace
 * Copylight (C) 2023 mocchi
 * mocchi_2003@yahoo.co.jp
 * License: Boost ver.1
 */

#include "PhisicalProperties.h"

#include <map>
#include <set>
#include <algorithm>
#include <vector>

#include "opennurbs.h"
#include "randomizer.h"
#include "mist/quaternion.h"

#include "MonteCarlo.h"
#include "calc_duration.h"

static float get_ieee754(uint8_t p[4]){
	return *reinterpret_cast<float *>(p);
}

/// ========== LightSources ==========
// 光源データ
struct LightSourceImpl{
	bool valid;
	virtual ~LightSourceImpl(){
	}
	virtual int64_t Count() const = 0;
	virtual bool Get(int64_t idx, int &blockseed, ON_3dRay &ray, double &flux, double &wavelength) const = 0;
};

struct LightSources::Impl{
	int64_t count;
	int cur_ls;
	ON_SimpleArray<int64_t> accum_count;
	ON_SimpleArray<LightSourceImpl *> impls;
	virtual ~Impl(){
		for (int i = 0; i < impls.Count(); ++i) delete impls[i];
		impls.Destroy();
	}
};

struct ParallelRayImpl : public LightSourceImpl{
	int64_t count;
	double wavelength, total_flux;

	enum Shape{
		Circle
	}shape;
	ON_Plane pln;
	double radius;
	ParallelRayImpl(nlohmann::json &lsv){
		valid = false;
		auto &jorig = lsv["origin"];
		auto &jdir = lsv["direction"];
		if (!jorig.is_array() || jorig.size() != 3 || !jdir.is_array() || jdir.size() != 3) return;
		for (size_t j = 0; j < 3; ++j){
			
			if (!jorig[j].is_number() || !jdir[j].is_number()) return;
		}

		pln.CreateFromNormal(
			ON_3dPoint(jorig[0], jorig[1], jorig[2]),
			ON_3dVector(jdir[0], jdir[1], jdir[2])
		);

		count = lsv["num_ray"];
		if (count < 0) return;

		wavelength = lsv["wavelength"];
		total_flux = lsv["total_flux"];
		auto &jshape = lsv["shape"];
		radius = 0;
		if (jshape.is_object()){
			// とりあえず円決め打ち
			radius = jshape["radius"];
		}
		if (radius <= 0){
			valid = false; return;
		}
		shape = Circle;

		valid = true;
	}
	int64_t Count() const{
		return count;
	}
	bool Get(int64_t idx, int &blockseed, ON_3dRay &ray, double &flux, double &wavelength) const{
		double x, y;
		do{
			x = (CalcHaltonSequence(10 + blockseed, 2) - 0.5) * 2.0;
			y = (CalcHaltonSequence(10 + blockseed, 3) - 0.5) * 2.0;
			++blockseed;
		}while(x * x + y * y > 1.0);
		x *= radius, y *= radius;
		ray.m_P = pln.PointAt(x, y);
		ray.m_V = pln.zaxis;

		flux = total_flux / static_cast<double>(count);
		wavelength = this->wavelength;

		return true;
	}
};

struct OsramRayImpl : public LightSourceImpl{
	int64_t count, count_infile;
	double wavelength, total_flux;
	struct Item{
		ON_3dRay ray;
		double flux, wavelength;
	};
	ON_ClassArray<Item> raies;
	ON_Plane axis;
	OsramRayImpl(nlohmann::json &lsv) {
		valid = false;

		// 原点座標の取得
		auto &jorig = lsv["origin"];
		if (!jorig.is_array() || jorig.size() != 3) return;
		ON_3dPoint origin(jorig[0], jorig[1], jorig[2]);

		// 方向の取得
		// "x_dir", "y_dir", "z_dir" のうち、2つの方向を指定していること
		auto &jxdir = lsv["x_dir"];
		auto &jydir = lsv["y_dir"];
		auto &jzdir = lsv["z_dir"];

		int dir_cnt = 0;
		int dir_exist = 0;
		if (jxdir.is_array() && jxdir.size() == 3) ++dir_cnt, dir_exist |= 1;
		if (jydir.is_array() && jydir.size() == 3) ++dir_cnt, dir_exist |= 2;
		if (jzdir.is_array() && jzdir.size() == 3) ++dir_cnt, dir_exist |= 4;
		if (dir_cnt != 2) return;

		// optical の方向のベクトルを指定していること
		auto &jopt = lsv["optical"];
		const char opt = (jopt.is_string() && jopt.size() > 0) ? jopt.get<std::string>()[0] : '\0';
		int dir_opt = 0;
		if ((opt == 'x' && (dir_opt = 1)) && (dir_exist & 1) == 0) return;
		if ((opt == 'y' && (dir_opt = 2)) && (dir_exist & 2) == 0) return;
		if ((opt == 'z' && (dir_opt = 4)) && (dir_exist & 4) == 0) return;
		if (dir_opt == 0) return;

		ON_3dVector xdir, ydir, zdir;
		if (dir_exist & 1) xdir.Set(jxdir[0], jxdir[1], jxdir[2]);
		if (dir_exist & 2) ydir.Set(jydir[0], jydir[1], jydir[2]);
		if (dir_exist & 4) zdir.Set(jzdir[0], jzdir[1], jzdir[2]);

		// 残りの方向の計算、および opticalではない方向を垂直にする計算
		if (dir_opt == 1){
			if (dir_exist & 2){
				zdir = ON_CrossProduct(xdir, ydir);
				ydir = ON_CrossProduct(zdir, xdir);
			}else if (dir_exist & 4){
				ydir = ON_CrossProduct(zdir, xdir);
				zdir = ON_CrossProduct(xdir, ydir);
			}
		}else if (dir_opt == 2){
			if (dir_exist & 4){
				xdir = ON_CrossProduct(ydir, zdir);
				zdir = ON_CrossProduct(xdir, ydir);
			}else if (dir_exist & 1){
				zdir = ON_CrossProduct(xdir, ydir);
				xdir = ON_CrossProduct(ydir, zdir);
			}
		}else if (dir_opt == 4){
			if (dir_exist & 1){
				ydir = ON_CrossProduct(zdir, xdir);
				xdir = ON_CrossProduct(ydir, zdir);
			}else if (dir_exist & 2){
				xdir = ON_CrossProduct(ydir, zdir);
				ydir = ON_CrossProduct(zdir, xdir);
			}
		}
		xdir.Unitize(), ydir.Unitize(), zdir.Unitize();
		axis.CreateFromFrame(origin, xdir, ydir);

		count = lsv["num_ray"];
		if (count < 0) return;

		total_flux = lsv["total_flux"];

		FILE *fp = std::fopen(lsv["path"].get<std::string>().c_str(), "rb");
		if (!fp) return;
		std::fseek(fp, 0, SEEK_END);
		size_t sz = std::ftell(fp);
		std::rewind(fp);
		count_infile = 0;
		double flux_raies = 0;
		if (sz > 320){
			count_infile = (sz - 320) / 32;
			raies.SetCapacity(count_infile);
			if (count > count_infile) count = count_infile;

			for (int64_t i = 0; i < count; ++i){
				int64_t idx = static_cast<int64_t>(
					CalcHaltonSequence(i, 9) * static_cast<double>(count_infile)
				);
				if (idx >= count_infile) idx = count_infile-1;
				std::fseek(fp, idx * 32 + 320, SEEK_SET);
				uint8_t buf[32];
				std::fread(buf, 1, 32, fp);
				Item &item = raies.AppendNew();
				item.ray.m_P = axis.PointAt(get_ieee754(buf   ), get_ieee754(buf+ 4), get_ieee754(buf+ 8));
				item.ray.m_V = axis.xaxis * get_ieee754(buf+12) + axis.yaxis * get_ieee754(buf+16) + axis.zaxis * get_ieee754(buf+20);
				item.flux = get_ieee754(buf+24);
				item.wavelength = get_ieee754(buf+28);
				flux_raies += item.flux;
			}
			// raies の flux の合計値が total_flux になるように比率を変更
			double r = total_flux / flux_raies;
			for (int i = 0; i < raies.Count(); ++i) raies[i].flux *= r;
		}
		std::fclose(fp);
		if (count_infile <= 0) return;

		valid = true;
	}
	int64_t Count() const{
		return count;
	}
	bool Get(int64_t idx, int &blockseed, ON_3dRay &ray, double &flux, double &wavelength) const{
		if (idx < 0 || idx >= raies.Count()) return false;
		ray = raies[idx].ray;
		flux = raies[idx].flux;
		wavelength = raies[idx].wavelength;
		return true;
	}
};

LightSources::LightSources(nlohmann::json &lsp){
	pimpl = new Impl();
	pimpl->count = 0;
	pimpl->cur_ls = 0;
	pimpl->accum_count.Destroy();
	pimpl->accum_count.Append(0);
	if (!lsp.is_array()) return;
	for (size_t k = 0; k < lsp.size(); ++k){
		LightSourceImpl *&cimpl = pimpl->impls.AppendNew();
		cimpl = 0;
		auto &lsi = lsp[k];
		if (!lsi.is_object()) continue;
		auto &type = lsi["type"];
		if (type == "parallel"){
			cimpl = new ParallelRayImpl(lsi);
		}else if (type == "osram"){
			cimpl = new OsramRayImpl(lsi);
		}
		if (cimpl && !cimpl->valid){
			delete cimpl; cimpl = 0;
		}
		if (cimpl) pimpl->count += cimpl->Count();
		pimpl->accum_count.Append(*pimpl->accum_count.Last() + pimpl->count);
	}
}
LightSources::~LightSources(){
	delete pimpl; pimpl = 0;
}
int LightSources::LSCount() const {
	return pimpl->impls.Count();
}
int64_t LightSources::RayCount() const{
	return pimpl->count;
}
int64_t LightSources::RayCount(int lsidx) const{
	if (lsidx < 0 || lsidx >= pimpl->impls.Count()) return 0;
	if (!pimpl->impls[lsidx]) return 0;
	return pimpl->impls[lsidx]->Count();
}

bool LightSources::Get(int64_t idx, int &blockseed, int &lsidx, int64_t &rayidx, ON_3dRay &ray, double &flux, double &wavelength) const{
	int64_t ls_start = pimpl->accum_count[pimpl->cur_ls];
	LightSourceImpl *cur_impl = 0;
	if (ls_start <= idx && idx < pimpl->accum_count[pimpl->cur_ls+1]){
		cur_impl = pimpl->impls[pimpl->cur_ls];
		lsidx = pimpl->cur_ls;
	}else{
		lsidx = static_cast<int>(
			std::upper_bound(pimpl->accum_count.First(), pimpl->accum_count.Last()+1, idx)
			- pimpl->accum_count.First()) - 1;
		if (lsidx < 0 || lsidx >= pimpl->impls.Count()){
			lsidx = -1;
			return false;
		}
		pimpl->cur_ls = lsidx;
		ls_start = pimpl->accum_count[lsidx];
	}
	rayidx = idx - ls_start;
	return pimpl->impls[lsidx] ? pimpl->impls[lsidx]->Get(idx - ls_start, blockseed, ray, flux, wavelength) : false;
}

/// ========== Materials ==========
struct tictoc_dump {
	uint64_t durations[12] = { 0 }, count[12] = { 0 };
	DWORD threadid_to_measure;
	tictoc_dump() : threadid_to_measure(0) {}
	~tictoc_dump() {
		LARGE_INTEGER freq;
		::QueryPerformanceFrequency(&freq);
		double mult = 1000.0 / static_cast<double>(freq.QuadPart);
		std::printf("piecewise_linear_function time, total, average, count\n");
		for (int i = 0; i < sizeof(durations) / sizeof(uint64_t); ++i) {
			double total = static_cast<double>(durations[i]) * mult, countd = static_cast<double>(count[i]);
			std::printf("[%d]:%f msec, %f msec, %llu times\n", i, total, total / countd, count[i]);
		}
		std::getchar();
	}
}plfd;

inline void Rotate(ON_3dVector &rot, double rad_theta, const ON_3dVector &axis) {
//	auto tictoc1 = calc_duration::tic(plfd.durations, 1, 0);
#if 1
	// MIST の vector::rotate の処理を流用
	double cs = std::cos(rad_theta), sn = std::sin(rad_theta);
	double r_cs = 1.0 - cs;
	double xx = (axis.x * axis.x * r_cs + cs) * rot.x + (axis.x * axis.y * r_cs - axis.z * sn) * rot.y + (axis.x * axis.z * r_cs + axis.y * sn) * rot.z;
	double yy = (axis.x * axis.y * r_cs + axis.z * sn) * rot.x + (axis.y * axis.y * r_cs + cs) * rot.y + (axis.y * axis.z * r_cs - axis.x * sn) * rot.z;
	double zz = (axis.x * axis.z * r_cs - axis.y * sn) * rot.x + (axis.y * axis.z * r_cs + axis.x * sn) * rot.y + (axis.z * axis.z * r_cs + cs) * rot.z;
	rot.x = xx, rot.y = yy, rot.z = zz;
#elif 0
	rot.Rotate(rad_theta, axis);
#else
	mist::quaternion<double> q(mist::vector3<double>(axis.x, axis.y, axis.z), rad_theta * ON_RADIANS_TO_DEGREES);
	auto rotted = q.rotate(mist::vector3<double>(rot.x, rot.y, rot.z));
	rot.x = rotted.x, rot.y = rotted.y, rot.z = rotted.z;
#endif
}

// データ数３個以上であること
void simplify_piecewise_linear_function(double *v_ary, double *p_ary, int data_count, std::set<int> &selected_indices) {
	const static double HEIGHT_ACCUM_RATIO_TOLERANCE = 0.05;
	selected_indices.clear();
	if (data_count < 3) {
		for (int i = 0; i < data_count; ++i) selected_indices.insert(i);
		return;
	}
	double max_value = -1;
	selected_indices.insert(0);
	selected_indices.insert(data_count - 1);

	// 手順2：極大・極小値となるインデックスを取得する。最大値も取得する。
	double p1 = p_ary[0], p2 = p_ary[1];
	for (int i = 2; i < data_count; ++i) {
		double p3 = p_ary[i];
		if ((p1 < p2) != (p2 < p3)) {
			if (max_value < p2) max_value = p2;
			selected_indices.insert(i - 1);
		}
		p1 = p2;
		p2 = p3;
	}

	// 手順3：区分的な累積値のずれ量が HEIGHT_ACCUM_RATIO_TOLERANCE を下回るまでインデックスを追加する。
	for (;;) {
		size_t si_cnt = selected_indices.size();
		auto iter_prev = selected_indices.begin();
		auto iter = iter_prev; ++iter;
		for (; iter != selected_indices.end(); ++iter) {
			double i1 = *iter_prev, i2 = *iter;
			double p1 = p_ary[*iter_prev], p2 = p_ary[*iter];

			// p1 と p2 の線形内挿と p_ary[i]の乖離が最大となるインデックスを取得しつつ、面積ずれ量を求める。
			double vd = v_ary[static_cast<int>(i2)] - v_ary[static_cast<int>(i1)];
			double p_dif_max = -1;
			int im;
			double p_dif_accum = 0;
			for (int i = i1 + 1; i < i2; ++i) {
				double t = static_cast<double>(i - i1) / static_cast<double>(i2 - i1);
				double p = p1 + t * (p2 - p1);
				double p_dif = std::abs(p - p_ary[i]);
				if (p_dif_max < p_dif) p_dif_max = p_dif, im = i;
				p_dif_accum += p_dif * (v_ary[i] - v_ary[static_cast<int>(i1)]);
			}
			double area_base = max_value * vd; // 面積ずれ量の基準は、最大値 x 区間の範囲
			if (p_dif_accum / area_base > HEIGHT_ACCUM_RATIO_TOLERANCE) {
				selected_indices.insert(im);
			}
			iter_prev = iter;
		}
		if (si_cnt == selected_indices.size()) break;
	}
}

struct piecewise_linear_distribution {
	ON_SimpleArray<double> v_ary, p_ary;
	ON_SimpleArray<double> p_accum_ary;
	ON_SimpleArray<int> p_accum_revlookup;

	double p_sum;
	piecewise_linear_distribution() : p_sum(0) {}
	piecewise_linear_distribution(double *v_ary_, double *p_ary_, int data_count, int lookup_count) {
		init(v_ary_, p_ary_, data_count, lookup_count);
	}
	// v_ary_, p_ary_ を新たに指定して計算用データを作成する
	void init(double *v_ary_, double *p_ary_, int data_count, int lookup_count) {
		v_ary.SetCapacity(data_count), p_ary.SetCapacity(data_count);
		v_ary.SetCount(data_count), p_ary.SetCount(data_count);
		for (int i = 0; i < data_count; ++i) {
			v_ary[i] = v_ary_[i], p_ary[i] = p_ary_[i];
		}
		init(lookup_count);
	}
	// 入力済の v_ary, p_ary を基に計算用データを作成する
	void init(int lookup_count) {
		int data_count = p_ary.Count();
		p_accum_ary.SetCapacity(data_count - 1);
		p_accum_ary.SetCount(data_count - 1);
		// 累積配列の作成
		double *v_ary_ = v_ary.First();
		double *p_ary_ = p_ary.First();
		double *p_accum_ary_ = p_accum_ary.First();
		p_sum = 0;
		for (int i = 0; i < data_count - 1; ++i) {
			double v_delta = v_ary_[i + 1] - v_ary_[i];
			double p_accum = p_sum + (p_ary_[i + 1] + p_ary_[i]) * 0.5 * v_delta;
			p_accum_ary_[i] = p_accum;
			p_sum = p_accum;
		}
		update_revlookup(lookup_count);
	}
	// 初期化済の p_accum_ary を基に、逆引きインデックス配列を再作成する。
	void update_revlookup(int lookup_count) {
		// 逆引きインデックス配列の再作成
		p_accum_revlookup.SetCapacity(lookup_count);
		p_accum_revlookup.SetCount(lookup_count);
		if (lookup_count <= 0) return;
		double *p_accum_begin = p_accum_ary.First();
		double *p_accum_end = p_accum_begin + p_accum_ary.Count();
		int *p_accum_revlookup_ = p_accum_revlookup.First();
		double *p_accum_iter = p_accum_begin;
		for (int i = 0; i < lookup_count; ++i) {
			double pa = static_cast<double>(i) * p_sum / static_cast<double>(lookup_count - 1);
			p_accum_iter = std::lower_bound(p_accum_iter, p_accum_end, pa);
			p_accum_revlookup_[i] = static_cast<int>(p_accum_iter - p_accum_begin);
		}
	}
	// 一様乱数を２回振って指定された分布に従う乱数を生成する。
	template <typename F> double operator() (F &rnd) const {
		double a_dist = rnd();
		double rv_idx_i = std::floor(a_dist * static_cast<double>(p_accum_revlookup.Count() - 1));
		double t = rnd();
		return sample_detail(a_dist, rv_idx_i, t);
	}
	// 累積分布関数の逆関数を求める。
	double sample(double p) const {
		double a_dist = p;
		double rv_idx_i;
		double t = std::modf(a_dist * static_cast<double>(p_accum_revlookup.Count() - 1), &rv_idx_i);
		return sample_detail(a_dist, rv_idx_i, t);
	}
	// v_ary のインデックス値での累積値を求める。非整数の場合は、内挿する。
	double accum_idx(double v_idx) const {
		if (v_idx <= 0) {
			return 0;
		} else if (v_idx >= v_ary.Count() - 1) {
			return *p_accum_ary.Last();
		}
		double v_idx_i_d, v_idx_r = std::modf(v_idx, &v_idx_i_d);
		int v_idx_i = static_cast<int>(v_idx_i_d);
		double p_accum1 = v_idx_i < 1 ? 0 : p_accum_ary[v_idx_i - 1];
		if (v_idx_r == 0) return p_accum1;
		double p_accum2 = p_accum_ary[v_idx_i];
		return p_accum1 + (p_accum2 - p_accum1) * v_idx_r;
	}
	// p_ary の累積値を求める。
	double accum() const {
		return p_accum_ary.Count() > 0 ? *p_accum_ary.Last() : 0;
	}
private:
	inline double sample_detail(double a_dist, double rv_idx_i, double t) const {
		double a_p = a_dist * p_sum;
		int rv_idx = static_cast<int>(rv_idx_i);
		int rv = (p_accum_revlookup.Count() > 0) ? p_accum_revlookup[rv_idx] : 0;
		while (p_accum_ary[rv] < a_p && rv < p_accum_ary.Count() - 1) ++rv;
		const double *p_ary_ = p_ary.First();
		double p0 = p_ary_[rv], p1 = p_ary_[rv + 1];
		if (std::abs(p1 - p0) > ON_ZERO_TOLERANCE) {
			t = (std::sqrt(p0 * p0 * (1.0 - t) + p1 * p1 * t) - p0) / (p1 - p0);
		}
		const double *v_ary_ = v_ary.First();
		double v0 = v_ary_[rv], v1 = v_ary_[rv + 1];
		return v0 + t * (v1 - v0);
	}
};
void simplify(piecewise_linear_distribution &pld, ON_SimpleArray<int> *selected_indices_ = nullptr) {
	if (selected_indices_) selected_indices_->Empty();
	std::set<int> selected_indices;
	simplify_piecewise_linear_function(pld.v_ary.First(), pld.p_ary.First(), pld.v_ary.Count(), selected_indices);
	int i = 0;
	for (auto iter = selected_indices.begin(); iter != selected_indices.end(); ++iter) {
		int idx = *iter;
		pld.v_ary[i] = pld.v_ary[idx], pld.p_ary[i] = pld.p_ary[idx];
		if (selected_indices_) selected_indices_->Append(i);
		++i;
	}
	int new_count = static_cast<int>(selected_indices.size());
	pld.v_ary.SetCapacity(new_count);
	pld.p_ary.SetCapacity(new_count);
	pld.init(new_count);
}


double NDF_GGX(double a2, double cos_h) {
	if (cos_h <= 0.0 || cos_h > 1.0) return 0.0;
	double c2 = cos_h * cos_h;
	double brc = 1 - (1 - a2) * c2;
	return a2 / (ON_PI * (brc*brc));
}

// https://qiita.com/_Pheema_/items/f1ffb2e38cc766e6e668
double Masking_Smith_GGX(double a2, double cos_i_n, double cos_e_n) {
	double lmd_i = (-1 + std::sqrt(1 + a2 * (1 / (cos_i_n*cos_i_n) - 1))) / 2.0;
	double lmd_e = (-1 + std::sqrt(1 + a2 * (1 / (cos_e_n*cos_e_n) - 1))) / 2.0;
	return 1.0 / ((1 + lmd_i) * (1 + lmd_e));
}

/// http://homepage2.nifty.com/yees/RayTrace/RayTraceVersion001b.pdf
struct FresnelCalc {
	double cost1, cos2t1;
	double sin2t1, sin2t2, cos2t2, cost2;
	ON_3dVector incident, nrm; // incident, nrm は必ず単位ベクトル。nrmの向きがincidentと逆のときはReset関数内で自動的に反転される
	double n1, n2, alpha; // alpha = n1 / n2
	int flg; // 1 : n1とn2が同じ 2 : 全反射 0 : それ以外

	// nrm, incident は単位ベクトルであること
	void Reset(const ON_3dVector &nrm_, const ON_3dVector &incident_, double n1_, double n2_) {
		n1 = n1_, n2 = n2_;
		incident = incident_;
		cost1 = ON_DotProduct(nrm_, incident_);
		if (cost1 > 0) {
			nrm = nrm_;
		} else {
			cost1 *= -1;
			nrm = nrm_ * -1;
		}
		if (n1 == n2) {
			flg = 1; return;
		}
		alpha = n1 / n2;
		cos2t1 = cost1 * cost1;
		if (cos2t1 > 1.0) cos2t1 = 1.0;
		sin2t1 = 1.0 - cos2t1;
		sin2t2 = sin2t1 * alpha * alpha;
		cos2t2 = 1.0 - sin2t2;
		if (cos2t2 < 0) {
			flg = 2; return;
		}
		cost2 = std::sqrt(cos2t2);
		flg = 0;
	}
	FresnelCalc() {
		n1 = n2 = 0; flg = 0;
		incident.Zero(), nrm.Zero();
	}
	// nrm, incident は単位ベクトルであること
	FresnelCalc(const ON_3dVector &nrm, const ON_3dVector &incident, double n1_, double n2_) {
		Reset(nrm, incident, n1_, n2_);
	}
	bool IsSameIndex() const { return (flg == 1); }
	bool IsTotalReflection() const { return (flg == 2); }

	bool CalcRefractDir(ON_3dVector &emit) const {
		if (this->IsSameIndex()) {
			emit = this->incident;
			return true;
		} else if (this->IsTotalReflection()) {
			return false;
		}
		double gamma = this->cost2 - this->alpha * this->cost1;
		if (this->cost1 < 0) gamma = -gamma;
		emit = this->incident * this->alpha + this->nrm * gamma;
		emit.Unitize();
		return true;
	}

	// http://qiita.com/edo_m18/items/b145f2f5d2d05f0f29c9
	bool CalcReflectDir(ON_3dVector &emit) const {
		emit = incident - nrm * (cost1 * 2.0);
		emit.Unitize();
		return true;
	}

	bool CalcRefRatio(double &rp, double &rs) const {
		const FresnelCalc &fc = *this;
		if (fc.IsSameIndex()) {
			rp = rs = 0.0;
			return true;
		} else if (fc.IsTotalReflection()) {
			rp = rs = 1.0;
			return true;
		}
		double cost1_abs = std::abs(fc.cost1);
		double cost2_abs = std::abs(fc.cost2);
		double n1_cost1 = fc.n1 * cost1_abs, n2_cost1 = fc.n2 * cost1_abs;
		double n1_cost2 = fc.n1 * cost2_abs, n2_cost2 = fc.n2 * cost2_abs;
		rp = (n2_cost1 - n1_cost2) / (n2_cost1 + n1_cost2);
		rs = (n1_cost1 - n2_cost2) / (n1_cost1 + n2_cost2);
		return true;
	}

	double CalcRefRatio() const {
		double rp, rs;
		CalcRefRatio(rp, rs);
		return (rp * rp + rs * rs) * 0.5;
	}
};

// ni は常に材質外、 no は常に材質内。 材質内外は in_mediumで判定する。
// test_transmit が true の時は ref から確率を求めて反射、透過し、透過の時は transmitted が true になる。
// test_transmit が false の時は 常に反射で計算し、 flux 値を変える。
template<typename R> void Delta_Sample(FresnelCalc &fc, double const_ref, bool test_transmit, double &flux,  R &rnd, bool &transmitted, ON_3dVector &emit) {
	double ref = const_ref + (1.0 - const_ref) * fc.CalcRefRatio();
	if (ref < 0) ref = 0;
	else if (ref > 1) ref = 1;
	if (test_transmit && ref < 1) {
		if ((ref == 0 || ref <= rnd()) && fc.CalcRefractDir(emit)) {
			transmitted = true;
			return;
		}
	} else {
		flux *= ref;
	}
	fc.CalcReflectDir(emit);
	transmitted = false;
};

struct BSDF_Sampler {
	struct srf {
		piecewise_linear_distribution phis;
		ON_ClassArray<piecewise_linear_distribution> thetas; // phi 毎に確率密度分布作成
	};
	std::vector<srf> incidents;
	const srf *get_srf(double incident_rad) const {
		int idx = static_cast<int>(std::floor(incident_rad * static_cast<double>(incidents.size() - 1) / (ON_PI * 0.5) + 0.5));
		if (idx < 0 || idx >= static_cast<int>(incidents.size())) return nullptr;
		return &incidents[idx];
	}
	template<typename F1, typename F2> void create(double ni, double no, double constant_ref, F1 &ndf, F2 &masking) {
		static const ON_3dVector yaxis(0, 1, 0), zaxis(0, 0, 1);
		static const ON_3dVector nrm(0, 0, 1); // 物体平面の法線方向
		double no2 = no * no;

		ON_SimpleArray<int> selected_indices;
		FresnelCalc fc;
		for (double z = 0; z <= 1.0; z += 0.00390625) {
			double refl = 0, total = 0;
			ON_3dVector incident(0, 0, 1);
			double incident_rad = z * ON_PI * 0.5;
			incident.Rotate(-incident_rad, yaxis);
			incident.Unitize();

			this->incidents.push_back(srf());
			auto &srf = this->incidents.back();

			// https://qiita.com/UWATechnology/items/bf16153c9363dc78bf3d
			// https://qiita.com/_Pheema_/items/f1ffb2e38cc766e6e668
			// https://tatsy.github.io/blog/applications/graphics/1742/
			int idx = 0;
			for (double y = 0; y <= 1.0; y += 0.00390625) {
				double phi = y * ON_PI * 2.0;
				piecewise_linear_distribution &theta_dist = srf.thetas.AppendNew();
				int horz_index = -1;
				// x:theta 0～1 : 反射、 1～2 : 透過
				for (double x = 0; x <= 2.0; x += 0.0078125, ++idx) {
					double theta = x * ON_PI * 0.5;
					ON_3dVector emit(0, 0, 1);
					emit.Rotate(theta, yaxis);
					double sin_theta = emit.x;
					emit.Rotate(phi, zaxis);
					emit.Unitize();

					ON_3dVector half;
					if (x <= 1.0) {
						half = incident + emit;
					} else {
						emit.z *= -1.0;
						half = ni * incident + no * emit;
					}
					half.Unitize();
					fc.Reset(half, incident, ni, no);
					double cos_i_h = fc.cost1;
					double cos_e_h = ON_DotProduct(emit, half);

					double F = constant_ref + (1.0 - constant_ref) * fc.CalcRefRatio();
					double D = ndf(half.z);
					double G = masking(incident.z, emit.z);

					double nume, denom;
					double Fr;
					if (x <= 1.0) {
						// 反射の時
						nume = 1.0;
						denom = 4.0 * incident.z /* * emit.z */; // emit.z は面積分時に掛け算することになるため、省く
						Fr = F;
					} else {
						// 透過の時
						double brc = ni * cos_i_h + no * cos_e_h;
						nume = cos_i_h * cos_e_h * no2;
						denom = incident.z /* * emit.z */ * brc * brc; // emit.z は面積分時に掛け算することになるため、省く
						Fr = 1 - F;
					}
					double p = (denom > ON_ZERO_TOLERANCE) ? Fr * D * G * nume / denom : 0;

					if (p < 0) p = 0;
					else {
						p *= half.z;    // microfacet の面積 を nrm へ投影した面積に変換
						p *= sin_theta; // 球座標の面積分のため、仰角のsinを掛ける
						/* p *= emit.z */
						// Heitz 2014 の論文の式 (4) で emit.z を掛けているが、上記 denom の emit.z を相殺することになるため、
						// denom の emit.z とセットで外す
					}
					theta_dist.v_ary.Append(theta);
					theta_dist.p_ary.Append(p);
					if (x >= 1 && horz_index < 0) horz_index = theta_dist.p_ary.Count() - 1;
				}
				theta_dist.init(0);
				srf.phis.v_ary.Append(phi);
				srf.phis.p_ary.Append(*theta_dist.p_accum_ary.Last());
			}
			srf.phis.init(0);

			simplify(srf.phis, &selected_indices);
			for (int i = 0; i < selected_indices.Count(); ++i) {
				srf.thetas.Swap(i, selected_indices[i]);
				simplify(srf.thetas[i]);
			}
			srf.thetas.SetCapacity(selected_indices.Count());
		}
	}

	template<typename R> void sample(double incident_rad, R &rnd, double &phi_rad, double &theta_rad) const {
//		auto tictoc1 = calc_duration::tic(plfd.durations, 1, 0);
		auto srf = get_srf(incident_rad);
		double phi_n = srf->phis(rnd) / *srf->phis.v_ary.Last();
		int theta_idx = static_cast<int>(std::floor(phi_n * static_cast<double>(srf->thetas.Count() - 1) + 0.5));

//		tictoc1.toc();
//		auto tictoc2 = calc_duration::tic(plfd.durations, 2, 0);
		const piecewise_linear_distribution &theta_dist = srf->thetas[theta_idx];
		double theta_n = theta_dist(rnd) / *theta_dist.v_ary.Last();

		phi_rad = phi_n * ON_PI * 2.0;
		theta_rad = theta_n * ON_PI;
	}
};

// 材質データ
struct Materials::Impl{
	struct Material{
		ON_String name;
		double constant_ref_ratio, transmittance, ior, roughness_alpha;
		ON_SimpleArray<double> diffuse_color, absorption_coef;
		Material(){
			name = "";
			constant_ref_ratio = 0, transmittance = 0, ior = 0, roughness_alpha = 0;
		}
		~Material() {
			DestroySampler();
		}
		void CreateSampler() {
			DestroySampler();
			if (roughness_alpha != 0) {
				double a2 = roughness_alpha * roughness_alpha;
				for (int i = 0; i < ((transmittance > 0) ? 2 : 1); ++i){
					bsdf[i].create((i == 0) ? 1.0 : ior, (i == 1) ? 1.0 : ior, constant_ref_ratio,
						[a2](double half_z) {
							return NDF_GGX(a2, half_z);
						},
						[a2](double incident_z, double emit_z){
							return Masking_Smith_GGX(a2, incident_z, emit_z);
						}
					);
				}
			}
		}
		void DestroySampler() {
		}
		// 入力時の nrm の向きは fndm の設定に従う。 (OUTER:材質外側、 INNER:材質内側、 AUTO: 自動検出)、処理終了時に入射の反対向きにして返す。
		bool Sample(FaceNormalDirectionMode fndm, ON_3dVector &nrm, const ON_3dVector &incident_dir, bool &in_medium, xorshift_rnd_32bit &rnd, int power_count, double *power, ON_3dVector &emit_dir) const{
			double phi_rad_scattering, theta_rad_scattering;
			// zaxis は常に入射の反対向きにする。
			bool calc_scattering = false, calc_diffuse = (diffuse_color.Count() && transmittance < 1);
			bool in_medium_prev = in_medium;

			if ((fndm == FaceNormalDirectionMode::OUTER && in_medium_prev) || (fndm == FaceNormalDirectionMode::INNER && !in_medium_prev)) {
				nrm.Reverse();
			}

			if (roughness_alpha == 0) {
//				auto tictoc = calc_duration::tic(plfd.durations, plfd.count, 1, 0);
				double base_power = 1.0;
				FresnelCalc fc;
				if (!in_medium) fc.Reset(nrm, incident_dir, 1.0, ior);
				else fc.Reset(nrm, incident_dir, ior, 1.0);

				// ni は常に材質外、 no は常に材質内。 材質内外は in_mediumで判定する。
				// test_transmit が true の時は ref から確率を求めて反射、透過し、透過の時は transmitted が true になる。
				// test_transmit が false の時は 常に反射で計算し、 flux 値を変える。
				bool test_transmit = (calc_diffuse || transmittance > 0);
				bool transmitted;
				Delta_Sample(fc, constant_ref_ratio, test_transmit, base_power, rnd, transmitted, emit_dir);

				if (fndm == FaceNormalDirectionMode::AUTO) {
					nrm = fc.nrm;
					nrm.Reverse();
				}

				if (transmitted) {
					if (calc_diffuse) calc_diffuse = (transmittance <= rnd());

					if (!calc_diffuse) {
						in_medium = !in_medium;
					}
				} else {
					calc_diffuse = false;
					if (base_power < 1.0) {
						for (int i = 0; i < power_count; ++i) {
							power[i] *= base_power;
						}
					}
				}
			} else {
//				auto tictoc = calc_duration::tic(plfd.durations, plfd.count, 2, 0);
				auto &cur_bsdf = bsdf[in_medium ? 1 : 0];
				if (cur_bsdf.incidents.size() == 0) return false;

				double incident_cos = ON_DotProduct(nrm, incident_dir);

				if (fndm == FaceNormalDirectionMode::AUTO && incident_cos > 0) {
					nrm.Reverse();
				}

				double incident_rad = std::acos(std::abs(incident_cos));
				cur_bsdf.sample(incident_rad, rnd, phi_rad_scattering, theta_rad_scattering);
				bool transmitted = (theta_rad_scattering > ON_PI * 0.5);
				if (transmitted) {
					if (calc_diffuse) calc_diffuse = (transmittance <= rnd());

					if (calc_diffuse) {
						calc_scattering = false;
					} else if (transmittance > 0) {
						in_medium = !in_medium;
						calc_scattering = true;
					} else {
						emit_dir.Zero();
						for (int i = 0; i < power_count; ++i) power[i] = 0;
						return true;
					}
				} else {
					calc_diffuse = false;
					calc_scattering = true;
				}

			}
			if (calc_scattering || calc_diffuse) {
//				auto tictoc = calc_duration::tic(plfd.durations, plfd.count, 3, 0);
				ON_3dVector &zaxis = nrm;
				ON_3dVector yaxis = ON_CrossProduct(zaxis, incident_dir);
				yaxis.Unitize();
				emit_dir = zaxis;
				if (calc_scattering) {
					Rotate(emit_dir, theta_rad_scattering, yaxis);
					Rotate(emit_dir, phi_rad_scattering, zaxis);
				} else {
					double theta_rad = std::asin(rnd());
					const static double max_rad = ON_PI * 0.5 - ON_DEFAULT_ANGLE_TOLERANCE;
					if (theta_rad > max_rad) theta_rad = max_rad;
					double phi_rad = rnd() * ON_PI * 2.0;

					Rotate(emit_dir, theta_rad, yaxis);
					Rotate(emit_dir, phi_rad, zaxis);

//					double cos_theta = std::cos(theta_rad);
					for (int i = 0; i < power_count; ++i) {
//						power[i] *= cos_theta * diffuse_color[i];
						power[i] *= diffuse_color[i];
					}
				}
				emit_dir.Unitize();
			}
			return true;
		}
		BSDF_Sampler bsdf[2]; // 0: air_to_medium, 1: medium_to_air
	};
	ON_ClassArray<Material> mats;
};


Materials::Materials(nlohmann::json &jmats, nlohmann::json &jshapes, ON_SimpleArray<int> &shape2matidx) {
	std::map<ON_String, size_t> matname2matidx;
	pimpl = new Impl();
	if (!jmats.is_array()) return;

	if (!jshapes.is_array()) return;

	auto read_nreal = [](ON_SimpleArray<double> &dest, nlohmann::json &jarr, size_t count) {
		if (jarr.is_array() && jarr.size() == count) {
			for (size_t i = 0; i < count; ++i) dest.Append(jarr[i]);
			return true;
		}
		return false;
	};

	// (1)  constant_ref_ratio の確率で一律に反射する。
	// (2)  (1) で反射しなかった分のうち、ior でフレネル反射する。
	// (3)  (2) で反射しなかった分のうち、transmittance 分が材質内に入る。
	// (4)  (3) で材質内に入らなかった分のうち、 diffuse_color 分が拡散反射する。
	// (5)  (3) で材質内に入ると、absorpt_coef 分だけ体積減衰しながら光線が伝播する。

	for (size_t k = 0; k < jmats.size(); ++k) {
		Impl::Material &mat = pimpl->mats.AppendNew();
		auto &jmat = jmats[k];
		if (!jmat.is_object()) continue;

		mat.name = jmat["name"].get<std::string>().c_str();
		mat.roughness_alpha = jmat["roughness_alpha"];
		mat.constant_ref_ratio = jmat["constant_ref_ratio"];
		mat.ior = jmat["ior"];
		mat.transmittance = jmat["transmittance"];
		if (!read_nreal(mat.diffuse_color, jmat["diffuse_color"], 3)) mat.diffuse_color.Empty();
		read_nreal(mat.absorption_coef, jmat["absorption_coef"], 3);
		matname2matidx.insert(std::make_pair(mat.name, k));
	}

	shape2matidx.Destroy();
	for (size_t k = 0; k < jshapes.size(); ++k) {
		ON_String matname = jshapes[k]["material"].get<std::string>().c_str();
		auto iter = matname2matidx.find(matname);
		if (iter == matname2matidx.end()) shape2matidx.Append(-1);
		else shape2matidx.Append(iter->second);
	}

	// 使っているマテリアルのみ生成する。
	ON_SimpleArray<int> matidx_created(pimpl->mats.Count());
	matidx_created.SetCount(matidx_created.Capacity());
	for (int k = 0; k < shape2matidx.Count(); ++k) {
		int matidx = shape2matidx[k];
		if (matidx_created[matidx]) continue;
		pimpl->mats[matidx].CreateSampler();
		matidx_created[matidx] = 1;
	}
}

Materials::~Materials(){
	delete pimpl;
}

int Materials::Count() const{
	return pimpl->mats.Count();
}

bool Materials::VolumeAttenuate(int midx, int power_count, double *power, double length) const{
	if (midx < 0 || midx >= Count()) return false;
	Impl::Material &mat = pimpl->mats[midx];
	if (mat.absorption_coef.Count() > 0 && mat.absorption_coef.Count() == power_count) {
		for (int i = 0; i < power_count; ++i) {
			power[i] *= std::pow(10.0, -length * mat.absorption_coef[i]);
		}
		return true;
	}
	for (int i = 0; i < power_count; ++i) {
		power[i] = 0;
	}
	return false;
}

bool Materials::CalcBSDF(int midx, FaceNormalDirectionMode fndm, ON_3dVector &nrm, const ON_3dVector &incident_dir, bool &in_medium, xorshift_rnd_32bit &rnd, int power_count, double *power, ON_3dVector &emit_dir) const{
//	auto tictoc = calc_duration::tic(plfd.durations, plfd.count, 0, 0);
	if (midx < 0 || midx >= Count()) return false;
	Impl::Material &mat = pimpl->mats[midx];
	return mat.Sample(fndm, nrm, incident_dir, in_medium, rnd, power_count, power, emit_dir);
}
