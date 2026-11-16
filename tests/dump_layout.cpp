// 輸出面板版面（JSON），供 tools/panel_tool.py 檢查與預覽
#include <cstdio>
#include "../src/Layout714.hpp"
#include "../src/Theme.hpp"
using namespace st::layout;

int main() {
	auto items = build();
	printf("{\"panel\":{\"w\":%g,\"h\":%g,\"rows_right\":%g,\"row0\":%g,\"row_dy\":%g,\"tracks\":%d,\"band0\":%g,\"band1\":%g,\"outblock\":%g},\n", PANEL_W, PANEL_H, ROWS_RIGHT, ROW0_Y, ROW_DY, MAX_TRACKS, HEADER_BAND_Y0, HEADER_BAND_Y1, OUTPUT_BLOCK_Y0);
	printf("\"radius\":{\"port\":%g,\"knob\":%g,\"latch\":%g,\"led\":%g,\"button\":%g},\n\"items\":[\n", R_PORT, R_KNOB, R_LATCH, R_LED, R_BUTTON);
	for (size_t i = 0; i < items.size(); i++) {
		const Item& it = items[i];
		printf("{\"kind\":%d,\"role\":%d,\"index\":%d,\"x\":%g,\"y\":%g,\"w\":%g,\"h\":%g,\"text\":\"%s\",\"fs\":%g}%s\n",
		       (int) it.kind, (int) it.role, it.index, it.x, it.y, it.w, it.h, it.text.c_str(), it.fontSize,
		       i + 1 < items.size() ? "," : "");
	}
	printf("],\n\"themes\":[");
	using namespace st::theme;
	for (int t = 0; t < NUM_THEMES; t++) {
		const Palette& p = palette(t);
		struct N { const char* n; C c; };
		const N cols[] = {
			{"BRAND", p.BRAND}, {"RED", p.RED}, {"YELLOW", p.YELLOW}, {"PAPER", p.PAPER}, {"INK", p.INK},
			{"BG", p.BG}, {"ROW_STRIPE", p.ROW_STRIPE}, {"BORDER", p.BORDER}, {"SEPARATOR", p.SEPARATOR},
			{"BAND", p.BAND}, {"BAND_TEXT", p.BAND_TEXT}, {"OUTBLOCK", p.OUTBLOCK}, {"OUTBLOCK_TEXT", p.OUTBLOCK_TEXT},
			{"TITLE", p.TITLE}, {"TEXT", p.TEXT}, {"TEXT_DIM", p.TEXT_DIM},
			{"CUBE_BG", p.CUBE_BG}, {"CUBE_BORDER", p.CUBE_BORDER}, {"CUBE_EDGE", p.CUBE_EDGE}, {"CUBE_FLOOR", p.CUBE_FLOOR},
			{"CUBE_TEXT", p.CUBE_TEXT}, {"SPEAKER", p.SPEAKER}, {"SPEAKER_TOP", p.SPEAKER_TOP}, {"CAPSULE_OUTLINE", p.CAPSULE_OUTLINE},
			{"LED_BASE", p.LED_BASE}, {"LED_BASE_OUT", p.LED_BASE_OUT}, {"LED_RING", p.LED_RING},
			{"BUTTON_OFF", p.BUTTON_OFF}, {"BUTTON_EDGE", p.BUTTON_EDGE}, {"BUTTON_TEXT", p.BUTTON_TEXT},
			{"DISPLAY_BG", p.DISPLAY_BG}, {"DISPLAY_TEXT", p.DISPLAY_TEXT}, {"CHIP_OUTLINE", p.CHIP_OUTLINE},
		};
		const int nc = sizeof(cols) / sizeof(cols[0]);
		printf("%s{\"name\":\"%s\",\"suffix\":\"%s\",\"logoWhite\":%d,\"c\":{", t ? "," : "", p.name, p.svgSuffix, p.logoWhite ? 1 : 0);
		for (int i = 0; i < nc; i++)
			printf("\"%s\":[%d,%d,%d]%s", cols[i].n, cols[i].c.r, cols[i].c.g, cols[i].c.b, i + 1 < nc ? "," : "");
		printf("}}");
	}
	printf("]}\n");
}
