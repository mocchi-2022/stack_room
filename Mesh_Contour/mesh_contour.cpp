// Copyright (C) Mocchi
// License: Boost Software License   See LICENSE.txt for the full license.

#include "opennurbs.h"
#include "ONGEO.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <unordered_map>
#include <windows.h>

namespace std{
	template<> struct hash<ON_2dex> {
		size_t operator() (const ON_2dex &data) const {
			hash<long long int> h;
			return h(static_cast<long long int>(data.j) * 4294967296LL + data.i);
		}
	};
}
bool operator == (const ON_2dex &lhs, const ON_2dex &rhs){
	return lhs.i == rhs.i && lhs.j == rhs.j;
}

// 指定された mesh の 各頂点に対応する height 値が 0 となる位置にコンターを生成し、 ON_Polyline の配列形式で出力する。
void Mesh_CalculateContourPolylines(const ON_Mesh &mesh, ON_ClassArray<ON_Polyline> &contours, const double *height_array) {

	ON_ClassArray<ON_3dPoint> intersect_points;
	std::unordered_multimap<int, int> intersections;

	{
		std::unordered_map<ON_2dex, int> edge_cache;
		ON_Interval hint;
		int edgeline_ptidx[2];
		// height がゼロと交わる face を走査し、その edge の height 0 となる交点を求め、交線を記録していく。
		for (int j = 0; j < mesh.m_F.Count(); ++j) {
			const ON_MeshFace &f = mesh.m_F[j];
			int iter_cnt = 0;
			for (int i = 0; i < 4; ++i) {
				int vs = f.vi[i], ve = f.vi[(i + 1) & 3];
				if (vs == ve) continue;

				double hs = height_array[vs], he = height_array[ve];
				if (hs * he > 0) continue;
				if (vs > ve) std::swap(vs, ve), std::swap(hs, he);

				ON_2dex edge = { vs, ve };
				auto iter = edge_cache.find(edge);
				if (iter == edge_cache.end()) {
					double t;
					if (hs == he && hs == 0) t = 0.5;
					else {
						hint.m_t[0] = hs, hint.m_t[1] = he;
						t = hint.NormalizedParameterAt(0);
					}
					iter = (edge_cache.insert(std::make_pair(edge, intersect_points.Count()))).first;
					ON_3dPoint pt_s = mesh.Vertex(edge.i), pt_e = mesh.Vertex(edge.j);
					ON_3dPoint pt = (1 - t) * pt_s + t * pt_e;
					intersect_points.Append(pt);
				}

				edgeline_ptidx[iter_cnt++] = iter->second;
				if (iter_cnt == 2) break; // Todo: 3つ以上ある場合
			}
			if (iter_cnt != 2) continue;
			intersections.insert(std::make_pair(edgeline_ptidx[0], edgeline_ptidx[1]));
			intersections.insert(std::make_pair(edgeline_ptidx[1], edgeline_ptidx[0]));
		}
	}

#if 1
	// edgelines を出来る限り繋いで Polyline として書き出す
	while (intersections.size()) {
		auto iter_b = intersections.begin();
		int edgeline_ptidx[2] = { iter_b->first, iter_b->second };
		intersections.erase(iter_b);

		ON_Polyline &pol = contours.AppendNew();
		int ptidx_pol0 = edgeline_ptidx[0];
		pol.Append(intersect_points[edgeline_ptidx[0]]);
		pol.Append(intersect_points[edgeline_ptidx[1]]);

		// j = 0: 折れ線の終点側を延ばすループ、 j = 1: 折れ線の始点側を延ばすループ (折れ線の向きを反転させて同じ処理をする)
		for (int j = 0; j < 2; ++j) {
			for (;;) {
				int ptidx_next = -1; // 次のセグメントの終点
				auto iter_range = intersections.equal_range(edgeline_ptidx[1]);
				std::vector<decltype(iter_b)> iters_to_remove; // 同じセグメントの向き違い(削除対象)
				// 同じセグメントの向き違いと次のセグメントを探す
				for (auto iter = iter_range.first; iter != iter_range.second; ++iter) {
					if (iter->second == edgeline_ptidx[0]) {
						iters_to_remove.push_back(iter);
					}
					else if (ptidx_next < 0) {
						ptidx_next = iter->second;
					}
					else continue;
				}
				// 同じセグメントの向き違いは削除する。
				for (size_t i = 0; i < iters_to_remove.size(); ++i) intersections.erase(iters_to_remove[i]);

				// 次のセグメントがあった場合は、セグメント情報を更新して折れ線を延ばす。
				if (ptidx_next >= 0) {
					pol.Append(intersect_points[ptidx_next]);
					edgeline_ptidx[0] = edgeline_ptidx[1];
					edgeline_ptidx[1] = ptidx_next;
					auto iter_range = intersections.equal_range(edgeline_ptidx[0]);
					for (auto iter = iter_range.first; iter != iter_range.second; ++iter) {
						if (iter->second != edgeline_ptidx[1]) continue;
						intersections.erase(iter);
						break;
					}
				}
				else break;
			}

			if (j == 0 && pol.Count() > 0) {
				iter_b = intersections.find(ptidx_pol0);
				if (iter_b != intersections.end()) {
					edgeline_ptidx[0] = ptidx_pol0;
					edgeline_ptidx[1] = iter_b->second;
					intersections.erase(iter_b);
					pol.Reverse();
					pol.Append(intersect_points[edgeline_ptidx[1]]);
				}
			}
		}
	}

#else
	// edgelines 一切繋がずに Polyline として書き出す
	for (auto iter = edgelines.begin(); iter != edgelines.end(); ++iter) {
		ON_Polyline &pol = contours.AppendNew();
		pol.Append(intersect_points[iter->first]);
		pol.Append(intersect_points[iter->second]);
	}

#endif
}

// 指定された mesh の func_height の出力値(高さ)が 0 となる位置にコンターを生成し、 ON_Polyline の配列形式で出力する。
// func_height: 入力:頂点インデックス、出力:高さ となる関数。
template <typename F> void Mesh_CalculateContourPolylines(const ON_Mesh &mesh, ON_ClassArray<ON_Polyline> &contours, F &func_height) {
	ON_SimpleArray<double> height(mesh.VertexCount());
	for (int i = 0; i < mesh.VertexCount(); ++i) height.Append(func_height(i));
	Mesh_CalculateContourPolylines(mesh, contours, height.Array());
}

int main(int argc, char *argv[]){
	if (argc < 2) return 0;
	ON_Mesh mesh;
	ON_String header;
	ONGEO_ReadBinarySTL(ON_BinaryFile(ON::read, ON_FileStream::Open(argv[1], "rb")), mesh, header);

	ON_BoundingBox bb;
	mesh.GetTightBoundingBox(bb);

	ONX_Model model;
	ON_ClassArray<ON_Polyline> contours;

	ON_Interval y_int(bb.Min().y, bb.Max().y);

	LARGE_INTEGER freq, count1, count2;
	::QueryPerformanceFrequency(&freq);
	::QueryPerformanceCounter(&count1);

	for (double t = 0; t <= 1; t += 0.125){
		double y_sect = y_int.ParameterAt(t);
		Mesh_CalculateContourPolylines(mesh, contours, [&](int vi){
			return mesh.Vertex(vi).y - y_sect;
		});
	}

	::QueryPerformanceCounter(&count2);
	std::printf("%f msec\n", static_cast<double>(count2.QuadPart - count1.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart));

	ONX_Model_Object &obj_mesh = model.m_object_table.AppendNew();
	obj_mesh.m_bDeleteObject = false;
	obj_mesh.m_object = &mesh;

	for (int i = 0; i < contours.Count(); ++i){
		ON_PolylineCurve pc(contours[i]);
		ONX_Model_Object &obj_crv = model.m_object_table.AppendNew();
		obj_crv.m_bDeleteObject = true;
		obj_crv.m_object = pc.Duplicate();
	}
	model.Write("mesh_contour.3dm", 4);
	return 0;
}
