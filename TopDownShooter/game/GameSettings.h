#pragma once
#include <Windows.h>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include "Input.h"

// 日本語の文字列を、ImGuiに渡せる形（UTF-8のconst char*）にする（C++20のu8リテラルはchar8_tなので変換が要る）
#define JP(text) reinterpret_cast<const char*>(u8##text)

// ゲームの操作の種類（キー配置を変えられる操作）
enum class GameAction : int32_t {
	kMoveUp,
	kMoveDown,
	kMoveLeft,
	kMoveRight,
	kRoll,
	kInteract,
	kCount,
};

// ゲームの設定（キー配置と音量）。settings.ini に保存して、次に起動したときも使う
// 他のクラスは、キーを直接（DIK_W など）見ずに、ここから「この操作のキー」を聞く
// メニューの決定（スペース）・選択（矢印・A/D）・ポーズ（Esc）は固定で、変えられない
class GameSettings
{
public:
	static GameSettings* GetInstance()
	{
		static GameSettings instance;
		return &instance;
	}

	// この操作に割り当てているキー（DIK_*）
	BYTE GetKey(GameAction action) const { return keys[static_cast<size_t>(action)]; }
	bool Push(Input* input, GameAction action) const { return input->PushKey(GetKey(action)); }
	bool Trigger(Input* input, GameAction action) const { return input->TriggerKey(GetKey(action)); }

	// キーを割り当てる。ほかの操作が同じキーを使っていたら、そちらにこの操作の元のキーを渡して入れかえる
	void SetKey(GameAction action, BYTE key)
	{
		size_t index = static_cast<size_t>(action);
		BYTE old = keys[index];
		for (size_t i = 0; i < kActionCount; ++i) {
			if (i != index && keys[i] == key) {
				keys[i] = old;
			}
		}
		keys[index] = key;
	}

	// 初期設定のキー
	static BYTE DefaultKey(GameAction action)
	{
		switch (action) {
		case GameAction::kMoveUp: return DIK_W;
		case GameAction::kMoveDown: return DIK_S;
		case GameAction::kMoveLeft: return DIK_A;
		case GameAction::kMoveRight: return DIK_D;
		case GameAction::kRoll: return DIK_SPACE;
		case GameAction::kInteract: return DIK_E;
		default: return 0;
		}
	}
	void ResetKeys()
	{
		for (size_t i = 0; i < kActionCount; ++i) {
			keys[i] = DefaultKey(static_cast<GameAction>(i));
		}
	}

	// 操作の名前（画面に出す）
	static const char* ActionLabel(GameAction action)
	{
		switch (action) {
		case GameAction::kMoveUp: return JP("上へ移動");
		case GameAction::kMoveDown: return JP("下へ移動");
		case GameAction::kMoveLeft: return JP("左へ移動");
		case GameAction::kMoveRight: return JP("右へ移動");
		case GameAction::kRoll: return JP("ロール（回避）");
		case GameAction::kInteract: return JP("調べる・使う");
		default: return "";
		}
	}

	// キーの名前（画面に出す。英数字だけの名前にして、文字画像に入れる文字を少なくする）
	static std::string KeyName(BYTE key)
	{
		struct Entry { BYTE key; const char* name; };
		static const Entry kNames[] = {
			{ DIK_ESCAPE, "Esc" }, { DIK_1, "1" }, { DIK_2, "2" }, { DIK_3, "3" }, { DIK_4, "4" }, { DIK_5, "5" },
			{ DIK_6, "6" }, { DIK_7, "7" }, { DIK_8, "8" }, { DIK_9, "9" }, { DIK_0, "0" }, { DIK_MINUS, "-" },
			{ DIK_EQUALS, "=" }, { DIK_BACK, "Backspace" }, { DIK_TAB, "Tab" },
			{ DIK_Q, "Q" }, { DIK_W, "W" }, { DIK_E, "E" }, { DIK_R, "R" }, { DIK_T, "T" }, { DIK_Y, "Y" },
			{ DIK_U, "U" }, { DIK_I, "I" }, { DIK_O, "O" }, { DIK_P, "P" }, { DIK_LBRACKET, "[" }, { DIK_RBRACKET, "]" },
			{ DIK_RETURN, "Enter" }, { DIK_LCONTROL, "Ctrl" }, { DIK_A, "A" }, { DIK_S, "S" }, { DIK_D, "D" },
			{ DIK_F, "F" }, { DIK_G, "G" }, { DIK_H, "H" }, { DIK_J, "J" }, { DIK_K, "K" }, { DIK_L, "L" },
			{ DIK_SEMICOLON, ";" }, { DIK_APOSTROPHE, "'" }, { DIK_GRAVE, "`" }, { DIK_LSHIFT, "Shift" },
			{ DIK_BACKSLASH, "\\" }, { DIK_Z, "Z" }, { DIK_X, "X" }, { DIK_C, "C" }, { DIK_V, "V" }, { DIK_B, "B" },
			{ DIK_N, "N" }, { DIK_M, "M" }, { DIK_COMMA, "," }, { DIK_PERIOD, "." }, { DIK_SLASH, "/" },
			{ DIK_RSHIFT, "R-Shift" }, { DIK_MULTIPLY, "Num*" }, { DIK_LMENU, "Alt" }, { DIK_SPACE, "Space" },
			{ DIK_CAPITAL, "CapsLock" }, { DIK_F1, "F1" }, { DIK_F2, "F2" }, { DIK_F3, "F3" }, { DIK_F4, "F4" },
			{ DIK_F5, "F5" }, { DIK_F6, "F6" }, { DIK_F7, "F7" }, { DIK_F8, "F8" }, { DIK_F9, "F9" }, { DIK_F10, "F10" },
			{ DIK_NUMLOCK, "NumLock" }, { DIK_SCROLL, "ScrollLock" },
			{ DIK_NUMPAD7, "Num7" }, { DIK_NUMPAD8, "Num8" }, { DIK_NUMPAD9, "Num9" }, { DIK_SUBTRACT, "Num-" },
			{ DIK_NUMPAD4, "Num4" }, { DIK_NUMPAD5, "Num5" }, { DIK_NUMPAD6, "Num6" }, { DIK_ADD, "Num+" },
			{ DIK_NUMPAD1, "Num1" }, { DIK_NUMPAD2, "Num2" }, { DIK_NUMPAD3, "Num3" }, { DIK_NUMPAD0, "Num0" },
			{ DIK_DECIMAL, "Num." }, { DIK_F11, "F11" }, { DIK_F12, "F12" }, { DIK_NUMPADENTER, "Num-Enter" },
			{ DIK_RCONTROL, "R-Ctrl" }, { DIK_DIVIDE, "Num/" }, { DIK_RMENU, "R-Alt" }, { DIK_HOME, "Home" },
			{ DIK_UP, "Up" }, { DIK_PRIOR, "PageUp" }, { DIK_LEFT, "Left" }, { DIK_RIGHT, "Right" }, { DIK_END, "End" },
			{ DIK_DOWN, "Down" }, { DIK_NEXT, "PageDown" }, { DIK_INSERT, "Insert" }, { DIK_DELETE, "Delete" },
		};
		if (key == 0) {
			return "-";
		}
		for (const Entry& entry : kNames) {
			if (entry.key == key) {
				return entry.name;
			}
		}
		return "Key " + std::to_string(static_cast<int>(key));
	}

	// 音量（0.0〜1.0）
	float GetMasterVolume() const { return masterVolume; }
	void SetMasterVolume(float volume) { masterVolume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume); }

	// 保存と読み込み（1行に「名前=数字」）
	void Save() const
	{
		std::ofstream file(kFileName);
		if (!file) {
			return;
		}
		for (size_t i = 0; i < kActionCount; ++i) {
			file << "key" << i << "=" << static_cast<int>(keys[i]) << "\n";
		}
		file << "volume=" << masterVolume << "\n";
	}
	void Load()
	{
		std::ifstream file(kFileName);
		if (!file) {
			return;
		}
		std::string line;
		while (std::getline(file, line)) {
			size_t eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			std::string name = line.substr(0, eq);
			std::string value = line.substr(eq + 1);
			try {
				if (name == "volume") {
					SetMasterVolume(std::stof(value));
				} else if (name.rfind("key", 0) == 0) {
					size_t index = static_cast<size_t>(std::stoi(name.substr(3)));
					int key = std::stoi(value);
					if (index < kActionCount && key > 0 && key < 256) {
						keys[index] = static_cast<BYTE>(key);
					}
				}
			} catch (...) {
				// 壊れた行は無視する
			}
		}
		// 同じキーが2つの操作に割り当たっていたら、初期設定に戻す（壊れた設定で動けなくならないように）
		for (size_t i = 0; i < kActionCount; ++i) {
			for (size_t j = i + 1; j < kActionCount; ++j) {
				if (keys[i] == keys[j]) {
					ResetKeys();
					return;
				}
			}
		}
	}

private:
	GameSettings()
	{
		ResetKeys();
		Load();
	}

	static constexpr size_t kActionCount = static_cast<size_t>(GameAction::kCount);
	static inline const char* kFileName = "settings.ini";
	BYTE keys[kActionCount] = {};
	float masterVolume = 1.0f;
};
