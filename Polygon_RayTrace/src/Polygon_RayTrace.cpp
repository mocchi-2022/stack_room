/*
 * Polygon_RayTrace
 * Copylight (C) 2023 mocchi
 * mocchi_2003@yahoo.co.jp
 * License: Boost ver.1
 */

#define USE_EMBREE

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <chrono>

#include "opennurbs.h"
#include "thpool.h"
#include "mist/thread.h"
#include "nlohmann/json.hpp"
#include "rply.h"
#include "gd.h"

#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#ifdef min
#undef min
#undef max
#endif
#include "tinyexr.h"

#include "PhisicalProperties.h"
#include "randomizer.h"
#include "calc_duration.h"

#include <windows.h>

#define RAY_IOTA_PROGRESS 0.005
#define TRACE_TERMINAL_LENGTH 10.0
#define MAX_INTERSECTION_COUNT 5000

#ifdef USE_EMBREE
#include "embree3/rtcore.h"
#endif

//#define USE_COROUTINE

#ifdef USE_COROUTINE
#include "cppcoro/generator.hpp"
#define SIMD_COUNT 8
#endif

float get_ieee754(ON__UINT8 p[4]){
	return *reinterpret_cast<float *>(p);
}

// 参考文献: http://ja.wikipedia.org/wiki/Standard_Triangulated_Language
bool ConvertFromBinarySTL(ON_SimpleArray<ON__UINT8> &stl, double scale, const ON_3dPoint &position, ON_Mesh &mesh){
	mesh.Destroy();
	if (stl.Count() < 84) return false;
	ON__UINT32 num_facets =
		static_cast<ON__UINT32>(stl[80]) + static_cast<ON__UINT32>(stl[81]) * 256 +
		static_cast<ON__UINT32>(stl[82]) * 65536 + static_cast<ON__UINT32>(stl[83]) * 16777216;

	if (stl.Count() < 84 + static_cast<int>(num_facets) * 50 - 2) return false;

	mesh.DestroyDoublePrecisionVertices();
	mesh.SetSinglePrecisionVerticesAsValid();
	ON__UINT8 *p = stl.First() + 84;
	for (ON__UINT32 i = 0; i < num_facets; ++i, p += 50){
		int vcnt = mesh.m_V.Count();
		mesh.m_FN.AppendNew().Set(get_ieee754(p   ), get_ieee754(p+ 4), get_ieee754(p+ 8));
		mesh.m_FN.Last()->Unitize();
		mesh.m_V.AppendNew().Set(get_ieee754(p+12), get_ieee754(p+16), get_ieee754(p+20));
		mesh.m_V.AppendNew().Set(get_ieee754(p+24), get_ieee754(p+28), get_ieee754(p+32));
		mesh.m_V.AppendNew().Set(get_ieee754(p+36), get_ieee754(p+40), get_ieee754(p+44));
		mesh.SetTriangle(mesh.m_F.Count(), vcnt, vcnt+1, vcnt+2);
	}
	mesh.CombineIdenticalVertices();
	for (int i = 0; i < mesh.m_V.Count(); ++i) {
		mesh.m_V[i] *= scale;
		mesh.m_V[i] += ON_3fPoint(position);
	}
	mesh.Compact();
	return true;
}

bool ConvertFromPLY(const char *filename, double scale, const ON_3dPoint &position, ON_Mesh &mesh) {
	bool rc = false;
	auto ply = ply_open(filename, [](p_ply ply, const char *msg) { }, 0, nullptr);
	if (!ply) return false;
	if (!ply_read_header(ply)) goto END;
	mesh.Destroy();
	mesh.DestroyDoublePrecisionVertices();
	mesh.SetSinglePrecisionVerticesAsValid();

	{
		auto add_vertex = [](p_ply_argument arg) {
			void *pdata;
			long xyz;
			ply_get_argument_user_data(arg, &pdata, &xyz);
			ON_Mesh *mesh = static_cast<ON_Mesh *>(pdata);
			long index;
			p_ply_element elm;
			ply_get_argument_element(arg, &elm, &index);
			mesh->m_V[index][xyz] = ply_get_argument_value(arg);
			return 1;
		};
		long nvertices = ply_set_read_cb(ply, "vertex", "x", add_vertex, &mesh, 0);
		ply_set_read_cb(ply, "vertex", "y", add_vertex, &mesh, 1);
		ply_set_read_cb(ply, "vertex", "z", add_vertex, &mesh, 2);
		mesh.m_V.SetCapacity(nvertices);
		mesh.m_V.SetCount(nvertices);

		auto add_face = [](p_ply_argument arg) {
			void *pdata;
			ply_get_argument_user_data(arg, &pdata, nullptr);
			ON_Mesh *mesh = static_cast<ON_Mesh *>(pdata);
			long index;
			p_ply_element elm;
			ply_get_argument_element(arg, &elm, &index);
			long length, vi;
			ply_get_argument_property(arg, nullptr, &length, &vi);
			if (vi < 0) return 1;
			mesh->m_F[index].vi[vi] = static_cast<int>(ply_get_argument_value(arg));
			if (vi == 2) {
				mesh->m_F[index].vi[3] = mesh->m_F[index].vi[2];
			}
			return 1;
		};
		long ntriangles = ply_set_read_cb(ply, "face", "vertex_indices", add_face, &mesh, 0);
		mesh.m_F.SetCapacity(ntriangles);
		mesh.m_F.SetCount(ntriangles);
		if (!ply_read(ply)) goto END;
	}
	mesh.CombineIdenticalVertices();
	mesh.ComputeVertexNormals();
	for (int i = 0; i < mesh.m_V.Count(); ++i) {
		mesh.m_V[i] *= scale;
		mesh.m_V[i] += ON_3fPoint(position);
	}
	mesh.Compact();

	rc = true;
END:
	ply_close(ply);
	return true;
}

bool ReadFile(const char *filename, ON_SimpleArray<ON__UINT8> &data){
	FILE *fp = std::fopen(filename, "rb");
	if (!fp) return false;
	std::fseek(fp, 0, SEEK_END);
	data.SetCapacity(static_cast<int>(std::ftell(fp)));
	data.SetCount(data.Capacity());
	std::rewind(fp);
	std::fread(data, 1, data.Count(), fp);
	std::fclose(fp);
	return true;
}

struct MeshRayIntersection{
	std::unique_ptr<RTCIntersectContext> context;
	RTCScene *scene;
	ON_Mesh *mesh;

	void Initialize(ON_Mesh *mesh_, RTCScene *scene_){
		scene = scene_;
		mesh = mesh_;
		context.reset(new RTCIntersectContext());
		rtcInitIntersectContext(context.get());
	}

	// 計算結果
	struct Result {
		ON_3dPoint pt;
		int mesh_idx;
		int face_idx;
		double u, v;
	};
	bool RayIntersection(const ON_3dRay &ray, Result &result){

		RTCRayHit rayhit;
		rayhit.ray.org_x = ray.m_P.x;
		rayhit.ray.org_y = ray.m_P.y;
		rayhit.ray.org_z = ray.m_P.z;
		rayhit.ray.dir_x = ray.m_V.x;
		rayhit.ray.dir_y = ray.m_V.y;
		rayhit.ray.dir_z = ray.m_V.z;
		rayhit.ray.tnear = 0;
		rayhit.ray.tfar = std::numeric_limits<float>::infinity();
		rayhit.ray.mask = -1;
		rayhit.ray.flags = 0;
		rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
		rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;

		rtcIntersect1(*scene, context.get(), &rayhit);

		if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID){
			result.mesh_idx = rayhit.hit.geomID;
			result.face_idx = rayhit.hit.primID;
			result.pt = ray.m_P + ray.m_V * rayhit.ray.tfar;
			result.u = rayhit.hit.u;
			result.v = rayhit.hit.v;
			return true;
		}else return false;
	}

	void RayIntersection8(const ON_3dRay rays[8], Result results[8]) {

		RTCRayHit8 rayhit;
		int valid[8];
		for (int i = 0; i < 8; ++i) {
			const ON_3dRay &ray = rays[i];
			rayhit.ray.org_x[i] = ray.m_P.x;
			rayhit.ray.org_y[i] = ray.m_P.y;
			rayhit.ray.org_z[i] = ray.m_P.z;
			rayhit.ray.dir_x[i] = ray.m_V.x;
			rayhit.ray.dir_y[i] = ray.m_V.y;
			rayhit.ray.dir_z[i] = ray.m_V.z;
			rayhit.ray.tnear[i] = 0;
			rayhit.ray.tfar[i] = std::numeric_limits<float>::infinity();
			rayhit.ray.mask[i] = -1;
			rayhit.ray.flags[i] = 0;
			rayhit.hit.geomID[i] = RTC_INVALID_GEOMETRY_ID;
			rayhit.hit.instID[0][i] = RTC_INVALID_GEOMETRY_ID;
			valid[i] = ray.m_V.IsZero() ? 0 : -1;
		}

		rtcIntersect8(valid, *scene, context.get(), &rayhit);

		for (int i = 0; i < 8; ++i) {
			Result &result = results[i];
			const ON_3dRay &ray = rays[i];
			if (rayhit.hit.geomID[i] != RTC_INVALID_GEOMETRY_ID) {
				result.mesh_idx = rayhit.hit.geomID[i];
				result.face_idx = rayhit.hit.primID[i];
				result.pt = ray.m_P + ray.m_V * rayhit.ray.tfar[i];
				result.u = rayhit.hit.u[i];
				result.v = rayhit.hit.v[i];
			} else {
				result.mesh_idx = result.face_idx = -1;
			}
		}
	}


};

void read_3real(nlohmann::json &jarr, double *dest) {
	if (jarr.is_array() && jarr.size() == 3) {
		dest[0] = jarr[0];
		dest[1] = jarr[1];
		dest[2] = jarr[2];
	} else {
		dest[0] = dest[1] = dest[2] = 0;
	}
};

struct Environment {
	struct fRGB {
		float r, g, b;
	};
	ON_ClassArray<fRGB> image;
	int width, height;
	ON_3dVector zenith, center, equator;
	Environment(nlohmann::json &env) {
		width = height = 0;
		if (!env.is_object()) return;

		read_3real(env["zenith_dir"], static_cast<double *>(zenith));
		read_3real(env["center_dir"], static_cast<double *>(center));

		equator = ON_CrossProduct(center, zenith);
		zenith.Unitize();
		center.Unitize();
		equator.Unitize();
		double multiplier = env["multiplier"];
		LoadEXR(env["path"].get<std::string>().c_str(), multiplier);
	}

	const fRGB &operator ()(ON_3dVector &ray_dir) {
		//                      Z:zenith
		//         dr(dx,dy,dz)  |    X:center
		//        (0,ty,dz) *-_  |   / 
		//             _  +-:--~-+dz/
		//              ~-: :    | /
		//             u( :~*-_  |/
		// Y:equator    --+----~-+--------
		// Y軸をZ軸で v[deg]回した後に、 X軸で u[deg] 回すしたときに dr になる。
		// (1) 2回回転後のベクトルの長さ^2: dx^2 + dy^2 + dz^2
		// (2) 1回回転後のベクトルの長さ^2: ty^2 + dz^2
		// (1) = (2) より、ty^2 = dx^2 + dy^2 => ty = sqrt(dx^2 + dy^2)
		// tan(v) = dz / ty
		// tan(u) = dy / dx

		double dz = ON_DotProduct(zenith, ray_dir);
		double dy = ON_DotProduct(equator, ray_dir);
		double dx = ON_DotProduct(center, ray_dir);
		double ty = std::sqrt(dx * dx + dy * dy);

		double v_rad = (ty < ON_ZERO_TOLERANCE) ? 0 : std::atan2(dz, ty);
		double u_rad = (std::abs(dx) < ON_ZERO_TOLERANCE) ? 0 : std::atan2(dy, dx);

		// u_rad : [-PI to PI], v_rad : [-PI/2 to PI/2]
		// (0,0) のとき上下左右中央, (X,PI/2) のとき上中央
		int px = (u_rad / (2.0 * ON_PI) + 0.5) * static_cast<double>(width);
		if (px < 0) px = 0;
		else if (px >= width) px = width;
		int py = (-v_rad / ON_PI + 0.5) * static_cast<double>(height);
		if (py < 0) py = 0;
		else if (py >= height) py = height;
		return (*this)(px, py);
	}

private:
	fRGB &operator ()(int px, int py) {
		return image[py*width + px];
	}
	fRGB &operator[] (size_t idx) {
		return image[idx];
	}

	void resize(int width_, int height_) {
		width = width_;
		height = height_;
		image.SetCapacity(width * height);
		image.SetCount(image.Capacity());
	}


	void LoadEXR(const char *path_exr, double multiplier) {
		if (!path_exr) return;
		std::fprintf(stderr, "  %s\n", path_exr);
		image.Empty();
		ON_SimpleArray<uint8_t> exrdata;
		{
			std::unique_ptr<FILE, decltype(&std::fclose)> fp(std::fopen(path_exr, "rb"), std::fclose);
			if (!fp.get()) return;
			std::fseek(fp.get(), 0, SEEK_END);
			exrdata.SetCapacity(std::ftell(fp.get()));
			exrdata.SetCount(exrdata.Capacity());
			std::fseek(fp.get(), 0, SEEK_SET);
			std::fread(exrdata.First(), exrdata.Count(), 1, fp.get());
		}

		EXRVersion exr_version;
		EXRImage exr_image;
		EXRHeader exr_header;
		const char *err = 0;
		InitEXRHeader(&exr_header);
		std::unique_ptr<EXRHeader, decltype(&::FreeEXRHeader)> deleter_exr_header(&exr_header, ::FreeEXRHeader);
		InitEXRImage(&exr_image);
		std::unique_ptr<EXRImage, decltype(&::FreeEXRImage)> deleter_exr_image(&exr_image, ::FreeEXRImage);

		{
			int ret = ParseEXRVersionFromMemory(&exr_version, exrdata, exrdata.Count());
			if (ret != TINYEXR_SUCCESS) return;
			// "Failed to open EXR file or read version info from EXR file."

			if (exr_version.multipart || exr_version.non_image) return;
			// "Loading multipart or DeepImage is not supported  in LoadEXR() API"
		}

		{
			int ret = ParseEXRHeaderFromMemory(&exr_header, &exr_version, exrdata, exrdata.Count(), &err);
			if (ret != TINYEXR_SUCCESS) return;
		}

		// Read HALF channel as FLOAT.
		for (int i = 0; i < exr_header.num_channels; i++) {
			if (exr_header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
				exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
			}
		}

		double xr, yr, xg, yg, xb, yb, xw, yw;
		bool chromacity_predifined;
		OpenEXR_ReadChromacity(exr_header, xr, yr, xg, yg, xb, yb, xw, yw, &chromacity_predifined);

		{
			int ret = LoadEXRImageFromMemory(&exr_image, &exr_header, exrdata, exrdata.Count(), &err);
			if (ret != TINYEXR_SUCCESS) return;
		}
		this->resize(exr_image.width, exr_image.height);

		// Get RGBA channel Index
		int idxR = -1, idxG = -1, idxB = -1, idxA = -1;
		for (int c = 0; c < exr_header.num_channels; c++) {
			if (std::strcmp(exr_header.channels[c].name, "R") == 0) {
				idxR = c;
			}
			else if (std::strcmp(exr_header.channels[c].name, "G") == 0) {
				idxG = c;
			}
			else if (std::strcmp(exr_header.channels[c].name, "B") == 0) {
				idxB = c;
			}
			else if (std::strcmp(exr_header.channels[c].name, "A") == 0) {
				idxA = c;
			}
		}

		if (exr_header.num_channels == 1) {
			// Grayscale channel only.

			if (exr_header.tiled) {
				for (int it = 0; it < exr_image.num_tiles; it++) {
					for (int j = 0; j < exr_header.tile_size_y; j++) {
						for (int i = 0; i < exr_header.tile_size_x; i++) {
							const int ii = exr_image.tiles[it].offset_x * exr_header.tile_size_x + i;
							const int jj = exr_image.tiles[it].offset_y * exr_header.tile_size_y + j;
							const int idx = ii + jj * exr_image.width;

							// out of region check.
							if (ii >= exr_image.width) continue;
							if (jj >= exr_image.height) continue;

							const int srcIdx = i + j * exr_header.tile_size_x;
							unsigned char **src = exr_image.tiles[it].images;
							float gray = reinterpret_cast<float **>(src)[0][srcIdx];
							auto &rgb = (*this)(ii, jj);
							rgb.r = rgb.g = rgb.b = gray;
						}
					}
				}
			}
			else {
				for (int i = 0; i < exr_image.width * exr_image.height; i++) {
					auto &rgb = (*this)[i];
					rgb.r = rgb.g = rgb.b = reinterpret_cast<float **>(exr_image.images)[0][i];
				}
			}
		}
		else {
			if (idxR == -1 || idxG == -1 || idxB == -1) return;

			if (exr_header.tiled) {
				for (int it = 0; it < exr_image.num_tiles; it++) {
					for (int j = 0; j < exr_header.tile_size_y; j++) {
						for (int i = 0; i < exr_header.tile_size_x; i++) {
							const int ii = exr_image.tiles[it].offset_x * exr_header.tile_size_x + i;
							const int jj = exr_image.tiles[it].offset_y * exr_header.tile_size_y + j;
							const int idx = ii + jj * exr_image.width;

							// out of region check.
							if (ii >= exr_image.width) continue;
							if (jj >= exr_image.height) continue;

							const int srcIdx = i + j * exr_header.tile_size_x;
							unsigned char **src = exr_image.tiles[it].images;
							auto &rgb = (*this)(ii, jj);
							rgb.r = reinterpret_cast<float **>(src)[idxR][srcIdx];
							rgb.g = reinterpret_cast<float **>(src)[idxG][srcIdx];
							rgb.b = reinterpret_cast<float **>(src)[idxB][srcIdx];
						}
					}
				}
			}
			else {
				for (int i = 0; i < exr_image.width * exr_image.height; i++) {
					auto &rgb = (*this)[i];
					rgb.r = reinterpret_cast<float **>(exr_image.images)[idxR][i];
					rgb.g = reinterpret_cast<float **>(exr_image.images)[idxG][i];
					rgb.b = reinterpret_cast<float **>(exr_image.images)[idxB][i];
				}
			}
		}

		fRGB rgb_max = { 0,0,0 };
		for (int i = 0; i < exr_image.width * exr_image.height; i++) {
			auto &rgb = (*this)[i];
			rgb.r *= multiplier;
			rgb.g *= multiplier;
			rgb.b *= multiplier;
			if (rgb_max.r < rgb.r) rgb_max.r = rgb.r;
			if (rgb_max.g < rgb.g) rgb_max.g = rgb.g;
			if (rgb_max.b < rgb.b) rgb_max.b = rgb.b;
		}
		std::fprintf(stderr, "  rgb_max:(%f, %f, %f)\n", rgb_max.r, rgb_max.g, rgb_max.b);

	}
	bool OpenEXR_ReadChromacity(const EXRHeader &exr_header, double &xr, double &yr, double &xg, double &yg, double &xb, double &yb, double &xw, double &yw, bool *pchromacities_defined) {
		bool rc = false;
		const char *err = 0;

		auto get_float = [](unsigned char *p) {
			float v = *reinterpret_cast<float *>(p);
			tinyexr::swap4(&v);
			return v;
		};

		bool chromacities_defined = false;
		for (int i = 0; i < exr_header.num_custom_attributes; ++i) {
			EXRAttribute &attr = exr_header.custom_attributes[i];
			if (std::strcmp(attr.name, "chromaticities") != 0) continue;
			chromacities_defined = true;
			// 内部形式をリトルエンディアンと仮定
			xr = get_float(attr.value);
			yr = get_float(attr.value + 4);
			xg = get_float(attr.value + 8);
			yg = get_float(attr.value + 12);
			xb = get_float(attr.value + 16);
			yb = get_float(attr.value + 20);
			xw = get_float(attr.value + 24);
			yw = get_float(attr.value + 28);
			break;
		}
		if (!chromacities_defined) {
			// https://openexr.readthedocs.io/en/latest/TechnicalIntroduction.html
			// Recommendations - RGB Color
			xr = 0.6400, yr = 0.3300;
			xg = 0.3000, yg = 0.6000;
			xb = 0.1500, yb = 0.0600;
			xw = 0.3127, yw = 0.3290;
		}
		if (pchromacities_defined) *pchromacities_defined = chromacities_defined;

		rc = true;
		return rc;
	}
};

struct Cameras {
	struct Camera {
		enum ProjectionMode {
			Parallel, Perspective
		}proj_mode;
		ON_3dPoint origin;
		ON_3dVector eye, vert, horz;
		ON_Plane pln;
		int pixel_width, pixel_height;
		double horz_range, vert_range;
		double horz_pixelsize, vert_pixelsize;
		int pass;
		ON_String output_filename;

		struct pixel_info {
			ON_3dRay ray_init;
			bool no_intersection;
		};
		ON_SimpleArray<pixel_info> pixel_info;
		void IntersectionTest(MeshRayIntersection &mri) {
			for (int iy = 0; iy < pixel_height; ++iy) {
				for (int ix = 0; ix < pixel_width; ++ix) {
					auto &pixel = pixel_info[iy * pixel_width + ix];
					MeshRayIntersection::Result result;
					pixel.no_intersection = !mri.RayIntersection(pixel.ray_init, result);
				}
			}
		}
	};
	ON_ClassArray<Camera> cameras;
	Cameras(nlohmann::json &j_cmrs) {
		if (!j_cmrs.is_array()) return;

		for (size_t i = 0; i < j_cmrs.size(); ++i) {
			Camera &cmr = cameras.AppendNew();
			auto &j_cmr = j_cmrs[i];

			read_3real(j_cmr["origin"], static_cast<double *>(cmr.origin));
			read_3real(j_cmr["eye_dir"], static_cast<double *>(cmr.eye));
			read_3real(j_cmr["horz_dir"], static_cast<double *>(cmr.horz));
			read_3real(j_cmr["vert_dir"], static_cast<double *>(cmr.vert));
			int zero_count = (cmr.eye.IsZero() ? 1 : 0) + (cmr.horz.IsZero() ? 1 : 0) + (cmr.vert.IsZero() ? 1 : 0);
			if (zero_count >= 2) {
				// Todo エラーメッセージ
				continue;
			}
			// 軸ベクトルが2つの時は右手系として3つ目の軸を作る。また、それぞれ直交させる。
			if (cmr.eye.IsZero()) {
				cmr.eye = ON_CrossProduct(cmr.horz, cmr.vert);
				cmr.horz = ON_CrossProduct(cmr.vert, cmr.eye);
			} else if (cmr.horz.IsZero()) {
				cmr.horz = ON_CrossProduct(cmr.vert, cmr.eye);
				cmr.vert = ON_CrossProduct(cmr.eye, cmr.horz);
			} else if (cmr.vert.IsZero()) {
				cmr.vert = ON_CrossProduct(cmr.eye, cmr.horz);
				cmr.horz = ON_CrossProduct(cmr.vert, cmr.eye);
			} else {
				// 左手系の設定もできるようにする。
				cmr.vert = ON_CrossProduct(cmr.eye, cmr.horz);
				ON_3dVector h = ON_CrossProduct(cmr.vert, cmr.eye);
				cmr.horz = (ON_DotProduct(cmr.horz, h) < 0) ? -h : h;
			}
			cmr.eye.Unitize();
			cmr.horz.Unitize();
			cmr.vert.Unitize();
			cmr.pixel_width = j_cmr["pixel_width"];
			cmr.pixel_height = j_cmr["pixel_height"];
			cmr.horz_range = j_cmr["horz_range"];
			cmr.vert_range = j_cmr["vert_range"];
			cmr.output_filename = j_cmr["output_filename"].get<std::string>().c_str();
			double far_ = j_cmr["far"];
			cmr.pass = j_cmr["pass"];

			if (j_cmr["projection_mode"] == "parallel") {
				cmr.proj_mode = Camera::Parallel;
				cmr.pln.CreateFromFrame(cmr.origin, cmr.horz, cmr.vert);
			} else {
				cmr.proj_mode = Camera::Perspective;
				cmr.pln.CreateFromFrame(cmr.origin + cmr.eye * far_, cmr.horz, cmr.vert);
			}

			// カメラが定義された時点で初期レイも確定するため、一緒に生成する。
			cmr.pixel_info.SetCapacity(cmr.pixel_width * cmr.pixel_height);
			cmr.pixel_info.SetCount(cmr.pixel_info.Capacity());

			cmr.horz_pixelsize = cmr.horz_range / static_cast<double>(cmr.pixel_width);
			cmr.vert_pixelsize = cmr.vert_range / static_cast<double>(cmr.pixel_height);
			for (int iy = 0; iy < cmr.pixel_height; ++iy) {
				double v = static_cast<double>(iy * 2 - cmr.pixel_height) * cmr.vert_pixelsize;
				for (int ix = 0; ix < cmr.pixel_width; ++ix) {
					double u = static_cast<double>(ix * 2 - cmr.pixel_width) * cmr.horz_pixelsize;

					auto &pixel = cmr.pixel_info[iy * cmr.pixel_width + ix];
					switch (cmr.proj_mode) {
						case Cameras::Camera::Parallel:
						{
							// 平行投影
							pixel.ray_init.m_P = cmr.pln.PointAt(u, v);
							pixel.ray_init.m_V = cmr.eye;
							break;
						}
						case Cameras::Camera::Perspective:
						{
							// 透視投影
							pixel.ray_init.m_P = cmr.origin;
							pixel.ray_init.m_V = cmr.pln.PointAt(u, v) - pixel.ray_init.m_P;
							pixel.ray_init.m_V.Unitize();
							break;
						}
					}
					pixel.no_intersection = false;
				}
			}
		}
	}
};

struct CommonInfo{
	LightSources *light_src;
	Materials *materials;
	Environment *environment;
	int cnt_10;
	pthread_mutex_t mtx_read, mtx_write;

	CommonInfo() {
		::pthread_mutex_init(&mtx_read, 0);
		::pthread_mutex_init(&mtx_write, 0);
	}
	~CommonInfo() {
		::pthread_mutex_destroy(&mtx_read);
		::pthread_mutex_destroy(&mtx_write);
	}

	// read
	struct Scene {
		ON_Mesh *mesh;
		ON_3dPoint model_center;
		double rough_radius;

		RTCDevice device;
		RTCScene scene;

		static void errorFunction(void* userPtr, enum RTCError error, const char* str) {
			std::printf("error %d: %s\n", error, str);
		}

		void Initialize(ON_Mesh *mesh_) {
			mesh = mesh_;
			ON_BoundingBox tbb;
			mesh_->GetTightBoundingBox(tbb);
			rough_radius = tbb.Diagonal().Length() * 0.5;
			model_center = tbb.Center();

			device = rtcNewDevice(0);
			if (!device) {
				std::printf("error %d: cannot create device\n", rtcGetDeviceError(0));
				return;
			}
			rtcSetDeviceErrorFunction(device, errorFunction, 0);

			scene = rtcNewScene(device);

			RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
			float* vertices = static_cast<float*>(
				rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * sizeof(float), mesh_->VertexCount())
				);
			unsigned int* indices = static_cast<unsigned int*>(
				rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * sizeof(unsigned), mesh_->FaceCount())
				);

			for (int i = 0, i3 = 0; i < mesh_->VertexCount(); ++i, i3 += 3) {
				ON_3dPoint &pt = mesh_->Vertex(i);
				vertices[i3] = pt.x;
				vertices[i3 + 1] = pt.y;
				vertices[i3 + 2] = pt.z;
			}
			for (int i = 0, i3 = 0; i < mesh_->FaceCount(); ++i, i3 += 3) {
				ON_MeshFace &f = mesh_->m_F[i];
				indices[i3] = f.vi[0];
				indices[i3 + 1] = f.vi[1];
				indices[i3 + 2] = f.vi[2];
			}

			rtcCommitGeometry(geom);

			rtcAttachGeometry(scene, geom);
			rtcReleaseGeometry(geom);
			rtcCommitScene(scene);
		}
		~Scene() {
			if (!device) return;
			rtcReleaseScene(scene);
			rtcReleaseDevice(device);
		}
	}scene;

	int ray_cursor;
	ON_SimpleArray<unsigned int> *shapeidx2fidx;
	ON_SimpleArray<int> *shape2matidx;
	ON_SimpleArray<FaceNormalDirectionMode> *shape2fndm;

	// write
	ON_ClassArray<ON_ClassArray<ON_3dRay> > raies_last;
	ON_ClassArray<ON_SimpleArray<double> > flux_last;
	ON_ClassArray<ON_Polyline> traces;
};

#ifdef USE_COROUTINE
cppcoro::generator<const int> RayTrace(const ON_3dRay &ray_init, double flux, ON_3dRay &ray_toits, ON_Mesh &cshape, MeshRayIntersection::Result &result, CommonInfo *ci, xorshift_rnd_32bit &rnd, ON_3dRay &ray_o, double power[3], ON_Polyline *trace, bool &error, uint64_t *durations, uint64_t *count, int &cnt) {
	cnt = 0;
#else
int RayTrace(const ON_3dRay &ray_init, double flux, MeshRayIntersection &mri, CommonInfo *ci, xorshift_rnd_32bit &rnd, ON_3dRay &ray_o, double power[3], ON_Polyline *trace, bool &error, uint64_t *durations, uint64_t *count){
	int cnt = 0;
	MeshRayIntersection::Result result;
	ON_Mesh &cshape = *mri.mesh;
#endif
	error = false;
#ifdef USE_COROUTINE
	ON_3dRay &ray = ray_toits;
	ray = ray_init;
#else
	ON_3dRay ray = ray_init;
#endif
	if (trace) trace->Append(ray.m_P);
	bool is_inside = false;
	bool absorbed = false;


	bool rc;
	for (;;) {
		
#ifdef USE_COROUTINE
		co_yield 1;
		if (result.mesh_idx < 0 && result.face_idx < 0) break;
#else
		// 23000ms
		{
//			auto tic = calc_duration::tic(durations, count, 0, 0);
			if (!(rc = mri.RayIntersection(ray, result))) break;
		}
#endif
		++cnt;

		// flat shading
		// 1400ms
//		auto tic2 = calc_duration::tic(durations, count, 1, 0);
		ON_3dVector flat_nrm = cshape.m_FN[result.face_idx];
//		tic2.toc();

		// phong shading
		// 4200ms
		ON_3dVector phong_nrm;
		{
//			auto tic = calc_duration::tic(durations, count, 2, 0);
			auto &face = cshape.m_F[result.face_idx];
			double u = result.u, v = result.v;
			phong_nrm =
				cshape.m_N[face.vi[0]] * (1.0 - (u + v)) +
				cshape.m_N[face.vi[1]] * u +
				cshape.m_N[face.vi[2]] * v;
			phong_nrm.Unitize();
		}

		// 面番号から形状番号を取得
		// 2900ms
		int shape_idx, midx;
		{
//			auto tic = calc_duration::tic(durations, count, 3, 0);
			shape_idx = static_cast<int>(
				std::upper_bound(ci->shapeidx2fidx->First(),
								 ci->shapeidx2fidx->Last() + 1, static_cast<unsigned int>(result.face_idx)) - ci->shapeidx2fidx->First()
				) - 1;

			midx = (*ci->shape2matidx)[shape_idx];

			// 材質内部の時は吸収係数を適用
			if (is_inside) {
				double dist = ray.m_P.DistanceTo(result.pt);
				ci->materials->VolumeAttenuate(midx, 3, power, dist);
			}

		}

		bool is_inside_prev = is_inside;
		ON_3dVector emit_dir;
		{
			// 23800ms
//			auto tic = calc_duration::tic(durations, count, 4, 0);
			FaceNormalDirectionMode fndm = (*ci->shape2fndm)[shape_idx];
			if (!ci->materials->CalcBSDF(midx, fndm, phong_nrm, ray.m_V, is_inside, rnd, 3, power, emit_dir)){
				error = true;
				break;
			}
		}
		{
//			auto tic = calc_duration::tic(durations, count, 5, 0);
			// 2220ms
			// 反射なのに突き抜けている、または、透過なのに同じ媒質内にとどまるケースのチェック。
			// phong_nrm と flat_nrm のずれにより、曲率の大きいところや diffuse で浅い角度への反射、等で起きやすい。
			if (ON_DotProduct(phong_nrm, flat_nrm) < 0) flat_nrm.Reverse();
			// この時点で flat_nrm は incident と反対向きになっている。
			if (!emit_dir.IsZero()) {
				bool new_dir_is_reflection = (ON_DotProduct(flat_nrm, emit_dir) > 0);
				if ((new_dir_is_reflection && (is_inside != is_inside_prev)) || (!new_dir_is_reflection && (is_inside == is_inside_prev))) {
					error = true;
					break;
				}
			}
			ray.m_V = emit_dir;
			if (ray.m_V.IsZero() || (power[0] == 0 && power[1] == 0 && power[2] == 0)) {
				absorbed = true;
				break;
			}
			if (trace) trace->Append(result.pt);
			ray.m_P = ON_3dPoint(result.pt) + ray.m_V * RAY_IOTA_PROGRESS;
			if (cnt >= MAX_INTERSECTION_COUNT) {
				error = true;
				break;
			}
		}
	}

	if (trace && !absorbed) trace->Append(ray.m_P + ray.m_V * TRACE_TERMINAL_LENGTH);
	ray_o = ray;
#ifndef USE_COROUTINE
	return cnt;
#endif
}

int main(int argc, char *argv[]){
	if (argc == 1){
		return 0;
	}
	nlohmann::json args_doc;
	{
		FILE *fp = std::fopen(argv[1], "rb");
		std::fseek(fp, 0, SEEK_END);
		ON_SimpleArray<char> settingfile(std::ftell(fp));
		std::rewind(fp);
		settingfile.SetCount(settingfile.Capacity());
		std::fread(settingfile, 1, settingfile.Count(), fp);
		std::fclose(fp);
		args_doc = nlohmann::json::parse(settingfile.Array());
	}

	ON_ClassArray<ON_Mesh> shapes;
	ON_SimpleArray<int> shape2matidx;
	ON_SimpleArray<FaceNormalDirectionMode> shape2fndm;
	// 全てのメッシュの合成
	ON_Mesh cshape;

	// 合成後の面番号から合成前の形状番号を取得するために使用する。
	ON_SimpleArray<unsigned int> shapeidx2fidx;

	std::fprintf(stderr, "Reading shapes.\n");
	auto &jshapes = args_doc["shapes"];
	if (jshapes.is_array()){
		for (size_t k = 0; k < jshapes.size(); ++k){
			ON_Mesh &shape = shapes.AppendNew();
			auto &jshape = jshapes[k];
			std::string filename = jshape["filename"].get<std::string>();
			if (filename[0] == '\0') continue;

			auto &jscale = jshape["scale"];
			double scale = jscale.is_number() ? static_cast<double>(jscale) : 1.0;
			ON_3dPoint position;
			read_3real(jshape["position"], position);

			auto &jfacedir = jshape["face_direction"];
			FaceNormalDirectionMode fndm = FaceNormalDirectionMode::AUTO;
			if (jfacedir == "outer") {
				fndm = FaceNormalDirectionMode::OUTER;
			} else if (jfacedir == "inner") {
				fndm = FaceNormalDirectionMode::INNER;
			}
			shape2fndm.Append(fndm);
			std::fprintf(stderr, "  %s\n", filename.c_str());

			if (std::strstr(filename.c_str(), ".3dm") != 0) {
				ONX_Model model;
				model.Read(filename.c_str());
				for (int i = 0; i < model.m_object_table.Count(); ++i) {
					const ON_Mesh *mesh = ON_Mesh::Cast(model.m_object_table[i].m_object);
					if (!mesh) continue;
					shape.Append(*mesh);

					for (int i = 0; i < shape.m_V.Count(); ++i) {
						shape.m_V[i] *= scale;
						shape.m_V[i] += ON_3fPoint(position);
					}
				}
				if (!shape.HasVertexNormals()) shape.ComputeVertexNormals();
			} else if (std::strstr(filename.c_str(), ".ply") != 0) {
				ConvertFromPLY(filename.c_str(), scale, position, shape);
			} else if (std::strstr(filename.c_str(), ".stl") != 0){
				ON_SimpleArray<ON__UINT8> data;
				if (!ReadFile(filename.c_str(), data)) continue;
				ConvertFromBinarySTL(data, scale, position, shape);
				shape.ComputeVertexNormals();
			}
		}
	}

	std::fprintf(stderr, "Constructing tree.\n");
	shapeidx2fidx.Append(0U);
	for (int k = 0; k < shapes.Count(); ++k){
		cshape.Append(shapes[k]);
		shapeidx2fidx.Append(*shapeidx2fidx.Last() + shapes[k].FaceCount());
	}

	// 材質の定義
	Materials mats(args_doc["materials"], jshapes, shape2matidx);
//	double ref_index = 1.1;

	// 環境光の定義
	std::fprintf(stderr, "Defining environment.\n");
	Environment environment(args_doc["environment"]);

	// 光源データ
	std::fprintf(stderr, "Defining lightsource.\n");
	LightSources light_src(args_doc["lightsources"]);

	// カメラ
	std::fprintf(stderr, "Definig cameras.\n");
	Cameras cameras(args_doc["cameras"]);

	std::fprintf(stderr, "Raytracing.\n");

	// 光線追跡
	CommonInfo ci;
	ci.light_src = &light_src;
	ci.materials = &mats;
	ci.environment = &environment;
	ci.cnt_10 = light_src.RayCount() / 10;
	ci.ray_cursor = 0;
	ci.scene.Initialize(&cshape);
	ci.shapeidx2fidx = &shapeidx2fidx;
	ci.shape2matidx = &shape2matidx;
	ci.shape2fndm = &shape2fndm;

	{
		ON_ClassArray<ON_ClassArray<ON_3dRay> > &raies_last = ci.raies_last;
		raies_last.SetCapacity(light_src.LSCount());
		raies_last.SetCount(light_src.LSCount());
		ON_ClassArray<ON_SimpleArray<double> > &flux_last = ci.flux_last;
		flux_last.SetCapacity(light_src.LSCount());
		flux_last.SetCount(light_src.LSCount());
		for (int i = 0; i < light_src.LSCount(); ++i){
			raies_last[i].SetCapacity(light_src.RayCount(i));
			raies_last[i].SetCount(raies_last[i].Capacity());
			flux_last[i].SetCapacity(light_src.RayCount(i));
			flux_last[i].SetCount(flux_last[i].Capacity());
		}
	}

	// 乱数の初期化、スレッド数の取得
	size_t threads_count = mist::get_cpu_num();
//	size_t threads_count = 1;

#define DURATION_NUMBER 7
	struct Thread {
		Thread() {}
		// 全てのカメラで共通の内容
		int thread_idx, num_threads;
		MeshRayIntersection *mri;
		xorshift_rnd_32bit rnd;
		Cameras::Camera *camera;
		CommonInfo *ci;
		uint64_t durations[DURATION_NUMBER], count[DURATION_NUMBER];
#ifdef USE_COROUTINE
		ON_3dRay ray_toitc[SIMD_COUNT];
		MeshRayIntersection::Result results[SIMD_COUNT];
#endif

		// カメラ毎に初期化される内容
		uint64_t total_intersect_cnt, total_error_cnt;
		int progress_current, progress_total;
		enum class OutputType {
			LDR, HDR
		}output_type;
		union{
			gdImagePtr ldr;
			float *exr;
		}img;
		ON_ClassArray<ON_Polyline> pols;

		struct pixel_accum_item {
			double rgb[3];
			int counter_per_pass_performed;
		};
		ON_SimpleArray<pixel_accum_item> pixel_accum;
		void init() {
			total_intersect_cnt = 0;
			total_error_cnt = 0;
			progress_total = camera->pass;
			progress_current = 0;
			pixel_accum.SetCapacity(camera->pixel_width * camera->pixel_height);
			pixel_accum.SetCount(pixel_accum.Capacity());
			pixel_accum.Zero();
		}

#ifdef USE_COROUTINE
		cppcoro::generator<const int> execute(int coroutine_idx) {
#else
		void execute() {
#endif
			int count_per_pass = camera->pass;
			int pixel_width = camera->pixel_width;
			int pixel_height = camera->pixel_height;

#ifdef USE_COROUTINE
			for (int k = thread_idx; k < count_per_pass; k += num_threads*SIMD_COUNT) {
#else
			for (int k = thread_idx; k < count_per_pass; k += num_threads) {
#endif
				progress_current = k;
				for (int iy = 0, pi_y = 0; iy < pixel_height; ++iy, pi_y += pixel_width) {
					for (int ix = 0; ix < pixel_width; ++ix) {
						int pixel_index = pi_y + ix;
						auto &info = camera->pixel_info[pixel_index];
						if (info.no_intersection) continue;
						ON_3dRay ray_init = info.ray_init;
#if 1
						double inte, frac = std::modf(rnd()*65536.0, &inte);
						inte /= 65536.0;
						double ru = (inte - 0.5) * camera->horz_pixelsize;
						double rv = (frac - 0.5) * camera->vert_pixelsize;

						ON_Plane &pln = camera->pln;
						ray_init.m_P += pln.xaxis * ru + pln.yaxis * rv;
#endif

						ON_3dRay ray_o;
						double power[3] = { 1, 1, 1 };
						ON_Polyline pol;
						bool error = false;
						// 51200ms
						{
#ifdef USE_COROUTINE
							int cnt;
							auto rt = RayTrace(info.ray_init, 1.0, ray_toitc[coroutine_idx], *(mri->mesh), results[coroutine_idx], ci, rnd, ray_o, power, nullptr, error, durations, count, cnt);
							for (auto iter = rt.begin(); iter != rt.end(); ++iter) {
								co_yield *iter;
							}
#else
							int cnt = RayTrace(ray_init, 1.0, *mri, ci, rnd, ray_o, power, nullptr, error, durations, count);
#endif

							if (error) {
								++total_error_cnt;
								continue;
							}

							total_intersect_cnt += cnt;
						}

						{
							auto &accum = pixel_accum[pixel_index];
							auto env_rgb = (*ci->environment)(ray_o.m_V);
							accum.rgb[0] += env_rgb.r * power[0];
							accum.rgb[1] += env_rgb.g * power[1];
							accum.rgb[2] += env_rgb.b * power[2];
							++accum.counter_per_pass_performed;
						}
					}
				}
			}

			update_image();

			progress_current = progress_total;
		}
		void update_image() {
			int pixel_width = camera->pixel_width;
			int pixel_height = camera->pixel_height;
			for (int iy = 0, pi_y = 0; iy < pixel_height; ++iy, pi_y += pixel_width) {
				for (int ix = 0; ix < pixel_width; ++ix) {
					int pixel_index = pi_y + ix;
					auto &info = camera->pixel_info[pixel_index];
					double rgb[3];
					if (info.no_intersection) {
						auto env_rgb = (*ci->environment)(info.ray_init.m_V);
						rgb[0] = env_rgb.r;
						rgb[1] = env_rgb.g;
						rgb[2] = env_rgb.b;
					} else {
						auto &accum = pixel_accum[iy * pixel_width + ix];
						double inv_cppp = 1.0 / static_cast<double>(accum.counter_per_pass_performed);
						for (int h = 0; h < 3; ++h) {
							rgb[h] = accum.rgb[h] * inv_cppp;
						}
					}
					if (output_type == OutputType::LDR) {
						for (int h = 0; h < 3; ++h) {
							rgb[h] = std::pow(rgb[h], 1 / 2.2) * 255.0;
							if (rgb[h] >= 255.0) rgb[h] = 255;
						}
						img.ldr->tpixels[pixel_height - iy - 1][pixel_width - ix - 1] = gdTrueColor(static_cast<int>(rgb[0]), static_cast<int>(rgb[1]), static_cast<int>(rgb[2]));
					} else {
						float *p = &img.exr[((pixel_height - iy - 1)*pixel_width + (pixel_width - ix - 1)) * 4];
						for (int h = 0; h < 3; ++h) {
							p[h] = static_cast<float>(rgb[h]);
						}
					}
				}
			}
		}
	};

	std::printf("start\n");
	auto c1 = std::chrono::system_clock::now();
	xorshift_rnd_32bit rnd;
	rnd.init(444, 2531);
	ON_ClassArray<Thread> threads;
	MeshRayIntersection mri;
	mri.Initialize(&cshape, &ci.scene.scene);
	for (int i = 0; i < threads_count; ++i) {
		Thread &th = threads.AppendNew();
		th.thread_idx = i;
		th.num_threads = threads_count;
		th.mri = &mri;
		th.rnd.init(static_cast<int>(rnd() * 10000000.0) + 1234, static_cast<int>(rnd() * 10000000.0) + 1234);
		th.ci = &ci;
		for (int h = 0; h < DURATION_NUMBER; ++h) th.durations[h] = 0;
	}

	std::unique_ptr<thpool_, decltype(&thpool_destroy)> thpool(thpool_init(threads_count), thpool_destroy);

#ifdef USE_COROUTINE
	struct col_item {
		cppcoro::generator<const int> instance;
		cppcoro::generator<const int>::iterator iter;
		bool started;
		col_item() : started(false) {}
		bool next() {
			if (!started) {
				iter = instance.begin();
				started = true;
				return true;
			} else if (iter != instance.end()) {
				++iter;
				return true;
			}
			return false;
		}
		double value() {
			*iter;
		}
	};
#endif

	for (int j = 0; j < cameras.cameras.Count(); ++j){
		auto &cmr = cameras.cameras[j];
		std::vector<float> exr;
		Thread::OutputType ot;
		gdImagePtr ldr = nullptr;
		if (cmr.output_filename.Right(4) == ".exr") {
			ot = Thread::OutputType::HDR;
			exr.resize(cmr.pixel_width * cmr.pixel_height * 4);
		} else {
			ot = Thread::OutputType::LDR;
			ldr = gdImageCreateTrueColor(cmr.pixel_width, cmr.pixel_height);
		}

		std::printf("camera # %d\n", j + 1);

		// 一回通り衝突判定して、衝突しないところを背景として確定させる
		cmr.IntersectionTest(mri);

		// 本計算
		for (int i = 0; i < threads_count; ++i) {
			Thread &th = threads[i];

			th.output_type = ot;
			th.camera = &cameras.cameras[j];
			if (ot == Thread::OutputType::HDR) {
				th.img.exr = exr.data();
			} else {
				th.img.ldr = ldr;
			}
			th.init();
			::thpool_add_work(thpool.get(), [](void *arg) {
#ifdef USE_COROUTINE
				Thread &th = *static_cast<Thread *>(arg);
				col_item cols[SIMD_COUNT];
				for (size_t i = 0; i < SIMD_COUNT; ++i) {
					cols[i].instance = th.execute(i);
				}

				for (;;) {
					bool end_all = true;
					for (size_t i = 0; i < SIMD_COUNT; ++i) {
						auto &itm = cols[i];
						bool end_this = !itm.next();
						if (end_this) th.ray_toitc[i].m_V.Zero();
						end_all &= end_this;
					}
					if (end_all) break;

//					auto tic = calc_duration::tic(th.durations, th.count, 0, 0);
					MeshRayIntersection &mri = *th.mri;
					mri.RayIntersection8(th.ray_toitc, th.results);
				}
#else
				static_cast<Thread *>(arg)->execute();
#endif
			}, &th);
		}
		for (;;) {
			uint64_t progress_current = 0, progress_total = 0;
			for (int i = 0; i < threads.Count(); ++i) {
				auto &th = threads[i];
				progress_current += th.progress_current;
				progress_total += th.progress_total;
			}
			if (progress_total > 0) {
				double ratio = static_cast<double>(progress_current) * 100.0 / static_cast<double>(progress_total);
				std::printf("%lld / %lld (%5.1f %%)\n", progress_current, progress_total, ratio);
				std::fflush(stdout);
				if (progress_current == progress_total) break;
			}
			::sleep(2);
		}
		::thpool_wait(thpool.get());
		if (ldr != nullptr) {
			::gdImageFile(ldr, cmr.output_filename);
			::gdImageDestroy(ldr);
		} else if (exr.size()) {
			const char *err;
			SaveEXR(exr.data(), cmr.pixel_width, cmr.pixel_height, 4, 0, cmr.output_filename, &err);
		}
	}

	uint64_t total_durations[DURATION_NUMBER] = { 0 };
	uint64_t total_intersect_cnt = 0, total_error_cnt = 0;
	for (int i = 0; i < threads_count; ++i) {
		Thread &th = threads[i];
		total_intersect_cnt += th.total_intersect_cnt;
		total_error_cnt += th.total_error_cnt;
		for (int h = 0; h < DURATION_NUMBER; ++h) total_durations[h] += th.durations[h];
	}
	std::printf("total_intersection:%lld\n", total_intersect_cnt);
	std::printf("total_error:%lld\n", total_error_cnt);
	std::printf("total_durations:\n");
	LARGE_INTEGER freq;
	::QueryPerformanceFrequency(&freq);
	double coef = 1000.0 / static_cast<double>(freq.QuadPart);
	for (int h = 0; h < DURATION_NUMBER; ++h) {
		std::printf("  [%d]:%f msec.\n", h, static_cast<double>(total_durations[h]) * coef);
	}

	auto c2 = std::chrono::system_clock::now();
	std::printf("%f msec.\n", static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(c2 - c1).count()) / 1000.0);
	std::getchar();
	return 0;
}
