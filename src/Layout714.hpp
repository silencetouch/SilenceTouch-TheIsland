// Layout714.hpp — 面板元件座標（不依賴 VCV Rack）。
// 模組介面與 tests/verify_panel.py 的面板檢查共用同一份座標，檢查的就是實際使用的版面。
// 單位：像素（Rack 面板 1 HP = 15 px，高度 380 px）。面板 50 HP = 750 px。
#pragma once
#include <string>
#include <vector>

namespace st { namespace layout {

constexpr float PANEL_W = 750.f;
constexpr float PANEL_H = 380.f;
constexpr int MAX_TRACKS = 12;       // 輸入軌數（每軌為立體聲 L/R）；要擴充到 16 時加大此值並重排面板

constexpr float ROW0_Y = 48.f;       // 第 1 軌的中心 y
constexpr float ROW_DY = 27.f;       // 每軌的列距

// 軌道列的各欄中心 x
// 第一欄（編號圓片 + LED）放在外框與第一條分隔線（x = 27.5）之間的正中央：圓片 4.5～17.5、LED 19.5～24.5
constexpr float COL_NUM = 11.f, COL_LED = 22.f;                       // 軌道編號與該軌的電平 LED
constexpr float COL_AUDIO = 40.f, COL_AUDIOR = 66.f;                  // 每軌 L / R 兩個音訊輸入並排
constexpr float COL_LVL = 93.f;                                       // 音量：緊貼在 L / R 輸入右邊
constexpr float COL_PX = 118.f, COL_XCV = 143.f;
constexpr float COL_PY = 168.f, COL_YCV = 193.f;
constexpr float COL_PZ = 218.f, COL_ZCV = 243.f;
constexpr float COL_WIDTH = 268.f, COL_LFE = 293.f;
constexpr float COL_ORBBTN = 315.f, COL_SPLIT = 334.f;                                         // 繞場按鈕、SPLIT（L / R 分開繞）
constexpr float COL_ORBDIA = 355.f, COL_ORBSPD = 380.f, COL_ORBCV = 406.f;                     // 繞場：直徑、速度、LFO 輸入
constexpr float COL_MUTE = 433.f, COL_SOLO = 452.f;                                              // 靜音、獨奏小按鈕
constexpr float ROWS_RIGHT = 470.f;  // 軌道區右緣

// 右側立方體顯示區（中心與大小）
constexpr float RIGHT_CX = 610.f;   // 右側區中心
constexpr float CUBE_CX = RIGHT_CX, CUBE_CY = 121.f, CUBE_W = 244.f, CUBE_H = 170.f;

// 元件半徑（像素，偏保守）
constexpr float R_PORT = 12.f;
constexpr float R_KNOB = 11.f;
constexpr float R_LATCH = 8.f;
constexpr float R_BUTTON = 8.f;      // TL1105 小按鈕（約 15 px）
constexpr float R_LED = 3.5f;        // 檢查用半徑（實際畫 3 px）
constexpr float OUT_LED_DX = 17.f;   // 輸出 LED 在輸出孔右側的距離

enum Kind { K_INPUT, K_OUTPUT, K_KNOB, K_LATCH, K_LABEL, K_CUBE, K_TESTDISP, K_LED, K_BUTTON, K_LOGO };

enum Role {
	R_NONE,
	R_AUDIO, R_XCV, R_YCV, R_ZCV,
	R_PX, R_PY, R_PZ, R_LVL, R_LFE,
	R_ROTATE, R_ROTCV, R_FOCUS, R_BLUR, R_MASTER,
	R_TEST, R_TESTCH, R_POLYOUT, R_CHOUT,
	R_AUDIOR, R_WIDTH,  // 加在最後，前面各值的編號不變（tools/panel_tool.py 依編號檢查）
	R_TRACKLED, R_OUTLED,
	R_ORBBTN, R_ORBDIA, R_ORBCV, R_ORBTIME,
	R_MUTE, R_SOLO,
	R_ORBSPD,
	R_TESTAUTO,
	R_SPLIT
};

struct Item {
	Kind kind;
	Role role;
	int index;          // 軌道編號（0..）或輸出聲道編號
	float x, y;         // 中心
	float w, h;         // 僅 K_CUBE / K_TESTDISP / K_LABEL 使用
	std::string text;   // 僅 K_LABEL
	float fontSize;
};

inline float rowY(int t) { return ROW0_Y + ROW_DY * t; }

// 標籤的 index：>= 0 是軌道編號；LABEL_BAND = 欄位標題色帶上的字；LABEL_OUT = 輸出區色塊上的字；其餘為 -1
constexpr int LABEL_BAND = -2;
constexpr int LABEL_OUT = -3;

// 包浩斯色塊的位置（面板 SVG 與預覽共用）
constexpr float HEADER_BAND_Y0 = 19.f, HEADER_BAND_Y1 = 33.f;   // 左側欄位標題的品牌綠色帶
constexpr float OUTPUT_BLOCK_Y0 = 290.f;                        // 右下輸出區色塊的上緣
constexpr float TEST_ROW_LABEL_Y = 257.f, TEST_ROW_Y = 274.f;  // 測試列（與輸出區色塊保持 5 px 以上距離）

inline Item comp(Kind k, Role r, int idx, float x, float y) {
	return Item{k, r, idx, x, y, 0.f, 0.f, "", 0.f};
}

inline Item label(float x, float y, const std::string& text, float fs = 7.f, int trackIdx = -1) {
	// 標籤盒尺寸為估計值：寬 ≈ 字數 × 字級 × 0.62，高 ≈ 字級 + 2
	return Item{K_LABEL, R_NONE, trackIdx, x, y, text.size() * fs * 0.62f, fs + 2.f, text, fs};
}

inline std::vector<Item> build() {
	std::vector<Item> v;

	// ---- 欄位標題 ----
	v.push_back(label(235.f, 12.f, "12 STEREO INPUTS", 10.f));
	v.push_back(label(COL_AUDIO, 26.f, "L", 7.f, LABEL_BAND));
	v.push_back(label(COL_AUDIOR, 26.f, "R", 7.f, LABEL_BAND));
	v.push_back(label(COL_LVL, 26.f, "LVL", 7.f, LABEL_BAND));
	v.push_back(label(COL_PX, 26.f, "X", 7.f, LABEL_BAND));
	v.push_back(label(COL_XCV, 26.f, "CV", 7.f, LABEL_BAND));
	v.push_back(label(COL_PY, 26.f, "Y", 7.f, LABEL_BAND));
	v.push_back(label(COL_YCV, 26.f, "CV", 7.f, LABEL_BAND));
	v.push_back(label(COL_PZ, 26.f, "Z", 7.f, LABEL_BAND));
	v.push_back(label(COL_ZCV, 26.f, "CV", 7.f, LABEL_BAND));
	v.push_back(label(COL_WIDTH, 26.f, "WIDTH", 7.f, LABEL_BAND));
	v.push_back(label(COL_LFE, 26.f, "LFE", 7.f, LABEL_BAND));
	v.push_back(label(COL_ORBBTN, 26.f, "ORB", 7.f, LABEL_BAND));
	v.push_back(label(COL_SPLIT, 26.f, "SP", 7.f, LABEL_BAND));
	v.push_back(label(COL_ORBDIA, 26.f, "DIA", 7.f, LABEL_BAND));
	v.push_back(label(COL_ORBSPD, 26.f, "SPD", 7.f, LABEL_BAND));
	v.push_back(label(COL_ORBCV, 26.f, "LFO", 7.f, LABEL_BAND));
	v.push_back(label(COL_MUTE, 26.f, "M", 7.f, LABEL_BAND));
	v.push_back(label(COL_SOLO, 26.f, "S", 7.f, LABEL_BAND));

	// ---- 12 條軌道 ----
	for (int t = 0; t < MAX_TRACKS; t++) {
		float y = rowY(t);
		v.push_back(label(COL_NUM, y, std::to_string(t + 1), 9.f, t));
		v.push_back(comp(K_LED, R_TRACKLED, t, COL_LED, y));
		v.push_back(comp(K_INPUT, R_AUDIO, t, COL_AUDIO, y));
		v.push_back(comp(K_INPUT, R_AUDIOR, t, COL_AUDIOR, y));
		v.push_back(comp(K_KNOB, R_LVL, t, COL_LVL, y));
		v.push_back(comp(K_KNOB, R_PX, t, COL_PX, y));
		v.push_back(comp(K_INPUT, R_XCV, t, COL_XCV, y));
		v.push_back(comp(K_KNOB, R_PY, t, COL_PY, y));
		v.push_back(comp(K_INPUT, R_YCV, t, COL_YCV, y));
		v.push_back(comp(K_KNOB, R_PZ, t, COL_PZ, y));
		v.push_back(comp(K_INPUT, R_ZCV, t, COL_ZCV, y));
		v.push_back(comp(K_KNOB, R_WIDTH, t, COL_WIDTH, y));
		v.push_back(comp(K_KNOB, R_LFE, t, COL_LFE, y));
		v.push_back(comp(K_BUTTON, R_ORBBTN, t, COL_ORBBTN, y));
		v.push_back(comp(K_BUTTON, R_SPLIT, t, COL_SPLIT, y));
		v.push_back(comp(K_KNOB, R_ORBDIA, t, COL_ORBDIA, y));
		v.push_back(comp(K_KNOB, R_ORBSPD, t, COL_ORBSPD, y));
		v.push_back(comp(K_INPUT, R_ORBCV, t, COL_ORBCV, y));
		v.push_back(comp(K_BUTTON, R_MUTE, t, COL_MUTE, y));
		v.push_back(comp(K_BUTTON, R_SOLO, t, COL_SOLO, y));
	}

	// ---- 右上：標題與立方體 ----
	// 右上：模組名稱 THE ISLAND，下方置中放深觸 LOGO（res/logo.png，深色字透明背景）
	v.push_back(label(RIGHT_CX, 12.f, "THE ISLAND", 11.f));
	Item logo = comp(K_LOGO, R_NONE, 0, RIGHT_CX, 27.f);
	logo.w = 64.f;
	logo.h = 11.f;
	v.push_back(logo);
	Item cube = comp(K_CUBE, R_NONE, 0, CUBE_CX, CUBE_CY);
	cube.w = CUBE_W;
	cube.h = CUBE_H;
	v.push_back(cube);

	// ---- 全域控制列（標籤 y=216、元件 y=234）----
	const float gx[6] = {RIGHT_CX - 100.f, RIGHT_CX - 60.f, RIGHT_CX - 20.f, RIGHT_CX + 20.f, RIGHT_CX + 60.f, RIGHT_CX + 100.f};
	v.push_back(label(gx[0], 216.f, "ROTATE"));
	v.push_back(comp(K_KNOB, R_ROTATE, 0, gx[0], 234.f));
	v.push_back(label(gx[1], 216.f, "ROT CV"));
	v.push_back(comp(K_INPUT, R_ROTCV, 0, gx[1], 234.f));
	v.push_back(label(gx[2], 216.f, "FOCUS"));
	v.push_back(comp(K_KNOB, R_FOCUS, 0, gx[2], 234.f));
	v.push_back(label(gx[3], 216.f, "BLUR"));
	v.push_back(comp(K_KNOB, R_BLUR, 0, gx[3], 234.f));
	v.push_back(label(gx[4], 216.f, "MASTER"));
	v.push_back(comp(K_KNOB, R_MASTER, 0, gx[4], 234.f));
	v.push_back(label(gx[5], 216.f, "12CH POLY"));
	v.push_back(comp(K_OUTPUT, R_POLYOUT, 0, gx[5], 234.f));

	// ---- 喇叭測試列（標籤 y=262、元件 y=280）----
	v.push_back(label(gx[0], TEST_ROW_LABEL_Y, "TEST"));
	v.push_back(comp(K_LATCH, R_TEST, 0, gx[0], TEST_ROW_Y));
	v.push_back(label(gx[1], TEST_ROW_LABEL_Y, "TEST CH"));
	v.push_back(comp(K_BUTTON, R_TESTCH, 0, gx[1], TEST_ROW_Y));
	v.push_back(label(gx[2], TEST_ROW_LABEL_Y, "AUTO"));
	v.push_back(comp(K_LATCH, R_TESTAUTO, 0, gx[2], TEST_ROW_Y));
	v.push_back(label(RIGHT_CX + 40.f, TEST_ROW_LABEL_Y, "NOW PLAYING"));
	v.push_back(label(gx[5], TEST_ROW_LABEL_Y, "ORB RATE"));
	v.push_back(comp(K_KNOB, R_ORBTIME, 0, gx[5], TEST_ROW_Y));
	Item td = comp(K_TESTDISP, R_NONE, 0, RIGHT_CX + 40.f, TEST_ROW_Y);
	td.w = 40.f;
	td.h = 16.f;
	v.push_back(td);

	// ---- 12 個單聲道輸出（SMPTE 順序），兩列各 6 個 ----
	static const char* const names[12] = {"L", "R", "C", "LFE", "Ls", "Rs", "Lrs", "Rrs", "Ltf", "Rtf", "Ltr", "Rtr"};
	for (int c = 0; c < 12; c++) {
		float x = gx[c % 6];
		bool second = c >= 6;
		float py = second ? 351.f : 315.f;
		float ly = second ? 333.f : 298.f;
		v.push_back(label(x, ly, std::to_string(c + 1) + " " + names[c], 7.f, LABEL_OUT));   // 輸出區色塊上的字
		v.push_back(comp(K_OUTPUT, R_CHOUT, c, x, py));
		v.push_back(comp(K_LED, R_OUTLED, c, x + OUT_LED_DX, py));
	}
	return v;
}

// ---------------------------------------------------------------------------
// ROOM 擴充模組的版面（6 HP = 90 px 寬）。模組介面與 tools/room_panel.py（透過 tests/dump_room.cpp）共用。
// ---------------------------------------------------------------------------
namespace room {
constexpr float PANEL_W = 90.f, PANEL_H = 380.f;
constexpr float CX = 45.f;
constexpr float TITLE_Y = 12.f, BAND_Y = 26.f;                       // 標題、色帶上的字（色帶與主模組同高：HEADER_BAND_Y0～Y1）
constexpr float PREVIEW_X = 9.f, PREVIEW_Y = 41.f, PREVIEW_W = 72.f, PREVIEW_H = 84.f;   // 房間預覽（俯視 + 側視）
constexpr float KNOB_Y0 = 154.f, KNOB_DY = 51.f;                     // WIDTH / DEPTH / HEIGHT 三顆旋鈕
constexpr float KNOB_LABEL_DY = 21.f, KNOB_VALUE_DY = 20.f;          // 旋鈕上方的名稱、下方的數值
constexpr float KNOB_R = 15.f;
constexpr float RATIO_LABEL_Y = 297.f, RATIO_Y = 310.f, RATIO_W = 72.f, RATIO_H = 14.f;
constexpr float ORBIT_LABEL_Y = 327.f, ORBIT_Y = 340.f;
constexpr float LINK_Y = 360.f, LINK_LABEL_X = 38.f, LINK_LED_X = 60.f;
} // namespace room

}} // namespace st::layout
