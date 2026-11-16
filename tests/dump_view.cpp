// 輸出立方體各視角的投影結果（JSON），供 tools/panel_tool.py 畫預覽圖
#include <cstdio>
#include "../src/Panner714.hpp"
#include "../src/CubeView.hpp"
#include "../src/Layout714.hpp"
using namespace st;

int main() {
	const float w = layout::CUBE_W, h = layout::CUBE_H;
	// 12 個示範立體聲來源：沿耳朵高度圓周、高度交錯，L/R 寬度 0.18（最擁擠的情況：24 個點）
	float src[12][2][3];
	for (int t = 0; t < 12; t++) {
		float th = t / 12.f * 2.f * 3.14159265f;
		float cx = -0.8f * std::sin(th), cy = 0.8f * std::cos(th), cz = (t % 3 - 1) * 0.5f;
		float xl, xr;
		stereoSplit(cx, 0.18f, xl, xr);
		src[t][0][0] = xl; src[t][1][0] = xr;
		for (int s = 0; s < 2; s++) { src[t][s][1] = cy; src[t][s][2] = cz; }
	}
	// 要輸出的畫面：四個視角（12 聲道）＋ ISO 搭配 4 聲道、8 聲道配置
	struct V { int mode; int layout; };
	const V vs[6] = {{view::ISO, LAYOUT_12}, {view::TOP, LAYOUT_12}, {view::FRONT, LAYOUT_12}, {view::SIDE, LAYOUT_12},
	                 {view::ISO, LAYOUT_4}, {view::ISO, LAYOUT_8}};
	printf("{\"w\":%g,\"h\":%g,\"views\":[\n", w, h);
	for (int vi = 0; vi < 6; vi++) {
		int m = vs[vi].mode;
		bool mask[NCH];
		layoutMask(vs[vi].layout, mask);
		printf("{\"name\":\"%s\",\"out\":%d,\"edges\":[", view::modeName(m), layoutCount(vs[vi].layout));
		for (int e = 0; e < 12; e++) {
			float ax, ay, az, bx, by, bz;
			view::corner(view::EDGES[e][0], ax, ay, az);
			view::corner(view::EDGES[e][1], bx, by, bz);
			auto a = view::project(m, ax, ay, az, w, h), b = view::project(m, bx, by, bz, w, h);
			printf("[%g,%g,%g,%g]%s", a.x, a.y, b.x, b.y, e < 11 ? "," : "");
		}
		printf("],\"speakers\":[");
		bool first = true;
		for (const auto& g : view::speakerLabels(m, w, h, mask)) {
			printf("%s{\"n\":\"%s\",\"x\":%g,\"y\":%g,\"top\":%d}", first ? "" : ",", g.text.c_str(), g.x, g.y, g.top);
			first = false;
		}
		printf("],\"sources\":[");
		for (int t = 0; t < 12; t++) {
			printf("%s{\"t\":%d,\"c\":[%d,%d,%d],\"pts\":[", t ? "," : "", t + 1,
			       view::TRACK_COLORS[t].r, view::TRACK_COLORS[t].g, view::TRACK_COLORS[t].b);
			for (int s = 0; s < 2; s++) {
				auto q = view::project(m, src[t][s][0], src[t][s][1], src[t][s][2], w, h);
				auto f = view::project(m, src[t][s][0], src[t][s][1], -1.f, w, h);
				printf("%s{\"x\":%g,\"y\":%g,\"d\":%g,\"fx\":%g,\"fy\":%g}", s ? "," : "", q.x, q.y, q.depth, f.x, f.y);
			}
			printf("]}");
		}
		printf("]}%s\n", vi + 1 < 6 ? "," : "");
	}
	printf("]}\n");
}
