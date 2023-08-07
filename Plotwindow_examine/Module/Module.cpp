// Copyright (C) 2022 mocchi
// License: Boost Software License   See LICENSE.txt for the full license.

#define DLLEXPORT extern "C" __declspec(dllexport)

#include <cstdint>
#include <unordered_map>

#include <windows.h>

#ifdef max
#undef max
#undef min
#endif

#include "mist/mist.h"
#include "mist/vector.h"
#include "mist/config/color.h"
#include "mist/drawing.h"
#include "stb_image.h"
#include "font_renderer.h"
#include "PPlot.h"

struct stb_image {
	typedef mist::rgba<uint8_t> color_type;
	mist::array2 <color_type> rgba;
	stb_image() {
	}
	void free() {
		if (rgba.is_memory_shared() && rgba.size() > 0) {
			stbi_image_free(&rgba[0]);
		}
		rgba.clear();
	}
	void load(const char *filename){
		free();
		int width, height, bpp;
		void *stb_buf = stbi_load(filename, &width, &height, &bpp, 4);
		if (!stb_buf) return;
		new (&rgba) mist::array2<color_type>(width, height, static_cast<color_type *>(stb_buf), width * height * 4);
	}
};

typedef mist::bgra<uint8_t> canvas_color_type;
typedef mist::array2<canvas_color_type> canvas_type;
class CanvasPainter : public Painter {
public:
	typedef canvas_color_type color_type;
	mist::array2<canvas_color_type> *canvas;
	color_type color_line, color_fill;
	font_renderer::font_renderer fr;
	std::unordered_map<std::string, font_renderer::font_renderer::array2_type> font_cache;

	CanvasPainter() {
		color_line = color_type(0, 0, 0);
		color_fill = color_type(255, 255, 255);
		canvas = nullptr;
	}
	void ResetCache() {
		font_cache.clear();
	}
	virtual void DrawLine(int inX1, int inY1, int inX2, int inY2) {
		if (!canvas) return;
		mist::draw_line(*canvas, inX1, inY1, inX2, inY2, color_line);
	}
	virtual void FillRect(int inX, int inY, int inW, int inH) {
		if (!canvas) return;
		mist::fill_rect(*canvas, inX, inY, inW, inH, color_fill);
	}
	virtual void SetClipRect(int inX, int inY, int inW, int inH) {
		// 実装しない
	}
	virtual long GetWidth() const {
		if (!canvas) return 0;
		return static_cast<long>(canvas->width());
	}
	virtual long GetHeight() const {
		if (!canvas) return 0;
		return static_cast<long>(canvas->height());
	}
	virtual void SetLineColor(int inR, int inG, int inB) {
		color_line.r = inR, color_line.g = inG, color_line.b = inB;
	}
	virtual void SetFillColor(int inR, int inG, int inB) {
		color_fill.r = inR, color_fill.g = inG, color_fill.b = inB;
	}

	font_renderer::font_renderer::array2_type &GetTextArray(const char *inString) {
		auto &text_arr = font_cache[inString];
		if (text_arr.size() == 0) fr.render(inString, text_arr, GetFontHeight(), false, false, true, color_type(255, 255, 255), color_line);
		return text_arr;
	}
	virtual long CalculateTextDrawSize(const char *inString) {
		auto &text_arr = GetTextArray(inString);
		return static_cast<int>(text_arr.width());
	}
	virtual long GetFontHeight() const {
		return 12;
	}
	virtual void DrawText(int inX, int inY, const char *inString) {
		if (!canvas) return;
		auto &text_arr = GetTextArray(inString);

		int x_offset = inX, y_offset = inY - GetFontHeight();
		int txt_h = static_cast<int>(text_arr.height()), txt_w = static_cast<int>(text_arr.width());
		for (int j = 0, ji_fr = 0; j < txt_h; ++j, ji_fr += txt_w) {
			if (j + y_offset < 0) continue;
			else if (j + y_offset >= canvas->height()) break;
			auto canvas_y = canvas->y_begin(j + y_offset);
			for (int i = 0; i < txt_w; ++i) {
				if (i + x_offset >= canvas->width()) break;
				canvas_y[i + x_offset] = text_arr[ji_fr + i];
			}
		}
	}
	virtual void DrawRotatedText(int inX, int inY, const char *inString) {
		if (!canvas) return;
		auto &text_arr = GetTextArray(inString);

		int x_offset = inX - GetFontHeight(), y_offset = inY;
		int txt_h = static_cast<int>(text_arr.height()), txt_w = static_cast<int>(text_arr.width());
		for (int j = -(txt_w - 1), j_fr = txt_w - 1; j <= 0; ++j, --j_fr) {
			if (j + y_offset < 0) continue;
			else if (j + y_offset >= canvas->height()) break;
			auto canvas_y = canvas->y_begin(j + y_offset);
			for (int i = 0, i_fr = 0; i < txt_h; ++i, i_fr += txt_w) {
				if (i + x_offset >= canvas->width()) break;
				canvas_y[i + x_offset] = text_arr[j_fr + i_fr];
			}
		}
	}
};

struct Application {
	stb_image img_data;

	struct ViewInfo {
		HDC hDC;
		double zoom;
		bool grabbing;
		int grab_x, grab_y, grab_dx, grab_dy, image_dx, image_dy;
		int view_width, view_height;
		struct chart_area {
			static const int plotarea_left = 50, plotarea_top = 5, offset_right = 20, offset_bottom = 30;
			int plotarea_width, plotarea_height;
			void update(int view_width, int view_height) {
				plotarea_width = view_width - (plotarea_left + offset_right);
				plotarea_height = view_height - (plotarea_top + offset_bottom);
			}
			bool in_plotarea(int x, int y) {
				return plotarea_left <= x && plotarea_left + plotarea_width > x && plotarea_top <= y && plotarea_top + plotarea_height > y;
			}
		}ca;
	}vi;
	struct PlotInfo {
		PPlot plot;
		CanvasPainter painter;
	}pi;
};


DLLEXPORT Application *Application_New() {
	return new Application();
}

DLLEXPORT void Application_Delete(Application *app) {
	delete app;
}

// ==== ビューの処理 ====

DLLEXPORT void Application_View_Init(Application *app, HWND hWnd, const char *imagefile_path) {
	app->img_data.load(imagefile_path);
	app->vi.zoom = 1.0;
	app->vi.grabbing = false;
	app->vi.image_dx = 0, app->vi.image_dy = 0;
	app->vi.hDC = ::GetDC(hWnd);

	app->pi.plot.mPlotDataContainer.AddXYPlot(new PlotData(), new PlotData(), new LegendData());
	app->pi.painter.ResetCache();

	auto &margin = app->pi.plot.mMargins;
	margin.mLeft = app->vi.ca.plotarea_left;
	margin.mRight = app->vi.ca.offset_right;
	margin.mTop = app->vi.ca.plotarea_top;
	margin.mBottom = app->vi.ca.offset_bottom;

	auto &xaxis = app->pi.plot.mXAxisSetup;
	xaxis.mLogScale = false;
	xaxis.mCrossOrigin = false;
	xaxis.mTickInfo.mAutoTick = false;
	xaxis.mTickInfo.mMajorTickSpan = 50;
	xaxis.mAutoScaleMax = false;
	xaxis.mAutoScaleMin = false;
	xaxis.mLabel = "テスト";
	auto &yaxis = app->pi.plot.mYAxisSetup;
	yaxis.mLogScale = false;
	yaxis.mAscending = true;
	yaxis.mCrossOrigin = false;
	yaxis.mTickInfo.mAutoTick = false;
	yaxis.mTickInfo.mMajorTickSpan = 50;
	yaxis.mAutoScaleMax = false;
	yaxis.mAutoScaleMin = false;
	yaxis.mLabel = "こんにちは世界";

	auto &grid = app->pi.plot.mGridInfo;
	grid.mXGridOn = true;
	grid.mYGridOn = true;

}

//　再描画の本処理
void redraw(Application *app, int view_width, int view_height, void *dibsect) {

	auto &ca = app->vi.ca;

	canvas_type canvas(view_width, view_height, static_cast<canvas_color_type *>(dibsect), view_width * view_height * 4);
	canvas.fill(canvas_color_type(255, 255, 255));

	// プロットエリアへの描画
	{
		// プロットエリアのサイズが width_plotarea, height_plotarea になるように、エリアの1ピクセル外側に枠を描く。
		mist::draw_rect(canvas, ca.plotarea_left - 1, ca.plotarea_top - 1, view_width - ca.offset_right, view_height - ca.offset_bottom, canvas_color_type(0, 0, 0));

		// 画像描画 nearest neighbor 内挿
		int dx = -ca.plotarea_left + app->vi.image_dx + app->vi.grab_dx, dy = -ca.plotarea_top + app->vi.image_dy + app->vi.grab_dy;
		double inv_zoom = 1.0 / app->vi.zoom;
		for (int j = ca.plotarea_top; j < ca.plotarea_top + ca.plotarea_height; ++j) {
			int jj = static_cast<int>(static_cast<double>(j + dy) * inv_zoom);
			if (jj < 0 || jj >= app->img_data.rgba.height()) continue;
			auto canvas_j = canvas.y_begin(j);
			auto img_data_jj = app->img_data.rgba.y_begin(jj);
			for (int i = ca.plotarea_left; i < ca.plotarea_left + ca.plotarea_width; ++i) {
				int ii = static_cast<int>(static_cast<double>(i + dx) * inv_zoom);
				if (ii < 0 || ii >= app->img_data.rgba.width()) continue;
				canvas_j[i] = img_data_jj[ii];
			}
		}
	
		// 軸の描画
		app->pi.painter.canvas = &canvas;
		{
			double inv_zoom = 1.0 / app->vi.zoom;
			auto &xaxis = app->pi.plot.mXAxisSetup;
			xaxis.SetMin(static_cast<float>((app->vi.image_dx + app->vi.grab_dx) * inv_zoom));
			xaxis.SetMax(static_cast<float>((app->vi.image_dx + app->vi.grab_dx + ca.plotarea_width) * inv_zoom));
			app->pi.plot.mXTickIterator->AdjustRange(xaxis.mMin, xaxis.mMax);
			auto &yaxis = app->pi.plot.mYAxisSetup;
			yaxis.SetMin(static_cast<float>((app->vi.image_dy + app->vi.grab_dy) * inv_zoom));
			yaxis.SetMax(static_cast<float>((app->vi.image_dy + app->vi.grab_dy + ca.plotarea_height) * inv_zoom));
			app->pi.plot.mYTickIterator->AdjustRange(yaxis.mMin, yaxis.mMax);
		}
		app->pi.plot.Draw(app->pi.painter);
	}
}

void adjust_to_boundary(Application *app) {
	auto &ca = app->vi.ca;
	double img_width = static_cast<double>(app->img_data.rgba.width()), img_height = static_cast<double>(app->img_data.rgba.height());
	double zm = app->vi.zoom;

	int image_dx_min = -app->vi.grab_dx, image_dy_min = -app->vi.grab_dy;
	int image_dx_max = static_cast<int>(img_width * zm) - ca.plotarea_width - app->vi.grab_dx, image_dy_max = static_cast<int>(img_height * zm) - ca.plotarea_height - app->vi.grab_dy;
	if (app->vi.image_dx < image_dx_min) app->vi.image_dx = image_dx_min;
	else if (app->vi.image_dx > image_dx_max) app->vi.image_dx = image_dx_max;
	if (app->vi.image_dy < image_dy_min) app->vi.image_dy = image_dy_min;
	else if (app->vi.image_dy > image_dy_max) app->vi.image_dy = image_dy_max;
}

void change_zoom(Application *app, double zoom_new_want, int px, int py) {
	double zc = app->vi.zoom, zn = zoom_new_want;
	auto &ca = app->vi.ca;

	double img_width = static_cast<double>(app->img_data.rgba.width()), img_height = static_cast<double>(app->img_data.rgba.height());

	double zm_x = static_cast<double>(ca.plotarea_width) / img_width;
	double zm_y = static_cast<double>(ca.plotarea_height) / img_height;
	if (zn < zm_x) zn = zm_x;
	if (zn < zm_y) zn = zm_y;

	if (zn == zc) {
		adjust_to_boundary(app);
	} else {
		double px_ca = static_cast<double>(px - ca.plotarea_left), py_ca = static_cast<double>(py - ca.plotarea_top);
		double zn_zc = zn - zc;
		app->vi.image_dx = static_cast<int>((zn_zc * px_ca + zn * app->vi.image_dx) / zc);
		app->vi.image_dy = static_cast<int>((zn_zc * py_ca + zn * app->vi.image_dy) / zc);
		app->vi.zoom = zn;
	}
}

DLLEXPORT void Application_View_Redraw(Application *app, HWND hWnd, int onpaint) {
	PAINTSTRUCT ps;
	HDC hdc;
	if (onpaint) {
		hdc = ::BeginPaint(hWnd, &ps);
	} else {
		hdc = app->vi.hDC;
	}
	RECT rt;
	::GetClientRect(hWnd, &rt);

	int view_width = rt.right - rt.left, view_height = rt.bottom - rt.top;
	app->vi.view_width = view_width, app->vi.view_height = view_height;
	app->vi.ca.update(view_width, view_height);

	BITMAPINFO bmp_info = { 0 };
	{
		auto &bmih = bmp_info.bmiHeader;
		bmih.biSize = sizeof(BITMAPINFOHEADER);
		bmih.biWidth = view_width, bmih.biHeight = -view_height;
		bmih.biPlanes = 1;
		bmih.biBitCount = 32;
		bmih.biCompression = BI_RGB;
	}
	HDC mem_dc = ::CreateCompatibleDC(hdc);

	void* dibsect = 0;

	HBITMAP bmp = ::CreateDIBSection( mem_dc, &bmp_info, DIB_RGB_COLORS, &dibsect, 0, 0 );
	HBITMAP temp = static_cast<HBITMAP>(::SelectObject(mem_dc, bmp));

	change_zoom(app, app->vi.zoom, 0, 0);
	redraw(app, view_width, view_height, dibsect);

	::BitBlt( hdc, rt.left, rt.top, view_width, view_height, mem_dc, 0, 0, SRCCOPY );
	::SelectObject(mem_dc, temp);
	::DeleteObject(bmp);
	::DeleteObject(mem_dc);

	if (onpaint) {
		::EndPaint(hWnd, &ps);
	}

}

DLLEXPORT void Application_View_OnMouseDown(Application *app, int button, int px, int py, int *invalidate_view) {
	*invalidate_view = 0;
	auto &ca = app->vi.ca;
	if (!ca.in_plotarea(px, py)) return;
	app->vi.grabbing = true;
	app->vi.grab_x = px, app->vi.grab_y = py;
	app->vi.grab_dx = 0, app->vi.grab_dy = 0;
	*invalidate_view = 1;
}

DLLEXPORT void Application_View_OnMouseMove(Application *app, int button, int px, int py, int *invalidate_view) {
	*invalidate_view = 0;
	if (!app->vi.grabbing) return;
	auto &ca = app->vi.ca;
	app->vi.grab_dx = app->vi.grab_x - px, app->vi.grab_dy = app->vi.grab_y - py;
	adjust_to_boundary(app);
	*invalidate_view = 1;
}

DLLEXPORT void Application_View_OnMouseUp(Application *app, int button, int px, int py, int *invalidate_view) {
	*invalidate_view = 0;
	app->vi.image_dx += app->vi.grab_dx, app->vi.image_dy += app->vi.grab_dy;
	app->vi.grab_dx = 0, app->vi.grab_dy = 0;
	app->vi.grabbing = false;
}

DLLEXPORT void Application_View_OnMouseWheel(Application *app, double delta, int px, int py, int *invalidate_view) {
	*invalidate_view = 0;
	app->vi.image_dx += app->vi.grab_dx, app->vi.image_dy += app->vi.grab_dy;
	app->vi.grab_dx = 0, app->vi.grab_dy = 0;
	if (app->vi.grabbing) app->vi.grab_x = px, app->vi.grab_y = py;

	change_zoom(app, app->vi.zoom * std::pow(2.0, delta * 0.25), px, py);
	*invalidate_view = 1;

}
