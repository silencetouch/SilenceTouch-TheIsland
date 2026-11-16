// 聲像演算法、旋轉、顯示投影的獨立測試（不需要 VCV Rack）
#include <cstdio>
#include <cmath>
#include "../src/Panner714.hpp"
#include "../src/IslandRoom.hpp"
#include "../src/CubeView.hpp"
using namespace st;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

static int argmaxCh(const float* g) {
	int m = 0;
	for (int i = 1; i < NCH; i++) if (g[i] > g[m]) m = i;
	return m;
}

static float secondShare(const float* g, int best) {
	float m2 = 0;
	for (int i = 0; i < NCH; i++) if (i != best && g[i] > m2) m2 = g[i];
	return m2;
}

int main() {
	float g[NCH];
	const float focus = 3.f, blur = 0.2f;   // 與模組預設值相同

	// 1) 喇叭座標：順序與數量
	CHECK(NCH == 12, "12 channels");
	CHECK(std::string(CH_NAMES[0]) == "L" && std::string(CH_NAMES[3]) == "LFE" && std::string(CH_NAMES[11]) == "Rtr",
	      "SMPTE channel name order");
	for (int c = 0; c < NCH; c++) {
		Vec3 p = speakerPos(c);
		CHECK(std::fabs(p.x) <= 1.f && std::fabs(p.y) <= 1.f && p.z >= 0.f && p.z <= 1.f, "speaker inside room");
	}
	// 方位角檢查：由喇叭座標反推，應為 L=+30° R=-30° C=0 Ls=+90 Rs=-90 Lrs=+135 Rrs=-135（左為正）
	const struct { Ch c; float az; } azs[] = {{CH_L, 30}, {CH_R, -30}, {CH_C, 0}, {CH_LS, 90}, {CH_RS, -90},
	                                          {CH_LRS, 135}, {CH_RRS, -135}, {CH_LTF, 45}, {CH_RTF, -45},
	                                          {CH_LTR, 135}, {CH_RTR, -135}};
	for (auto& a : azs) {
		Vec3 p = speakerPos(a.c);
		float az = std::atan2(-p.x, p.y) * 180.f / 3.14159265f;
		char msg[64];
		snprintf(msg, sizeof msg, "azimuth of %s is %.1f, expected %.0f", CH_NAMES[a.c], az, a.az);
		CHECK(std::fabs(az - a.az) < 1.0f, msg);
	}

	// 2) 能量歸一化：整個立方體內 sum(g^2) = 1，LFE = 0，無 NaN
	int bad = 0, n = 0;
	for (float x = -1; x <= 1.001f; x += 0.125f)
		for (float y = -1; y <= 1.001f; y += 0.125f)
			for (float z = -1; z <= 1.001f; z += 0.125f) {
				computeGains(x, y, z, focus, blur, g);
				float s = 0;
				for (int i = 0; i < NCH; i++) { s += g[i] * g[i]; if (!std::isfinite(g[i]) || g[i] < 0) bad++; }
				if (std::fabs(s - 1.f) > 1e-4f || g[CH_LFE] != 0.f) bad++;
				n++;
			}
	printf("energy check over %d grid points: %d bad\n", n, bad);
	CHECK(bad == 0, "energy normalisation / LFE zero / finite");

	// 3) 聲源放在各喇叭位置 → 該喇叭最大（SMPTE 對應正確）
	printf("\nsource placed on each speaker:\n");
	for (int c = 0; c < NCH; c++) {
		if (c == CH_LFE) continue;
		Vec3 p = speakerPos(c);
		computeGains(p.x, p.y, p.z, focus, blur, g);
		printf("  %-4s (out %2d)  own gain %.3f   strongest other %.3f\n", CH_NAMES[c], c + 1, g[c], secondShare(g, c));
		char msg[64];
		snprintf(msg, sizeof msg, "source at %s -> max is %s", CH_NAMES[c], CH_NAMES[argmaxCh(g)]);
		CHECK(argmaxCh(g) == c, msg);
		CHECK(g[c] > 0.9f, "speaker position is well localised");
	}

	// 4) 左右對稱：x 取反 → 左右聲道互換
	const int pairs[][2] = {{CH_L, CH_R}, {CH_LS, CH_RS}, {CH_LRS, CH_RRS}, {CH_LTF, CH_RTF}, {CH_LTR, CH_RTR}};
	float a[NCH], b[NCH];
	computeGains(0.6f, 0.3f, 0.4f, focus, blur, a);
	computeGains(-0.6f, 0.3f, 0.4f, focus, blur, b);
	for (auto& pr : pairs)
		CHECK(std::fabs(a[pr[0]] - b[pr[1]]) < 1e-4f && std::fabs(a[pr[1]] - b[pr[0]]) < 1e-4f, "left-right symmetry");
	CHECK(std::fabs(a[CH_C] - b[CH_C]) < 1e-4f, "centre symmetric");

	// 5) 方向性
	computeGains(0.f, 0.95f, 0.f, focus, blur, g);
	CHECK(argmaxCh(g) == CH_C, "front-centre source -> C");
	computeGains(0.f, -0.9f, 0.f, focus, blur, g);
	CHECK(g[CH_LRS] + g[CH_RRS] > 2.f * (g[CH_L] + g[CH_R]), "rear source -> rear speakers");
	computeGains(0.f, 0.f, 1.f, focus, blur, g);
	float top = g[CH_LTF] + g[CH_RTF] + g[CH_LTR] + g[CH_RTR];
	float ear = g[CH_L] + g[CH_R] + g[CH_C] + g[CH_LS] + g[CH_RS] + g[CH_LRS] + g[CH_RRS];
	CHECK(top > 2.f * ear, "overhead source -> top speakers dominate");
	computeGains(0.f, 0.f, -1.f, focus, blur, g);
	float topE = g[CH_LTF] * g[CH_LTF] + g[CH_RTF] * g[CH_RTF] + g[CH_LTR] * g[CH_LTR] + g[CH_RTR] * g[CH_RTR];
	printf("floor source: ceiling speakers carry %.1f%% of the energy\n", topE * 100.f);
	CHECK(topE < 0.10f, "floor-level source -> ceiling speakers carry < 10% of the energy");
	computeGains(-1.f, 0.f, 0.f, focus, blur, g);
	CHECK(argmaxCh(g) == CH_LS, "left-wall source -> Ls");
	computeGains(1.f, 1.f, 1.f, focus, blur, g);
	CHECK(argmaxCh(g) == CH_RTF, "front-right-top corner -> Rtf");

	// 6) 旋轉：正面聲源旋轉 +90° → Ls，-90° → Rs，180° → 後方
	printf("\nrotation of a front source (0, 0.95, 0):\n");
	for (float deg : {0.f, 45.f, 90.f, 135.f, 180.f, -90.f}) {
		float xr, yr;
		rotateXY(0.f, 0.95f, deg, xr, yr);
		computeGains(xr, yr, 0.f, focus, blur, g);
		printf("  %+5.0f deg -> pos (%+.2f, %+.2f)  peak %s\n", deg, xr, yr, CH_NAMES[argmaxCh(g)]);
	}
	float xr, yr;
	rotateXY(0.f, 0.95f, 90.f, xr, yr);
	computeGains(xr, yr, 0.f, focus, blur, g);
	CHECK(argmaxCh(g) == CH_LS, "rotate +90 -> Ls");
	rotateXY(0.f, 0.95f, -90.f, xr, yr);
	computeGains(xr, yr, 0.f, focus, blur, g);
	CHECK(argmaxCh(g) == CH_RS, "rotate -90 -> Rs");
	rotateXY(0.f, 0.95f, 180.f, xr, yr);
	computeGains(xr, yr, 0.f, focus, blur, g);
	CHECK(argmaxCh(g) == CH_LRS || argmaxCh(g) == CH_RRS, "rotate 180 -> rear");
	rotateXY(0.3f, 0.5f, 360.f, xr, yr);
	CHECK(std::fabs(xr - 0.3f) < 1e-4f && std::fabs(yr - 0.5f) < 1e-4f, "rotate 360 = identity");
	// 半徑 ≤ 1 的圓周旋轉後半徑不變（不會被夾住）
	rotateXY(0.6f, 0.8f, 37.f, xr, yr);
	CHECK(std::fabs(std::sqrt(xr * xr + yr * yr) - 1.f) < 1e-4f, "rotation keeps radius on the unit circle");

	// 7) 平滑度：沿耳朵高度圓周、以及沿房間對角線掃動，相鄰步增益差
	float prev[NCH];
	float maxJump = 0;
	for (int deg = 0; deg <= 720; deg++) {   // 每 0.5 度一步
		float th = deg * 0.5f * 3.14159265f / 180.f;
		computeGains(-std::sin(th) * 0.9f, std::cos(th) * 0.9f, 0.f, focus, blur, g);
		if (deg > 0) for (int i = 0; i < NCH; i++) maxJump = std::max(maxJump, std::fabs(g[i] - prev[i]));
		for (int i = 0; i < NCH; i++) prev[i] = g[i];
	}
	printf("\nmax gain step per 0.5 deg on horizontal circle r=0.9: %.4f\n", maxJump);
	CHECK(maxJump < 0.06f, "smooth horizontal sweep");
	maxJump = 0;
	for (int i = 0; i <= 400; i++) {
		float u = -1.f + 2.f * i / 400.f;
		computeGains(u, u, u, focus, blur, g);
		if (i > 0) for (int k = 0; k < NCH; k++) maxJump = std::max(maxJump, std::fabs(g[k] - prev[k]));
		for (int k = 0; k < NCH; k++) prev[k] = g[k];
	}
	printf("max gain step along the room diagonal (400 steps): %.4f\n", maxJump);
	CHECK(maxJump < 0.12f, "smooth diagonal sweep");

	// 8) 極端輸入不會產生 NaN / 超出範圍
	const float extremes[] = {-1e9f, -5.f, -1.f, 0.f, 1.f, 5.f, 1e9f};
	int nanCount = 0;
	for (float x : extremes) for (float y : extremes) for (float z : extremes) {
		computeGains(x, y, z, 1.f, 0.05f, g);
		float s = 0;
		for (int i = 0; i < NCH; i++) { s += g[i] * g[i]; if (!std::isfinite(g[i])) nanCount++; }
		if (std::fabs(s - 1.f) > 1e-3f) nanCount++;
	}
	CHECK(nanCount == 0, "extreme inputs stay finite and normalised");
	// 極端 FOCUS/BLUR 設定
	for (float f : {1.f, 6.f}) for (float bl : {0.05f, 0.5f}) {
		Vec3 p = speakerPos(CH_C);
		computeGains(p.x, p.y, p.z, f, bl, g);
		float s = 0;
		for (int i = 0; i < NCH; i++) s += g[i] * g[i];
		CHECK(std::fabs(s - 1.f) < 1e-3f && std::isfinite(s), "focus/blur extremes normalised");
	}

	// 9) 多聲源疊加：各聲源增益向量獨立，混音輸出等於各軌乘上增益的總和（線性）
	float g1[NCH], g2[NCH];
	computeGains(-0.7f, 0.5f, 0.2f, focus, blur, g1);
	computeGains(0.4f, -0.6f, 0.8f, focus, blur, g2);
	float mix[NCH] = {};
	for (int c = 0; c < NCH; c++) mix[c] = 1.0f * g1[c] + 0.5f * g2[c];
	float p1 = 0, p2 = 0, pm = 0;
	for (int c = 0; c < NCH; c++) { p1 += g1[c] * g1[c]; p2 += 0.25f * g2[c] * g2[c]; pm += mix[c] * mix[c]; }
	CHECK(pm <= (std::sqrt(p1) + std::sqrt(p2)) * (std::sqrt(p1) + std::sqrt(p2)) + 1e-4f, "mix power bounded by triangle inequality");

	// 10) LFE 低通：1 kHz 應被明顯衰減、50 Hz 基本通過
	LfeFilter f; f.setCutoff(120.f, 48000.f);
	auto rms = [&](float freq) {
		f.reset(); double acc = 0; int N = 48000;
		for (int i = 0; i < N; i++) {
			float o = f.process(std::sin(2 * 3.14159265f * freq * i / 48000.f));
			if (i > 4800) acc += o * o;
		}
		return std::sqrt(acc / (N - 4800)) / 0.7071;
	};
	double r50 = rms(50), r1k = rms(1000);
	printf("LFE filter gain: 50 Hz %.3f, 1 kHz %.4f\n", r50, r1k);
	CHECK(r50 > 0.8 && r1k < 0.05, "LFE lowpass");

	// 11) 立方體顯示投影：方向正確、點都在顯示區內
	const float W = 244.f, H = 170.f;
	for (int m = 0; m < view::NUM_MODES; m++) {
		for (int i = 0; i < 8; i++) {
			float x, y, z; view::corner(i, x, y, z);
			view::Pt p = view::project(m, x, y, z, W, H);
			char msg[64];
			snprintf(msg, sizeof msg, "view %s corner %d inside display", view::modeName(m), i);
			CHECK(p.x >= 4 && p.x <= W - 4 && p.y >= 4 && p.y <= H - 4, msg);
		}
		// 左喇叭在畫面左側（ISO / TOP / FRONT）
		if (m != view::SIDE) {
			Vec3 l = speakerPos(CH_L), r = speakerPos(CH_R);
			view::Pt pl = view::project(m, l.x, l.y, l.z, W, H), pr = view::project(m, r.x, r.y, r.z, W, H);
			char msg[64];
			snprintf(msg, sizeof msg, "view %s: L left of R", view::modeName(m));
			CHECK(pl.x < pr.x, msg);
		}
		// 頂部喇叭比耳朵高度的喇叭更靠畫面上方（ISO / FRONT / SIDE）
		if (m != view::TOP) {
			Vec3 t = speakerPos(CH_LTF), e = speakerPos(CH_L);
			view::Pt pt = view::project(m, t.x, t.y, t.z, W, H), pe = view::project(m, e.x, e.y, e.z, W, H);
			char msg[64];
			snprintf(msg, sizeof msg, "view %s: Ltf above L", view::modeName(m));
			CHECK(pt.y < pe.y, msg);
		}
	}
	// TOP：前方在畫面上方；SIDE：前方在畫面右側
	view::Pt pf = view::project(view::TOP, 0, 1, 0, W, H), pb = view::project(view::TOP, 0, -1, 0, W, H);
	CHECK(pf.y < pb.y, "TOP: front is up");
	pf = view::project(view::SIDE, 0, 1, 0, W, H); pb = view::project(view::SIDE, 0, -1, 0, W, H);
	CHECK(pf.x > pb.x, "SIDE: front is right");
	// ISO：前牆在遠處（depth 較大）
	pf = view::project(view::ISO, 0, 1, 0, W, H); pb = view::project(view::ISO, 0, -1, 0, W, H);
	CHECK(pf.depth > pb.depth, "ISO: front wall is farther away");
	// 標籤分組：FRONT 視角 Ls 與 Lrs 應被合併
	bool merged = false;
	for (auto& lb : view::speakerLabels(view::FRONT, W, H)) if (lb.text == "Ls/Lrs") merged = true;
	CHECK(merged, "FRONT view merges overlapping Ls/Lrs labels");
	// TOP 視角不應合併任何喇叭
	for (auto& lb : view::speakerLabels(view::TOP, W, H)) CHECK(lb.text.find('/') == std::string::npos, "TOP view has no merged labels");

	// 12) 輸出配置 4 / 8 / 12：聲道清單、遮罩、能量、被關掉的喇叭一定是 0
	{
		int idx[NCH];
		CHECK(layoutChannels(LAYOUT_4, idx) == 4 && idx[0] == CH_L && idx[1] == CH_R && idx[2] == CH_LRS && idx[3] == CH_RRS,
		      "4ch layout is L R Lrs Rrs");
		CHECK(layoutChannels(LAYOUT_8, idx) == 8 && idx[3] == CH_LFE && idx[7] == CH_RRS, "8ch layout is 7.1 in SMPTE order");
		CHECK(layoutChannels(LAYOUT_12, idx) == 12 && idx[11] == CH_RTR, "12ch layout is everything");
		CHECK(layoutChannels(LAYOUT_51, idx) == 6 && idx[0] == CH_L && idx[3] == CH_LFE && idx[4] == CH_LS && idx[5] == CH_RS,
		      "5.1 layout is L R C LFE Ls Rs (outputs 1-6)");
		{
			// 5.1：正後方的聲源要落在 Ls / Rs，後方與頂部喇叭完全沒聲音
			bool m51[NCH];
			layoutMask(LAYOUT_51, m51);
			float g[NCH];
			computeGains(0.f, -1.f, 0.f, 3.f, 0.2f, g, m51);
			float surround = g[CH_LS] * g[CH_LS] + g[CH_RS] * g[CH_RS];
			bool silent = g[CH_LRS] == 0.f && g[CH_RRS] == 0.f && g[CH_LTF] == 0.f && g[CH_RTR] == 0.f;
			CHECK(surround > 0.6f && silent, "5.1: rear source goes to Ls/Rs, unused speakers silent");
		}
		for (int l = 0; l < NUM_LAYOUTS; l++) {
			int n = layoutChannels(l, idx);
			bool mask[NCH];
			layoutMask(l, mask);
			int cnt = 0;
			for (int i = 0; i < NCH; i++) cnt += mask[i];
			CHECK(cnt == n && n == layoutCount(l), "mask count matches layout count");
			for (int i = 1; i < n; i++) CHECK(idx[i] > idx[i - 1], "layout channels keep SMPTE relative order");
			// 整個立方體內：能量歸一化，被關掉的喇叭恆為 0
			int bad2 = 0;
			for (float x = -1; x <= 1.001f; x += 0.25f)
				for (float y = -1; y <= 1.001f; y += 0.25f)
					for (float z = -1; z <= 1.001f; z += 0.25f) {
						computeGains(x, y, z, focus, blur, g, mask);
						float e = 0;
						for (int i = 0; i < NCH; i++) {
							if (!mask[i] && g[i] != 0.f) bad2++;
							e += g[i] * g[i];
						}
						if (std::fabs(e - 1.f) > 1e-4f) bad2++;
					}
			char msg[64];
			snprintf(msg, sizeof msg, "layout %d: energy + disabled speakers silent", layoutCount(l));
			CHECK(bad2 == 0, msg);
		}
		// 4 聲道：前方聲源 → 只有 L/R，後方聲源 → 只有 Lrs/Rrs，左側聲源 → L 與 Lrs 為主
		bool m4[NCH];
		layoutMask(LAYOUT_4, m4);
		computeGains(0.f, 0.95f, 0.f, focus, blur, g, m4);
		CHECK(g[CH_L] > 0.6f && g[CH_R] > 0.6f && std::fabs(g[CH_L] - g[CH_R]) < 1e-4f, "4ch: front-centre splits equally to L and R");
		computeGains(0.f, -0.95f, 0.f, focus, blur, g, m4);
		CHECK(g[CH_LRS] > 0.6f && g[CH_RRS] > 0.6f, "4ch: rear source goes to Lrs and Rrs");
		computeGains(-1.f, 0.f, 0.f, focus, blur, g, m4);
		CHECK(g[CH_L] > 0.5f && g[CH_LRS] > 0.5f && g[CH_R] < 0.3f && g[CH_RRS] < 0.3f, "4ch: left-wall source goes to L and Lrs");
		// 4 聲道下繞圈旋轉仍平順
		float pv[NCH], mj = 0;
		for (int deg = 0; deg <= 720; deg++) {
			float th = deg * 0.5f * 3.14159265f / 180.f;
			computeGains(-std::sin(th) * 0.9f, std::cos(th) * 0.9f, 0.f, focus, blur, g, m4);
			if (deg > 0) for (int i = 0; i < NCH; i++) mj = std::max(mj, std::fabs(g[i] - pv[i]));
			for (int i = 0; i < NCH; i++) pv[i] = g[i];
		}
		printf("4ch layout: max gain step per 0.5 deg on circle: %.4f\n", mj);
		CHECK(mj < 0.06f, "4ch: smooth horizontal sweep");
		// 預設參數（mask 省略）等同 12 聲道
		bool m12[NCH];
		layoutMask(LAYOUT_12, m12);
		float ga[NCH], gb[NCH];
		computeGains(0.3f, 0.2f, 0.5f, focus, blur, ga);
		computeGains(0.3f, 0.2f, 0.5f, focus, blur, gb, m12);
		for (int i = 0; i < NCH; i++) CHECK(std::fabs(ga[i] - gb[i]) < 1e-6f, "no mask == 12ch mask");
		// 立方體：4 聲道只畫 4 支喇叭標籤
		auto lab4 = view::speakerLabels(view::TOP, 244.f, 170.f, m4);
		CHECK(lab4.size() == 4, "cube shows only the 4 active speakers in the 4ch layout");
	}

	// 13) 立體聲 L/R 展開：左右對稱、寬度 0 = 單聲道、邊界夾住、聲場跟著旋轉
	{
		float xl, xr;
		stereoSplit(0.f, 0.3f, xl, xr);
		CHECK(std::fabs(xl + 0.3f) < 1e-6f && std::fabs(xr - 0.3f) < 1e-6f, "stereoSplit: L left, R right of centre");
		stereoSplit(0.5f, 0.f, xl, xr);
		CHECK(xl == 0.5f && xr == 0.5f, "stereoSplit: width 0 is mono");
		stereoSplit(0.9f, 0.5f, xl, xr);
		CHECK(xr == 1.f && std::fabs(xl - 0.4f) < 1e-6f, "stereoSplit: clamped to the room");
		// 前方立體聲、寬度 0.577：L 訊號主要進 L 喇叭、R 訊號主要進 R 喇叭
		float gl[NCH], gr[NCH];
		computeGains(-0.577f, 0.95f, 0.f, focus, blur, gl);
		computeGains(0.577f, 0.95f, 0.f, focus, blur, gr);
		CHECK(argmaxCh(gl) == CH_L && argmaxCh(gr) == CH_R, "wide front stereo: L signal -> L speaker, R signal -> R speaker");
		// 立體聲在前方旋轉 180° → 左右對調到後方（L 訊號在右後、R 訊號在左後）
		float x1, y1, x2, y2;
		rotateXY(-0.5f, 0.95f, 180.f, x1, y1);
		rotateXY(0.5f, 0.95f, 180.f, x2, y2);
		CHECK(x1 > 0.f && x2 < 0.f && y1 < 0.f && y2 < 0.f, "rotating stereo by 180 deg puts it behind, mirrored");
		// 兩個聲源同相相加的最大輸出：每個聲源能量歸一化，所以任何喇叭的增益都 <= 1
		float mxg = 0;
		for (float w = 0; w <= 1.f; w += 0.25f)
			for (float x = -1; x <= 1.001f; x += 0.25f) {
				stereoSplit(x, w, xl, xr);
				computeGains(xl, 0.5f, 0.f, focus, blur, gl);
				computeGains(xr, 0.5f, 0.f, focus, blur, gr);
				for (int i = 0; i < NCH; i++) mxg = std::max(mxg, std::max(gl[i], gr[i]));
			}
		CHECK(mxg <= 1.0001f, "per-source gains never exceed unity");
	}

	// 14) 電平 LED：門檻、紅燈維持時間、峰值回落
	{
		const float sr = 48000.f, dt = 1.f / sr, decay = std::exp(-1.f / (0.3f * sr));
		PeakMeter m;
		CHECK(m.state() == METER_OFF, "meter starts off");
		auto feed = [&](float amp, float seconds, float freq = 440.f) {
			int n = (int) (seconds * sr);
			for (int i = 0; i < n; i++) m.process(amp * std::sin(2.f * 3.14159265f * freq * i / sr), decay, dt);
		};
		feed(1.f, 0.1f);
		CHECK(m.state() == METER_SIGNAL, "1 V sine -> green");
		m.reset(); feed(6.f, 0.1f);
		CHECK(m.state() == METER_HOT, "6 V sine (-4.4 dBFS) -> yellow");
		m.reset(); feed(4.9f, 0.1f);
		CHECK(m.state() == METER_SIGNAL, "4.9 V sine stays green (below -6 dBFS)");
		m.reset(); feed(10.5f, 0.05f);
		CHECK(m.state() == METER_CLIP, "10.5 V -> red");
		// 單一取樣的爆音也要抓到，且紅燈維持約 1.5 秒
		m.reset();
		m.process(12.f, decay, dt);
		feed(0.5f, 1.4f);
		CHECK(m.state() == METER_CLIP, "single-sample over still red after 1.4 s");
		feed(0.5f, 0.2f);
		CHECK(m.state() != METER_CLIP, "red released after ~1.5 s");
		// 停止訊號後會回到熄燈
		feed(0.f, 3.f);
		CHECK(m.state() == METER_OFF, "meter falls back to off after silence");
		printf("meter: peak after 3 s of silence %.5f V\n", m.peak);
	}

	// 15) 安全限幅器：8 V 以下完全不變、輸出絕不超過 10 V、單調、連續、正負對稱
	{
		int bad3 = 0;
		float prevY = -1e9f;
		for (float x = -40.f; x <= 40.f; x += 0.01f) {
			float y = softLimit(x);
			if (std::fabs(y) > CLIP_V + 1e-5f) bad3++;
			if (y < prevY - 1e-6f) bad3++;
			if (std::fabs(y - prevY) > 0.0101f && prevY > -1e8f) bad3++;   // 斜率 <= 1，沒有跳躍
			if (std::fabs(softLimit(-x) + y) > 1e-6f) bad3++;
			prevY = y;
		}
		CHECK(bad3 == 0, "limiter: bounded by 10 V, monotonic, continuous, symmetric");
		bool clean = true;
		for (float x = -8.f; x <= 8.f; x += 0.01f) if (softLimit(x) != x) clean = false;
		CHECK(clean, "limiter: untouched below 8 V");
		printf("limiter: 9 V -> %.3f, 10 V -> %.3f, 15 V -> %.3f, 100 V -> %.4f\n",
		       softLimit(9.f), softLimit(10.f), softLimit(15.f), softLimit(100.f));
	}

	// 16) 繞場：起點與終點回到原位、中段半徑等於設定值、整圈平順、方向正確、立體聲跟著轉
	{
		CHECK(orbitEnvelope(0.f) == 0.f && orbitEnvelope(1.f) < 1e-6f && orbitEnvelope(0.5f) == 1.f, "orbit envelope 0 -> 1 -> 0");
		const float sx = 0.3f, sy = 0.6f, R = 0.9f;   // 聲源目前位置與繞場半徑
		float ox, oy;
		orbitPosition(sx, sy, R, 0.f, orbitEnvelope(0.f), ox, oy);
		CHECK(std::fabs(ox - sx) < 1e-5f && std::fabs(oy - sy) < 1e-5f, "orbit starts exactly at the current position");
		orbitPosition(sx, sy, R, 360.f, orbitEnvelope(1.f), ox, oy);
		CHECK(std::fabs(ox - sx) < 1e-4f && std::fabs(oy - sy) < 1e-4f, "orbit ends exactly back at the current position");
		float maxStep = 0.f, px = sx, py = sy, minR = 9, maxR = 0;
		const int N = 2000;
		for (int i = 1; i <= N; i++) {
			float ph = (float) i / N;
			orbitPosition(sx, sy, R, 360.f * ph, orbitEnvelope(ph), ox, oy);
			maxStep = std::max(maxStep, std::hypot(ox - px, oy - py));
			px = ox; py = oy;
			if (ph > 0.12f && ph < 0.88f) {
				float r = std::hypot(ox, oy);
				minR = std::min(minR, r); maxR = std::max(maxR, r);
			}
		}
		printf("orbit: radius in the middle of the revolution %.4f..%.4f (set %.2f), max step per 1/2000 rev %.4f\n", minR, maxR, R, maxStep);
		CHECK(std::fabs(minR - R) < 1e-4f && std::fabs(maxR - R) < 1e-4f, "orbit radius equals the DIA setting mid-revolution");
		CHECK(maxStep < 0.01f, "orbit path is smooth (no jumps)");
		// 正面聲源逆時針轉 90° → 到左牆（Ls 最大）；順時針 → 右牆
		orbitPosition(0.f, 0.9f, 0.9f, 90.f, 1.f, ox, oy);
		computeGains(ox, oy, 0.f, focus, blur, g);
		CHECK(argmaxCh(g) == CH_LS, "orbit +90 (counter-clockwise) -> left wall");
		orbitPosition(0.f, 0.9f, 0.9f, -90.f, 1.f, ox, oy);
		computeGains(ox, oy, 0.f, focus, blur, g);
		CHECK(argmaxCh(g) == CH_RS, "orbit -90 (clockwise) -> right wall");
		// 聲源在正中央時從正前方開始
		orbitPosition(0.f, 0.f, 0.8f, 0.f, 1.f, ox, oy);
		CHECK(std::fabs(ox) < 1e-5f && std::fabs(oy - 0.8f) < 1e-5f, "orbit from the centre starts at the front");
		// 立體聲跟著轉：轉 90° 時 L/R 從左右變成前後（L 在後、R 在前）
		float xl, yl, xr, yr;
		stereoSplitRotated(0.f, 0.f, 0.3f, 0.f, xl, yl, xr, yr);
		CHECK(std::fabs(xl + 0.3f) < 1e-6f && std::fabs(xr - 0.3f) < 1e-6f && yl == 0.f && yr == 0.f, "stereoSplitRotated(0) == stereoSplit");
		stereoSplitRotated(-0.9f, 0.f, 0.3f, 90.f, xl, yl, xr, yr);
		CHECK(yl < -0.29f && yr > 0.29f && std::fabs(xl + 0.9f) < 1e-5f, "stereo pair turns with the orbit");
		// LFO：±5 V = ±180°，鋸齒 LFO 一個週期剛好一圈
		float th0 = -5.f * 36.f, th1 = 5.f * 36.f;
		CHECK(std::fabs((th1 - th0) - 360.f) < 1e-4f, "saw LFO -5..+5 V = exactly one revolution");
	}

	// 17) 旋轉軸 X / Y / Z
	{
		// 平面座標換過去再換回來 = 原值
		int rt = 0;
		for (int a = 0; a < NUM_AXES; a++)
			for (float x = -1; x <= 1.01f; x += 0.5f)
				for (float y = -1; y <= 1.01f; y += 0.5f)
					for (float z = -1; z <= 1.01f; z += 0.5f) {
						float u, v, w, X, Y, Z;
						toPlane(a, x, y, z, u, v, w);
						fromPlane(a, u, v, w, X, Y, Z);
						if (std::fabs(X - x) + std::fabs(Y - y) + std::fabs(Z - z) > 1e-6f) rt++;
					}
		CHECK(rt == 0, "toPlane/fromPlane round trip");
		// Z 軸與原本的水平旋轉完全相同
		float x = 0.3f, y = 0.7f, z = 0.4f, xr, yr;
		rotateXY(x, y, 63.f, xr, yr);
		rotateAxis(AXIS_Z, x, y, z, 63.f);
		CHECK(std::fabs(x - xr) < 1e-6f && std::fabs(y - yr) < 1e-6f && std::fabs(z - 0.4f) < 1e-6f, "Z axis == original horizontal rotation, height kept");
		// X 軸：正前方 +90° → 天花板；+180° → 正後方；左右位置不變
		x = 0.2f; y = 0.9f; z = 0.f;
		rotateAxis(AXIS_X, x, y, z, 90.f);
		CHECK(std::fabs(z - 0.9f) < 1e-5f && std::fabs(y) < 1e-5f && std::fabs(x - 0.2f) < 1e-6f, "X axis +90: front -> ceiling, x kept");
		computeGains(0.f, 0.f, 0.95f, focus, blur, g);
		x = 0.f; y = 0.95f; z = 0.f;
		rotateAxis(AXIS_X, x, y, z, 180.f);
		CHECK(y < -0.94f && std::fabs(z) < 1e-5f, "X axis +180: front -> back");
		// Y 軸：右牆 +90° → 天花板；+180° → 左牆；前後位置不變
		x = 0.9f; y = 0.3f; z = 0.f;
		rotateAxis(AXIS_Y, x, y, z, 90.f);
		CHECK(std::fabs(z - 0.9f) < 1e-5f && std::fabs(x) < 1e-5f && std::fabs(y - 0.3f) < 1e-6f, "Y axis +90: right -> ceiling, y kept");
		x = 0.9f; y = 0.f; z = 0.f;
		rotateAxis(AXIS_Y, x, y, z, 180.f);
		CHECK(x < -0.89f, "Y axis +180: right -> left");
		// 垂直繞場（X 軸）：從正前方出發，轉到 90° 時在天花板，喇叭以頂部為主；繞完回原位；x 不變
		float ox, oy, oz;
		orbitPosition3D(AXIS_X, 0.1f, 0.8f, 0.f, 0.9f, 90.f, 1.f, ox, oy, oz);
		computeGains(ox, oy, oz, focus, blur, g);
		float topE = g[CH_LTF]*g[CH_LTF] + g[CH_RTF]*g[CH_RTF] + g[CH_LTR]*g[CH_LTR] + g[CH_RTR]*g[CH_RTR];
		CHECK(std::fabs(ox - 0.1f) < 1e-6f && oz > 0.89f && topE > 0.6f, "X-axis orbit at 90 deg is overhead (ceiling speakers dominate)");
		orbitPosition3D(AXIS_X, 0.1f, 0.8f, -0.2f, 0.9f, 360.f, orbitEnvelope(1.f), ox, oy, oz);
		CHECK(std::fabs(ox - 0.1f) < 1e-5f && std::fabs(oy - 0.8f) < 1e-4f && std::fabs(oz + 0.2f) < 1e-4f, "X-axis orbit returns to the start");
		// 垂直繞場的路徑也是平順的
		float px = 0.1f, py = 0.8f, pz = 0.f, mstep = 0;
		for (int i = 1; i <= 2000; i++) {
			float ph = i / 2000.f;
			orbitPosition3D(AXIS_Y, 0.1f, 0.8f, 0.f, 0.9f, 360.f * ph, orbitEnvelope(ph), ox, oy, oz);
			mstep = std::max(mstep, std::sqrt((ox-px)*(ox-px) + (oy-py)*(oy-py) + (oz-pz)*(oz-pz)));
			px = ox; py = oy; pz = oz;
		}
		CHECK(mstep < 0.01f, "Y-axis orbit path is smooth");
		printf("vertical orbit: max step per 1/2000 rev %.4f\n", mstep);
	}

	// 18) 繞場狀態：繞一圈模式、持續模式、中途停止、長時間運轉；整段位置不可跳動，停下時回到原位
	{
		const float sx = 0.3f, sy = 0.6f, R = 0.9f, step = 1.f / 1000.f;   // 每次更新走 1/1000 圈
		auto pos = [&](const OrbitState& o, int dir, float& ox, float& oy) {
			if (!o.active) { ox = sx; oy = sy; return; }
			orbitPosition(sx, sy, R, 360.f * o.turn() * dir, o.envelope(), ox, oy);
		};
		// 繞一圈
		OrbitState o;
		o.trigger(false);
		int n = 0;
		float px = sx, py = sy, mj = 0, ox, oy;
		while (o.active && n < 5000) {
			o.update(step, false);
			pos(o, 1, ox, oy);
			mj = std::max(mj, std::hypot(ox - px, oy - py)); px = ox; py = oy; n++;
		}
		CHECK(!o.active && n >= 999 && n <= 1001, "one-turn mode stops after exactly one revolution");
		CHECK(std::hypot(px - sx, py - sy) < 1e-3f && mj < 0.01f, "one-turn mode: smooth and back at the start");
		// 繞一圈途中再按：不理會
		o.trigger(false);
		for (int i = 0; i < 300; i++) o.update(step, false);
		float ph = o.phase;
		o.trigger(false);
		CHECK(o.active && o.phase == ph, "one-turn mode ignores a second press");

		// 持續模式：繞 3.4 圈後按停，應在第 4 圈結束時停下；整段平順
		OrbitState L;
		L.trigger(true);
		px = sx; py = sy; mj = 0;
		int i;
		for (i = 0; i < 3400; i++) {
			L.update(step, true);
			pos(L, -1, ox, oy);
			mj = std::max(mj, std::hypot(ox - px, oy - py)); px = ox; py = oy;
		}
		CHECK(L.active, "loop mode keeps going after 3.4 revolutions");
		// 中段半徑應該就是設定值
		CHECK(std::fabs(std::hypot(ox, oy) - R) < 1e-4f, "loop mode runs at the DIA radius");
		L.trigger(true);   // 要求停止
		CHECK(L.stopping(), "second press in loop mode requests a stop");
		int extra = 0;
		while (L.active && extra < 5000) {
			L.update(step, true);
			pos(L, -1, ox, oy);
			mj = std::max(mj, std::hypot(ox - px, oy - py)); px = ox; py = oy; extra++;
		}
		printf("loop mode: stop pressed at 3.4 turns, stopped %d steps later (%.3f turns), max jump %.4f\n", extra, extra * step, mj);
		CHECK(!L.active && extra >= 590 && extra <= 610, "loop mode finishes the current revolution after the stop press");
		CHECK(std::hypot(px - sx, py - sy) < 1e-3f, "loop mode: back at the start position after stopping");
		CHECK(mj < 0.01f, "loop mode: whole run is smooth (start, wrap, stop)");
		// 停止要求太接近一圈結尾時，延到下一圈結束（保證有 12% 的收回距離）
		OrbitState L2;
		L2.trigger(true);
		for (i = 0; i < 950; i++) L2.update(step, true);
		L2.trigger(true);
		CHECK(std::fabs(L2.stopAt - 2.f) < 1e-6f, "stop near the end of a turn is pushed to the next turn");
		// 持續模式途中改成「繞一圈」模式 → 繞完這圈停
		OrbitState L3;
		L3.trigger(true);
		for (i = 0; i < 1500; i++) L3.update(step, true);
		L3.update(step, false);
		CHECK(L3.stopping() && std::fabs(L3.stopAt - 2.f) < 1e-6f, "switching to one-turn mode while looping stops at the end of the turn");
		// 長時間運轉（約 10 萬圈）不會失準
		OrbitState L4;
		L4.trigger(true);
		for (i = 0; i < 2000000; i++) L4.update(0.05f, true);
		CHECK(L4.active && L4.phase >= 1.f && L4.phase < 2.f, "loop mode stays numerically bounded after 100k revolutions");
		// 方向：dir = -1 第 1/4 圈轉到右邊
		OrbitState D;
		D.trigger(true);
		for (i = 0; i < 250; i++) D.update(step, true);
		orbitPosition(0.f, 0.9f, 0.9f, 360.f * D.turn() * -1.f, 1.f, ox, oy);
		CHECK(ox > 0.85f, "direction -1 (right / clockwise): front -> right");
	}

	// 19) Mute / Solo 規則
	{
		CHECK(trackAudible(false, false, false), "no mute, no solo anywhere -> audible");
		CHECK(!trackAudible(true, false, false), "muted -> silent");
		CHECK(!trackAudible(false, false, true), "another track soloed -> silent");
		CHECK(trackAudible(false, true, true), "this track soloed -> audible");
		CHECK(!trackAudible(true, true, true), "muted + soloed -> mute wins");
		// 模擬 12 軌：第 3 軌 Mute，第 7、9 軌 Solo → 只有 7、9 聽得到
		bool mute[12] = {}, solo[12] = {};
		mute[2] = true; solo[6] = true; solo[8] = true;
		bool any = false;
		for (int t = 0; t < 12; t++) any = any || solo[t];
		int heard = 0;
		bool ok = true;
		for (int t = 0; t < 12; t++) {
			bool a = trackAudible(mute[t], solo[t], any);
			heard += a;
			if (a != (t == 6 || t == 8)) ok = false;
		}
		CHECK(ok && heard == 2, "12-track scenario: only the soloed tracks are heard");
		// 淡入淡出：5 ms 一階平滑，20 ms 後已經 < -40 dB（避免爆音又反應夠快）
		const float sr = 48000.f, coef = 1.f - std::exp(-1.f / (0.005f * sr));
		float gm = 1.f;
		for (int i = 0; i < (int) (0.02f * sr); i++) gm += (0.f - gm) * coef;
		float first = 1.f - coef;   // 第一個取樣只下降一點點，不是硬切
		printf("mute fade: first sample %.4f, after 20 ms %.5f\n", first, gm);
		CHECK(first > 0.99f && gm < 0.02f, "mute fades out smoothly within ~20 ms");
	}

	// 20) 繞場速度：SPD 往右越快（60 秒 ～ 1 秒），ORB RATE 倍率 0.25× ～ 4×；實際模擬一圈的時間
	{
		CHECK(std::fabs(orbitSeconds(0.f, 0.f) - 60.f) < 1e-3f, "SPD fully left = 60 s per revolution");
		CHECK(std::fabs(orbitSeconds(1.f, 0.f) - 1.f) < 1e-4f, "SPD fully right = 1 s per revolution");
		CHECK(std::fabs(orbitSeconds(0.5f, 0.f) - 7.746f) < 0.01f, "SPD default = about 7.7 s");
		CHECK(std::fabs(orbitSeconds(0.5f, 1.f) - 7.746f / 4.f) < 0.01f && std::fabs(orbitSeconds(0.5f, -1.f) - 7.746f * 4.f) < 0.05f,
		      "ORB RATE: +1 = 4x faster, -1 = 4x slower");
		bool mono = true;
		for (float v = 0.01f; v <= 1.f; v += 0.01f) if (orbitSeconds(v, 0.f) >= orbitSeconds(v - 0.01f, 0.f)) mono = false;
		CHECK(mono, "turning SPD to the right always makes the orbit faster");
		// 用模組相同的更新方式（每 32 個取樣更新一次）模擬：SPD 1.0、48 kHz → 一圈約 1 秒
		const float sr = 48000.f, dt = 32.f / sr;
		OrbitState o;
		o.trigger(false);
		int n = 0;
		while (o.active && n < 100000) { o.update(dt / orbitSeconds(1.f, 0.f), false); n++; }
		float secs = n * dt;
		printf("orbit speed: SPD max, 48 kHz -> one revolution took %.3f s\n", secs);
		CHECK(std::fabs(secs - 1.f) < 0.002f, "simulated revolution time matches the SPD setting");
	}

	// 21) 房間比例（ROOM 擴充模組）
	{
		// 立方體（或沒有 Room）時，結果和原本完全一樣
		Room cube;
		float g0[NCH], g1[NCH];
		bool same = true;
		for (float x = -1.f; x <= 1.f; x += 0.5f)
			for (float y = -1.f; y <= 1.f; y += 0.5f) {
				computeGains(x, y, 0.3f, 3.f, 0.2f, g0);
				computeGains(x, y, 0.3f, 3.f, 0.2f, g1, nullptr, &cube);
				for (int i = 0; i < NCH; i++) if (g0[i] != g1[i]) same = false;
			}
		CHECK(same && cube.isCube(), "room 1:1:1 gives exactly the same gains as no room");

		// 公尺 → 比例：最長的一軸 = 1；整體放大縮小不影響
		Room a = roomFromSize(8.f, 16.f, 4.f), b = roomFromSize(4.f, 8.f, 2.f);
		printf("room 8 x 16 x 4 m -> scale %.3f %.3f %.3f\n", a.sx, a.sy, a.sz);
		CHECK(std::fabs(a.sx - 0.5f) < 1e-6f && std::fabs(a.sy - 1.f) < 1e-6f && std::fabs(a.sz - 0.5f) < 1e-6f, "8x16x4 m room -> x 0.5, y 1, z 0.5");
		CHECK(a.sx == b.sx && a.sy == b.sy && a.sz == b.sz, "only the ratio matters (metres or ratio give the same result)");

		// 能量守恆在任何比例下都成立
		bool energy = true;
		for (float x = -1.f; x <= 1.f; x += 0.25f)
			for (float y = -1.f; y <= 1.f; y += 0.25f) {
				computeGains(x, y, 0.f, 3.f, 0.2f, g1, nullptr, &a);
				float e = 0.f;
				for (int i = 0; i < NCH; i++) e += g1[i] * g1[i];
				if (std::fabs(e - 1.f) > 1e-4f) energy = false;
			}
		CHECK(energy, "energy stays constant in a non-cubic room");

		// 深度是寬度兩倍的長房間：聲源在正中央時，左右牆的 Ls/Rs 比前方的 C 近，所以 Ls 要比 C 大聲；立方體時兩者一樣
		computeGains(0.f, 0.f, 0.f, 3.f, 0.2f, g0);
		computeGains(0.f, 0.f, 0.f, 3.f, 0.2f, g1, nullptr, &a);
		printf("centre source: cube Ls %.3f C %.3f | long room Ls %.3f C %.3f\n", g0[CH_LS], g0[CH_C], g1[CH_LS], g1[CH_C]);
		CHECK(std::fabs(g0[CH_LS] - g0[CH_C]) < 1e-4f && g1[CH_LS] > g1[CH_C] * 2.f, "long room: side speakers are closer than the front ones");

		// 低天花板（高度比例小）：頂部喇叭比較近，同一個位置分到的頂部能量比立方體多
		Room low = roomFromSize(10.f, 10.f, 2.f);
		computeGains(0.f, 0.f, 0.f, 3.f, 0.2f, g1, nullptr, &low);
		float top0 = 0.f, top1 = 0.f;
		for (int i = CH_LTF; i <= CH_RTR; i++) { top0 += g0[i] * g0[i]; top1 += g1[i] * g1[i]; }
		printf("centre source: top energy cube %.3f, low ceiling %.3f\n", top0, top1);
		CHECK(top1 > top0, "low ceiling: more energy in the top speakers");

		// 繞場：FIT（預設）在正規化座標裡碰到四面牆；ROUND 在真實空間裡是正圓（半徑以較短邊為準）
		Room fit = roomFromSize(8.f, 16.f, 4.f, false), rnd = roomFromSize(8.f, 16.f, 4.f, true);
		float ox, oy, oz;
		bool fitOk = true, roundOk = true;
		for (float th = 0.f; th < 360.f; th += 15.f) {
			orbitPositionRoom(AXIS_Z, 0.f, 1.f, 0.f, 1.f, th, 1.f, &fit, ox, oy, oz);
			if (std::fabs(ox * ox + oy * oy - 1.f) > 1e-3f) fitOk = false;         // 正規化座標裡的圓 = 真實空間的橢圓
			orbitPositionRoom(AXIS_Z, 0.f, 1.f, 0.f, 1.f, th, 1.f, &rnd, ox, oy, oz);
			float rx = ox * rnd.sx, ry = oy * rnd.sy;                                // 換回真實比例
			if (std::fabs(std::sqrt(rx * rx + ry * ry) - 0.5f) > 1e-3f) roundOk = false;   // 半徑 = 較短邊 0.5
		}
		CHECK(fitOk, "orbit FIT: fills the room (ellipse in real space)");
		CHECK(roundOk, "orbit ROUND: a true circle in real space, radius set by the shorter side");
		orbitPositionRoom(AXIS_Z, 0.3f, 0.7f, 0.2f, 0.9f, 0.f, 0.f, &rnd, ox, oy, oz);
		CHECK(std::fabs(ox - 0.3f) < 1e-4f && std::fabs(oy - 0.7f) < 1e-4f && std::fabs(oz - 0.2f) < 1e-4f, "orbit ROUND: starts and ends at the original position");
	}

	// 22) ROOM 擴充模組：比例文字輸入、單位換算、範圍
	{
		using namespace st::roomexp;
		float r[3];
		CHECK(parseRatio("1:2:0.5", r) && r[0] == 1.f && r[1] == 2.f && r[2] == 0.5f, "ratio text 1:2:0.5");
		CHECK(parseRatio(" 4 x 3 x 1.5 ", r) && r[0] == 4.f && r[1] == 3.f && r[2] == 1.5f, "ratio text with x and spaces");
		CHECK(parseRatio("16, 9, 4", r) && r[1] == 9.f, "ratio text with commas");
		CHECK(parseRatio("1 : 2.35 : 0.75", r) && std::fabs(r[1] - 2.35f) < 1e-6f, "ratio text with decimals");
		CHECK(!parseRatio("1:2", r) && !parseRatio("", r) && !parseRatio("a:b:c", r) && !parseRatio("1:0:2", r) && !parseRatio("1:-2:3", r) && !parseRatio("1:2:3:4", r),
		      "bad ratio text is rejected");
		float big[3] = {50.f, 400.f, 25.f};   // 太大：整組等比例縮小，比例不變
		fitRange(big);
		CHECK(std::fabs(big[1] - MAX_SIZE) < 1e-3f && std::fabs(big[0] / big[1] - 0.125f) < 1e-4f && std::fabs(big[2] / big[1] - 0.0625f) < 1e-4f,
		      "oversized room is scaled down, ratio kept");
		float small[3] = {1.f, 2.f, 0.02f};   // 太小：整組等比例放大
		fitRange(small);
		CHECK(std::fabs(small[2] - MIN_SIZE) < 1e-4f && std::fabs(small[1] / small[0] - 2.f) < 1e-4f, "undersized value is scaled up, ratio kept");
		// 單位只是顯示換算：250 cm = 2.5 m，10 ft ≈ 3.048 m
		CHECK(std::fabs(250.f / UNIT_PER_METRE[UNIT_CM] - 2.5f) < 1e-5f && std::fabs(10.f / UNIT_PER_METRE[UNIT_FT] - 3.048f) < 1e-3f, "unit conversion cm / ft");
		// 公尺、公分、英尺填同一個房間，得到的比例完全一樣
		Room m = roomFromSize(8.f, 16.f, 4.f), cm = roomFromSize(800.f, 1600.f, 400.f), ft = roomFromSize(8.f * 3.28084f, 16.f * 3.28084f, 4.f * 3.28084f);
		CHECK(std::fabs(m.sx - cm.sx) < 1e-6f && std::fabs(m.sz - cm.sz) < 1e-6f && std::fabs(m.sx - ft.sx) < 1e-5f && std::fabs(m.sz - ft.sz) < 1e-5f,
		      "any unit gives the same room ratio");
	}

	printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
	return fails ? 1 : 0;
}
