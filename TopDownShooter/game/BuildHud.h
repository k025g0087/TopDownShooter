#pragma once
#include <array>
#include <memory>
#include <string>
#include "Mathutility.h"
#include "Sprite.h"
#include "BuildSystem.h"

class SpriteCommon;

// ビルドの表示（左上のハートの下。ステータスごとのアイコンと「+○○%」。伸びたときは「○○UP!」が浮かぶ）
class BuildHud
{
public:
	// 使う画像（シーンで読み込む）
	static inline const char* kTextureDigits = "resources/hud_digits.png"; // 「0123456789+%」が横に並んだ画像
	static const char* GetIconTexture(GrowthStat stat);
	static const char* GetPopupTexture(GrowthStat stat);

	void Initialize(SpriteCommon* spriteCommon);
	void Update(const BuildSystem& build, float deltaTime);
	void Draw();

	// ステータスが伸びた（その行を光らせて、「○○UP!」を浮かべる）
	void OnGrowth(GrowthStat stat);

private:
	// 数字の画像の中の、1文字の大きさと並び
	static inline const float kGlyphWidth = 20.0f;
	static inline const float kGlyphHeight = 28.0f;
	static inline const char* kGlyphOrder = "0123456789+%";
	static inline const size_t kMaxDigits = 5; // 「+100%」まで

	static inline const size_t kStatCount = static_cast<size_t>(GrowthStat::kCount);

	// 1行分（アイコンと数字）
	struct Row {
		std::unique_ptr<Sprite> icon;
		std::array<std::unique_ptr<Sprite>, kMaxDigits> digits;
		size_t digitCount = 0;
		float flashTimer = 0.0f; // 伸びたときに光る残り時間
	};
	std::array<Row, kStatCount> rows;

	// 浮かぶ「○○UP!」（いくつか同時に出せるように使い回す）
	struct Popup {
		std::unique_ptr<Sprite> sprite;
		float timer = 0.0f; // 残り時間（0なら使っていない）
		size_t row = 0;
	};
	std::array<Popup, 6> popups;

	// 文字列を数字の画像で並べる
	void SetText(Row& row, const std::string& text, float x, float y, const Vector4& color);

	// ---- 見た目の数値（ピクセル）----
	static inline const float kLeft = 20.0f;
	static inline const float kTop = 66.0f;
	static inline const float kRowHeight = 36.0f;
	static inline const float kIconSize = 30.0f;
	static inline const float kDigitScale = 0.75f;   // 数字の表示の大きさ（画像に対する倍率）
	static inline const float kFlashTime = 0.5f;
	static inline const float kPopupTime = 1.1f;
};
