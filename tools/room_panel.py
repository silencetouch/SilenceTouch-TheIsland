#!/usr/bin/env python3
"""ROOM 擴充模組的面板工具：產生 res/TheIslandRoom(.svg / _mist.svg / _sand.svg)、檢查版面、輸出預覽圖。

用法（在專案根目錄）：
    python3 tools/room_panel.py [輸出資料夾]

面板底圖只含色塊（不含文字，文字由程式碼繪製），版面數值來自 src/Layout714.hpp（透過 tests/dump_room.cpp）。
"""
import json, os, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "preview")
os.makedirs(OUT, exist_ok=True)

tmp = tempfile.mkdtemp()
exe = os.path.join(tmp, "dump_room")
subprocess.check_call(["g++", "-std=c++17", "-Wall", "-o", exe, os.path.join(ROOT, "tests", "dump_room.cpp")])
L = json.loads(subprocess.check_output([exe]))
W, H, CX = L["w"], L["h"], L["cx"]

def hexc(c):
    return "#%02x%02x%02x" % tuple(c)

# ---------------------------------------------------------------- SVG（與主模組同一套包浩斯平塗風格）
def write_svg(theme):
    c = theme["c"]
    svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="{W/15*5.08:g}mm" height="128.693mm" viewBox="0 0 {W:g} {H:g}">
  <rect x="0" y="0" width="{W:g}" height="{H:g}" fill="{hexc(c["BG"])}"/>
  <rect x="2" y="2" width="{W-4:g}" height="{H-8:g}" fill="{hexc(c["BG"])}" stroke="{hexc(c["BORDER"])}" stroke-width="1.5"/>
  <!-- 品牌綠色帶 -->
  <rect x="2.75" y="{L["band0"]:g}" width="{W-5.5:g}" height="{L["band1"]-L["band0"]:g}" fill="{hexc(c["BAND"])}"/>
  <!-- 分隔線：預覽 | 尺寸旋鈕 | 比例與繞場 -->
  <line x1="9" y1="{L["ratio_label_y"]-10:g}" x2="{W-9:g}" y2="{L["ratio_label_y"]-10:g}" stroke="{hexc(c["SEPARATOR"])}" stroke-width="0.8"/>
  <!-- 底部色條：和標題色帶同色（淺色主題是品牌綠，Dark 是米白） -->
  <rect x="0" y="{H-4:g}" width="{W:g}" height="4" fill="{hexc(c["BAND"])}"/>
</svg>
'''
    with open(os.path.join(ROOT, "res", f"TheIslandRoom{theme['suffix']}.svg"), "w") as f:
        f.write(svg)

for th in L["themes"]:
    write_svg(th)

# ---------------------------------------------------------------- 版面檢查
boxes = []   # (名稱, x0, y0, x1, y1)
def box(name, x0, y0, x1, y1):
    boxes.append((name, x0, y0, x1, y1))

def text_box(name, cx, cy, text, size):
    w = len(text) * size * 0.62
    box(name, cx - w / 2, cy - size / 2, cx + w / 2, cy + size / 2)

text_box("title", CX, L["title_y"], "ROOM", 11)
box("band", 2.75, L["band0"], W - 2.75, L["band1"])
px, py, pw, ph = L["preview"]
box("preview", px, py, px + pw, py + ph)
for i, n in enumerate(["WIDTH", "DEPTH", "HEIGHT"]):
    y = L["knob_y0"] + L["knob_dy"] * i
    text_box(n + " label", CX, y - L["knob_label_dy"], n, 7)
    box(n + " knob", CX - L["knob_r"], y - L["knob_r"], CX + L["knob_r"], y + L["knob_r"])
    text_box(n + " value", CX, y + L["knob_value_dy"], "60.0 m", 10)
text_box("ratio label", CX, L["ratio_label_y"], "RATIO  W : D : H", 6.5)
box("ratio display", CX - L["ratio_w"] / 2, L["ratio_y"] - L["ratio_h"] / 2, CX + L["ratio_w"] / 2, L["ratio_y"] + L["ratio_h"] / 2)
text_box("orbit label", CX, L["orbit_label_y"], "ORBIT", 7)
box("orbit button", CX - 23, L["orbit_y"] - 7, CX + 23, L["orbit_y"] + 7)
text_box("link label", L["link_label_x"], L["link_y"], "LINK", 7)
box("link led", L["link_led_x"] - 4, L["link_y"] - 4, L["link_led_x"] + 4, L["link_y"] + 4)

problems = []
for name, x0, y0, x1, y1 in boxes:
    if name == "band":
        continue
    if x0 < 4 or x1 > W - 4 or y0 < 3 or y1 > H - 7:
        problems.append(f"{name} 超出面板 ({x0:.1f},{y0:.1f})-({x1:.1f},{y1:.1f})")
for i in range(len(boxes)):
    for j in range(i + 1, len(boxes)):
        a, b = boxes[i], boxes[j]
        if a[1] < b[3] and b[1] < a[3] and a[2] < b[4] and b[2] < a[4]:
            problems.append(f"{a[0]} 與 {b[0]} 重疊")
if problems:
    print("版面檢查：有問題")
    for p in problems:
        print("  ✗", p)
else:
    print("ROOM 版面檢查：全部通過（無超出邊界、無重疊）")

# ---------------------------------------------------------------- 預覽圖（三個主題並排；示範 8 × 16 × 4 m、ROUND、已連線）
from PIL import Image, ImageDraw, ImageFont
S = 3
def font(size, bold=False):
    p = os.path.join(ROOT, "res", "fonts", "Jost-700-Bold.ttf" if bold else "Jost-500-Medium.ttf")
    return ImageFont.truetype(p, int(size * S))

def trim(v, n):
    s = f"{v:.{n}f}"
    return s.rstrip("0").rstrip(".") if "." in s else s

DEMO = (8.0, 16.0, 4.0, True)

def draw_panel(theme):
    c = {k: tuple(v) for k, v in theme["c"].items()}
    img = Image.new("RGB", (int(W * S), int(H * S)), c["BG"])
    d = ImageDraw.Draw(img)
    R = lambda x0, y0, x1, y1, **kw: d.rectangle([x0 * S, y0 * S, x1 * S, y1 * S], **kw)
    T = lambda x, y, t, size, col, bold=False: d.text((x * S, y * S), t, fill=col, font=font(size, bold), anchor="mm")
    R(2, 2, W - 2, H - 6, outline=c["BORDER"], width=int(1.5 * S))
    R(2.75, L["band0"], W - 2.75, L["band1"], fill=c["BAND"])
    R(0, H - 4, W, H, fill=c["BAND"])
    d.line([9 * S, (L["ratio_label_y"] - 10) * S, (W - 9) * S, (L["ratio_label_y"] - 10) * S], fill=c["SEPARATOR"], width=2)
    T(CX, L["title_y"], "ROOM", 11, c["TITLE"], True)
    T(CX, L["band_y"], "EXPANDER", 7, c["BAND_TEXT"], True)
    # 房間預覽（與 RoomPreview::draw 相同的算法）
    w, dd, h, rnd = DEMO
    R(px, py, px + pw, py + ph, fill=c["CUBE_BG"], outline=c["CUBE_BORDER"], width=S)
    pad, gap = 7.0, 9.0
    sc = min((pw - 2 * pad) / max(w, dd), (ph - 2 * pad - gap) / (dd + h))
    tw, td, sh = w * sc, dd * sc, h * sc
    y0 = py + (ph - (td + gap + sh)) / 2
    cx = px + pw / 2
    R(cx - tw / 2, y0, cx + tw / 2, y0 + td, outline=c["CUBE_EDGE"], width=int(1.2 * S))
    d.line([(cx - tw / 2) * S, y0 * S, (cx + tw / 2) * S, y0 * S], fill=c["BRAND"], width=int(2.2 * S))
    rx, ry = tw / 2 * 0.9, td / 2 * 0.9
    if rnd:
        rx = ry = min(rx, ry)
    ccy = y0 + td / 2
    d.ellipse([(cx - rx) * S, (ccy - ry) * S, (cx + rx) * S, (ccy + ry) * S], outline=c["CUBE_FLOOR"], width=S)
    d.ellipse([(cx - 1.4) * S, (ccy - 1.4) * S, (cx + 1.4) * S, (ccy + 1.4) * S], fill=c["TEXT"])
    sy = y0 + td + gap
    R(cx - td / 2, sy, cx + td / 2, sy + sh, outline=c["SPEAKER_TOP"], width=S)
    d.line([(cx + td / 2) * S, sy * S, (cx + td / 2) * S, (sy + sh) * S], fill=c["BRAND"], width=int(2.2 * S))
    # 旋鈕與數值
    for i, (n, v) in enumerate(zip(["WIDTH", "DEPTH", "HEIGHT"], DEMO[:3])):
        y = L["knob_y0"] + L["knob_dy"] * i
        T(CX, y - L["knob_label_dy"], n, 7, c["TEXT"])
        r = L["knob_r"]
        d.ellipse([(CX - r) * S, (y - r) * S, (CX + r) * S, (y + r) * S], fill=(34, 34, 34), outline=(90, 90, 90), width=S)
        d.line([CX * S, y * S, CX * S, (y - r + 2) * S], fill=(240, 240, 240), width=int(1.6 * S))
        T(CX, y + L["knob_value_dy"], trim(v, 1) + " m", 10, c["TEXT"])
    T(CX, L["ratio_label_y"], "RATIO  W : D : H", 6.5, c["TEXT"])
    R(CX - L["ratio_w"] / 2, L["ratio_y"] - L["ratio_h"] / 2, CX + L["ratio_w"] / 2, L["ratio_y"] + L["ratio_h"] / 2, fill=c["DISPLAY_BG"], outline=c["SEPARATOR"], width=2)
    T(CX, L["ratio_y"] + 0.5, f"1 : {trim(dd / w, 2)} : {trim(h / w, 2)}", 9.5, c["DISPLAY_TEXT"])
    T(CX, L["orbit_label_y"], "ORBIT", 7, c["TEXT"])
    R(CX - 23, L["orbit_y"] - 7, CX + 23, L["orbit_y"] + 7, fill=c["BRAND"] if rnd else c["BUTTON_OFF"], outline=c["BUTTON_EDGE"], width=2)
    T(CX, L["orbit_y"] + 0.5, "ROUND" if rnd else "FIT", 8.5, c["PAPER"] if rnd else c["BUTTON_TEXT"], True)
    T(L["link_label_x"], L["link_y"], "LINK", 7, c["TEXT"])
    lx = L["link_led_x"]
    d.ellipse([(lx - 4) * S, (L["link_y"] - 4) * S, (lx + 4) * S, (L["link_y"] + 4) * S], fill=c["BRAND"], outline=c["LED_RING"], width=2)
    return img

panels = [draw_panel(t) for t in L["themes"]]
gapx = 16 * S
sheet = Image.new("RGB", (len(panels) * int(W * S) + (len(panels) + 1) * gapx, int(H * S) + 2 * gapx), (128, 128, 128))
for i, p in enumerate(panels):
    sheet.paste(p, (gapx + i * (int(W * S) + gapx), gapx))
sheet.save(os.path.join(OUT, "room_preview.png"))
print("已寫入 res/TheIslandRoom.svg / _mist.svg / _sand.svg、" + os.path.join(OUT, "room_preview.png"))
