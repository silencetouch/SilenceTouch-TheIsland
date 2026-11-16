#include "plugin.hpp"
#include "Panner714.hpp"
#include "CubeView.hpp"
#include "Layout714.hpp"
#include "Theme.hpp"
#include "IslandRoom.hpp"

// ---------------------------------------------------------------------------
// Atmos 7.1.4 Panner
// 12 軌立體聲輸入（每軌 L / R 兩個輸入並排），每軌有 X/Y/Z 位置（皆可 CV 控制）與 WIDTH（L/R 展開寬度）。
// 兩個輸入都接時，L 與 R 當成兩個聲源，放在中心點左右各 WIDTH 處；只接一個輸入時當成單聲道放在中心點。
// 立方體顯示每個聲源目前的位置。
// 輸出配置可選 4 / 8 / 12 聲道（右鍵選單）。12 聲道的順序為 SMPTE / Dolby 7.1.4：
//   L R C LFE Ls Rs Lrs Rrs Ltf Rtf Ltr Rtr
// 可直接接到 Audio-16 的 1–12 輸入，對應 H9000 USB 播放通道 1–12。
// ---------------------------------------------------------------------------

static const int NUM_TRACKS = st::layout::MAX_TRACKS;   // 12
static const int PARAMS_PER_TRACK = 11;  // X Y Z WIDTH LEVEL LFE ORBIT_BUTTON ORBIT_DIAMETER MUTE SOLO ORBIT_SPEED
static const int INPUTS_PER_TRACK = 6;   // AUDIO_L AUDIO_R X_CV Y_CV Z_CV ORBIT_CV
static const int GAIN_UPDATE_INTERVAL = 32;

struct Atmos714 : Module {
	enum ParamId {
		TRACK_PARAMS_START,
		ROTATE_PARAM = TRACK_PARAMS_START + NUM_TRACKS * PARAMS_PER_TRACK,
		FOCUS_PARAM,
		BLUR_PARAM,
		MASTER_PARAM,
		TEST_PARAM,
		TEST_CH_PARAM,
		ORBIT_RATE_PARAM,
		TEST_AUTO_PARAM,   // 加在最後，舊 patch 的參數編號不變
		SPLIT_PARAMS_START,   // 每軌的 SPLIT 按鈕（L / R 分開繞），12 個，同樣加在最後
		PARAMS_LEN = SPLIT_PARAMS_START + NUM_TRACKS
	};
	enum InputId {
		TRACK_INPUTS_START,
		ROTATE_CV_INPUT = TRACK_INPUTS_START + NUM_TRACKS * INPUTS_PER_TRACK,
		INPUTS_LEN
	};
	enum OutputId {
		POLY_OUTPUT,
		CH_OUTPUTS_START,
		OUTPUTS_LEN = CH_OUTPUTS_START + st::NCH
	};

	static int pX(int t) { return TRACK_PARAMS_START + t * PARAMS_PER_TRACK + 0; }
	static int pY(int t) { return TRACK_PARAMS_START + t * PARAMS_PER_TRACK + 1; }
	static int pZ(int t) { return TRACK_PARAMS_START + t * PARAMS_PER_TRACK + 2; }
	static int pWidth(int t) { return TRACK_PARAMS_START + t * PARAMS_PER_TRACK + 3; }
	static int pLvl(int t) { return TRACK_PARAMS_START + t * PARAMS_PER_TRACK + 4; }
	static int pLfe(int t) { return TRACK_PARAMS_START + t * PARAMS_PER_TRACK + 5; }
	static int pOrbBtn(int t) { return TRACK_PARAMS_START + t * PARAMS_PER_TRACK + 6; }
	static int pOrbDia(int t) { return TRACK_PARAMS_START + t * PARAMS_PER_TRACK + 7; }
	static int pMute(int t) { return TRACK_PARAMS_START + t * PARAMS_PER_TRACK + 8; }
	static int pSolo(int t) { return TRACK_PARAMS_START + t * PARAMS_PER_TRACK + 9; }
	static int pSplit(int t) { return SPLIT_PARAMS_START + t; }
	static int pOrbSpd(int t) { return TRACK_PARAMS_START + t * PARAMS_PER_TRACK + 10; }
	static int iAudioL(int t) { return TRACK_INPUTS_START + t * INPUTS_PER_TRACK + 0; }
	static int iAudioR(int t) { return TRACK_INPUTS_START + t * INPUTS_PER_TRACK + 1; }
	static int iX(int t) { return TRACK_INPUTS_START + t * INPUTS_PER_TRACK + 2; }
	static int iY(int t) { return TRACK_INPUTS_START + t * INPUTS_PER_TRACK + 3; }
	static int iZ(int t) { return TRACK_INPUTS_START + t * INPUTS_PER_TRACK + 4; }
	static int iOrb(int t) { return TRACK_INPUTS_START + t * INPUTS_PER_TRACK + 5; }

	// 每軌最多 2 個聲源（立體聲的 L 與 R；單聲道時只用第 0 個）
	float gainTarget[NUM_TRACKS][2][st::NCH] = {};
	float gainCurrent[NUM_TRACKS][2][st::NCH] = {};
	int updateCounter = 0;
	float smoothCoef = 0.002f;

	st::LfeFilter lfeFilter;

	// 輸出配置（4 / 8 / 12 聲道）
	int outLayout = st::LAYOUT_12;

	// 給立方體顯示用：每軌的聲源數（0 = 未接，1 = 單聲道，2 = 立體聲）與
	// 套用 CV、寬度與旋轉後的實際位置
	int srcCount[NUM_TRACKS] = {};
	float effPos[NUM_TRACKS][2][3] = {};
	int viewMode = st::view::ISO;

	// 繞場：按一下 ORB 繞一圈（時間由 ORB TIME 決定）；LFO 插孔接上時由 LFO 自由控制角度
	st::OrbitState orbit[NUM_TRACKS];
	// LFO 繞場：LFO 孔有接線時，ORB 按鈕變成開／關。開啟後由 LFO 控制角度；lfoEnv 是 0～1 的淡入淡出（約 0.3 秒）
	bool lfoOrbitOn[NUM_TRACKS] = {};
	// SPLIT：L / R 從立體聲中心往兩側分開，鏡像各自繞圈（L 往左、R 往右，在後方交會後繼續繞）
	float splitPhase[NUM_TRACKS] = {};   // 已經分開的角度（度）
	float splitEnv[NUM_TRACKS] = {};     // 0～1 淡入淡出（約 0.3 秒）
	float lfoEnv[NUM_TRACKS] = {};
	bool orbitLoop[NUM_TRACKS] = {};      // 每軌的繞場模式：false = 繞一圈，true = 持續繞（再按一下停）
	dsp::BooleanTrigger orbitTrig[NUM_TRACKS];
	int orbitDir[NUM_TRACKS] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};   // 每軌方向：1 = 往左（逆時針），-1 = 往右（順時針）
	int orbitAxis[NUM_TRACKS] = {};       // 每軌的繞場軸（st::AXIS_Z / X / Y），預設 Z = 水平轉圈
	int rotateAxis = st::AXIS_Z;          // 整體 ROTATE 的旋轉軸
	bool orbitEngaged[NUM_TRACKS] = {};   // 顯示用：這一軌目前是否在繞場（ORB 指示燈）
	bool orbitCircle[NUM_TRACKS] = {};    // 顯示用：立方體上要不要畫繞場圓圈（繞場或 SPLIT）
	float orbitRadius[NUM_TRACKS] = {};
	float orbitFixed[NUM_TRACKS] = {};    // 顯示用：繞場軸上的座標（繞場圓圈所在的位置）

	// 靜音／獨奏：只要有任何一軌開 Solo，就只聽得到有開 Solo 的軌道；開了 Mute 的軌道一定沒聲音。
	// 切換時以約 5 ms 淡入淡出，避免爆音。
	float msGain[NUM_TRACKS] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
	float msCoef = 0.004f;
	bool trackSilenced[NUM_TRACKS] = {};   // 顯示用：被 Mute 或 Solo 關掉

	// 電平指示：每個輸出、每一軌（顯示用）
	st::PeakMeter outMeter[st::NCH];
	st::PeakMeter trackMeter[NUM_TRACKS];
	float meterDecay = 0.9999f;
	// 安全限幅器（預設關閉）：輸出超過 8 V 開始柔和壓縮，最大 10 V（0 dBFS）。
	// 電平 LED 量的是限幅「之前」的訊號，所以限幅器開著時紅燈仍會提醒你訊號太大。
	bool limiter = false;

	// 面板主題（st::theme::ThemeId）：Sky（預設）、Mist、Sand、Dark；-1 = Auto（跟著 Rack 的「Use dark panels」切換 Sky / Dark）
	int panelTheme = st::theme::THEME_DEFAULT;
	int resolvedTheme() const {
		if (panelTheme < 0)
			return settings::preferDarkPanels ? st::theme::THEME_DARK : st::theme::THEME_DEFAULT;
		return clamp(panelTheme, 0, st::theme::NUM_THEMES - 1);
	}

	// 喇叭測試
	bool autoStep = true;   // 由 TEST_AUTO_PARAM 每個取樣更新
	int cvMode = 0;         // CV 電壓範圍：0 = ±5 V（預設）、1 = ±10 V。兩者都是 0 V = 不偏移（停在旋鈕位置）
	bool testWasActive = false;
	int testIndex = 0;      // 在目前輸出配置的聲道清單中的位置
	int testChannel = 0;    // 對應的 SMPTE 聲道
	float testTimer = 0.f;
	float testPhase = 0.f;
	bool testActive = false;

	Atmos714() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, 0);

		for (int t = 0; t < NUM_TRACKS; t++) {
			std::string n = "Track " + std::to_string(t + 1) + " ";
			// 預設位置：前牆中央、耳朵高度（x=0, y=1, z=0），跟前方 L / R 喇叭同一面牆
			configParam(pX(t), -1.f, 1.f, 0.f, n + "X (left-right)");
			configParam(pY(t), -1.f, 1.f, 1.f, n + "Y (rear-front)");
			configParam(pZ(t), -1.f, 1.f, 0.f, n + "Z (floor-ceiling, 0 = ear level)");
			configParam(pWidth(t), 0.f, 1.f, 0.3f, n + "stereo width (L/R spread around X)");
			configParam(pLvl(t), 0.f, 1.f, 0.8f, n + "level", "%", 0.f, 100.f);
			configParam(pLfe(t), 0.f, 1.f, 0.f, n + "LFE send", "%", 0.f, 100.f);
			configButton(pOrbBtn(t), n + "orbit on/off (LFO jack patched: LFO drives the angle)");
			configParam(pOrbDia(t), 0.f, 1.f, 0.9f, n + "orbit diameter (100% = touches the walls)", "%", 0.f, 100.f);
			configSwitch(pMute(t), 0.f, 1.f, 0.f, n + "mute", {"Off", "On"});
			configSwitch(pSolo(t), 0.f, 1.f, 0.f, n + "solo", {"Off", "On"});
			// Randomize 不動 M / S / ORB 按鈕（避免隨機時把軌道靜音或觸發繞場）
			getParamQuantity(pMute(t))->randomizeEnabled = false;
			getParamQuantity(pSolo(t))->randomizeEnabled = false;
			getParamQuantity(pOrbBtn(t))->randomizeEnabled = false;
			configSwitch(pSplit(t), 0.f, 1.f, 0.f, n + "split: L and R circle apart in mirror (speed = SPD; uses the LFO jack when ORB is off)", {"Off", "On"});
			getParamQuantity(pSplit(t))->randomizeEnabled = false;
			// 繞場速度：往右轉越快。顯示為繞一圈的秒數：最左 60 秒 ～ 最右 1 秒（指數刻度），預設約 7.7 秒
			configParam(pOrbSpd(t), 0.f, 1.f, 0.5f, n + "orbit speed (time per revolution)", " s", 1.f / 60.f, 60.f);
			configInput(iAudioL(t), n + "audio L (mono if R is unpatched)");
			configInput(iAudioR(t), n + "audio R");
			configInput(iX(t), n + "X CV");
			configInput(iY(t), n + "Y CV");
			configInput(iZ(t), n + "Z CV");
			configInput(iOrb(t), n + "orbit LFO (used only while ORB is on; ±5 V = ±180°, a saw LFO = continuous circling)");
		}
		configParam(ROTATE_PARAM, -180.f, 180.f, 0.f, "Rotate all sources", "°");
		configParam(FOCUS_PARAM, 1.f, 6.f, 3.f, "Focus (higher = tighter)");
		configParam(BLUR_PARAM, 0.05f, 0.5f, 0.2f, "Blur radius");
		configParam(MASTER_PARAM, 0.f, 1.f, 0.8f, "Master level", "%", 0.f, 100.f);
		configSwitch(TEST_PARAM, 0.f, 1.f, 0.f, "Speaker test tone", {"Off", "On"});
		configSwitch(TEST_AUTO_PARAM, 0.f, 1.f, 1.f, "Speaker test auto-step", {"Manual (TEST CH)", "Auto (one by one)"});
		configParam(TEST_CH_PARAM, 1.f, 12.f, 1.f, "Test channel (when AUTO is off; counts only active outputs)");
		getParamQuantity(TEST_CH_PARAM)->snapEnabled = true;
		// 全部軌道的繞場速度倍率：0.25× ～ 4×，預設 1×（現場一顆旋鈕整體加速或放慢）
		configParam(ORBIT_RATE_PARAM, -1.f, 1.f, 0.f, "Orbit rate (all tracks)", "x", 4.f, 1.f);
		configInput(ROTATE_CV_INPUT, "Rotate CV (±5 V = ±180°)");

		configOutput(POLY_OUTPUT, "Poly (SMPTE order; 4, 8 or 12 channels per output layout)");
		for (int c = 0; c < st::NCH; c++)
			configOutput(CH_OUTPUTS_START + c, std::string("Ch ") + std::to_string(c + 1) + " " + st::CH_NAMES[c] + " (SMPTE slot)");

		setupSampleRate(APP->engine->getSampleRate());
	}

	void setupSampleRate(float sampleRate) {
		lfeFilter.setCutoff(120.f, sampleRate);
		smoothCoef = 1.f - std::exp(-1.f / (0.01f * sampleRate));
		meterDecay = std::exp(-1.f / (0.3f * sampleRate));   // 峰值約 0.3 秒回落
		msCoef = 1.f - std::exp(-1.f / (0.005f * sampleRate));   // Mute/Solo 淡入淡出約 5 ms
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		setupSampleRate(e.sampleRate);
	}

	void onReset() override {
		lfeFilter.reset();
		outLayout = st::LAYOUT_12;
		limiter = false;
		cvMode = 0;
		rotateAxis = st::AXIS_Z;
		for (int t = 0; t < NUM_TRACKS; t++) {
			orbitAxis[t] = st::AXIS_Z;
			orbitDir[t] = 1;
			orbitLoop[t] = false;
			orbit[t] = st::OrbitState();
			lfoOrbitOn[t] = false;
			lfoEnv[t] = 0.f;
			splitPhase[t] = splitEnv[t] = 0.f;
		}
		for (int c = 0; c < st::NCH; c++)
			outMeter[c].reset();
		for (int t = 0; t < NUM_TRACKS; t++)
			trackMeter[t].reset();
		for (int t = 0; t < NUM_TRACKS; t++)
			for (int s = 0; s < 2; s++)
				for (int c = 0; c < st::NCH; c++)
					gainCurrent[t][s][c] = gainTarget[t][s][c] = 0.f;
	}

	// 讀 CV：NaN / 無限大一律當 0，不讓壞掉的訊號把聲源釘在邊緣（std::min/max 遇到 NaN 會得到 +1）
	static float safeV(float v) {
		return std::isfinite(v) ? v : 0.f;
	}

	// 位置 CV（X / Y / Z）換成 −1 ～ +1 的位移，加在旋鈕位置上（雙極性，0 V = 不偏移）：
	//   ±5 V 模式：+5 V = +1、−5 V = −1；±10 V 模式：±10 V = ±1
	// 旋鈕在中間（0 = 房間中心）時，雙極性 LFO 會以中心為準左右（前後、上下）擺動；
	// 單極性 0～10 V 的訊號則從旋鈕位置往一個方向走。
	float cvPos(float v) const {
		return safeV(v) / (cvMode == 1 ? 10.f : 5.f);
	}

	// 角度 CV（ROTATE、繞場 LFO）換成度數：±5 V 模式 = ±180°，±10 V 模式 = ±180°（滿幅 10 V）
	float cvAngle(float v) const {
		return safeV(v) * (cvMode == 1 ? 18.f : 36.f);
	}

	// ROOM 擴充模組：緊貼在左邊或右邊時，讀取它的房間尺寸（寬 / 深 / 高）與繞場形狀。
	// 沒有擴充模組時 room 是 1 : 1 : 1 的正立方體，計算結果和以前完全一樣。
	st::Room room;
	bool roomLinked = false;

	void readRoomExpander() {
		Module* m = rightExpander.module;
		if (!(m && m->model == modelIslandRoom))
			m = leftExpander.module;
		if (m && m->model == modelIslandRoom && (int) m->params.size() >= st::roomexp::NUM_PARAMS) {
			room = st::roomFromSize(m->params[st::roomexp::W_PARAM].getValue(),
			                        m->params[st::roomexp::D_PARAM].getValue(),
			                        m->params[st::roomexp::H_PARAM].getValue(),
			                        m->params[st::roomexp::ORBIT_PARAM].getValue() > 0.5f);
			roomLinked = true;
		}
		else {
			room = st::Room();
			roomLinked = false;
		}
	}

	void updateTargets(float dt) {
		readRoomExpander();
		float rotate = params[ROTATE_PARAM].getValue();
		if (inputs[ROTATE_CV_INPUT].isConnected())
			rotate += cvAngle(inputs[ROTATE_CV_INPUT].getVoltage());
		float focus = params[FOCUS_PARAM].getValue();
		float blur = params[BLUR_PARAM].getValue();
		bool mask[st::NCH];
		st::layoutMask(outLayout, mask);
		float orbitRateKnob = params[ORBIT_RATE_PARAM].getValue();   // 全部軌道的速度倍率（0.25× ～ 4×）

		for (int t = 0; t < NUM_TRACKS; t++) {
			bool hasL = inputs[iAudioL(t)].isConnected();
			bool hasR = inputs[iAudioR(t)].isConnected();
			int n = (hasL && hasR) ? 2 : ((hasL || hasR) ? 1 : 0);
			srcCount[t] = n;

			float x = params[pX(t)].getValue();
			float y = params[pY(t)].getValue();
			float z = params[pZ(t)].getValue();
			if (inputs[iX(t)].isConnected())
				x = st::clamp1(x + cvPos(inputs[iX(t)].getVoltage()));
			if (inputs[iY(t)].isConnected())
				y = st::clamp1(y + cvPos(inputs[iY(t)].getVoltage()));
			if (inputs[iZ(t)].isConnected())
				z = st::clamp1(z + cvPos(inputs[iZ(t)].getVoltage()));

			// 繞場只在按下 ORB 之後才發生；沒在繞場時 X / Y / Z 旋鈕與 CV 照常決定位置。
			//   LFO 孔沒接線：繞一圈模式按一下繞一圈；持續模式按一下開始、再按一下（繞完這圈後）停
			//   LFO 孔有接線：ORB 是開／關，開啟後角度由 LFO 決定（轉速跟著 LFO），再按一下約 0.3 秒內回到原位
			bool lfoPatched = inputs[iOrb(t)].isConnected();
			bool pressed = orbitTrig[t].process(params[pOrbBtn(t)].getValue() > 0.5f);
			if (lfoPatched) {
				if (orbit[t].active) {   // 繞場途中接上 LFO：改由 LFO 接手
					orbit[t] = st::OrbitState();
					lfoOrbitOn[t] = true;
				}
				if (pressed)
					lfoOrbitOn[t] = !lfoOrbitOn[t];
			}
			else {
				lfoOrbitOn[t] = false;
				if (pressed)
					orbit[t].trigger(orbitLoop[t]);
			}
			float lfoTarget = (lfoPatched && lfoOrbitOn[t]) ? 1.f : 0.f;
			lfoEnv[t] += (lfoTarget - lfoEnv[t]) * (1.f - std::exp(-dt / 0.3f));
			if (lfoTarget == 0.f && lfoEnv[t] < 1e-3f)
				lfoEnv[t] = 0.f;

			float orbitTime = st::orbitSeconds(params[pOrbSpd(t)].getValue(), orbitRateKnob);   // 這一軌繞一圈的秒數
			orbit[t].update(dt / orbitTime, orbitLoop[t]);
			float theta = 0.f, env = 0.f;
			bool engaged = false;
			if (orbit[t].active) {
				theta = 360.f * orbit[t].turn() * (float) orbitDir[t];
				env = orbit[t].envelope();
				engaged = true;
			}
			else if (lfoEnv[t] > 0.f) {
				// 角度與半徑一起淡入淡出：開始和結束時都在原本的位置，不會跳
				float e = lfoEnv[t];
				theta = (lfoPatched ? cvAngle(inputs[iOrb(t)].getVoltage()) : 0.f) * (float) orbitDir[t] * e;
				env = e;
				engaged = true;
			}
			float dia = params[pOrbDia(t)].getValue();
			int axis = orbitAxis[t];
			float pu, pv, pw;
			st::toPlane(axis, x, y, z, pu, pv, pw);
			orbitEngaged[t] = engaged;
			orbitCircle[t] = engaged;
			orbitRadius[t] = dia;
			orbitFixed[t] = pw;
			float cx = x, cy = y, cz = z;
			if (engaged)
				st::orbitPositionRoom(axis, x, y, z, dia, theta, env, &room, cx, cy, cz);

			// 聲源位置：立體聲為中心左右各 WIDTH，單聲道在中心。
			// 水平繞場（Z 軸）時 L/R 方向跟著轉；垂直繞場時 L/R 維持左右。
			float xs[2] = {cx, cx}, ys[2] = {cy, cy}, zs[2] = {cz, cz};
			if (n == 2) {
				if (axis == st::AXIS_Z)
					st::stereoSplitRotated(cx, cy, params[pWidth(t)].getValue(), theta, xs[0], ys[0], xs[1], ys[1]);
				else
					st::stereoSplit(cx, params[pWidth(t)].getValue(), xs[0], xs[1]);
			}

			// SPLIT：L / R 從立體聲中心的方向出發，L 往左、R 往右鏡像繞圈（圓的大小 = DIA，旋轉軸 = 繞場軸）。
			//   速度 = 這一軌的 SPD（乘上 ORB RATE）；ORB 沒開而 LFO 孔有接線時，分開的角度改由 LFO 決定。
			//   ORB 同時開啟時，整組再跟著繞場一起轉。只對立體聲（L、R 都有接）有效。
			bool splitOn = params[pSplit(t)].getValue() > 0.5f && n == 2;
			splitEnv[t] += ((splitOn ? 1.f : 0.f) - splitEnv[t]) * (1.f - std::exp(-dt / 0.3f));
			if (!splitOn && splitEnv[t] < 1e-3f) {
				splitEnv[t] = 0.f;
				splitPhase[t] = 0.f;
			}
			if (splitEnv[t] > 0.f) {
				if (splitOn)
					splitPhase[t] = std::fmod(splitPhase[t] + 360.f * dt / orbitTime, 360.f);
				float phi = (lfoPatched && !engaged) ? cvAngle(inputs[iOrb(t)].getVoltage()) : splitPhase[t];
				phi = std::remainder(phi, 360.f);   // −180～180°：關閉時走最短的路滑回原位
				float e = splitEnv[t];
				float envR = std::max(env, e);
				for (int s = 0; s < 2; s++) {
					float sign = (s == 0) ? 1.f : -1.f;   // L 往左（逆時針），R 往右（順時針）
					float qx, qy, qz;
					st::orbitPositionRoom(axis, x, y, z, dia, theta + sign * phi * e, envR, &room, qx, qy, qz);
					xs[s] += (qx - xs[s]) * e;
					ys[s] += (qy - ys[s]) * e;
					zs[s] += (qz - zs[s]) * e;
				}
				orbitCircle[t] = true;   // 立方體上畫出繞場圓圈
			}

			for (int s = 0; s < 2; s++) {
				float px = xs[s], py = ys[s], pz = zs[s];
				st::rotateAxis(rotateAxis, px, py, pz, rotate);
				effPos[t][s][0] = px;
				effPos[t][s][1] = py;
				effPos[t][s][2] = pz;
				if (s < n)
					st::computeGains(px, py, pz, focus, blur, gainTarget[t][s], mask, &room);
				else
					for (int c = 0; c < st::NCH; c++)
						gainTarget[t][s][c] = 0.f;
			}
		}
	}

	void process(const ProcessArgs& args) override {
		float out[st::NCH] = {};
		float master = params[MASTER_PARAM].getValue();

		int chIdx[st::NCH];
		int chCount = st::layoutChannels(outLayout, chIdx);

		testActive = params[TEST_PARAM].getValue() > 0.5f;
		autoStep = params[TEST_AUTO_PARAM].getValue() > 0.5f;

		if (updateCounter-- <= 0) {
			updateCounter = GAIN_UPDATE_INTERVAL - 1;   // 每 GAIN_UPDATE_INTERVAL 個取樣更新一次
			updateTargets(GAIN_UPDATE_INTERVAL * args.sampleTime);
		}

		if (testActive) {
			// 喇叭測試：1 kHz 正弦（LFE 用 80 Hz）依序輪流送到目前輸出配置的每個聲道，一次只有一個聲道有聲音
			if (!testWasActive) {
				testIndex = 0;   // 每次按下 TEST 都從第一個聲道（L）開始
				testTimer = 0.f;
				testPhase = 0.f;
			}
			int prevChannel = testChannel;
			if (autoStep) {
				testTimer += args.sampleTime;
				if (testTimer >= 1.5f) {
					testTimer = 0.f;
					testIndex++;
				}
				testIndex = ((testIndex % chCount) + chCount) % chCount;
			}
			else {
				testIndex = clamp((int) std::round(params[TEST_CH_PARAM].getValue()) - 1, 0, chCount - 1);
			}
			testChannel = chIdx[testIndex];
			if (testChannel != prevChannel || !testWasActive) {
				// 換聲道時，其他聲道的電平燈立刻熄滅（否則 0.3 秒回落加上 -60 dB 門檻，上一個燈會多亮將近 2 秒，看起來像兩格同時亮）
				for (int c = 0; c < st::NCH; c++)
					if (c != testChannel)
						outMeter[c].reset();
				testPhase = 0.f;
			}
			float freq = (testChannel == st::CH_LFE) ? 80.f : 1000.f;
			testPhase += freq * args.sampleTime;
			if (testPhase >= 1.f)
				testPhase -= 1.f;
			out[testChannel] = 3.f * std::sin(2.f * M_PI * testPhase) * master;
			for (int t = 0; t < NUM_TRACKS; t++)
				trackMeter[t].process(0.f, meterDecay, args.sampleTime);
		}
		else {
			bool anySolo = false;
			for (int t = 0; t < NUM_TRACKS; t++)
				anySolo = anySolo || params[pSolo(t)].getValue() > 0.5f;
			float lfeIn = 0.f;
			for (int t = 0; t < NUM_TRACKS; t++) {
				bool audible = st::trackAudible(params[pMute(t)].getValue() > 0.5f, params[pSolo(t)].getValue() > 0.5f, anySolo);
				trackSilenced[t] = !audible;
				bool hasL = inputs[iAudioL(t)].isConnected();
				bool hasR = inputs[iAudioR(t)].isConnected();
				if (!hasL && !hasR) {
					trackMeter[t].process(0.f, meterDecay, args.sampleTime);
					msGain[t] = audible ? 1.f : 0.f;
					continue;
				}
				msGain[t] += ((audible ? 1.f : 0.f) - msGain[t]) * msCoef;
				float lvl = params[pLvl(t)].getValue();
				float sig[2] = {0.f, 0.f};
				float lfeMono = 0.f;
				if (hasL && hasR) {
					sig[0] = safeV(inputs[iAudioL(t)].getVoltageSum()) * lvl;
					sig[1] = safeV(inputs[iAudioR(t)].getVoltageSum()) * lvl;
					lfeMono = 0.5f * (sig[0] + sig[1]);
				}
				else {
					// 只接一個輸入：當成單聲道放在中心點
					int id = hasL ? iAudioL(t) : iAudioR(t);
					sig[0] = safeV(inputs[id].getVoltageSum()) * lvl;
					lfeMono = sig[0];
				}
				lfeIn += lfeMono * params[pLfe(t)].getValue() * msGain[t];
				// 該軌的電平（套用 LVL 之後、Mute/Solo 之前，取 L/R 中較大者）：靜音中也看得到有沒有訊號進來
				trackMeter[t].process(std::max(std::fabs(sig[0]), std::fabs(sig[1])), meterDecay, args.sampleTime);
				sig[0] *= msGain[t];
				sig[1] *= msGain[t];
				for (int s = 0; s < 2; s++) {
					for (int c = 0; c < st::NCH; c++) {
						gainCurrent[t][s][c] += (gainTarget[t][s][c] - gainCurrent[t][s][c]) * smoothCoef;
						out[c] += sig[s] * gainCurrent[t][s][c];
					}
				}
			}
			float lfe = lfeFilter.process(lfeIn);
			for (int c = 0; c < st::NCH; c++)
				out[c] *= master;
			out[st::CH_LFE] = (outLayout == st::LAYOUT_4) ? 0.f : lfe * master;
		}

		testWasActive = testActive;

		// 輸出電平（限幅之前量測），再視需要套用安全限幅器
		for (int c = 0; c < st::NCH; c++) {
			outMeter[c].process(out[c], meterDecay, args.sampleTime);
			if (limiter)
				out[c] = st::softLimit(out[c]);
		}

		// 單聲道輸出維持 SMPTE 固定位置（未使用的聲道為 0）；poly 輸出只含目前配置的聲道，依序排列
		outputs[POLY_OUTPUT].setChannels(chCount);
		for (int k = 0; k < chCount; k++)
			outputs[POLY_OUTPUT].setVoltage(out[chIdx[k]], k);
		for (int c = 0; c < st::NCH; c++)
			outputs[CH_OUTPUTS_START + c].setVoltage(out[c]);
	}

	// 打開 patch 時 M / S 一律回到關閉（不沿用存檔裡的狀態）。
	// 也避免舊版 patch 的參數編號錯位時，其他旋鈕的值被讀成「靜音開啟」。
	void fromJson(json_t* rootJ) override {
		Module::fromJson(rootJ);
		for (int t = 0; t < NUM_TRACKS; t++) {
			params[pMute(t)].setValue(0.f);
			params[pSolo(t)].setValue(0.f);
			lfoOrbitOn[t] = false;   // 打開 patch 時繞場一律關閉
			params[pSplit(t)].setValue(0.f);
			splitPhase[t] = splitEnv[t] = 0.f;
			lfoEnv[t] = 0.f;
			msGain[t] = 1.f;
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "outLayout", json_integer(outLayout));
		json_object_set_new(rootJ, "viewMode", json_integer(viewMode));
		json_object_set_new(rootJ, "limiter", json_boolean(limiter));
		json_object_set_new(rootJ, "theme", json_integer(panelTheme));
		json_t* dirJ = json_array();
		json_t* loopJ = json_array();
		for (int t = 0; t < NUM_TRACKS; t++) {
			json_array_append_new(dirJ, json_integer(orbitDir[t]));
			json_array_append_new(loopJ, json_boolean(orbitLoop[t]));
		}
		json_object_set_new(rootJ, "orbitDir", dirJ);
		json_object_set_new(rootJ, "orbitLoop", loopJ);
		json_object_set_new(rootJ, "rotateAxis", json_integer(rotateAxis));
		json_object_set_new(rootJ, "cvRange", json_integer(cvMode));
		json_t* axesJ = json_array();
		for (int t = 0; t < NUM_TRACKS; t++)
			json_array_append_new(axesJ, json_integer(orbitAxis[t]));
		json_object_set_new(rootJ, "orbitAxis", axesJ);
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// 舊版 patch 把 auto-step 存在右鍵選單設定裡，這裡轉成面板上的 AUTO 按鈕
		json_t* j = json_object_get(rootJ, "autoStep");
		if (j)
			params[TEST_AUTO_PARAM].setValue(json_boolean_value(j) ? 1.f : 0.f);
		j = json_object_get(rootJ, "outLayout");
		if (j)
			outLayout = clamp((int) json_integer_value(j), 0, st::NUM_LAYOUTS - 1);
		j = json_object_get(rootJ, "cvRange");   // 舊的 "cvMode"（0～10 V 試作版）不沿用
		if (j && json_is_integer(j))
			cvMode = clamp((int) json_integer_value(j), 0, 1);
		j = json_object_get(rootJ, "rotateAxis");
		if (j)
			rotateAxis = clamp((int) json_integer_value(j), 0, st::NUM_AXES - 1);
		j = json_object_get(rootJ, "orbitAxis");
		if (j && json_is_array(j)) {
			for (int t = 0; t < NUM_TRACKS && t < (int) json_array_size(j); t++)
				orbitAxis[t] = clamp((int) json_integer_value(json_array_get(j, t)), 0, st::NUM_AXES - 1);
		}
		j = json_object_get(rootJ, "orbitDir");
		if (j && json_is_array(j)) {
			for (int t = 0; t < NUM_TRACKS && t < (int) json_array_size(j); t++)
				orbitDir[t] = json_integer_value(json_array_get(j, t)) < 0 ? -1 : 1;
		}
		else if (j) {
			// 舊版存檔：全部軌道共用一個方向
			for (int t = 0; t < NUM_TRACKS; t++)
				orbitDir[t] = json_integer_value(j) < 0 ? -1 : 1;
		}
		j = json_object_get(rootJ, "orbitLoop");
		if (j && json_is_array(j)) {
			for (int t = 0; t < NUM_TRACKS && t < (int) json_array_size(j); t++)
				orbitLoop[t] = json_boolean_value(json_array_get(j, t));
		}
		// 主題存在 "theme"；舊版的 "panelTheme"（Paper / Ink / Sky / Auto）不沿用，一律用預設的 Sky
		j = json_object_get(rootJ, "theme");
		if (j)
			panelTheme = clamp((int) json_integer_value(j), -1, st::theme::NUM_THEMES - 1);
		j = json_object_get(rootJ, "limiter");
		if (j)
			limiter = json_boolean_value(j);
		j = json_object_get(rootJ, "viewMode");
		if (j)
			viewMode = clamp((int) json_integer_value(j), 0, st::view::NUM_MODES - 1);
	}
};



// ---------------------------------------------------------------------------
// 介面
// ---------------------------------------------------------------------------

// 字體：Jost（包浩斯風格的幾何無襯線字，SIL Open Font License，見 res/fonts/Jost-OFL.txt）
static std::shared_ptr<window::Font> stFont() {
	return APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Jost-500-Medium.ttf"));
}

static std::shared_ptr<window::Font> stFontBold() {
	return APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Jost-700-Bold.ttf"));
}

// 色票（src/Theme.hpp）→ NanoVG 顏色
// 目前主題的調色盤（模組瀏覽器預覽時沒有模組，用 Paper）
static const st::theme::Palette& palOf(Atmos714* m) {
	return st::theme::palette(m ? m->resolvedTheme() : st::theme::THEME_DEFAULT);
}

static NVGcolor th(st::theme::C c, int alpha = 255) {
	return nvgRGBA(c.r, c.g, c.b, alpha);
}

static NVGcolor trackColor(int t, int alpha = 255) {
	st::view::Rgb c = st::view::TRACK_COLORS[t % 12];
	return nvgRGBA(c.r, c.g, c.b, alpha);
}

struct StLabel : TransparentWidget {
	enum Kind { NORMAL, TITLE, BAND, OUT };
	std::string text;
	float fontSize;
	Kind kind = NORMAL;
	bool bold = false;
	Atmos714* module = nullptr;
	std::function<int()> themeFn;   // 有設定時用它決定主題（ROOM 擴充模組用）

	StLabel(Vec centerPos, std::string text, float fontSize, Kind kind, Atmos714* module) {
		this->kind = kind;
		this->module = module;
		this->bold = (kind != NORMAL);
		box.size = Vec(std::max(60.f, (float) text.size() * fontSize), fontSize + 4.f);
		box.pos = Vec(centerPos.x - box.size.x / 2.f, centerPos.y - box.size.y / 2.f);
		this->text = text;
		this->fontSize = fontSize;
	}

	void draw(const DrawArgs& args) override {
		std::shared_ptr<window::Font> font = bold ? stFontBold() : stFont();
		if (!font)
			return;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, fontSize);
		nvgTextLetterSpacing(args.vg, bold ? 1.2f : 0.4f);   // 大寫字母稍微拉開字距
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		const st::theme::Palette& pal = themeFn ? st::theme::palette(themeFn()) : palOf(module);
		st::theme::C c = kind == TITLE ? pal.TITLE : kind == BAND ? pal.BAND_TEXT : kind == OUT ? pal.OUTBLOCK_TEXT : pal.TEXT;
		nvgFillColor(args.vg, th(c));
		nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, text.c_str(), NULL);
	}
};

// 電平 LED：綠 = 有訊號、黃 = 偏熱（-6 dBFS 以上）、紅 = 爆音（0 dBFS，紅燈維持 1.5 秒）
static NVGcolor meterColor(int state) {
	switch (state) {
		case st::METER_CLIP: return th(st::theme::RED_);
		case st::METER_HOT: return th(st::theme::YELLOW_);
		case st::METER_SIGNAL: return th(st::theme::BRAND_);
	}
	return nvgRGB(0, 0, 0);
}

struct LevelLed : Widget {
	Atmos714* module = nullptr;
	bool isOutput = false;
	int index = 0;

	int state() {
		if (!module)
			return st::METER_OFF;
		return isOutput ? module->outMeter[index].state() : module->trackMeter[index].state();
	}

	void draw(const DrawArgs& args) override {
		// 底座（燈熄滅時的樣子）
		float cx = box.size.x / 2.f, cy = box.size.y / 2.f;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, 3.f);
		const st::theme::Palette& pal = palOf(module);
		nvgFillColor(args.vg, th(isOutput ? pal.LED_BASE_OUT : pal.LED_BASE));   // 輸出 LED 在輸出區色塊上
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, th(pal.LED_RING));
		nvgStrokeWidth(args.vg, 0.6f);
		nvgStroke(args.vg);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		// 發光層：暗房模式下也看得到
		if (layer == 1) {
			int st_ = state();
			if (st_ != st::METER_OFF) {
				float cx = box.size.x / 2.f, cy = box.size.y / 2.f;
				NVGcolor c = meterColor(st_);
				if (st_ >= st::METER_HOT) {
					NVGcolor glow = c;
					glow.a = 0.35f;
					NVGpaint p = nvgRadialGradient(args.vg, cx, cy, 2.f, 8.f, glow, nvgRGBA(0, 0, 0, 0));
					nvgBeginPath(args.vg);
					nvgCircle(args.vg, cx, cy, 8.f);
					nvgFillPaint(args.vg, p);
					nvgFill(args.vg);
				}
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, cx, cy, 2.6f);
				nvgFillColor(args.vg, c);
				nvgFill(args.vg);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

// 靜音／獨奏小按鈕：按一下切換開關，開啟時亮燈（M 紅色、S 黃色）
struct MSButton : app::Switch {
	std::string label;
	bool isMute = true;
	bool isBrand = false;   // SPLIT 按鈕：開啟時亮品牌綠
	Atmos714* module = nullptr;

	MSButton() {
		box.size = Vec(15.f, 12.f);
	}

	bool isOn() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq && pq->getValue() > 0.5f;
	}

	void drawButton(const DrawArgs& args, bool on) {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f);   // 包浩斯：直角方塊
		const st::theme::Palette& pal = palOf(module);
		NVGcolor onColor = th(isBrand ? st::theme::BRAND_ : (isMute ? st::theme::RED_ : st::theme::YELLOW_));
		nvgFillColor(args.vg, on ? onColor : th(pal.BUTTON_OFF));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, th(pal.BUTTON_EDGE));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStroke(args.vg);
		std::shared_ptr<window::Font> font = stFontBold();
		if (!font)
			return;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 8.5f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, on ? th(isBrand ? st::theme::PAPER_ : st::theme::INK_) : th(pal.BUTTON_TEXT));
		nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f + 0.5f, label.c_str(), NULL);
	}

	void draw(const DrawArgs& args) override {
		drawButton(args, false);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		// 開啟時畫在發光層，暗房模式也看得到
		if (layer == 1 && isOn())
			drawButton(args, true);
		app::Switch::drawLayer(args, layer);
	}
};

// ORB 按鈕上的指示燈：這一軌正在繞場（按了 ORB，或 LFO 孔有接線）時亮品牌綠。
// 繞場時聲音被固定在 DIA 決定的圓上，X / Y 旋鈕與 CV 只決定起始方向；燈亮就知道這一軌現在由繞場控制。
struct OrbIndicator : TransparentWidget {
	Atmos714* module = nullptr;
	int track = 0;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1 || !module || !module->orbitEngaged[track])
			return;
		float r = box.size.x / 2.f;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, r, r, r);
		nvgFillColor(args.vg, th(st::theme::BRAND_, 230));
		nvgFill(args.vg);
	}
};

// 輸出孔的淡化遮罩：這個聲道不在目前的輸出配置（4 / 5.1 / 7.1）裡時，蓋上一層輸出區底色，讓輸出孔與標籤變淡。
// TransparentWidget 不吃滑鼠事件，仍然可以照常接線。
struct OutDimmer : TransparentWidget {
	Atmos714* module = nullptr;
	int channel = 0;

	void draw(const DrawArgs& args) override {
		if (!module)
			return;
		bool mask[st::NCH];
		st::layoutMask(module->outLayout, mask);
		if (mask[channel])
			return;
		const st::theme::Palette& pal = palOf(module);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, th(pal.OUTBLOCK, 190));
		nvgFill(args.vg);
	}
};

// TEST CH：點一下就跳到下一個聲道（1 → 2 → … → 最後一個 → 回到 1），右鍵直接從清單選。
// 按鈕上顯示目前選的聲道名稱（L、R、C、LFE…）；只算目前輸出配置有接的聲道。
struct TestChButton : app::ParamWidget {
	Atmos714* module = nullptr;

	TestChButton() {
		box.size = Vec(24.f, 14.f);
	}

	int activeCount(int* idx) {
		return st::layoutChannels(module ? module->outLayout : st::LAYOUT_12, idx);
	}

	int current() {   // 目前選的是第幾個（1 起算），超出目前配置時夾回範圍內
		engine::ParamQuantity* pq = getParamQuantity();
		int idx[st::NCH];
		int n = activeCount(idx);
		int k = pq ? (int) std::round(pq->getValue()) : 1;
		return clamp(k, 1, n);
	}

	void setChannel(int k) {
		engine::ParamQuantity* pq = getParamQuantity();
		if (pq)
			pq->setValue((float) k);
	}

	void draw(const DrawArgs& args) override {
		const st::theme::Palette& pal = palOf(module);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f);
		nvgFillColor(args.vg, th(pal.BUTTON_OFF));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, th(pal.BUTTON_EDGE));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStroke(args.vg);
		std::shared_ptr<window::Font> font = stFontBold();
		if (!font)
			return;
		int idx[st::NCH];
		int n = activeCount(idx);
		int k = clamp(current(), 1, n);
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 9.f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, th(pal.BUTTON_TEXT));
		nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f + 0.5f, st::CH_NAMES[idx[k - 1]], NULL);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action != GLFW_PRESS)
			return;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			int idx[st::NCH];
			int n = activeCount(idx);
			setChannel(current() % n + 1);   // 下一個，最後一個之後回到第 1 個
			e.consume(this);
		}
		else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu* menu = createMenu();
			menu->addChild(createMenuLabel("Test channel"));
			int idx[st::NCH];
			int n = activeCount(idx);
			for (int k = 1; k <= n; k++) {
				std::string label = std::to_string(k) + "  " + st::CH_NAMES[idx[k - 1]];
				menu->addChild(createCheckMenuItem(label, "", [=]() { return current() == k; }, [=]() { setChannel(k); }));
			}
			e.consume(this);
		}
	}

	void onDoubleClick(const DoubleClickEvent& e) override {}   // 不要在雙擊時重設參數
};

// 深觸 LOGO：讀取 res/logo.png（白字透明背景），也支援 res/logo.svg（字要先轉外框）。
// 等比例縮放、置中放進預留的方框；找不到檔案時什麼都不畫。
struct LogoWidget : Widget {
	// [0] = 深色字（淺色面板用），[1] = 白字（深色面板用）
	// 讀取 res/logo.png / res/logo_white.png；也支援同名 .svg（字要先轉外框），有 SVG 時優先使用
	//
	// 注意（當機修正）：這個元件「不可以」把 window::Image（PNG 圖片）存成成員。
	// Rack 關閉時是先刪除視窗（連同 NanoVG 繪圖環境），之後才刪除模組畫面；
	// 如果元件還握著圖片，圖片會在繪圖環境已經不存在時才被釋放（nvgDeleteImage），
	// 造成「每次關閉 Rack 都未預期結束」。所以 PNG 每次繪製時才向 Rack 的圖片快取取用，用完就放手。
	// SVG（window::Svg）只是向量資料、不佔繪圖環境的資源，可以保留。
	std::shared_ptr<window::Svg> svg[2];
	std::string pngPath[2];
	bool loaded = false;
	Atmos714* module = nullptr;

	void loadOne(int i, const std::string& base) {
		std::string sp = asset::plugin(pluginInstance, base + ".svg");
		std::string pp = asset::plugin(pluginInstance, base + ".png");
		if (system::isFile(sp))
			svg[i] = window::Svg::load(sp);
		else if (system::isFile(pp))
			pngPath[i] = pp;
	}

	void draw(const DrawArgs& args) override {
		if (!loaded) {
			loaded = true;
			loadOne(0, "res/logo");
			loadOne(1, "res/logo_white");
		}
		int i = palOf(module).logoWhite ? 1 : 0;
		if (!svg[i] && pngPath[i].empty())
			i = 1 - i;   // 只有其中一種時就用那一種
		float bw = box.size.x, bh = box.size.y;
		if (svg[i] && svg[i]->handle) {
			float sw = svg[i]->handle->width, sh = svg[i]->handle->height;
			if (sw <= 0.f || sh <= 0.f)
				return;
			float sc = std::min(bw / sw, bh / sh);
			nvgSave(args.vg);
			nvgTranslate(args.vg, (bw - sw * sc) / 2.f, (bh - sh * sc) / 2.f);
			nvgScale(args.vg, sc, sc);
			window::svgDraw(args.vg, svg[i]->handle);
			nvgRestore(args.vg);
			return;
		}
		if (pngPath[i].empty() || !APP->window)
			return;
		// PNG 圖片屬於主視窗的繪圖環境；在模組瀏覽器的預覽圖（另一個繪圖環境）裡不畫，避免用錯圖片
		if (args.vg != APP->window->vg)
			return;
		std::shared_ptr<window::Image> image = APP->window->loadImage(pngPath[i]);   // Rack 會快取，這裡只暫時借用
		if (!image || image->handle < 0)
			return;
		int iw = 0, ih = 0;
		nvgImageSize(args.vg, image->handle, &iw, &ih);
		if (iw <= 0 || ih <= 0)
			return;
		float sc = std::min(bw / iw, bh / ih);
		float dw = iw * sc, dh = ih * sc;
		float x = (bw - dw) / 2.f, y = (bh - dh) / 2.f;
		nvgBeginPath(args.vg);
		nvgRect(args.vg, x, y, dw, dh);
		nvgFillPaint(args.vg, nvgImagePattern(args.vg, x, y, dw, dh, 0.f, image->handle, 1.f));
		nvgFill(args.vg);
	}
};

// 軌道編號：彩色小圓片＋數字（數字依圓片顏色深淺自動用黑字或白字）
struct TrackChip : TransparentWidget {
	int track = 0;
	Atmos714* module = nullptr;

	void draw(const DrawArgs& args) override {
		float cx = box.size.x / 2.f, cy = box.size.y / 2.f;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, 6.5f);
		nvgFillColor(args.vg, trackColor(track));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, th(palOf(module).CHIP_OUTLINE, 160));
		nvgStrokeWidth(args.vg, 0.7f);
		nvgStroke(args.vg);
		std::shared_ptr<window::Font> font = stFontBold();
		if (!font)
			return;
		std::string num = std::to_string(track + 1);
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, num.size() > 1 ? 7.5f : 8.5f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, st::view::darkText(st::view::TRACK_COLORS[track % 12]) ? th(st::theme::INK_) : th(st::theme::PAPER_));
		nvgText(args.vg, cx, cy + 0.5f, num.c_str(), NULL);
	}
};

// 目前測試中的聲道名稱
struct TestDisplay : TransparentWidget {
	Atmos714* module = nullptr;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 2.f);
		const st::theme::Palette& pal = palOf(module);
		nvgFillColor(args.vg, th(pal.DISPLAY_BG));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, th(pal.BORDER, 80));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStroke(args.vg);

		std::string txt = "---";
		NVGcolor col = th(pal.TEXT_DIM, 160);
		if (module && module->testActive) {
			txt = st::CH_NAMES[module->testChannel];
			col = th(pal.DISPLAY_TEXT);
		}
		std::shared_ptr<window::Font> font = stFont();
		if (!font)
			return;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 11.f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, col);
		nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, txt.c_str(), NULL);
	}
};

// 立方體：顯示每個輸入目前的位置（點擊切換視角：ISO / TOP / FRONT / SIDE）
struct CubeDisplay : Widget {
	Atmos714* module = nullptr;

	void onButton(const ButtonEvent& e) override {
		if (module && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			module->viewMode = (module->viewMode + 1) % st::view::NUM_MODES;
			e.consume(this);
		}
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			drawCube(args);
		Widget::drawLayer(args, layer);
	}

	void line(NVGcontext* vg, st::view::Pt a, st::view::Pt b, NVGcolor col, float w) {
		nvgBeginPath(vg);
		nvgMoveTo(vg, a.x, a.y);
		nvgLineTo(vg, b.x, b.y);
		nvgStrokeColor(vg, col);
		nvgStrokeWidth(vg, w);
		nvgStroke(vg);
	}

	// 兩端圓頭的粗線（膠囊）；兩點重合時畫成圓點
	void capsule(NVGcontext* vg, st::view::Pt a, st::view::Pt b, float width, NVGcolor col) {
		nvgBeginPath(vg);
		if (std::fabs(a.x - b.x) + std::fabs(a.y - b.y) < 0.5f) {
			nvgCircle(vg, a.x, a.y, width * 0.5f);
			nvgFillColor(vg, col);
			nvgFill(vg);
			return;
		}
		nvgMoveTo(vg, a.x, a.y);
		nvgLineTo(vg, b.x, b.y);
		nvgLineCap(vg, NVG_ROUND);
		nvgStrokeColor(vg, col);
		nvgStrokeWidth(vg, width);
		nvgStroke(vg);
	}

	void text(NVGcontext* vg, float x, float y, const char* s, float size, NVGcolor col, int align = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE) {
		std::shared_ptr<window::Font> font = stFont();
		if (!font)
			return;
		nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, size);
		nvgTextAlign(vg, align);
		nvgFillColor(vg, col);
		nvgText(vg, x, y, s, NULL);
	}

	void drawCube(const DrawArgs& args) {
		NVGcontext* vg = args.vg;
		const float w = box.size.x, h = box.size.y;
		int mode = module ? module->viewMode : st::view::ISO;

		// 背景
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0.f, 0.f, w, h, 3.f);
		const st::theme::Palette& pal = palOf(module);
		nvgFillColor(vg, th(pal.CUBE_BG));
		nvgFill(vg);
		nvgStrokeColor(vg, th(pal.CUBE_BORDER));
		nvgStrokeWidth(vg, 1.5f);
		nvgStroke(vg);

		// 房間比例（ROOM 擴充模組）：立方體跟著變成長方體，所有位置一起縮放
		st::Room rm = module ? module->room : st::Room();
		auto P = [&](float x, float y, float z) { return st::view::project(mode, x * rm.sx, y * rm.sy, z * rm.sz, w, h); };

		// 立方體邊框
		for (int e = 0; e < 12; e++) {
			float ax, ay, az, bx, by, bz;
			st::view::corner(st::view::EDGES[e][0], ax, ay, az);
			st::view::corner(st::view::EDGES[e][1], bx, by, bz);
			bool floorEdge = (az < 0.f && bz < 0.f);
			line(vg, P(ax, ay, az), P(bx, by, bz), floorEdge ? th(pal.CUBE_FLOOR, 200) : th(pal.CUBE_EDGE, 190), 0.8f);
		}
		// 耳朵高度平面 z = 0（金色細線）與聆聽位置
		const float ring[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
		for (int i = 0; i < 4; i++) {
			int j = (i + 1) % 4;
			line(vg, P(ring[i][0], ring[i][1], 0.f), P(ring[j][0], ring[j][1], 0.f), th(pal.SPEAKER_TOP, 160), 0.7f);
		}
		st::view::Pt c0 = P(0.f, 0.f, 0.f);
		line(vg, {c0.x - 3.f, c0.y, 0}, {c0.x + 3.f, c0.y, 0}, th(pal.CUBE_TEXT, 200), 0.9f);
		line(vg, {c0.x, c0.y - 3.f, 0}, {c0.x, c0.y + 3.f, 0}, th(pal.CUBE_TEXT, 200), 0.9f);

		// 喇叭（LFE 不顯示；只畫目前輸出配置有的喇叭；重疊的喇叭合併成一個標籤）
		int layoutId = module ? module->outLayout : st::LAYOUT_12;
		bool mask[st::NCH];
		st::layoutMask(layoutId, mask);
		// 目前輸出配置沒用到的喇叭畫得很淡（還看得到位置，但一眼就知道沒在用）
		bool unused[st::NCH];
		for (int c = 0; c < st::NCH; c++)
			unused[c] = !mask[c];
		for (const st::view::SpeakerLabel& g : st::view::speakerLabels(mode, w, h, unused, rm.sx, rm.sy, rm.sz)) {
			NVGcolor col = th(pal.SPEAKER, 45);
			nvgBeginPath(vg);
			nvgRect(vg, g.x - 2.f, g.y - 2.f, 4.f, 4.f);
			nvgStrokeColor(vg, col);
			nvgStrokeWidth(vg, 0.8f);
			nvgStroke(vg);
			text(vg, g.x, g.y - 7.f, g.text.c_str(), 6.f, col);
		}
		for (const st::view::SpeakerLabel& g : st::view::speakerLabels(mode, w, h, mask, rm.sx, rm.sy, rm.sz)) {
			// 喇叭退到背景：顏色調暗（頂部喇叭偏藍）
			NVGcolor col = g.top ? th(pal.SPEAKER_TOP, 210) : th(pal.SPEAKER, 180);
			nvgBeginPath(vg);
			nvgRect(vg, g.x - 2.f, g.y - 2.f, 4.f, 4.f);
			nvgStrokeColor(vg, col);
			nvgStrokeWidth(vg, 1.f);
			nvgStroke(vg);
			text(vg, g.x, g.y - 7.f, g.text.c_str(), 6.f, col);
		}

		// 聲源：立體聲的 L / R 畫成一條「膠囊」（兩端就是 L、R 的位置），單聲道畫成圓點，編號在中間。
		// 不畫垂直線；ISO 視角改在地板上畫一道淡影子，提示聲源的水平位置與高度。沒接線的軌道不顯示。
		struct Src { int t; st::view::Pt a, b; st::view::Pt fa, fb; float depth; };
		Src srcs[NUM_TRACKS];
		int n = 0;
		for (int t = 0; t < NUM_TRACKS; t++) {
			float pos[2][3];
			int count;
			if (module) {
				count = module->srcCount[t];
				for (int s = 0; s < 2; s++)
					for (int k = 0; k < 3; k++)
						pos[s][k] = module->effPos[t][s][k];
			}
			else {
				// 模組瀏覽器預覽：沿耳朵高度圓周排列的立體聲來源
				float th = (float) t / NUM_TRACKS * 2.f * 3.14159265f;
				float cx = -0.8f * std::sin(th), cy = 0.8f * std::cos(th), cz = (t % 3 - 1) * 0.5f;
				float xl, xr;
				st::stereoSplit(cx, 0.18f, xl, xr);
				pos[0][0] = xl; pos[0][1] = cy; pos[0][2] = cz;
				pos[1][0] = xr; pos[1][1] = cy; pos[1][2] = cz;
				count = 2;
			}
			if (count == 0)
				continue;
			int bi = (count == 2) ? 1 : 0;
			Src s;
			s.t = t;
			s.a = P(pos[0][0], pos[0][1], pos[0][2]);
			s.b = P(pos[bi][0], pos[bi][1], pos[bi][2]);
			s.fa = P(pos[0][0], pos[0][1], -1.f);
			s.fb = P(pos[bi][0], pos[bi][1], -1.f);
			s.depth = 0.5f * (s.a.depth + s.b.depth);
			srcs[n++] = s;
		}
		std::sort(srcs, srcs + n, [](const Src& a, const Src& b) { return a.depth > b.depth; });

		// 繞場軌跡：正在繞場的軌道，在它的高度畫一個淡色圓圈
		if (module) {
			for (int i = 0; i < n; i++) {
				int t = srcs[i].t;
				if (!module->orbitCircle[t])
					continue;
				float r = module->orbitRadius[t], w = module->orbitFixed[t];
				float rot = module->params[Atmos714::ROTATE_PARAM].getValue();
				if (module->inputs[Atmos714::ROTATE_CV_INPUT].isConnected())
					rot += module->inputs[Atmos714::ROTATE_CV_INPUT].getVoltage() * 36.f;
				// ROOM 擴充模組選 ROUND 時，軌跡是真實空間裡的正圓（半徑以較短邊為準）
				float ku = 1.f, kv = 1.f;
				if (rm.roundOrbit && !rm.isCube()) {
					float su, sv, sw;
					st::toPlane(module->orbitAxis[t], rm.sx, rm.sy, rm.sz, su, sv, sw);
					su = std::fabs(su);
					sv = std::fabs(sv);
					float smin = std::min(su, sv);
					ku = smin / su;
					kv = smin / sv;
				}
				nvgBeginPath(vg);
				for (int k = 0; k <= 64; k++) {
					float a = k / 64.f * 2.f * 3.14159265f;
					float cx, cy, cz;
					st::fromPlane(module->orbitAxis[t], r * ku * std::cos(a), r * kv * std::sin(a), w, cx, cy, cz);
					st::rotateAxis(module->rotateAxis, cx, cy, cz, rot);
					st::view::Pt q = P(cx, cy, cz);
					if (k == 0)
						nvgMoveTo(vg, q.x, q.y);
					else
						nvgLineTo(vg, q.x, q.y);
				}
				nvgStrokeColor(vg, trackColor(t, 90));
				nvgStrokeWidth(vg, 1.f);
				nvgStroke(vg);
			}
		}

		// 地板影子（只在 ISO）
		if (mode == st::view::ISO) {
			for (int i = 0; i < n; i++)
				capsule(vg, srcs[i].fa, srcs[i].fb, 5.f, trackColor(srcs[i].t, 55));
		}
		// 膠囊：由遠到近畫
		for (int i = 0; i < n; i++) {
			const Src& s = srcs[i];
			bool clip = module && module->trackMeter[s.t].state() == st::METER_CLIP;
			if (clip)
				capsule(vg, s.a, s.b, 14.f, th(st::theme::RED_));           // 爆音：紅框
			bool silenced = module && module->trackSilenced[s.t];
			capsule(vg, s.a, s.b, 11.4f, th(pal.CAPSULE_OUTLINE, silenced ? 70 : 230));   // 細外框，重疊時分得清楚
			capsule(vg, s.a, s.b, 10.f, trackColor(s.t, silenced ? 60 : 255));   // 被 Mute/Solo 關掉時變暗
			std::string num = std::to_string(s.t + 1);
			NVGcolor numCol = st::view::darkText(st::view::TRACK_COLORS[s.t % 12]) ? th(st::theme::INK_) : th(st::theme::PAPER_);
			text(vg, 0.5f * (s.a.x + s.b.x), 0.5f * (s.a.y + s.b.y) + 0.5f, num.c_str(), num.size() > 1 ? 5.5f : 6.5f,
			     silenced ? trackColor(s.t, 200) : numCol);
		}

		// 視角名稱與提示
		std::string label = std::string(st::view::modeName(mode)) + "  (click: view)";
		text(vg, 6.f, 8.f, label.c_str(), 7.f, th(pal.CUBE_TEXT), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		std::string outTxt = std::string("OUT ") + st::layoutShort(layoutId);
		if (module && module->limiter)
			outTxt += " LIM";
		if (module && module->roomLinked)
			outTxt = "ROOM  " + outTxt;
		// 爆音中的輸出聲道
		if (module) {
			std::string clipTxt;
			for (int c = 0; c < st::NCH; c++)
				if (module->outMeter[c].state() == st::METER_CLIP)
					clipTxt += std::string(clipTxt.empty() ? "" : " ") + st::CH_NAMES[c];
			if (!clipTxt.empty())
				text(vg, 6.f, h - 7.f, ("CLIP " + clipTxt).c_str(), 7.f, th(st::theme::RED_), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		}
		text(vg, w - 6.f, 8.f, outTxt.c_str(), 7.f, th(pal.TEXT_DIM, 230), NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
	}
};

struct Atmos714Widget : ModuleWidget {
	struct DimSpot {
		int ch;
		Vec pos;
	};
	std::vector<DimSpot> dimSpots;   // 建構時暫存各輸出孔位置

	Atmos714Widget(Atmos714* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, std::string("res/TheIsland") + st::theme::palette(st::theme::THEME_DEFAULT).svgSuffix + ".svg")));

		using namespace st::layout;

		for (const Item& it : build()) {
			Vec pos(it.x, it.y);
			switch (it.kind) {
				case K_LABEL: {
					if (it.index >= 0) {
						// 軌道編號
						TrackChip* chip = new TrackChip();
						chip->track = it.index;
						chip->module = module;
						chip->box.size = Vec(14.f, 14.f);
						chip->box.pos = Vec(it.x - 7.f, it.y - 7.f);
						addChild(chip);
					}
					else {
						StLabel::Kind kind = it.index == LABEL_BAND ? StLabel::BAND : it.index == LABEL_OUT ? StLabel::OUT
						                   : (it.fontSize >= 10.f ? StLabel::TITLE : StLabel::NORMAL);
						addChild(new StLabel(pos, it.text, it.fontSize, kind, module));
					}
				} break;

				case K_INPUT: {
					int id = -1;
					switch (it.role) {
						case R_AUDIO: id = Atmos714::iAudioL(it.index); break;
							case R_AUDIOR: id = Atmos714::iAudioR(it.index); break;
						case R_XCV: id = Atmos714::iX(it.index); break;
						case R_YCV: id = Atmos714::iY(it.index); break;
						case R_ZCV: id = Atmos714::iZ(it.index); break;
						case R_ROTCV: id = Atmos714::ROTATE_CV_INPUT; break;
						case R_ORBCV: id = Atmos714::iOrb(it.index); break;
						default: break;
					}
					if (id >= 0)
						addInput(createInputCentered<PJ301MPort>(pos, module, id));
				} break;

				case K_OUTPUT: {
					int id = (it.role == R_POLYOUT) ? (int) Atmos714::POLY_OUTPUT : Atmos714::CH_OUTPUTS_START + it.index;
					addOutput(createOutputCentered<PJ301MPort>(pos, module, id));
					if (it.role == R_CHOUT)
						dimSpots.push_back({it.index, pos});
				} break;

				case K_KNOB: {
					int id = -1;
					switch (it.role) {
						case R_PX: id = Atmos714::pX(it.index); break;
						case R_PY: id = Atmos714::pY(it.index); break;
						case R_PZ: id = Atmos714::pZ(it.index); break;
							case R_WIDTH: id = Atmos714::pWidth(it.index); break;
						case R_LVL: id = Atmos714::pLvl(it.index); break;
						case R_LFE: id = Atmos714::pLfe(it.index); break;
						case R_ROTATE: id = Atmos714::ROTATE_PARAM; break;
						case R_FOCUS: id = Atmos714::FOCUS_PARAM; break;
						case R_BLUR: id = Atmos714::BLUR_PARAM; break;
						case R_MASTER: id = Atmos714::MASTER_PARAM; break;
						case R_ORBDIA: id = Atmos714::pOrbDia(it.index); break;
						case R_ORBTIME: id = Atmos714::ORBIT_RATE_PARAM; break;
							case R_ORBSPD: id = Atmos714::pOrbSpd(it.index); break;
						default: break;
					}
					if (id >= 0)
						addParam(createParamCentered<Trimpot>(pos, module, id));
				} break;

				case K_LATCH:
					addParam(createParamCentered<VCVLatch>(pos, module, it.role == R_TESTAUTO ? Atmos714::TEST_AUTO_PARAM : Atmos714::TEST_PARAM));
					break;

				case K_CUBE: {
					CubeDisplay* d = new CubeDisplay();
					d->module = module;
					d->box.size = Vec(it.w, it.h);
					d->box.pos = Vec(it.x - it.w / 2.f, it.y - it.h / 2.f);
					addChild(d);
				} break;

				case K_BUTTON:
					if (it.role == R_MUTE || it.role == R_SOLO) {
						bool mute = (it.role == R_MUTE);
						MSButton* b = createParamCentered<MSButton>(pos, module, mute ? Atmos714::pMute(it.index) : Atmos714::pSolo(it.index));
						b->label = mute ? "M" : "S";
						b->isMute = mute;
						b->module = module;
						addParam(b);
					}
					else if (it.role == R_SPLIT) {
						MSButton* b = createParamCentered<MSButton>(pos, module, Atmos714::pSplit(it.index));
						b->label = "SP";
						b->isBrand = true;
						b->module = module;
						addParam(b);
					}
					else if (it.role == R_TESTCH) {
						TestChButton* b = createParamCentered<TestChButton>(pos, module, Atmos714::TEST_CH_PARAM);
						b->module = module;
						addParam(b);
					}
					else {
						addParam(createParamCentered<TL1105>(pos, module, Atmos714::pOrbBtn(it.index)));
						OrbIndicator* ind = new OrbIndicator();
						ind->module = module;
						ind->track = it.index;
						ind->box.size = Vec(6.f, 6.f);
						ind->box.pos = Vec(pos.x - 3.f, pos.y - 3.f);
						addChild(ind);
					}
					break;

				case K_LED: {
					LevelLed* led = new LevelLed();
					led->module = module;
					led->isOutput = (it.role == R_OUTLED);
					led->index = it.index;
					led->box.size = Vec(8.f, 8.f);
					led->box.pos = Vec(it.x - 4.f, it.y - 4.f);
					addChild(led);
				} break;

				case K_LOGO: {
					LogoWidget* lg = new LogoWidget();
					lg->module = module;
					lg->box.size = Vec(it.w, it.h);
					lg->box.pos = Vec(it.x - it.w / 2.f, it.y - it.h / 2.f);
					addChild(lg);
				} break;

				case K_TESTDISP: {
					TestDisplay* d = new TestDisplay();
					d->module = module;
					d->box.size = Vec(it.w, it.h);
					d->box.pos = Vec(it.x - it.w / 2.f, it.y - it.h / 2.f);
					addChild(d);
				} break;
			}
		}

		// 輸出區：目前輸出配置沒用到的輸出孔蓋上一層半透明色塊，看起來很淡（不擋接線）
		for (const DimSpot& ds : dimSpots) {
			OutDimmer* od = new OutDimmer();
			od->module = module;
			od->channel = ds.ch;
			od->box.pos = Vec(ds.pos.x - 19.f, ds.pos.y - 24.f);
			od->box.size = Vec(42.f, 37.f);
			addChild(od);
		}
	}

	// 面板底圖跟著主題切換（res/TheIsland.svg、TheIsland_mist.svg、TheIsland_sand.svg）
	int shownTheme = -99;

	void step() override {
		Atmos714* m = getModule<Atmos714>();
		int t = m ? m->resolvedTheme() : st::theme::THEME_DEFAULT;
		if (t != shownTheme) {
			shownTheme = t;
			app::SvgPanel* sp = dynamic_cast<app::SvgPanel*>(getPanel());
			if (sp) {
				std::string path = asset::plugin(pluginInstance, std::string("res/TheIsland") + st::theme::palette(t).svgSuffix + ".svg");
				if (system::isFile(path))
					sp->setBackground(window::Svg::load(path));
			}
		}
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		Atmos714* module = getModule<Atmos714>();
		if (!module)
			return;
		menu->addChild(new MenuSeparator);

		// 面板主題
		{
			static const char* const THEME_LABELS[st::theme::NUM_THEMES] = {
				"Sky (light blue, default)", "Mist (grey-green)", "Sand (warm beige)", "Dark (black)"};
			std::string cur = module->panelTheme < 0 ? "Auto" : st::theme::palette(module->resolvedTheme()).name;
			menu->addChild(createSubmenuItem("Panel theme", cur, [=](Menu* sub) {
				sub->addChild(createCheckMenuItem("Auto (follow Rack's \"Use dark panels\": Sky / Dark)", "",
					[=]() { return module->panelTheme < 0; }, [=]() { module->panelTheme = -1; }));
				for (int t = 0; t < st::theme::NUM_THEMES; t++)
					sub->addChild(createCheckMenuItem(THEME_LABELS[t], "",
						[=]() { return module->panelTheme == t; }, [=]() { module->panelTheme = t; }));
			}));
		}

		menu->addChild(createMenuItem("Unmute all tracks", "", [=]() {
			for (int t = 0; t < NUM_TRACKS; t++)
				module->params[Atmos714::pMute(t)].setValue(0.f);
		}));
		menu->addChild(createMenuItem("Unsolo all tracks", "", [=]() {
			for (int t = 0; t < NUM_TRACKS; t++)
				module->params[Atmos714::pSolo(t)].setValue(0.f);
		}));

		menu->addChild(createSubmenuItem("Output layout", st::layoutShort(module->outLayout), [=](Menu* sub) {
			for (int oi = 0; oi < st::NUM_LAYOUTS; oi++) {
				int l = st::LAYOUT_MENU_ORDER[oi];
				sub->addChild(createCheckMenuItem(st::layoutName(l), "",
					[=]() { return module->outLayout == l; },
					[=]() { module->outLayout = l; }));
			}
		}));

		menu->addChild(createSubmenuItem("Cube view", st::view::modeName(module->viewMode), [=](Menu* sub) {
			for (int m = 0; m < st::view::NUM_MODES; m++)
				sub->addChild(createCheckMenuItem(st::view::modeName(m), "",
					[=]() { return module->viewMode == m; },
					[=]() { module->viewMode = m; }));
		}));

static const char* const CV_MODE_NAMES[2] = {"±5 V", "±10 V"};
static const char* const CV_MODE_LABELS[2] = {
	"±5 V = full travel (standard bipolar LFO; 0 V = knob position)",
	"±10 V = full travel (hot sources; 0-10 V unipolar goes knob → edge)"};
		static const char* const AXIS_LABELS[st::NUM_AXES] = {
			"Z: horizontal circle (front -> left -> back -> right)",
			"X: vertical circle (front -> ceiling -> back -> floor)",
			"Y: vertical circle (right -> ceiling -> left -> floor)",
		};
		// 繞場設定：模式、方向、軸向（全部軌道一起設，或每軌各自設）
		menu->addChild(createSubmenuItem("Orbit", "", [=](Menu* sub) {
			sub->addChild(createSubmenuItem("All tracks", "", [=](Menu* all) {
				all->addChild(createMenuLabel("Mode"));
				all->addChild(createMenuItem("  One turn (press = one revolution)", "", [=]() {
					for (int t = 0; t < NUM_TRACKS; t++) module->orbitLoop[t] = false;
				}));
				all->addChild(createMenuItem("  Loop (press = start, press again = stop)", "", [=]() {
					for (int t = 0; t < NUM_TRACKS; t++) module->orbitLoop[t] = true;
				}));
				all->addChild(createMenuLabel("Direction"));
				all->addChild(createMenuItem("  Left (counter-clockwise)", "", [=]() {
					for (int t = 0; t < NUM_TRACKS; t++) module->orbitDir[t] = 1;
				}));
				all->addChild(createMenuItem("  Right (clockwise)", "", [=]() {
					for (int t = 0; t < NUM_TRACKS; t++) module->orbitDir[t] = -1;
				}));
				all->addChild(createMenuLabel("Axis"));
				for (int a = 0; a < st::NUM_AXES; a++)
					all->addChild(createMenuItem(std::string("  ") + AXIS_LABELS[a], "", [=]() {
						for (int t = 0; t < NUM_TRACKS; t++) module->orbitAxis[t] = a;
					}));
			}));
			sub->addChild(new MenuSeparator);
			for (int t = 0; t < NUM_TRACKS; t++) {
				std::string summary = std::string(module->orbitLoop[t] ? "Loop" : "1 turn") + " / "
					+ (module->orbitDir[t] > 0 ? "L" : "R") + " / " + st::axisName(module->orbitAxis[t]);
				sub->addChild(createSubmenuItem("Track " + std::to_string(t + 1), summary, [=](Menu* tr) {
					tr->addChild(createMenuLabel("Mode"));
					tr->addChild(createCheckMenuItem("One turn (press = one revolution)", "",
						[=]() { return !module->orbitLoop[t]; }, [=]() { module->orbitLoop[t] = false; }));
					tr->addChild(createCheckMenuItem("Loop (press = start, press again = stop)", "",
						[=]() { return module->orbitLoop[t]; }, [=]() { module->orbitLoop[t] = true; }));
					tr->addChild(createMenuLabel("Direction"));
					tr->addChild(createCheckMenuItem("Left (counter-clockwise)", "",
						[=]() { return module->orbitDir[t] > 0; }, [=]() { module->orbitDir[t] = 1; }));
					tr->addChild(createCheckMenuItem("Right (clockwise)", "",
						[=]() { return module->orbitDir[t] < 0; }, [=]() { module->orbitDir[t] = -1; }));
					tr->addChild(createMenuLabel("Axis"));
					for (int a = 0; a < st::NUM_AXES; a++)
						tr->addChild(createCheckMenuItem(AXIS_LABELS[a], "",
							[=]() { return module->orbitAxis[t] == a; },
							[=]() { module->orbitAxis[t] = a; }));
				}));
			}
		}));

		menu->addChild(createSubmenuItem("Rotate axis (ROTATE knob)", st::axisName(module->rotateAxis), [=](Menu* sub) {
			for (int a = 0; a < st::NUM_AXES; a++)
				sub->addChild(createCheckMenuItem(AXIS_LABELS[a], "",
					[=]() { return module->rotateAxis == a; },
					[=]() { module->rotateAxis = a; }));
		}));

		menu->addChild(createSubmenuItem("CV range (X/Y/Z, ROT, ORB LFO)", CV_MODE_NAMES[module->cvMode], [=](Menu* sub) {
			for (int m = 0; m < 2; m++)
				sub->addChild(createCheckMenuItem(CV_MODE_LABELS[m], "",
					[=]() { return module->cvMode == m; },
					[=]() { module->cvMode = m; }));
		}));

		menu->addChild(createBoolPtrMenuItem("Output safety limiter (soft, max 10 V = 0 dBFS)", "", &module->limiter));
	}
};

Model* modelAtmos714 = createModel<Atmos714, Atmos714Widget>("TheIsland");   // 模組代號（上架後永遠不能改）


// ===========================================================================
// ROOM 擴充模組（The Island 專用）
// 把聲像計算用的「房間」從正立方體改成任意長寬高比例。放在 The Island 的左邊或右邊、緊貼著就會生效。
//   WIDTH ：左牆到右牆（Ls ↔ Rs）的距離
//   DEPTH ：前牆到後牆（L/R ↔ Lrs/Rrs）的距離
//   HEIGHT：耳朵高度的喇叭到天花板喇叭的高度
// 單位寫公尺，但只有三者的比例會影響聲像，所以直接填比例（例如 1、2、0.5）也可以。
// ORBIT：FIT = 繞場貼合房間（真實空間裡是橢圓）；ROUND = 真實空間裡的正圓（以較短邊為準）。
// ===========================================================================
struct IslandRoom : Module {
	enum ParamIds { PARAMS_LEN = st::roomexp::NUM_PARAMS };
	enum InputIds { INPUTS_LEN };
	enum OutputIds { OUTPUTS_LEN };
	enum LightIds { LIGHTS_LEN };

	// 面板主題：-1 = 跟著相鄰的 The Island（沒有相鄰時用 Sky，Rack 開了「Use dark panels」時用 Dark）
	int panelTheme = -1;
	// 顯示單位：只影響面板上的數字與輸入，不影響聲像（聲像只看三者的比例）。內部一律以公尺儲存。
	int unit = st::roomexp::UNIT_M;
	bool linked = false;         // 顯示用：是否緊貼著一個 The Island
	int linkedTheme = -1;        // 相鄰 The Island 目前的主題
	int divider = 0;

	IslandRoom() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(st::roomexp::W_PARAM, st::roomexp::MIN_SIZE, st::roomexp::MAX_SIZE, st::roomexp::DEFAULT_W, "Room width (left wall to right wall)", " m");
		configParam(st::roomexp::D_PARAM, st::roomexp::MIN_SIZE, st::roomexp::MAX_SIZE, st::roomexp::DEFAULT_D, "Room depth (front wall to rear wall)", " m");
		configParam(st::roomexp::H_PARAM, st::roomexp::MIN_SIZE, st::roomexp::MAX_SIZE, st::roomexp::DEFAULT_H, "Height (ear-level speakers to ceiling speakers)", " m");
		configSwitch(st::roomexp::ORBIT_PARAM, 0.f, 1.f, 0.f, "Orbit shape", {"Fit the room (ellipse)", "Round (true circle)"});
	}

	Atmos714* neighbour() {
		Module* m = leftExpander.module;
		if (!(m && m->model == modelAtmos714))
			m = rightExpander.module;
		if (m && m->model == modelAtmos714)
			return dynamic_cast<Atmos714*>(m);
		return nullptr;
	}

	void process(const ProcessArgs& args) override {
		if (++divider < 256)
			return;
		divider = 0;
		Atmos714* n = neighbour();
		linked = (n != nullptr);
		linkedTheme = n ? n->resolvedTheme() : -1;
	}

	int resolvedTheme() const {
		if (panelTheme >= 0)
			return panelTheme;
		if (linked && linkedTheme >= 0)
			return linkedTheme;
		return settings::preferDarkPanels ? st::theme::THEME_DARK : st::theme::THEME_DEFAULT;
	}

	void setSize(float w, float d, float h) {
		params[st::roomexp::W_PARAM].setValue(w);
		params[st::roomexp::D_PARAM].setValue(d);
		params[st::roomexp::H_PARAM].setValue(h);
	}

	// 換顯示單位：旋鈕的提示、右鍵輸入的數字都跟著換（輸入 250 cm 就是 2.5 m）
	void setUnit(int u) {
		unit = clamp(u, 0, st::roomexp::NUM_UNITS - 1);
		for (int i = st::roomexp::W_PARAM; i <= st::roomexp::H_PARAM; i++) {
			engine::ParamQuantity* pq = getParamQuantity(i);
			if (!pq)
				continue;
			pq->unit = st::roomexp::UNIT_SUFFIX[unit];
			pq->displayMultiplier = st::roomexp::UNIT_PER_METRE[unit];
		}
	}

	// 直接輸入比例「寬 : 深 : 高」，例如 "1:2:0.5"、"4 x 3 x 1.5"、"16 9 4"。回傳是否成功。
	// 保留目前的寬度，深和高依比例換算；超出旋鈕範圍時整組等比例縮放到範圍內。
	bool setRatioFromText(const std::string& text) {
		float r[3];
		if (!st::roomexp::parseRatio(text.c_str(), r))
			return false;
		float w = params[st::roomexp::W_PARAM].getValue();
		float size[3] = {w, w * r[1] / r[0], w * r[2] / r[0]};
		st::roomexp::fitRange(size);
		setSize(size[0], size[1], size[2]);
		return true;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "theme", json_integer(panelTheme));
		json_object_set_new(rootJ, "unit", json_integer(unit));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* j = json_object_get(rootJ, "theme");
		if (j)
			panelTheme = clamp((int) json_integer_value(j), -1, st::theme::NUM_THEMES - 1);
		j = json_object_get(rootJ, "unit");
		if (j)
			setUnit((int) json_integer_value(j));
	}

	void onReset() override {
		setUnit(st::roomexp::UNIT_M);
	}
};

static const st::theme::Palette& palOfRoom(IslandRoom* m) {
	return st::theme::palette(m ? m->resolvedTheme() : st::theme::THEME_DEFAULT);
}

// 數字去掉多餘的 0：10.0 → "10"、2.50 → "2.5"
static std::string trimNum(float v, int decimals) {
	std::string s = string::f("%.*f", decimals, v);
	if (s.find('.') != std::string::npos) {
		while (!s.empty() && s.back() == '0')
			s.pop_back();
		if (!s.empty() && s.back() == '.')
			s.pop_back();
	}
	return s;
}

// 旋鈕下方的數值（公尺）
struct RoomValue : TransparentWidget {
	IslandRoom* module = nullptr;
	int paramId = 0;

	void draw(const DrawArgs& args) override {
		std::shared_ptr<window::Font> font = stFont();
		if (!font)
			return;
		float v = module ? module->params[paramId].getValue()
		                 : (paramId == st::roomexp::H_PARAM ? st::roomexp::DEFAULT_H : st::roomexp::DEFAULT_W);
		const st::theme::Palette& pal = palOfRoom(module);
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 10.f);
		nvgTextLetterSpacing(args.vg, 0.3f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, th(pal.TEXT));
		int u = module ? module->unit : (int) st::roomexp::UNIT_M;
		std::string txt = trimNum(v * st::roomexp::UNIT_PER_METRE[u], st::roomexp::UNIT_DECIMALS[u]) + st::roomexp::UNIT_SUFFIX[u];
		nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, txt.c_str(), NULL);
	}
};

// 比例輸入欄（出現在選單裡）：輸入「寬 : 深 : 高」後按 Enter 套用
struct RoomRatioField : ui::TextField {
	IslandRoom* module = nullptr;

	RoomRatioField() {
		box.size.x = 150.f;
		multiline = false;
	}

	void onSelectKey(const SelectKeyEvent& e) override {
		if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
			if (module)
				module->setRatioFromText(text);
			ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
			if (overlay)
				overlay->requestDelete();
			e.consume(this);
		}
		if (!e.getTarget())
			ui::TextField::onSelectKey(e);
	}
};

static std::string roomRatioText(IslandRoom* module) {
	float w = st::roomexp::DEFAULT_W, d = st::roomexp::DEFAULT_D, h = st::roomexp::DEFAULT_H;
	if (module) {
		w = module->params[st::roomexp::W_PARAM].getValue();
		d = module->params[st::roomexp::D_PARAM].getValue();
		h = module->params[st::roomexp::H_PARAM].getValue();
	}
	return "1 : " + trimNum(d / w, 2) + " : " + trimNum(h / w, 2);
}

// 在選單裡放一個比例輸入欄（比例顯示窗點一下、或右鍵選單都會用到）
static void addRatioEntry(ui::Menu* menu, IslandRoom* module) {
	menu->addChild(createMenuLabel("Ratio  W : D : H  (type, then Enter)"));
	RoomRatioField* f = new RoomRatioField();
	f->module = module;
	f->text = roomRatioText(module);
	menu->addChild(f);
	f->selectAll();
	APP->event->setSelectedWidget(f);
}

// 比例顯示窗：寬 : 深 : 高（以寬為 1）。點一下可以直接輸入比例。
struct RoomRatioDisplay : OpaqueWidget {
	IslandRoom* module = nullptr;

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			addRatioEntry(createMenu(), module);
			e.consume(this);
		}
	}

	void draw(const DrawArgs& args) override {
		const st::theme::Palette& pal = palOfRoom(module);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, th(pal.DISPLAY_BG));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, th(pal.BORDER, 80));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStroke(args.vg);
		std::shared_ptr<window::Font> font = stFont();
		if (!font)
			return;
		std::string txt = roomRatioText(module);
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 9.5f);
		nvgTextLetterSpacing(args.vg, 0.2f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, th(pal.DISPLAY_TEXT));
		nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f + 0.5f, txt.c_str(), NULL);
	}
};

// 房間預覽：上面是俯視圖（寬 × 深，前方在上），下面是側視圖（深 × 高，前方在右）。兩張圖用同一個比例尺。
struct RoomPreview : TransparentWidget {
	IslandRoom* module = nullptr;

	void draw(const DrawArgs& args) override {
		const st::theme::Palette& pal = palOfRoom(module);
		NVGcontext* vg = args.vg;
		nvgBeginPath(vg);
		nvgRect(vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(vg, th(pal.CUBE_BG));
		nvgFill(vg);
		nvgStrokeColor(vg, th(pal.CUBE_BORDER));
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);

		float w = st::roomexp::DEFAULT_W, d = st::roomexp::DEFAULT_D, h = st::roomexp::DEFAULT_H;
		bool round = false;
		if (module) {
			w = module->params[st::roomexp::W_PARAM].getValue();
			d = module->params[st::roomexp::D_PARAM].getValue();
			h = module->params[st::roomexp::H_PARAM].getValue();
			round = module->params[st::roomexp::ORBIT_PARAM].getValue() > 0.5f;
		}
		// 版面：俯視圖區 (topW × topH)、側視圖區 (sideW × sideH)，中間留一條縫
		const float pad = 7.f, gap = 9.f;
		float areaW = box.size.x - 2.f * pad;
		float areaH = box.size.y - 2.f * pad - gap;
		float sc = std::min(areaW / std::max(w, d), areaH / (d + h));   // 每公尺幾個像素
		float tw = w * sc, td = d * sc, sh = h * sc;
		float total = td + gap + sh;
		float y0 = (box.size.y - total) / 2.f;
		float cx = box.size.x / 2.f;

		// 俯視圖
		nvgBeginPath(vg);
		nvgRect(vg, cx - tw / 2.f, y0, tw, td);
		nvgStrokeColor(vg, th(pal.CUBE_EDGE));
		nvgStrokeWidth(vg, 1.2f);
		nvgStroke(vg);
		// 前方（舞台）那一邊用品牌綠粗線標出
		nvgBeginPath(vg);
		nvgMoveTo(vg, cx - tw / 2.f, y0);
		nvgLineTo(vg, cx + tw / 2.f, y0);
		nvgStrokeColor(vg, th(st::theme::BRAND_));
		nvgStrokeWidth(vg, 2.2f);
		nvgStroke(vg);
		// 繞場的形狀
		float ccy = y0 + td / 2.f;
		float rx = tw / 2.f * 0.9f, ry = td / 2.f * 0.9f;
		if (round)
			rx = ry = std::min(rx, ry);
		nvgBeginPath(vg);
		nvgEllipse(vg, cx, ccy, rx, ry);
		nvgStrokeColor(vg, th(pal.CUBE_FLOOR, 200));
		nvgStrokeWidth(vg, 0.9f);
		nvgStroke(vg);
		// 中心（聽者位置）
		nvgBeginPath(vg);
		nvgCircle(vg, cx, ccy, 1.4f);
		nvgFillColor(vg, th(pal.CUBE_TEXT, 200));
		nvgFill(vg);

		// 側視圖：深 × 高，底邊是耳朵高度
		float sy = y0 + td + gap;
		nvgBeginPath(vg);
		nvgRect(vg, cx - td / 2.f, sy, td, sh);
		nvgStrokeColor(vg, th(pal.SPEAKER_TOP));
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);
		nvgBeginPath(vg);
		nvgMoveTo(vg, cx + td / 2.f, sy);
		nvgLineTo(vg, cx + td / 2.f, sy + sh);
		nvgStrokeColor(vg, th(st::theme::BRAND_));
		nvgStrokeWidth(vg, 2.2f);
		nvgStroke(vg);
	}
};

// ORBIT 形狀按鈕：FIT / ROUND（平面方塊，ROUND 時亮品牌綠）
struct RoomOrbitButton : app::Switch {
	IslandRoom* module = nullptr;

	RoomOrbitButton() {
		box.size = Vec(46.f, 14.f);
	}

	void draw(const DrawArgs& args) override {
		engine::ParamQuantity* pq = getParamQuantity();
		bool round = pq && pq->getValue() > 0.5f;
		const st::theme::Palette& pal = palOfRoom(module);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f);
		nvgFillColor(args.vg, round ? th(st::theme::BRAND_) : th(pal.BUTTON_OFF));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, th(pal.BUTTON_EDGE));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStroke(args.vg);
		std::shared_ptr<window::Font> font = stFontBold();
		if (!font)
			return;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 8.5f);
		nvgTextLetterSpacing(args.vg, 1.f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, round ? th(st::theme::PAPER_) : th(pal.BUTTON_TEXT));
		nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f + 0.5f, round ? "ROUND" : "FIT", NULL);
	}
};

// LINK 指示：緊貼著 The Island 時亮品牌綠
struct RoomLinkLed : TransparentWidget {
	IslandRoom* module = nullptr;

	void draw(const DrawArgs& args) override {
		const st::theme::Palette& pal = palOfRoom(module);
		bool on = module && module->linked;
		float r = box.size.x / 2.f;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, r, r, r - 0.5f);
		nvgFillColor(args.vg, on ? th(st::theme::BRAND_) : th(pal.LED_BASE));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, th(pal.LED_RING));
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStroke(args.vg);
	}
};

struct IslandRoomWidget : ModuleWidget {
	int shownTheme = -1;   // -1：第一次 step 一定會載入目前主題的面板底圖

	void label(float x, float y, const std::string& text, float size, StLabel::Kind kind, IslandRoom* module) {
		StLabel* l = new StLabel(Vec(x, y), text, size, kind, nullptr);
		l->themeFn = [=]() { return module ? module->resolvedTheme() : (int) st::theme::THEME_DEFAULT; };
		addChild(l);
	}

	IslandRoomWidget(IslandRoom* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, std::string("res/TheIslandRoom") + st::theme::palette(st::theme::THEME_DEFAULT).svgSuffix + ".svg")));

		using namespace st::layout::room;
		label(CX, TITLE_Y, "ROOM", 11.f, StLabel::TITLE, module);
		label(CX, BAND_Y, "EXPANDER", 7.f, StLabel::BAND, module);

		RoomPreview* pv = new RoomPreview();
		pv->module = module;
		pv->box.pos = Vec(PREVIEW_X, PREVIEW_Y);
		pv->box.size = Vec(PREVIEW_W, PREVIEW_H);
		addChild(pv);

		static const char* const names[3] = {"WIDTH", "DEPTH", "HEIGHT"};
		for (int i = 0; i < 3; i++) {
			float y = KNOB_Y0 + KNOB_DY * i;
			label(CX, y - KNOB_LABEL_DY, names[i], 7.f, StLabel::NORMAL, module);
			addParam(createParamCentered<RoundBlackKnob>(Vec(CX, y), module, st::roomexp::W_PARAM + i));
			RoomValue* v = new RoomValue();
			v->module = module;
			v->paramId = st::roomexp::W_PARAM + i;
			v->box.size = Vec(60.f, 12.f);
			v->box.pos = Vec(CX - 30.f, y + KNOB_VALUE_DY - 6.f);
			addChild(v);
		}

		label(CX, RATIO_LABEL_Y, "RATIO  W : D : H", 6.5f, StLabel::NORMAL, module);
		RoomRatioDisplay* rd = new RoomRatioDisplay();
		rd->module = module;
		rd->box.size = Vec(RATIO_W, RATIO_H);
		rd->box.pos = Vec(CX - RATIO_W / 2.f, RATIO_Y - RATIO_H / 2.f);
		addChild(rd);

		label(CX, ORBIT_LABEL_Y, "ORBIT", 7.f, StLabel::NORMAL, module);
		RoomOrbitButton* ob = createParamCentered<RoomOrbitButton>(Vec(CX, ORBIT_Y), module, st::roomexp::ORBIT_PARAM);
		ob->module = module;
		addParam(ob);

		label(LINK_LABEL_X, LINK_Y, "LINK", 7.f, StLabel::NORMAL, module);
		RoomLinkLed* led = new RoomLinkLed();
		led->module = module;
		led->box.size = Vec(8.f, 8.f);
		led->box.pos = Vec(LINK_LED_X - 4.f, LINK_Y - 4.f);
		addChild(led);
	}

	void step() override {
		IslandRoom* m = getModule<IslandRoom>();
		int t = m ? m->resolvedTheme() : st::theme::THEME_DEFAULT;
		if (t != shownTheme) {
			shownTheme = t;
			app::SvgPanel* sp = dynamic_cast<app::SvgPanel*>(getPanel());
			if (sp) {
				std::string path = asset::plugin(pluginInstance, std::string("res/TheIslandRoom") + st::theme::palette(t).svgSuffix + ".svg");
				if (system::isFile(path))
					sp->setBackground(window::Svg::load(path));
			}
		}
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		IslandRoom* module = getModule<IslandRoom>();
		if (!module)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(module->linked ? "Linked to The Island" : "Not linked: place right next to The Island"));

		// 顯示單位
		menu->addChild(createSubmenuItem("Units", st::roomexp::UNIT_NAMES[module->unit], [=](Menu* sub) {
			for (int u = 0; u < st::roomexp::NUM_UNITS; u++)
				sub->addChild(createCheckMenuItem(st::roomexp::UNIT_NAMES[u], "",
					[=]() { return module->unit == u; }, [=]() { module->setUnit(u); }));
		}));

		// 直接輸入比例
		menu->addChild(createSubmenuItem("Enter ratio  W : D : H", roomRatioText(module), [=](Menu* sub) {
			addRatioEntry(sub, module);
		}));

		// 常用比例：直接套用一組尺寸
		menu->addChild(createSubmenuItem("Room presets", "", [=](Menu* sub) {
			struct Preset { const char* name; float w, d, h; };
			static const Preset presets[] = {
				{"Cube 1 : 1 : 1 (same as no expander)", 10.f, 10.f, 5.f},
				{"Long room 1 : 2 (10 x 20 x 5 m)", 10.f, 20.f, 5.f},
				{"Wide room 2 : 1 (20 x 10 x 5 m)", 20.f, 10.f, 5.f},
				{"Low ceiling (10 x 10 x 2.5 m)", 10.f, 10.f, 2.5f},
				{"High ceiling (10 x 10 x 8 m)", 10.f, 10.f, 8.f},
				{"Small studio (4 x 5 x 1.5 m)", 4.f, 5.f, 1.5f},
			};
			for (const Preset& pr : presets) {
				float w = pr.w, d = pr.d, h = pr.h;
				sub->addChild(createMenuItem(pr.name, "", [=]() { module->setSize(w, d, h); }));
			}
		}));

		static const char* const THEME_LABELS[st::theme::NUM_THEMES] = {"Sky (light blue)", "Mist (grey-green)", "Sand (warm beige)", "Dark (black)"};
		std::string cur = module->panelTheme < 0 ? "Follow The Island" : st::theme::palette(module->panelTheme).name;
		menu->addChild(createSubmenuItem("Panel theme", cur, [=](Menu* sub) {
			sub->addChild(createCheckMenuItem("Follow The Island", "",
				[=]() { return module->panelTheme < 0; }, [=]() { module->panelTheme = -1; }));
			for (int t = 0; t < st::theme::NUM_THEMES; t++)
				sub->addChild(createCheckMenuItem(THEME_LABELS[t], "",
					[=]() { return module->panelTheme == t; }, [=]() { module->panelTheme = t; }));
		}));
	}
};

Model* modelIslandRoom = createModel<IslandRoom, IslandRoomWidget>("TheIslandRoom");   // 模組代號（上架後永遠不能改）
