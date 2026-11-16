// CubeView.hpp — 立方體顯示的投影與配色（不依賴 VCV Rack，介面與預覽共用同一份程式碼）
#pragma once
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include "Panner714.hpp"

namespace st { namespace view {

constexpr int NCH_ = st::NCH;

enum Mode { ISO = 0, TOP, FRONT, SIDE, NUM_MODES };

inline const char* modeName(int m) {
	switch (m) {
		case ISO: return "ISO";
		case TOP: return "TOP";
		case FRONT: return "FRONT";
		case SIDE: return "SIDE";
	}
	return "?";
}

struct Pt {
	float x, y;    // 顯示區內的像素座標
	float depth;   // 越大越遠（用於由遠到近排序）
};

// 房間座標 (x 右, y 前, z 上) → 顯示區 (w × h 像素) 內的位置。
//   ISO   ：從房間後方偏右上方往前看（舞台在遠端、左喇叭在畫面左側）
//   TOP   ：由上往下看，前方在畫面上方
//   FRONT ：從後方往前看（x 向右、z 向上）
//   SIDE  ：從右側往左看（前方在畫面右側）
inline Pt project(int mode, float x, float y, float z, float w, float h) {
	const float d2r = 3.14159265358979f / 180.f;
	float cx = w * 0.5f, cy = h * 0.5f;
	Pt p;
	switch (mode) {
		case TOP: {
			float sc = std::min(w * 0.5f, h * 0.5f) - 30.f;
			p.x = cx + sc * x;
			p.y = cy - sc * y;
			p.depth = -z;
		} break;
		case FRONT: {
			float sc = std::min(w * 0.5f, h * 0.5f) - 30.f;
			p.x = cx + sc * x;
			p.y = cy - sc * z;
			p.depth = y;
		} break;
		case SIDE: {
			float sc = std::min(w * 0.5f, h * 0.5f) - 30.f;
			p.x = cx + sc * y;
			p.y = cy - sc * z;
			p.depth = -x;
		} break;
		case ISO:
		default: {
			const float yaw = 30.f * d2r, pitch = 22.f * d2r;
			float rx = x * std::cos(yaw) - y * std::sin(yaw);     // 畫面向右
			float dp = x * std::sin(yaw) + y * std::cos(yaw);     // 離觀察者的距離
			float up = z * std::cos(pitch) + dp * std::sin(pitch);
			float extentX = std::cos(yaw) + std::sin(yaw);
			float extentY = std::cos(pitch) + extentX * std::sin(pitch);
			float sc = std::min((w * 0.5f - 12.f) / extentX, (h * 0.5f - 12.f) / extentY);
			p.x = cx + sc * rx;
			p.y = cy - sc * up;
			p.depth = dp;
		} break;
	}
	return p;
}

// 喇叭標籤分組：在 FRONT / SIDE 等視角中，前後排列的喇叭會投影到同一點，
// 把重疊的喇叭合併成一個標籤（例如 "Ls/Lrs"），避免文字疊在一起。LFE 不顯示。
struct SpeakerLabel {
	float x, y;
	std::string text;
	bool top;
};

inline std::vector<SpeakerLabel> speakerLabels(int mode, float w, float h, const bool* mask = nullptr,
                                               float sx = 1.f, float sy = 1.f, float sz = 1.f) {
	std::vector<SpeakerLabel> out;
	for (int c = 0; c < NCH_; c++) {
		if (c == st::CH_LFE || (mask && !mask[c]))
			continue;
		st::Vec3 sp = st::speakerPos(c);
		Pt p = project(mode, sp.x * sx, sp.y * sy, sp.z * sz, w, h);   // sx/sy/sz：房間比例（ROOM 擴充模組）
		bool top = sp.z > 0.5f;
		bool merged = false;
		for (auto& g : out) {
			if (g.top == top && std::fabs(g.x - p.x) < 4.f && std::fabs(g.y - p.y) < 4.f) {
				g.text += std::string("/") + st::CH_NAMES[c];
				merged = true;
				break;
			}
		}
		if (!merged)
			out.push_back({p.x, p.y, st::CH_NAMES[c], top});
	}
	return out;
}

// 立方體 8 個角（bit0 = x, bit1 = y, bit2 = z）與 12 條邊
inline void corner(int i, float& x, float& y, float& z) {
	x = (i & 1) ? 1.f : -1.f;
	y = (i & 2) ? 1.f : -1.f;
	z = (i & 4) ? 1.f : -1.f;
}

static const int EDGES[12][2] = {
	{0, 1}, {2, 3}, {4, 5}, {6, 7},
	{0, 2}, {1, 3}, {4, 6}, {5, 7},
	{0, 4}, {1, 5}, {2, 6}, {3, 7},
};

struct Rgb {
	unsigned char r, g, b;
};

// 12 個輸入各自的顏色（面板編號、立方體上的點使用同一組顏色）
static const Rgb TRACK_COLORS[12] = {
	// 印刷感的色調（包浩斯海報常見的朱紅、赭黃、鈷藍……），在紙白底上也分得清楚
	{214, 69, 43},    // 1 朱紅
	{232, 128, 40},   // 2 橘
	{232, 178, 30},   // 3 赭黃
	{150, 168, 52},   // 4 橄欖綠
	{58, 150, 92},    // 5 綠
	{0, 136, 136},    // 6 深觸綠
	{72, 152, 210},   // 7 天藍
	{36, 78, 168},    // 8 鈷藍
	{118, 78, 168},   // 9 紫
	{188, 64, 136},   // 10 洋紅
	{232, 122, 142},  // 11 玫瑰
	{88, 84, 78},     // 12 炭灰
};

// 色塊上的編號用黑字還是白字（依亮度決定，深色塊用白字）
inline bool darkText(const Rgb& c) {
	return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b > 140.f;
}

}} // namespace st::view
