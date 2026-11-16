// 輸出 ROOM 擴充模組的版面（JSON），供 tools/room_panel.py 產生面板與預覽
#include <cstdio>
#include "../src/Layout714.hpp"
#include "../src/Theme.hpp"
using namespace st::layout;
using namespace st::layout::room;

int main() {
	printf("{\"w\":%g,\"h\":%g,\"cx\":%g,\"title_y\":%g,\"band_y\":%g,\"band0\":%g,\"band1\":%g,\n", room::PANEL_W, room::PANEL_H, CX, TITLE_Y, BAND_Y, HEADER_BAND_Y0, HEADER_BAND_Y1);
	printf("\"preview\":[%g,%g,%g,%g],\"knob_y0\":%g,\"knob_dy\":%g,\"knob_label_dy\":%g,\"knob_value_dy\":%g,\"knob_r\":%g,\n", PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H, KNOB_Y0, KNOB_DY, KNOB_LABEL_DY, KNOB_VALUE_DY, KNOB_R);
	printf("\"ratio_label_y\":%g,\"ratio_y\":%g,\"ratio_w\":%g,\"ratio_h\":%g,\"orbit_label_y\":%g,\"orbit_y\":%g,\"link_y\":%g,\"link_label_x\":%g,\"link_led_x\":%g,\n", RATIO_LABEL_Y, RATIO_Y, RATIO_W, RATIO_H, ORBIT_LABEL_Y, ORBIT_Y, LINK_Y, LINK_LABEL_X, LINK_LED_X);
	printf("\"themes\":[");
	using namespace st::theme;
	for (int t = 0; t < NUM_THEMES; t++) {
		const Palette& p = palette(t);
		struct N { const char* n; C c; };
		const N cols[] = {{"BRAND", p.BRAND}, {"PAPER", p.PAPER}, {"BG", p.BG}, {"BORDER", p.BORDER}, {"SEPARATOR", p.SEPARATOR}, {"BAND", p.BAND},
		                  {"BAND_TEXT", p.BAND_TEXT}, {"TITLE", p.TITLE}, {"TEXT", p.TEXT}, {"CUBE_BG", p.CUBE_BG}, {"CUBE_BORDER", p.CUBE_BORDER},
		                  {"CUBE_EDGE", p.CUBE_EDGE}, {"CUBE_FLOOR", p.CUBE_FLOOR}, {"SPEAKER_TOP", p.SPEAKER_TOP}, {"DISPLAY_BG", p.DISPLAY_BG},
		                  {"DISPLAY_TEXT", p.DISPLAY_TEXT}, {"BUTTON_OFF", p.BUTTON_OFF}, {"BUTTON_EDGE", p.BUTTON_EDGE}, {"BUTTON_TEXT", p.BUTTON_TEXT},
		                  {"LED_BASE", p.LED_BASE}, {"LED_RING", p.LED_RING}};
		const int nc = sizeof(cols) / sizeof(cols[0]);
		printf("%s{\"name\":\"%s\",\"suffix\":\"%s\",\"c\":{", t ? "," : "", p.name, p.svgSuffix);
		for (int i = 0; i < nc; i++)
			printf("\"%s\":[%d,%d,%d]%s", cols[i].n, cols[i].c.r, cols[i].c.g, cols[i].c.b, i + 1 < nc ? "," : "");
		printf("}}");
	}
	printf("]}\n");
}
