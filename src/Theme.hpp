// Theme.hpp — The Island 面板主題（不依賴 VCV Rack）。
// 包浩斯風格：全部平塗，不用漸層、光暈、發光；深觸標準色 #008888 當作色塊，朱紅、赭黃作少量點綴。
// 四個主題（右鍵選單 Panel theme 切換，另有 Auto）：
//   SKY ：淺灰藍底、墨黑字、黑色輸出區（預設）
//   MIST：霧綠底、墨黑字、黑色輸出區
//   SAND：沙色底、墨黑字、黑色輸出區
//   DARK：墨黑底、米白字、米白輸出區（深色版）
// 舊版的 Paper（米白）與 Ink（黑）已移除；舊 patch 裡存的主題一律改用 Sky。
// 模組介面（TheIsland.cpp）與面板工具（tools/panel_tool.py，透過 tests/dump_layout.cpp）共用這份色票。
#pragma once

namespace st { namespace theme {

struct C {
	unsigned char r, g, b;
};

enum ThemeId { THEME_SKY = 0, THEME_MIST, THEME_SAND, THEME_DARK, NUM_THEMES };

// 深色主題：右鍵選 Dark，或選 Auto 時跟著 Rack 的「Use dark panels」設定自動切換 Sky / Dark

// 預設主題：新放的模組、模組瀏覽器的預覽圖都用這個（Sky 淺藍）。已存檔的 patch 會保留自己原本選的主題。
constexpr int THEME_DEFAULT = THEME_SKY;

struct Palette {
	const char* name;      // 主題名稱
	const char* svgSuffix; // 面板檔名後綴：res/Atmos714<後綴>.svg
	bool logoWhite;        // 深色底用白字 LOGO（res/logo_white.png）

	// 共用的品牌色與點綴色
	C BRAND;               // #008888 深觸標準色
	C RED;                 // 包浩斯朱紅（Mute、爆音）
	C YELLOW;              // 包浩斯赭黃（Solo、偏熱）
	C PAPER;               // 米白紙色
	C INK;                 // 墨黑

	// 面板
	C BG;                  // 面板底色
	C ROW_STRIPE;          // 軌道隔行底色
	C BORDER;              // 外框粗線
	C SEPARATOR;           // 細分隔線
	C BAND;                // 欄位標題色帶
	C BAND_TEXT;           // 色帶上的字
	C OUTBLOCK;            // 輸出區色塊
	C OUTBLOCK_TEXT;       // 輸出區上的字
	C TITLE;               // 大標題
	C TEXT;                // 一般標籤
	C TEXT_DIM;            // 次要文字

	// 立方體顯示窗
	C CUBE_BG;
	C CUBE_BORDER;
	C CUBE_EDGE;
	C CUBE_FLOOR;
	C CUBE_TEXT;           // 視角名稱
	C SPEAKER;             // 耳朵高度的喇叭
	C SPEAKER_TOP;         // 天花板喇叭
	C CAPSULE_OUTLINE;     // 膠囊外框

	// 小元件
	C LED_BASE;            // LED 熄滅時（面板上）
	C LED_BASE_OUT;        // LED 熄滅時（輸出區色塊上）
	C LED_RING;
	C BUTTON_OFF;          // M/S 未按下
	C BUTTON_EDGE;
	C BUTTON_TEXT;
	C DISPLAY_BG;          // NOW PLAYING 顯示窗
	C DISPLAY_TEXT;
	C CHIP_OUTLINE;        // 軌道編號圓片外框
};

// 共用色
constexpr C BRAND_ = {0, 136, 136};
constexpr C RED_ = {214, 69, 43};
constexpr C YELLOW_ = {232, 178, 30};
constexpr C PAPER_ = {243, 238, 227};
constexpr C INK_ = {22, 22, 22};

inline const Palette& palette(int id) {
	static const Palette P[NUM_THEMES] = {
		// ---- SKY：淺灰藍（預設）----
		{"Sky", "", false,
		 BRAND_, RED_, YELLOW_, PAPER_, INK_,
		 {214, 228, 236}, {201, 218, 228}, INK_, {170, 192, 204},
		 BRAND_, PAPER_, INK_, PAPER_,
		 INK_, INK_, {92, 110, 122},
		 {240, 246, 249}, INK_, INK_, {136, 154, 166}, INK_, {52, 60, 66}, BRAND_, INK_,
		 {190, 208, 218}, {58, 58, 56}, {120, 142, 156},
		 {226, 237, 243}, INK_, INK_, INK_, {79, 209, 201}, INK_},
		// ---- MIST：霧綠 ----
		{"Mist", "_mist", false,
		 BRAND_, RED_, YELLOW_, PAPER_, INK_,
		 {221, 230, 226}, {208, 220, 215}, INK_, {178, 196, 189},
		 BRAND_, PAPER_, INK_, PAPER_,
		 INK_, INK_, {98, 116, 108},
		 {242, 247, 245}, INK_, INK_, {140, 158, 150}, INK_, {54, 62, 58}, BRAND_, INK_,
		 {196, 210, 204}, {58, 58, 56}, {124, 144, 136},
		 {232, 239, 236}, INK_, INK_, INK_, {79, 209, 201}, INK_},
		// ---- SAND：沙色 ----
		{"Sand", "_sand", false,
		 BRAND_, RED_, YELLOW_, PAPER_, INK_,
		 {232, 220, 198}, {221, 207, 181}, INK_, {196, 180, 150},
		 BRAND_, PAPER_, INK_, PAPER_,
		 INK_, INK_, {122, 106, 82},
		 {246, 239, 226}, INK_, INK_, {158, 142, 114}, INK_, {62, 56, 46}, BRAND_, INK_,
		 {212, 198, 170}, {58, 58, 56}, {150, 134, 106},
		 {240, 231, 214}, INK_, INK_, INK_, {79, 209, 201}, INK_},
		// ---- DARK：墨黑（深色版）：米白字、米白標題色帶與輸出區、白字 LOGO ----
		{"Dark", "_dark", true,
		 BRAND_, RED_, YELLOW_, PAPER_, INK_,
		 {24, 24, 23}, {34, 34, 32}, {243, 238, 227}, {58, 57, 54},
		 PAPER_, INK_, PAPER_, INK_,   // 標題色帶不用綠色：改成米白色帶、墨黑字（和輸出區一致）
		 PAPER_, {232, 227, 216}, {140, 134, 122},
		 {14, 14, 14}, {243, 238, 227}, {226, 221, 210}, {110, 106, 98}, PAPER_, {200, 196, 186}, {79, 209, 201}, {14, 14, 14},
		 {52, 52, 50}, {214, 206, 190}, {110, 106, 98},
		 {30, 30, 29}, {232, 227, 216}, {232, 227, 216}, INK_, {79, 209, 201}, {14, 14, 14}},
	};
	if (id < 0 || id >= NUM_THEMES)
		id = THEME_DEFAULT;
	return P[id];
}

}} // namespace st::theme
