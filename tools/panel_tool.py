#!/usr/bin/env python3
"""面板工具：產生 res/TheIsland(.svg / _mist.svg / _sand.svg)、檢查元件版面、輸出預覽圖。

用法（在專案根目錄）：
    python3 tools/panel_tool.py [輸出資料夾]

做的事：
 1. 編譯 tests/dump_layout.cpp、tests/dump_view.cpp（與模組共用 src/Layout714.hpp、src/CubeView.hpp）
 2. 依版面產生 res/TheIsland*.svg（背景、列底色、分隔線；不含文字，文字由程式碼繪製）
 3. 檢查：元件是否超出面板、元件互相重疊、標籤與元件重疊、數量是否正確
 4. 輸出 panel_preview.png 與 cube_views.png
"""
import json, os, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "preview")
os.makedirs(OUT, exist_ok=True)

K_INPUT, K_OUTPUT, K_KNOB, K_LATCH, K_LABEL, K_CUBE, K_TESTDISP, K_LED, K_BUTTON, K_LOGO = range(10)
KIND_NAME = ["INPUT", "OUTPUT", "KNOB", "LATCH", "LABEL", "CUBE", "TESTDISP", "LED", "BUTTON", "LOGO"]
R_TRACKLED, R_OUTLED = 21, 22
R_ORBBTN, R_ORBDIA, R_ORBCV, R_ORBTIME = 23, 24, 25, 26
R_MUTE, R_SOLO = 27, 28
R_ORBSPD = 29
R_SPLIT = 31

def run_dump(name):
    tmp = tempfile.mkdtemp()
    exe = os.path.join(tmp, name)
    subprocess.check_call(["g++", "-std=c++17", "-Wall", "-o", exe, os.path.join(ROOT, "tests", name + ".cpp")])
    return json.loads(subprocess.check_output([exe]))

layout = run_dump("dump_layout")
views = run_dump("dump_view")
P = layout["panel"]
W, H = P["w"], P["h"]
RAD = layout["radius"]
items = layout["items"]
THEMES = layout["themes"]   # 三個面板主題（src/Theme.hpp）：Sky、Mist、Sand

def make_th(t):
    c = {k: tuple(v) for k, v in t["c"].items()}
    # 預覽程式裡使用的別名
    c.update({"ROWS_BG": c["BG"], "RIGHT_TOP": c["BG"], "BG_BOTTOM": c["BG"], "LED_BASE_DARK": c["LED_BASE_OUT"],
              "BUTTON_OFF": c["BUTTON_OFF"], "BUTTON_OFF_EDGE": c["BUTTON_EDGE"], "BUTTON_OFF_TEXT": c["BUTTON_TEXT"],
              "CUBE_BG_TOP": c["CUBE_BG"], "CUBE_BG_BOTTOM": c["CUBE_BG"]})
    return c

THEME = THEMES[0]
TH = make_th(THEME)
def hx(name):
    r, g, b = TH[name]
    return f"#{r:02x}{g:02x}{b:02x}"

# ---------------------------------------------------------------- SVG
def write_svg():
    """面板底圖（包浩斯風格，全部平塗）：米白紙色底、墨黑外框、品牌綠欄位標題色帶、
    輸出區色塊、底部品牌綠色條。
    只用 Rack（nanosvg）支援的 SVG 功能：rect / circle / polygon / line。"""
    RR = P["rows_right"]
    rows = []
    for t in range(P["tracks"]):
        y = P["row0"] + P["row_dy"] * t - P["row_dy"] / 2
        if t % 2 == 0:
            rows.append(f'  <rect x="2.75" y="{y:g}" width="{RR-3.5:g}" height="{P["row_dy"]:g}" fill="{hx("ROW_STRIPE")}"/>')
    # 群組分隔線：數字|L R|LVL|X 群組|Y 群組|Z 群組|WIDTH LFE|繞場（ORB SP DIA SPD LFO）|M S
    seps = [26.0, 77.5, 103.5, 153.5, 203.5, 253.5, 303.0, 419.5]
    poly = [it for it in items if it["kind"] == K_OUTPUT and it["role"] == 17][0]
    OB = P["outblock"]
    svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="{W/15*5.08:g}mm" height="128.5mm" viewBox="0 0 {W:g} {H:g}">
  <rect x="0" y="0" width="{W:g}" height="{H:g}" fill="{hx("BG")}"/>
  <!-- 左側軌道區 -->
  <rect x="2" y="2" width="{RR-2:g}" height="{H-8:g}" fill="{hx("BG")}" stroke="{hx("BORDER")}" stroke-width="1.5"/>
{chr(10).join(rows)}
  <g stroke="{hx("SEPARATOR")}" stroke-width="0.8">
''' + "".join(f'    <line x1="{x:g}" y1="{P["band1"]+1:g}" x2="{x:g}" y2="{H-11:g}"/>\n' for x in seps) + f'''  </g>
  <!-- 欄位標題的品牌綠色帶 -->
  <rect x="2.75" y="{P["band0"]:g}" width="{RR-3.5:g}" height="{P["band1"]-P["band0"]:g}" fill="{hx("BAND")}"/>
  <!-- 右側 -->
  <rect x="{RR+6:g}" y="2" width="{W-RR-8:g}" height="{H-8:g}" fill="{hx("BG")}" stroke="{hx("BORDER")}" stroke-width="1.5"/>
  <line x1="{RR+14:g}" y1="209" x2="{W-10:g}" y2="209" stroke="{hx("BORDER")}" stroke-width="1"/>
  <line x1="{RR+14:g}" y1="250" x2="{W-10:g}" y2="250" stroke="{hx("SEPARATOR")}" stroke-width="0.8"/>
  <!-- 12CH POLY 輸出：黑色方塊底座 -->
  <rect x="{poly["x"]-15:g}" y="{poly["y"]-15:g}" width="30" height="30" fill="{hx("OUTBLOCK")}"/>
  <!-- 右下輸出區：黑色色塊（反白標籤） -->
  <rect x="{RR+6.75:g}" y="{OB:g}" width="{W-RR-9.5:g}" height="{H-6.75-OB:g}" fill="{hx("OUTBLOCK")}"/>
  <!-- 底部色條：和標題色帶同色（淺色主題是品牌綠，Dark 是米白） -->
  <rect x="0" y="{H-4:g}" width="{W:g}" height="4" fill="{hx("BAND")}"/>
</svg>
'''
    with open(os.path.join(ROOT, "res", f"TheIsland{THEME['suffix']}.svg"), "w") as f:
        f.write(svg)

write_svg()

# ---------------------------------------------------------------- 檢查
problems = []

def shape(it):
    k = it["kind"]
    if k in (K_INPUT, K_OUTPUT):
        return ("c", it["x"], it["y"], RAD["port"])
    if k == K_KNOB:
        return ("c", it["x"], it["y"], RAD["knob"])
    if k == K_LATCH:
        return ("c", it["x"], it["y"], RAD["latch"])
    if k == K_LED:
        return ("c", it["x"], it["y"], RAD["led"])
    if k == K_BUTTON:
        return ("c", it["x"], it["y"], RAD["button"])
    if k == K_LOGO:
        return ("r", it["x"] - it["w"] / 2, it["y"] - it["h"] / 2, it["w"], it["h"])
    return ("r", it["x"] - it["w"] / 2, it["y"] - it["h"] / 2, it["w"], it["h"])

def bbox(s):
    if s[0] == "c":
        return (s[1] - s[3], s[2] - s[3], s[1] + s[3], s[2] + s[3])
    return (s[1], s[2], s[1] + s[3], s[2] + s[4])

def overlap(a, b, gap=0.0):
    if a[0] == "c" and b[0] == "c":
        d = ((a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2) ** 0.5
        return d < a[3] + b[3] + gap
    if a[0] == "r" and b[0] == "r":
        A, B = bbox(a), bbox(b)
        return not (A[2] + gap <= B[0] or B[2] + gap <= A[0] or A[3] + gap <= B[1] or B[3] + gap <= A[1])
    c, r = (a, b) if a[0] == "c" else (b, a)
    rx, ry, rw, rh = r[1], r[2], r[3], r[4]
    nx = min(max(c[1], rx), rx + rw)
    ny = min(max(c[2], ry), ry + rh)
    return ((c[1] - nx) ** 2 + (c[2] - ny) ** 2) ** 0.5 < c[3] + gap

def name(it):
    t = it.get("text", "")
    return f'{KIND_NAME[it["kind"]]}(role={it["role"]},idx={it["index"]}{","+t if t else ""})@({it["x"]:g},{it["y"]:g})'

shapes = [(it, shape(it)) for it in items]

# 邊界（距離面板邊緣至少 2 px；軌道數字標籤靠左，容許到 1 px）
for it, s in shapes:
    x0, y0, x1, y1 = bbox(s)
    if x0 < 2 or y0 < 2 or x1 > W - 2 or y1 > H - 2:
        problems.append(f"超出面板邊界: {name(it)}  bbox=({x0:.1f},{y0:.1f},{x1:.1f},{y1:.1f})")

# 互相重疊
for i in range(len(shapes)):
    for j in range(i + 1, len(shapes)):
        a, sa = shapes[i]
        b, sb = shapes[j]
        gap = 1.0 if (a["kind"] != K_LABEL and b["kind"] != K_LABEL) else 0.0
        if overlap(sa, sb, gap):
            problems.append(f"重疊: {name(a)}  ×  {name(b)}")

# 數量
def count(kind, role=None):
    return sum(1 for it in items if it["kind"] == kind and (role is None or it["role"] == role))
N = P["tracks"]
expect = [
    ("audio L inputs", count(K_INPUT, 1), N),
    ("audio R inputs", count(K_INPUT, 19), N),
    ("WIDTH knobs", count(K_KNOB, 20), N),
    ("X/Y/Z CV inputs", count(K_INPUT, 2) + count(K_INPUT, 3) + count(K_INPUT, 4), 3 * N),
    ("X/Y/Z/LVL/LFE knobs", sum(count(K_KNOB, r) for r in (5, 6, 7, 8, 9)), 5 * N),
    ("mono outputs", count(K_OUTPUT, 18), 12),
    ("poly output", count(K_OUTPUT, 17), 1),
    ("cube display", count(K_CUBE), 1),
    ("test display", count(K_TESTDISP), 1),
    ("test latches (TEST + AUTO)", count(K_LATCH), 2),
    ("test auto latch", count(K_LATCH, 30), 1),
    ("logo", count(K_LOGO), 1),
    ("track LEDs", count(K_LED, R_TRACKLED), N),
    ("output LEDs", count(K_LED, R_OUTLED), 12),
    ("orbit buttons", count(K_BUTTON, R_ORBBTN), N),
    ("split buttons", count(K_BUTTON, R_SPLIT), N),
    ("test channel button", count(K_BUTTON, 16), 1),
    ("orbit diameter knobs", count(K_KNOB, R_ORBDIA), N),
    ("orbit speed knobs", count(K_KNOB, R_ORBSPD), N),
    ("orbit LFO inputs", count(K_INPUT, R_ORBCV), N),
    ("orbit time knob", count(K_KNOB, R_ORBTIME), 1),
    ("mute buttons", count(K_BUTTON, R_MUTE), N),
    ("solo buttons", count(K_BUTTON, R_SOLO), N),
]
for label, got, want in expect:
    if got != want:
        problems.append(f"數量不符: {label} 應為 {want}，實際 {got}")

# 元件不可壓到色塊：右側非輸出區的元件，下緣要離輸出區色塊至少 3 px
OB = P["outblock"]
OUT_ITEMS = lambda it: (it["kind"] == K_OUTPUT and it["role"] == 18) or (it["kind"] == K_LED and it["role"] == R_OUTLED) or it["index"] == -3
for it, sh in shapes:
    if it["x"] > P["rows_right"] and not OUT_ITEMS(it):
        x0, y0, x1, y1 = bbox(sh)
        if y1 > OB - 3 and y0 < OB + 1:
            problems.append(f"壓到輸出區色塊（或距離不到 3 px）: {name(it)}  下緣 y={y1:.1f}，色塊上緣 y={OB:g}")
    if OUT_ITEMS(it):
        x0, y0, x1, y1 = bbox(sh)
        if y0 < OB + 2:
            problems.append(f"輸出區元件超出色塊上緣: {name(it)}  上緣 y={y0:.1f}")

# 每軌 (role,index) 不可重複
seen = {}
for it in items:
    if it["kind"] in (K_INPUT, K_OUTPUT, K_KNOB, K_LATCH, K_LED, K_BUTTON):
        key = (it["kind"], it["role"], it["index"])
        if key in seen:
            problems.append(f"重複的元件編號: {name(it)}")
        seen[key] = 1

# 每個輸出 LED 緊貼在同編號輸出孔的右側；每軌 LED 在該軌編號與 L 輸入之間
outs_by_idx = {it["index"]: it for it in items if it["kind"] == K_OUTPUT and it["role"] == 18}
for led in [it for it in items if it["kind"] == K_LED and it["role"] == R_OUTLED]:
    o = outs_by_idx.get(led["index"])
    if not o or led["y"] != o["y"] or not (12 < led["x"] - o["x"] < 22):
        problems.append(f"輸出 LED {led['index']+1} 沒有緊貼在輸出 {led['index']+1} 右側")
nums = {it["index"]: it for it in items if it["kind"] == K_LABEL and it["index"] >= 0 and it["text"].isdigit()}
for led in [it for it in items if it["kind"] == K_LED and it["role"] == R_TRACKLED]:
    t = led["index"]
    n_, l_ = nums.get(t), Ls_.get(t) if "Ls_" in globals() else None
    if not n_ or led["y"] != n_["y"] or not (n_["x"] < led["x"]):
        problems.append(f"軌道 LED {t+1} 位置不對（應在編號右側、同一排）")

# 每軌的 L / R 輸入必須在同一排，R 緊鄰 L 右側
Ls_ = {it["index"]: it for it in items if it["kind"] == K_INPUT and it["role"] == 1}
Rs_ = {it["index"]: it for it in items if it["kind"] == K_INPUT and it["role"] == 19}
for t in range(N):
    if t not in Ls_ or t not in Rs_:
        problems.append(f"軌道 {t+1} 缺少 L 或 R 輸入")
    elif Ls_[t]["y"] != Rs_[t]["y"] or not (0 < Rs_[t]["x"] - Ls_[t]["x"] < 40):
        problems.append(f"軌道 {t+1} 的 L/R 輸入沒有並排在同一排")

# 輸出聲道順序：第 c 個輸出的標籤要寫 "c+1 名稱"，位置要照 SMPTE 順序由左到右、由上到下
SMPTE = ["L", "R", "C", "LFE", "Ls", "Rs", "Lrs", "Rrs", "Ltf", "Rtf", "Ltr", "Rtr"]
outs = sorted([it for it in items if it["kind"] == K_OUTPUT and it["role"] == 18], key=lambda i: i["index"])
labs = [it for it in items if it["kind"] == K_LABEL and it["text"].split(" ")[0].isdigit() and " " in it["text"]]
for o in outs:
    want = f'{o["index"]+1} {SMPTE[o["index"]]}'
    l = [x for x in labs if x["text"] == want]
    if len(l) != 1 or abs(l[0]["x"] - o["x"]) > 0.1 or not (0 < o["y"] - l[0]["y"] < 40 or 0 < l[0]["y"] - o["y"] < 40):
        problems.append(f"輸出 {o['index']+1} 的標籤 '{want}' 與位置不符")
ordered = sorted(outs, key=lambda i: (round(i["y"]), i["x"]))
if [o["index"] for o in ordered] != list(range(12)):
    problems.append("12 個輸出的視覺順序不是 SMPTE 1→12（左到右、上到下）")

print("=" * 60)
print(f"面板 {W:g}×{H:g} px（{W/15:g} HP），元件 {len(items)} 個")
print("=" * 60)
if problems:
    print(f"發現 {len(problems)} 個問題：")
    for p in problems:
        print("  -", p)
else:
    print("版面檢查：全部通過（無超出邊界、無重疊、數量與輸出順序正確）")

# ---------------------------------------------------------------- 預覽圖
from PIL import Image, ImageDraw, ImageFont
S = 2
def font(size, bold=False):
    p = os.path.join(ROOT, "res", "fonts", "Jost-700-Bold.ttf" if bold else "Jost-500-Medium.ttf")
    return ImageFont.truetype(p, int(size * S))

def dark_text(c):   # 與 CubeView.hpp 的 darkText 相同
    return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2] > 140

# 預覽示範：第 9、12 軌沒接線（不顯示）、第 3 軌單聲道、第 5 軌爆音（與面板 LED 示範一致）
DEMO_UNPATCHED = {9, 12}
DEMO_MONO = {3}
DEMO_CLIP = {5}

def capsule(d, ox, oy, a, b, width, col):
    (x1, y1), (x2, y2) = a, b
    r = width / 2
    if abs(x1 - x2) + abs(y1 - y2) >= 0.5:
        d.line([(ox + x1) * S, (oy + y1) * S, (ox + x2) * S, (oy + y2) * S], fill=col, width=max(1, int(round(width * S))))
    for x, y in (a, b):
        d.ellipse([(ox + x - r) * S, (oy + y - r) * S, (ox + x + r) * S, (oy + y + r) * S], fill=col)

def draw_cube(d, ox, oy, view, scale_s=S):
    w, h = views["w"], views["h"]
    # 立方體背景：上下漸層（模擬模組裡的 nvgLinearGradient）
    t0, t1 = TH["CUBE_BG_TOP"], TH["CUBE_BG_BOTTOM"]
    for yy in range(int(h * S)):
        f = yy / (h * S)
        d.line([ox * S, oy * S + yy, (ox + w) * S, oy * S + yy], fill=tuple(int(t0[i] + (t1[i] - t0[i]) * f) for i in range(3)))
    d.rectangle([ox * S, oy * S, (ox + w) * S, (oy + h) * S], outline=TH["CUBE_BORDER"], width=3)
    bgc = TH["CUBE_BG_TOP"]
    mix = lambda c, a: tuple(int(c[i] * a + bgc[i] * (1 - a)) for i in range(3))   # 模擬半透明
    for e in view["edges"]:
        d.line([(ox + e[0]) * S, (oy + e[1]) * S, (ox + e[2]) * S, (oy + e[3]) * S], fill=mix(TH["CUBE_EDGE"], 0.75), width=2)
    for sp in view["speakers"]:
        col = mix(TH["SPEAKER_TOP"] if sp["top"] else TH["SPEAKER"], 0.7)
        x, y = (ox + sp["x"]) * S, (oy + sp["y"]) * S
        d.rectangle([x - 2 * S, y - 2 * S, x + 2 * S, y + 2 * S], outline=col)
        d.text((x, y - 7 * S), sp["n"], fill=col, font=font(6), anchor="mm")
    srcs = []
    for src in view["sources"]:
        t = src["t"]
        if t in DEMO_UNPATCHED:
            continue
        a_, b_ = src["pts"]
        if t in DEMO_MONO:
            mx = {k: (a_[k] + b_[k]) / 2 for k in ("x", "y", "d", "fx", "fy")}
            a_ = b_ = mx
        srcs.append(((a_["d"] + b_["d"]) / 2, t, src["c"], a_, b_))
    srcs.sort(key=lambda e: -e[0])
    if view["name"] == "ISO":
        for _, t, c, a_, b_ in srcs:
            sh = mix(c, 0.22)
            capsule(d, ox, oy, (a_["fx"], a_["fy"]), (b_["fx"], b_["fy"]), 5, sh)
    for _, t, c, a_, b_ in srcs:
        A, B = (a_["x"], a_["y"]), (b_["x"], b_["y"])
        if t in DEMO_CLIP:
            capsule(d, ox, oy, A, B, 14, TH["RED"])
        capsule(d, ox, oy, A, B, 11.4, TH["CAPSULE_OUTLINE"])
        capsule(d, ox, oy, A, B, 10, tuple(c))
        mx, my = (A[0] + B[0]) / 2, (A[1] + B[1]) / 2
        d.text(((ox + mx) * S, (oy + my + 0.5) * S), str(t), fill=TH["INK"] if dark_text(c) else TH["PAPER"], font=font(6.5 if t < 10 else 5.5, True), anchor="mm")
    d.text(((ox + 6) * S, (oy + 8) * S), f'{view["name"]}  (click: view)', fill=TH["CUBE_TEXT"], font=font(7), anchor="lm")
    d.text(((ox + w - 6) * S, (oy + 8) * S), f'OUT {view["out"]}CH', fill=TH["TEXT_DIM"], font=font(7), anchor="rm")
    d.text(((ox + 6) * S, (oy + h - 7) * S), "CLIP Lrs", fill=TH["RED"], font=font(7), anchor="lm")

import cairosvg, io

def render_panel(outname):
    global d
    _png = cairosvg.svg2png(url=os.path.join(ROOT, "res", f"TheIsland{THEME['suffix']}.svg"), output_width=int(W * S), output_height=int(H * S))
    img = Image.open(io.BytesIO(_png)).convert("RGB")
    d = ImageDraw.Draw(img)
    for it in items:
        k, x, y = it["kind"], it["x"] * S, it["y"] * S
        if k in (K_INPUT, K_OUTPUT):
            r = RAD["port"] * S
            r = 11.5 * S
            d.ellipse([x - r, y - r, x + r, y + r], fill=(206, 210, 214), outline=(150, 156, 162))
            r2 = 7 * S
            d.ellipse([x - r2, y - r2, x + r2, y + r2], fill=(48, 52, 56))
            r3 = 4.5 * S
            d.ellipse([x - r3, y - r3, x + r3, y + r3], fill=(12, 12, 14))
            if k == K_OUTPUT:
                pass   # 輸出孔外圈已畫在面板 SVG
        elif k == K_KNOB:
            r = RAD["knob"] * S
            r = 9 * S
            d.ellipse([x - r, y - r, x + r, y + r], fill=(232, 235, 237), outline=(140, 146, 152), width=2)
            d.line([x, y, x, y - r * 0.85], fill=(40, 44, 48), width=2)
        elif k == K_BUTTON and it["role"] in (R_MUTE, R_SOLO):
            # 示範：第 3 軌 Mute、第 7 軌 Solo
            on = (it["role"] == R_MUTE and it["index"] == 2) or (it["role"] == R_SOLO and it["index"] == 6)
            oncol = TH["RED"] if it["role"] == R_MUTE else TH["YELLOW"]
            d.rectangle([x - 7.5 * S, y - 6 * S, x + 7.5 * S, y + 6 * S], fill=oncol if on else TH["BUTTON_OFF"], outline=TH["BUTTON_OFF_EDGE"], width=2)
            d.text((x, y + 0.5 * S), "M" if it["role"] == R_MUTE else "S", fill=TH["INK"] if on else TH["BUTTON_TEXT"], font=font(8.5, True), anchor="mm")
        elif k == K_BUTTON and it["role"] == R_SPLIT:
            on = it["index"] == 4   # 示範：第 5 軌 SPLIT 開啟
            d.rectangle([x - 7.5 * S, y - 6 * S, x + 7.5 * S, y + 6 * S], fill=TH["BRAND"] if on else TH["BUTTON_OFF"], outline=TH["BUTTON_OFF_EDGE"], width=2)
            d.text((x, y + 0.5 * S), "SP", fill=TH["PAPER"] if on else TH["BUTTON_TEXT"], font=font(8.5, True), anchor="mm")
        elif k == K_BUTTON:
            r = 7.7 * S
            d.rounded_rectangle([x - r, y - r, x + r, y + r], 2 * S, fill=(210, 214, 218), outline=(140, 146, 152))
            d.ellipse([x - 4.5 * S, y - 4.5 * S, x + 4.5 * S, y + 4.5 * S], fill=(90, 96, 102))
        elif k == K_LATCH:
            r = RAD["latch"] * S
            d.ellipse([x - r, y - r, x + r, y + r], fill=(50, 54, 60), outline=(150, 156, 168))
        elif k == K_LABEL:
            col = TH["TEXT"]
            if it["index"] >= 0 and it["text"].isdigit():
                # 軌道編號：彩色小圓片＋深色數字（與模組的 TrackChip 相同）
                tc = tuple(views["views"][0]["sources"][it["index"]]["c"])
                d.ellipse([x - 6.5 * S, y - 6.5 * S, x + 6.5 * S, y + 6.5 * S], fill=tc, outline=TH["INK"])
                d.text((x, y + 0.5 * S), it["text"], fill=TH["INK"] if dark_text(tc) else TH["PAPER"], font=font(7.5 if len(it["text"]) > 1 else 8.5, True), anchor="mm")
                continue
            if it["index"] == -2:
                col = TH["BAND_TEXT"]
            elif it["index"] == -3:
                col = TH["OUTBLOCK_TEXT"]
            elif it["fs"] >= 10:
                col = TH["TITLE"]
            d.text((x, y), it["text"], fill=col, font=font(it["fs"], it["fs"] >= 10 or it["index"] in (-2, -3)), anchor="mm")
        elif k == K_LED:
            demo_out = {0: 1, 1: 1, 2: 2, 4: 1, 5: 1, 6: 3, 7: 1, 8: 1, 9: 2, 10: 1, 11: 1}
            demo_trk = {0: 1, 1: 2, 2: 1, 3: 1, 4: 3, 5: 1, 6: 1, 7: 1, 8: 0, 9: 1, 10: 1, 11: 0}
            st_ = (demo_out if it["role"] == R_OUTLED else demo_trk).get(it["index"], 0)
            col = {0: TH["LED_BASE_DARK"] if it["role"] == R_OUTLED else TH["LED_BASE"], 1: TH["BRAND"], 2: TH["YELLOW"], 3: TH["RED"]}[st_]
            if st_ >= 2:
                for rr, a in ((8, 50), (6, 90)):
                    bgv = sum(TH["OUTBLOCK"] if it["role"] == R_OUTLED else TH["BG"]) / 3
                    gc = tuple(int(c * a / 255 + bgv * (1 - a / 255)) for c in col)
                    d.ellipse([x - rr * S, y - rr * S, x + rr * S, y + rr * S], fill=gc)
            d.ellipse([x - 3 * S, y - 3 * S, x + 3 * S, y + 3 * S], fill=col, outline=TH["LED_RING"])
        elif k == K_TESTDISP:
            d.rectangle([x - it["w"] / 2 * S, y - it["h"] / 2 * S, x + it["w"] / 2 * S, y + it["h"] / 2 * S], fill=TH["DISPLAY_BG"], outline=tuple(int(TH["BORDER"][i] * 0.31 + TH["BG"][i] * 0.69) for i in range(3)), width=2)
            d.text((x, y), "---", fill=TH["TEXT_DIM"], font=font(11), anchor="mm")
        elif k == K_LOGO:
            lp = os.path.join(ROOT, "res", "logo_white.png" if THEME["logoWhite"] else "logo.png")
            bx, by, bw, bh = (it["x"] - it["w"] / 2) * S, (it["y"] - it["h"] / 2) * S, it["w"] * S, it["h"] * S
            if os.path.exists(lp):
                lg = Image.open(lp).convert("RGBA")
                sc = min(bw / lg.width, bh / lg.height)
                lg = lg.resize((max(1, int(lg.width * sc)), max(1, int(lg.height * sc))), Image.LANCZOS)
                base = img.convert("RGBA")
                base.alpha_composite(lg, (int(bx + (bw - lg.width) / 2), int(by + (bh - lg.height) / 2)))
                img.paste(base.convert("RGB"))
                d = ImageDraw.Draw(img)
            else:
                d.rectangle([bx, by, bx + bw, by + bh], outline=TH["TEXT_DIM"])
        elif k == K_CUBE:
            draw_cube(d, it["x"] - it["w"] / 2, it["y"] - it["h"] / 2, views["views"][0])
    img.save(os.path.join(OUT, outname))

render_panel("panel_preview.png")

# 六張：四個視角（12 聲道）＋ 4 聲道、8 聲道輸出配置
vw, vh = views["w"], views["h"]
sheet = Image.new("RGB", (int((vw * 2 + 30) * S), int((vh * 3 + 40) * S)), TH["BG_BOTTOM"])
d2 = ImageDraw.Draw(sheet)
for i, v in enumerate(views["views"]):
    draw_cube(d2, 10 + (i % 2) * (vw + 10), 10 + (i // 2) * (vh + 10), v)
sheet.save(os.path.join(OUT, "cube_views.png"))

# 其他主題（Ink、Sky）：面板 SVG 與預覽
for t in THEMES[1:]:
    THEME = t
    TH = make_th(t)
    write_svg()
    render_panel(f"panel_preview{t['suffix']}.png")

print(f"\n已寫入 res/TheIsland.svg / _mist.svg / _sand.svg、{OUT}/panel_preview(.png / _mist / _sand)、{OUT}/cube_views.png")
sys.exit(1 if problems else 0)
