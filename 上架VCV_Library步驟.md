# 上架 VCV Library 的步驟與待辦

整理自 VCV 官方的 library 說明與 Manifest 手冊（2026-11 查詢）。開源外掛由 VCV 團隊代為編譯 Mac / Windows / Linux 三個平台，你只需要提供原始碼。

## 一、上架前要先決定的事

1. **代號（已決定）**
   - 外掛名稱：`Silence Touch`；外掛代號：`SilenceTouch`；模組代號：`TheIsland`、`TheIslandRoom`。
   - VCV 規定外掛名稱與代號要代表品牌、不能跟單一模組同名，因為之後的新模組（例如 Key Recorder）也要放進同一個外掛。
   - 代號一旦上架就**永遠不能改**，改了會讓所有人的舊 patch 找不到模組。
   - 舊版代號（`SilenceTouchSpatial` / `Atmos714`）只在你自己的電腦上用過，舊 patch 用 `tools/convert_patches.py` 轉換。
2. **名稱與說明**
   - 外掛名稱 **Silence Touch**，主模組名稱 **The Island**，說明是「a spatial panner for multi-channel mixing」，不提及任何商標。
3. **授權**
   - 目前是 `GPL-3.0-or-later`（已附 `LICENSE-GPLv3.txt`）。代表任何人都可以取得、修改、再散布原始碼，但衍生作品也必須開源。
   - 深觸的 LOGO 與品牌名稱建議另外聲明保留權利（不隨 GPL 授權），可以在 README 加一段說明。
   - Jost 字體是 SIL Open Font License，授權檔已附在 `res/fonts/Jost-OFL.txt`。

## 二、準備工作

- [ ] 在你的 Mac 上完整編譯、安裝、實際操作測試（The Island 和 ROOM 擴充模組都要）。
- [ ] 確認關閉 Rack 不再當機。
- [ ] 建立 GitHub 帳號與**公開**的原始碼倉庫，把整個專案資料夾推上去。
- [x] `plugin.json` 網址已填好（GitHub：silencetouch/SilenceTouch-TheIsland，官網：silencetouch.com）。`authorEmail` 想公開再填。
- [ ] `version` 維持 `2.x.y` 的格式（開頭的 2 代表 Rack 2），每次更新都要遞增。
- [x] 英文 README（README.md）與中文說明（README.zh-TW.md）、CHANGELOG.md 已準備好。

## 三、送件

1. 到 https://github.com/VCVRack/library 的 Issues 開**一個**新的討論串，標題就是外掛代號：`SilenceTouch`。
2. 在討論串裡留言：外掛名稱、授權、原始碼網址，以及要上架的版本號和對應的 **commit 編號**（用 `git rev-parse HEAD` 取得，不能只寫 master）。
3. 等 VCV 團隊編譯與審核。如果某個平台編譯失敗，他們會在討論串回覆，修正後再留言新的 commit 編號。

## 四、之後每次更新

1. 遞增 `plugin.json` 的 `version`。
2. 推送到 GitHub。
3. 在**同一個討論串**留言新的版本號和 commit 編號。

## 五、目前還沒做到、上架前建議補上的

- 尚未在 Windows / Linux 編譯過（程式只用到標準 C++11 與 Rack API，預期可以，但要等 VCV 的編譯結果確認）。
- 尚未在 Rack 裡完整實跑新加的 ROOM 擴充模組。
- 模組瀏覽器裡的預覽圖沒有 LOGO（PNG 在預覽圖裡不畫）。把 LOGO 轉成 SVG 外框就能解決。
