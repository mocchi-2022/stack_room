// Copyright (C) Mocchi
// License: Boost Software License   See LICENSE.txt for the full license.

#include "opennurbs.h"
#include "ONGEO.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <unordered_map>
#include <windows.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace std{
	template<> struct hash<ON_2dex> {
		size_t operator() (const ON_2dex &data) const {
			hash<long long int> h;
			return h(static_cast<long long int>(data.j) * 4294967296LL + data.i);
		}
	};
	template<> struct hash<ON_3dPoint> {
		size_t operator() (const ON_3dPoint &data) const {
			hash<double> h;
			return h(data.x + data.y * 10000 + data.z * 100000000);
		}
	};
}
bool operator == (const ON_2dex &lhs, const ON_2dex &rhs){
	return lhs.i == rhs.i && lhs.j == rhs.j;
}

// 指定された mesh の func_height の出力値(高さ)が 0 となる位置にコンターを生成し、 ON_Polyline の配列形式で出力する。
// func_height: 入力:頂点インデックス、出力:高さ となる関数。
template <typename F> void Mesh_CalculateContourPolylines(const ON_Mesh &mesh, ON_ClassArray<ON_Polyline> &contours, F &func_height){
	ON_SimpleArray<ON_2dex> edges;
	mesh.GetMeshEdges(edges);

	ON_Interval hint;
	std::unordered_map<ON_2dex, ON_3dPoint> height_map;

	// func_height値で0を交差するエッジと、そのパラメータ値をheight_mapに保管する。
	for (int i = 0; i < edges.Count(); ++i){
		ON_2dex &edge = edges[i];
		double hi = func_height(edge.i);
		double hj = func_height(edge.j);
		if (hi * hj > 0) continue;
		double t;
		if (hi == hj && hi == 0) t = 0.5;
		else{
			hint.m_t[0] = hi;
			hint.m_t[1] = hj;
			t = hint.NormalizedParameterAt(0);
		}
		ON_3dPoint pt_s = mesh.Vertex(edge.i);
		ON_3dPoint pt_e = mesh.Vertex(edge.j);
		ON_3dPoint pt = (1 - t) * pt_s + t * pt_e;
		height_map.insert(std::make_pair(edge, pt));
	}

	std::unordered_multimap<ON_3dPoint, ON_3dPoint> edgelines;

	// height_map に記録されたエッジを持つ face を探す。
	// (face には該当するエッジが通常2つ存在するはず)。その2つのエッジのパラメータ値から求めた点を繋ぐ
	for (int j = 0; j < mesh.m_F.Count(); ++j){
		const ON_MeshFace &f = mesh.m_F[j];
		int iter_cnt = 0;
		ON_3dPoint edgeline_pt[2];
		for (int i = 0; i < 4; ++i){
			int vs = f.vi[i], ve = f.vi[(i+1)%4];
			if (vs == ve) continue;
			if (vs > ve) std::swap(vs, ve);
			ON_2dex edge = {vs, ve}; 
			auto iter = height_map.find(edge);
			if (iter == height_map.end()) continue;
			edgeline_pt[iter_cnt++] = iter->second;
			if (iter_cnt == 2) break; // Todo: 3つ以上ある場合
		}
		if (iter_cnt != 2) continue;
		edgelines.insert(std::make_pair(edgeline_pt[0], edgeline_pt[1]));
		edgelines.insert(std::make_pair(edgeline_pt[1], edgeline_pt[0]));
	}

#if 1
	// edgelines を出来る限り繋いで Polyline として書き出す
	while(edgelines.size()){
		auto iter_b = edgelines.begin();
		ON_3dPoint edgeline_pt[2] = {iter_b->first, iter_b->second};
		edgelines.erase(iter_b);

		ON_Polyline &pol = contours.AppendNew();
		pol.Append(edgeline_pt[0]);
		pol.Append(edgeline_pt[1]);

		// j = 0: 折れ線の終点側を延ばすループ、 j = 1: 折れ線の始点側を延ばすループ (折れ線の向きを反転させて同じ処理をする)
		for (int j = 0; j < 2; ++j){
			for (;;){
				ON_3dPoint pt_next(ON_UNSET_VALUE, ON_UNSET_VALUE, ON_UNSET_VALUE); // 次のセグメントの終点
				auto iter_range = edgelines.equal_range(edgeline_pt[1]);
				std::vector<decltype(iter_b)> iters_to_remove; // 同じセグメントの向き違い(削除対象)
				// 同じセグメントの向き違いと次のセグメントを探す
				for (auto iter = iter_range.first; iter != iter_range.second; ++iter){
					if (iter->second == edgeline_pt[0]){
						iters_to_remove.push_back(iter);
					}else if (pt_next.x == ON_UNSET_VALUE){
						pt_next = iter->second;
					}else continue;
				}
				// 同じセグメントの向き違いは削除する。
				for (size_t i = 0; i < iters_to_remove.size(); ++i) edgelines.erase(iters_to_remove[i]);

				// 次のセグメントがあった場合は、セグメント情報を更新して折れ線を延ばす。
				if (pt_next.x != ON_UNSET_VALUE){
					pol.Append(pt_next);
					edgeline_pt[0] = edgeline_pt[1];
					edgeline_pt[1] = pt_next;
					auto iter_range = edgelines.equal_range(edgeline_pt[0]);
					for (auto iter = iter_range.first; iter != iter_range.second; ++iter){
						if (iter->second != edgeline_pt[1]) continue;
						edgelines.erase(iter);
						break;
					}
				}else break;
			}
			if (j == 0 && pol.Count() > 0){
				iter_b = edgelines.find(pol[0]);
				if (iter_b != edgelines.end()){
					edgeline_pt[0] = pol[0];
					edgeline_pt[1] = iter_b->second;
					edgelines.erase(iter_b);
					pol.Reverse();
					pol.Append(edgeline_pt[1]);
				}
			}
		}
	}

#else
	// edgelines 一切繋がずに Polyline として書き出す
	for (auto iter = edgelines.begin(); iter != edgelines.end(); ++iter){
		ON_Polyline &pol = contours.AppendNew();
		pol.Append(iter->first);
		pol.Append(iter->second);
	}

#endif
}

int main(int argc, char *argv[]){
	if (argc < 2) return 0;
	ON_Mesh mesh;
	ON_String header;
	ONGEO_ReadBinarySTL(ON_BinaryFile(ON::read, ON_FileStream::Open(argv[1], "rb")), mesh, header);

	ON_BoundingBox bb;
	mesh.GetTightBoundingBox(bb);

	double y_mid = bb.Center().y;

	LARGE_INTEGER freq, count1, count2;
	::QueryPerformanceFrequency(&freq);
	::QueryPerformanceCounter(&count1);

	ON_ClassArray<ON_Polyline> contours;
	Mesh_CalculateContourPolylines(mesh, contours, [&](int vi){
		return mesh.Vertex(vi).y - y_mid;
	});

	::QueryPerformanceCounter(&count2);
	std::printf("%f msec\n", static_cast<double>(count2.QuadPart - count1.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart));

	ONX_Model model;
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
