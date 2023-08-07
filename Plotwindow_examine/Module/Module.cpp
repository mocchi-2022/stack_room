#define DLLEXPORT extern "C" __declspec(dllexport)

#include <cstdint>

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
	const color_type &operator () (int x, int y) const{
		if (x < 0 || y < 0 || x >= rgba.width() || y >= rgba.height()) return color_type();
		return rgba(x, y);
	}
};

struct Application {
	stb_image img_data;

	font_renderer::font_renderer fr;
	struct ViewInfo {
		HDC hDC;
		double zoom;
		bool grabbing;
		int grab_x, grab_y, grab_dx, grab_dy, image_dx, image_dy;
		int view_width, view_height;
		struct chart_area {
			const int plotarea_left = 10, plotarea_top = 5, offset_right = 20, offset_bottom = 30;
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
}

//　再描画の本処理
void redraw(Application *app, int view_width, int view_height, void *dibsect) {
	typedef mist::bgra<uint8_t> dibcolor_type;

	auto &ca = app->vi.ca;

	mist::array2<dibcolor_type> canvas(view_width, view_height, static_cast<dibcolor_type *>(dibsect), view_width * view_height * 4);
	canvas.fill(dibcolor_type(255, 255, 255));

	// プロットエリアへの描画
	{
		// プロットエリアのサイズが width_plotarea, height_plotarea になるように、エリアの1ピクセル外側に枠を描く。
		mist::draw_rect(canvas, ca.plotarea_left - 1, ca.plotarea_top - 1, view_width - ca.offset_right, view_height - ca.offset_bottom, dibcolor_type(0, 0, 0));

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
	
		// テキスト描画
		wchar_t txt[] = L"こんにちは世界";
		app->fr.render(txt, 12, false, false, true);
		auto text_img = reinterpret_cast<const mist::rgb<uint8_t> *>(app->fr.get_image());
		for (int j = 0; j < app->fr.get_height(); ++j) {
			auto *text_img_j = text_img + j * app->fr.get_width();
			auto canvas_j = canvas.y_begin(j);
			for (int i = 0; i < app->fr.get_width(); ++i) {
				canvas_j[i] = text_img_j[i];
			}
		}

		// Todo:マーカー描画

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
