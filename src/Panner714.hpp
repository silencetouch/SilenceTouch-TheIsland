// Panner714.hpp — 7.1.4 空間化核心演算法（不依賴 VCV Rack，可獨立測試）
//
// 座標系（房間立方體，邊長 2，中心為聆聽位置）：
//   x = 左(-) / 右(+)    y = 後(-) / 前(+)    z = 下(-) / 上(+)    範圍皆為 -1..1
//   耳朵高度平面 z = 0；頂部喇叭在天花板 z = +1。
// 聲道順序：SMPTE / Dolby 7.1.4（與 H9000 USB 播放通道 1–12 一致）：
//   1 L, 2 R, 3 C, 4 LFE, 5 Ls, 6 Rs, 7 Lrs, 8 Rrs, 9 Ltf, 10 Rtf, 11 Ltr, 12 Rtr
#pragma once
#include <cmath>
#include <algorithm>

namespace st {

constexpr int NCH = 12;

enum Ch {
	CH_L = 0, CH_R, CH_C, CH_LFE, CH_LS, CH_RS, CH_LRS, CH_RRS,
	CH_LTF, CH_RTF, CH_LTR, CH_RTR
};

static const char* const CH_NAMES[NCH] = {
	"L", "R", "C", "LFE", "Ls", "Rs", "Lrs", "Rrs", "Ltf", "Rtf", "Ltr", "Rtr"
};

struct Vec3 {
	float x, y, z;
};

// 喇叭在房間立方體中的位置。
//   前方 L/R 在前牆（方位 ±30°），C 在前牆中央，Ls/Rs 在左右牆中央（±90°），
//   Lrs/Rrs 在後方兩個角落（±135°），頂部四支在天花板 (±0.6, ±0.6, 1)（方位 ±45° / ±135°）。
//   LFE 不參與空間定位，座標僅為佔位。
static const Vec3 SPEAKER_POS[NCH] = {
	{-0.577f,  1.0f, 0.0f},   // 1  L
	{ 0.577f,  1.0f, 0.0f},   // 2  R
	{ 0.0f,    1.0f, 0.0f},   // 3  C
	{ 0.0f,    0.0f, 0.0f},   // 4  LFE（不使用）
	{-1.0f,    0.0f, 0.0f},   // 5  Ls
	{ 1.0f,    0.0f, 0.0f},   // 6  Rs
	{-1.0f,   -1.0f, 0.0f},   // 7  Lrs
	{ 1.0f,   -1.0f, 0.0f},   // 8  Rrs
	{-0.6f,    0.6f, 1.0f},   // 9  Ltf
	{ 0.6f,    0.6f, 1.0f},   // 10 Rtf
	{-0.6f,   -0.6f, 1.0f},   // 11 Ltr
	{ 0.6f,   -0.6f, 1.0f},   // 12 Rtr
};

inline Vec3 speakerPos(int ch) {
	return SPEAKER_POS[ch];
}

// 輸出配置：實際接了幾支喇叭。未使用的喇叭不參與聲像計算（增益 0）。
//   LAYOUT_4  ：L R Lrs Rrs（只有前左右與後左右）
//   LAYOUT_8  ：L R C LFE Ls Rs Lrs Rrs（7.1）
//   LAYOUT_12 ：完整 7.1.4
//   LAYOUT_51 ：L R C LFE Ls Rs（5.1，輸出 1～6）——加在最後，舊 patch 存的編號不變
// 輸出順序一律維持 SMPTE 相對順序（例如 4 聲道時為 1 L、2 R、3 Lrs、4 Rrs）。
enum OutLayout { LAYOUT_4 = 0, LAYOUT_8, LAYOUT_12, LAYOUT_51, NUM_LAYOUTS };

// 選單上的排列順序（由少到多）
static const int LAYOUT_MENU_ORDER[NUM_LAYOUTS] = {LAYOUT_4, LAYOUT_51, LAYOUT_8, LAYOUT_12};

inline int layoutCount(int layout) {
	return layout == LAYOUT_4 ? 4 : layout == LAYOUT_51 ? 6 : layout == LAYOUT_8 ? 8 : 12;
}

inline const char* layoutName(int layout) {
	return layout == LAYOUT_4 ? "4 ch (L R Lrs Rrs)" : layout == LAYOUT_51 ? "6 ch (5.1: L R C LFE Ls Rs)" : layout == LAYOUT_8 ? "8 ch (7.1)" : "12 ch (7.1.4)";
}

// 顯示用的短名稱
inline const char* layoutShort(int layout) {
	return layout == LAYOUT_4 ? "4CH" : layout == LAYOUT_51 ? "5.1" : layout == LAYOUT_8 ? "7.1" : "7.1.4";
}

// 取得此配置中的聲道（SMPTE 索引，由小到大），回傳數量
inline int layoutChannels(int layout, int idx[NCH]) {
	static const int l4[4] = {CH_L, CH_R, CH_LRS, CH_RRS};
	static const int l8[8] = {CH_L, CH_R, CH_C, CH_LFE, CH_LS, CH_RS, CH_LRS, CH_RRS};
	static const int l51[6] = {CH_L, CH_R, CH_C, CH_LFE, CH_LS, CH_RS};
	int n = layoutCount(layout);
	for (int i = 0; i < n; i++)
		idx[i] = layout == LAYOUT_4 ? l4[i] : layout == LAYOUT_51 ? l51[i] : layout == LAYOUT_8 ? l8[i] : i;
	return n;
}

inline void layoutMask(int layout, bool mask[NCH]) {
	int idx[NCH];
	int n = layoutChannels(layout, idx);
	for (int i = 0; i < NCH; i++)
		mask[i] = false;
	for (int i = 0; i < n; i++)
		mask[idx[i]] = true;
}

inline float clamp1(float v) {
	return std::max(-1.f, std::min(1.f, v));
}

// 整體水平旋轉（度，正值 = 由前向左逆時針，從上往下看）。
// 旋轉後各軸夾回 -1..1（半徑 ≤ 1 的圓周軌跡旋轉後不會被夾住）。
inline void rotateXY(float x, float y, float deg, float& xr, float& yr) {
	const float d2r = 3.14159265358979f / 180.f;
	float c = std::cos(deg * d2r), s = std::sin(deg * d2r);
	xr = clamp1(x * c - y * s);
	yr = clamp1(x * s + y * c);
}

// 立體聲輸入：以中心 x 與寬度 width 把 L / R 兩個聲源分開（x 向兩側各 width，夾回 -1..1）。
// 兩個聲源之後各自旋轉、各自計算增益，所以立體聲場會跟著整體旋轉。
inline void stereoSplit(float x, float width, float& xl, float& xr) {
	xl = clamp1(x - width);
	xr = clamp1(x + width);
}

// 立體聲展開，並把 L/R 的連線方向一起旋轉 deg 度（繞場時立體聲場會跟著轉）。
// deg = 0 時與 stereoSplit 相同。
inline void stereoSplitRotated(float cx, float cy, float width, float deg, float& xl, float& yl, float& xr, float& yr) {
	const float d2r = 3.14159265358979f / 180.f;
	float ox = width * std::cos(deg * d2r), oy = width * std::sin(deg * d2r);
	xl = clamp1(cx - ox);
	yl = clamp1(cy - oy);
	xr = clamp1(cx + ox);
	yr = clamp1(cy + oy);
}

// ---------------------------------------------------------------------------
// 旋轉軸：繞場與整體旋轉都可以選擇繞哪一個軸轉。
//   AXIS_Z：繞垂直軸，水平轉圈（前 → 左 → 後 → 右），高度不變（預設）
//   AXIS_X：繞左右軸，垂直轉圈（前 → 天花板 → 後 → 地板），左右位置不變
//   AXIS_Y：繞前後軸，垂直轉圈（右 → 天花板 → 左 → 地板），前後位置不變
// 實作方式：把 (x, y, z) 換成旋轉平面上的 (u, v) 與軸上的座標 w，
// 在平面上用與水平旋轉相同的公式轉，再換回來。正角度 = 上面列出的方向。
// ---------------------------------------------------------------------------
enum Axis { AXIS_Z = 0, AXIS_X, AXIS_Y, NUM_AXES };

inline const char* axisName(int axis) {
	return axis == AXIS_X ? "X" : axis == AXIS_Y ? "Y" : "Z";
}

inline void toPlane(int axis, float x, float y, float z, float& u, float& v, float& w) {
	switch (axis) {
		case AXIS_X: u = -z; v = y; w = x; break;
		case AXIS_Y: u = -z; v = x; w = y; break;
		default: u = x; v = y; w = z; break;
	}
}

inline void fromPlane(int axis, float u, float v, float w, float& x, float& y, float& z) {
	switch (axis) {
		case AXIS_X: z = -u; y = v; x = w; break;
		case AXIS_Y: z = -u; x = v; y = w; break;
		default: x = u; y = v; z = w; break;
	}
}

// 繞指定的軸旋轉 deg 度（AXIS_Z 時與 rotateXY 完全相同）
inline void rotateAxis(int axis, float& x, float& y, float& z, float deg) {
	float u, v, w, ur, vr;
	toPlane(axis, x, y, z, u, v, w);
	rotateXY(u, v, deg, ur, vr);
	fromPlane(axis, ur, vr, clamp1(w), x, y, z);
}

// ---------------------------------------------------------------------------
// 繞場（orbit）：聲源以房間中心為圓心，在 XY 平面上繞一圈，Z（高度）不變。
// 起點方向取聲源目前的方位；半徑在一圈的前後 12% 之間，從目前距離平滑過渡到設定半徑，
// 所以開始與結束時位置不會跳動，繞完一圈剛好回到原位。
// ---------------------------------------------------------------------------
inline float orbitEnvelope(float phase) {
	const float edge = 0.12f;
	float e = std::min(phase, 1.f - phase) / edge;
	e = std::max(0.f, std::min(1.f, e));
	return e * e * (3.f - 2.f * e);   // smoothstep
}

// (x, y)：聲源目前位置；radius：繞場半徑（0..1，1 = 碰到四面牆）；
// thetaDeg：已經轉了幾度（正值 = 逆時針，往左）；env：半徑過渡（0 = 目前距離，1 = 設定半徑）
inline void orbitPosition(float x, float y, float radius, float thetaDeg, float env, float& ox, float& oy) {
	float d0 = std::sqrt(x * x + y * y);
	float ux = 0.f, uy = 1.f;   // 在正中央時，從正前方開始
	if (d0 > 1e-4f) {
		ux = x / d0;
		uy = y / d0;
	}
	float r = d0 + (radius - d0) * env;
	rotateXY(ux * r, uy * r, thetaDeg, ox, oy);
}

// 繞場的狀態（每軌一個）。
//   繞一圈模式：按一下 → 繞一圈後停在原位；繞的途中再按不理會。
//   持續模式  ：按一下 → 一直繞；再按一下 → 把目前這一圈繞完（至少再走 12%，讓半徑平滑收回）後停在原位。
// phase 以「圈」為單位累加；stopAt 是要停下的圈數（持續模式未要求停止時為無限大）。
// 半徑過渡：開始的前 12% 圈從目前距離漸變到設定半徑，停止前的最後 12% 圈再漸變回來。
constexpr float ORBIT_EDGE = 0.12f;   // 起始／停止的半徑過渡長度（圈）
constexpr float ORBIT_NEVER = 1e9f;   // 持續模式：沒有要停

struct OrbitState {
	bool active = false;
	float phase = 0.f;
	float stopAt = 1.f;

	void requestStop() {
		if (active && stopAt >= ORBIT_NEVER)
			stopAt = std::ceil(phase + ORBIT_EDGE);
	}
	// 按下 ORB 按鈕
	void trigger(bool loop) {
		if (!active) {
			active = true;
			phase = 0.f;
			stopAt = loop ? ORBIT_NEVER : 1.f;
		}
		else if (loop) {
			requestStop();
		}
	}
	// dPhase：這段時間走了幾圈（dt / 一圈的秒數）
	void update(float dPhase, bool loop) {
		if (!active)
			return;
		if (!loop)
			requestStop();   // 持續繞的途中改成「繞一圈」模式 → 繞完這圈就停
		phase += dPhase;
		if (phase >= stopAt) {
			active = false;
			phase = 0.f;
			return;
		}
		// 持續模式長時間運轉時，把圈數往回減，避免浮點數越來越不準（保持 >= 1，讓起始過渡已完成）
		if (stopAt >= ORBIT_NEVER && phase >= 2.f)
			phase -= 1.f;
	}
	bool stopping() const { return active && stopAt < ORBIT_NEVER && stopAt > 1.f; }
	// 半徑過渡量（0 = 目前距離，1 = 設定半徑）
	float envelope() const {
		if (!active)
			return 0.f;
		float e = std::min(phase / ORBIT_EDGE, (stopAt - phase) / ORBIT_EDGE);
		e = std::max(0.f, std::min(1.f, e));
		return e * e * (3.f - 2.f * e);
	}
	// 目前角度（圈數的小數部分，0..1）
	float turn() const {
		return active ? phase - std::floor(phase) : 0.f;
	}
};

// 繞一圈要幾秒：每軌 SPD 旋鈕（0..1，往右越快：60 秒 ～ 1 秒，指數刻度）
// 除以全域 ORB RATE 倍率（旋鈕 -1..1 → 0.25× ～ 4×）
inline float orbitSeconds(float speedKnob, float rateKnob) {
	float seconds = 60.f * std::pow(1.f / 60.f, speedKnob);
	float rate = std::pow(4.f, rateKnob);
	return seconds / rate;
}

// 繞指定軸的繞場：在該軸的旋轉平面上繞圈，軸上的座標不變
inline void orbitPosition3D(int axis, float x, float y, float z, float radius, float thetaDeg, float env,
                            float& ox, float& oy, float& oz) {
	float u, v, w, ur, vr;
	toPlane(axis, x, y, z, u, v, w);
	orbitPosition(u, v, radius, thetaDeg, env, ur, vr);
	fromPlane(axis, ur, vr, w, ox, oy, oz);
}

// 靜音／獨奏：只要有任何一軌開 Solo，就只聽得到有開 Solo 的軌道；開了 Mute 的軌道一定沒聲音
// （同時開 Mute 和 Solo 時以 Mute 為準）。
inline bool trackAudible(bool mute, bool solo, bool anySolo) {
	return !mute && (!anySolo || solo);
}

// 計算空間聲道的增益（LFE 恆為 0），能量歸一化：sum(g^2) = 1。
// mask 不為空時，只在 mask 為 true 的喇叭之間分配（其餘增益為 0）。
//   focus  定位集中度（距離衰減指數，約 1..6，越大越集中在最近的喇叭）
//   blur   空間模糊半徑（約 0.05..0.5，避免聲源貼近喇叭時單一喇叭獨佔）
// ---------------------------------------------------------------------------
// 房間比例（ROOM 擴充模組）
// 聲像座標一律是正規化的 −1～+1 方盒子；Room 記錄的是「每一個座標單位在真實空間裡有多長」的比例，
// 最長的那一軸 = 1。沒有 ROOM 擴充模組時三軸都是 1（正立方體），計算結果和以前完全一樣。
//   寬 W：左牆到右牆（Ls ↔ Rs）的距離        → x 軸一個單位 = W / 2
//   深 D：前牆到後牆（L/R ↔ Lrs/Rrs）的距離   → y 軸一個單位 = D / 2
//   高 H：耳朵高度的喇叭到天花板喇叭的高度     → z 軸一個單位 = H
// 只有三者的「比例」會影響聲像（整體放大縮小不影響），所以填公尺或填比例都可以。
// ---------------------------------------------------------------------------
struct Room {
	float sx = 1.f, sy = 1.f, sz = 1.f;
	bool roundOrbit = false;   // 繞場走真實空間裡的正圓（否則是貼合房間的橢圓）
	bool isCube() const { return sx == 1.f && sy == 1.f && sz == 1.f; }
};

inline Room roomFromSize(float w, float d, float h, bool roundOrbit = false) {
	float ux = std::max(w, 1e-3f) * 0.5f, uy = std::max(d, 1e-3f) * 0.5f, uz = std::max(h, 1e-3f);
	float m = std::max(ux, std::max(uy, uz));
	Room r;
	r.sx = ux / m;
	r.sy = uy / m;
	r.sz = uz / m;
	r.roundOrbit = roundOrbit;
	return r;
}

// 繞場位置（支援房間比例）。roundOrbit 時在真實空間裡走正圓：
// 半徑以繞場平面上較短的那一邊為準（100% = 碰到較近的牆），再換回正規化座標。
inline void orbitPositionRoom(int axis, float x, float y, float z, float radius, float thetaDeg, float env,
                              const Room* room, float& ox, float& oy, float& oz) {
	if (!room || !room->roundOrbit || room->isCube()) {
		orbitPosition3D(axis, x, y, z, radius, thetaDeg, env, ox, oy, oz);
		return;
	}
	float u, v, w, su, sv, sw, ur, vr;
	toPlane(axis, x, y, z, u, v, w);
	toPlane(axis, room->sx, room->sy, room->sz, su, sv, sw);
	su = std::fabs(su);   // toPlane 會把某些軸變號，比例只取大小
	sv = std::fabs(sv);
	float smin = std::min(su, sv);
	orbitPosition(u * su, v * sv, radius * smin, thetaDeg, env, ur, vr);   // 真實比例下的正圓
	fromPlane(axis, clamp1(ur / su), clamp1(vr / sv), w, ox, oy, oz);
}

inline void computeGains(float x, float y, float z, float focus, float blur, float g[NCH], const bool* mask = nullptr,
                         const Room* room = nullptr) {
	x = clamp1(x);
	y = clamp1(y);
	z = clamp1(z);

	float sumSq = 0.f;
	for (int i = 0; i < NCH; i++) {
		if (i == CH_LFE || (mask && !mask[i])) {
			g[i] = 0.f;
			continue;
		}
		Vec3 sp = SPEAKER_POS[i];
		float dx = x - sp.x, dy = y - sp.y, dz = z - sp.z;
		if (room) {   // 換成真實空間的比例再算距離
			dx *= room->sx;
			dy *= room->sy;
			dz *= room->sz;
		}
		float d2 = dx * dx + dy * dy + dz * dz + blur * blur;
		float gi = std::pow(d2, -0.5f * focus);   // g = 1 / d^focus
		g[i] = gi;
		sumSq += gi * gi;
	}
	float norm = sumSq > 0.f ? 1.f / std::sqrt(sumSq) : 0.f;
	for (int i = 0; i < NCH; i++)
		g[i] *= norm;
}

// 兩級一階低通（12 dB/oct），用於 LFE 送出
struct LfeFilter {
	float a = 0.f, z1 = 0.f, z2 = 0.f;
	void setCutoff(float fc, float sampleRate) {
		a = 1.f - std::exp(-2.f * 3.14159265358979f * fc / sampleRate);
	}
	void reset() { z1 = z2 = 0.f; }
	float process(float in) {
		z1 += a * (in - z1);
		z2 += a * (z1 - z2);
		return z2;
	}
};

// ---------------------------------------------------------------------------
// 電平指示
// VCV 的 Audio 模組把 10 V 視為數位滿刻度（0 dBFS），超過就會被夾掉（爆音）。
// ---------------------------------------------------------------------------
constexpr float CLIP_V = 10.f;     // 0 dBFS：紅燈
constexpr float HOT_V = 5.f;       // -6 dBFS：黃燈
constexpr float SIGNAL_V = 0.01f;  // -60 dBFS 以上：綠燈
constexpr float CLIP_HOLD_S = 1.5f;

enum MeterState { METER_OFF = 0, METER_SIGNAL, METER_HOT, METER_CLIP };

struct PeakMeter {
	float peak = 0.f;       // 峰值（會慢慢回落）
	float clipHold = 0.f;   // 爆音後紅燈維持的剩餘秒數

	// decay：每個取樣的峰值回落係數；dt：取樣時間（秒）
	void process(float x, float decay, float dt) {
		float a = std::fabs(x);
		peak = std::max(a, peak * decay);
		if (a >= CLIP_V)
			clipHold = CLIP_HOLD_S;
		else if (clipHold > 0.f)
			clipHold -= dt;
	}
	void reset() { peak = 0.f; clipHold = 0.f; }
	int state() const {
		if (clipHold > 0.f || peak >= CLIP_V) return METER_CLIP;
		if (peak >= HOT_V) return METER_HOT;
		if (peak >= SIGNAL_V) return METER_SIGNAL;
		return METER_OFF;
	}
};

// 安全限幅器：8 V 以下完全不動；8 V 以上柔和壓縮，最大不超過 10 V（0 dBFS）。
// 在 8 V 處斜率為 1，曲線連續，不會產生硬切的爆音。
inline float softLimit(float x) {
	const float knee = 8.f, room = CLIP_V - knee;
	float a = std::fabs(x);
	if (a <= knee)
		return x;
	float y = knee + room * std::tanh((a - knee) / room);
	return x < 0.f ? -y : y;
}

} // namespace st
