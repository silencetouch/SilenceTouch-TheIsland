#!/usr/bin/env python3
"""舊 patch 轉換工具：把舊版 Silence Touch Spatial / Atmos714 的 patch 轉成新版 The Island。

改名之後，外掛代號與模組代號都換了（見下表），舊的 .vcv 打開時 Rack 會找不到模組。
這個工具只把 patch 裡的代號換掉，旋鈕、接線、設定全部保留。原始檔案不會被修改，
轉好的檔案會另存成「原檔名 (The Island).vcv」放在同一個資料夾。

    外掛代號   SilenceTouchSpatial  →  SilenceTouch-TheIsland
    模組代號   Atmos714             →  TheIsland
               IslandRoom           →  TheIslandRoom

用法（Mac 的「終端機」）：
    pip3 install zstandard                       # 第一次使用前安裝一次
    python3 convert_patches.py 我的演出.vcv        # 轉換一個檔案
    python3 convert_patches.py ~/Documents/Rack2  # 轉換資料夾裡所有 .vcv（含子資料夾）

注意：舊版的主題設定不沿用，轉換後一律是 Sky，打開後可在右鍵選單改成 Mist、Sand 或 Dark。
"""
import io, json, os, sys, tarfile

PLUGIN_MAP = {"SilenceTouchSpatial": "SilenceTouch-TheIsland"}
MODEL_MAP = {"Atmos714": "TheIsland", "IslandRoom": "TheIslandRoom"}
ZSTD_MAGIC = b"\x28\xb5\x2f\xfd"


def convert_patch_json(data):
    """改寫 patch.json 的內容，回傳 (新內容, 改了幾個模組)。"""
    patch = json.loads(data.decode("utf-8"))
    changed = 0
    for m in patch.get("modules", []):
        if m.get("plugin") in PLUGIN_MAP:
            m["plugin"] = PLUGIN_MAP[m["plugin"]]
            if m.get("model") in MODEL_MAP:
                m["model"] = MODEL_MAP[m["model"]]
            changed += 1
    return json.dumps(patch, indent=2, ensure_ascii=False).encode("utf-8"), changed


def convert_file(path):
    with open(path, "rb") as f:
        raw = f.read()
    if raw[:4] == ZSTD_MAGIC:
        # Rack 2 的 .vcv：zstd 壓縮的 tar，裡面有 patch.json 與各模組的資料夾
        try:
            import zstandard
        except ImportError:
            sys.exit("需要先安裝 zstandard：在終端機輸入  pip3 install zstandard")
        tar_bytes = zstandard.ZstdDecompressor().stream_reader(io.BytesIO(raw)).read()
        src = tarfile.open(fileobj=io.BytesIO(tar_bytes), mode="r:")
        out_tar = io.BytesIO()
        dst = tarfile.open(fileobj=out_tar, mode="w:", format=tarfile.USTAR_FORMAT)
        changed = 0
        for info in src.getmembers():
            content = src.extractfile(info).read() if info.isfile() else None
            if info.isfile() and os.path.basename(info.name) == "patch.json":
                content, changed = convert_patch_json(content)
                info.size = len(content)
            dst.addfile(info, io.BytesIO(content) if content is not None else None)
        dst.close()
        new_raw = zstandard.ZstdCompressor(level=1).compress(out_tar.getvalue())
    else:
        # 舊格式：純 JSON
        new_raw, changed = convert_patch_json(raw)
    if changed == 0:
        return 0, None
    base, ext = os.path.splitext(path)
    out_path = base + " (The Island)" + ext
    with open(out_path, "wb") as f:
        f.write(new_raw)
    return changed, out_path


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    files = []
    for arg in sys.argv[1:]:
        arg = os.path.expanduser(arg)
        if os.path.isdir(arg):
            for root, _, names in os.walk(arg):
                files += [os.path.join(root, n) for n in names if n.endswith(".vcv") and "(The Island)" not in n]
        else:
            files.append(arg)
    total = 0
    for p in files:
        try:
            n, out = convert_file(p)
        except Exception as e:
            print(f"✗ {p}：無法讀取（{e}）")
            continue
        if n:
            total += 1
            print(f"✓ {p} → {out}（{n} 個模組）")
        else:
            print(f"- {p}：沒有舊版 The Island，略過")
    print(f"\n完成：轉換了 {total} 個 patch。原始檔案沒有被修改。")


if __name__ == "__main__":
    main()
