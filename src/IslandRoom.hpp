// IslandRoom.hpp — ROOM 擴充模組與 The Island 主模組共用的參數編號。
// ROOM 擴充模組放在 The Island 的左邊或右邊（緊貼），主模組就會讀取這裡的房間尺寸。
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace st { namespace roomexp {

enum ParamIds {
	W_PARAM = 0,   // 寬：左牆到右牆（公尺，或任意比例單位）
	D_PARAM,       // 深：前牆到後牆
	H_PARAM,       // 高：耳朵高度的喇叭到天花板喇叭
	ORBIT_PARAM,   // 繞場形狀：0 = FIT（貼合房間的橢圓）、1 = ROUND（真實空間的正圓）
	NUM_PARAMS
};

constexpr float DEFAULT_W = 10.f, DEFAULT_D = 10.f, DEFAULT_H = 5.f;   // 預設 10 × 10 × 5 = 正立方體比例（1 : 1 : 1）
constexpr float MIN_SIZE = 0.1f, MAX_SIZE = 100.f;                      // 旋鈕範圍（公尺）：0.1 ～ 100 m

// 顯示單位：內部一律是公尺，這裡只決定面板上顯示與輸入時的換算
enum Unit { UNIT_M = 0, UNIT_CM, UNIT_FT, UNIT_NONE, NUM_UNITS };
static const char* const UNIT_NAMES[NUM_UNITS] = {"Metres (m)", "Centimetres (cm)", "Feet (ft)", "No unit (plain ratio numbers)"};
static const char* const UNIT_SUFFIX[NUM_UNITS] = {" m", " cm", " ft", ""};
static const float UNIT_PER_METRE[NUM_UNITS] = {1.f, 100.f, 3.28084f, 1.f};
static const int UNIT_DECIMALS[NUM_UNITS] = {2, 0, 2, 2};   // 面板上顯示的小數位數

// 解析比例文字「寬 : 深 : 高」。分隔符號可以是 : x X , / 或空白；三個數字都要大於 0。
inline bool parseRatio(const char* text, float out[3]) {
	if (!text)
		return false;
	char buf[128];
	int n = 0;
	for (const char* p = text; *p && n < 127; p++) {
		char ch = *p;
		buf[n++] = (ch == ':' || ch == 'x' || ch == 'X' || ch == ',' || ch == '/' || ch == '*') ? ' ' : ch;
	}
	buf[n] = 0;
	float a = 0.f, b = 0.f, c = 0.f;
	char extra = 0;
	if (std::sscanf(buf, "%f %f %f %c", &a, &b, &c, &extra) != 3)
		return false;
	if (!(a > 0.f && b > 0.f && c > 0.f) || !std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c))
		return false;
	out[0] = a;
	out[1] = b;
	out[2] = c;
	return true;
}

// 讓三個尺寸都落在旋鈕範圍內：整組等比例縮放（比例不變）；比例本身太極端時才夾住個別數值
inline void fitRange(float size[3]) {
	float mx = std::max(size[0], std::max(size[1], size[2]));
	float mn = std::min(size[0], std::min(size[1], size[2]));
	float k = 1.f;
	if (mx > MAX_SIZE)
		k = MAX_SIZE / mx;
	else if (mn < MIN_SIZE)
		k = std::min(MIN_SIZE / mn, MAX_SIZE / mx);
	for (int i = 0; i < 3; i++)
		size[i] = std::min(MAX_SIZE, std::max(MIN_SIZE, size[i] * k));
}

}} // namespace st::roomexp
