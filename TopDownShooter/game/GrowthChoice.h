#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "Mathutility.h"
#include "Sprite.h"
#include "BuildSystem.h"

class SpriteCommon;
class Input;

// 部屋をクリアしたときの「力を取り込む」画面
// その部屋で倒した敵の種類がカードで並び、1枚だけ選べる（ローグライクの強化選び）
class GrowthChoice
{
public:
	// 選択肢1つ
	struct Option {
		GrowthStat stat = GrowthStat::kMoveSpeed;
		int32_t amount = 0;   // 取り込む数（その部屋で倒した数）
		int32_t percent = 0;  // 何％伸びるか
	};

	// 使う画像（シーンで読み込む）
	static inline const char* kTextureTitle = "resources/choice_title.png";
	static inline const char* kTextureHint = "resources/choice_hint.png";
	static const char* GetCardTexture(GrowthStat stat);

	void Initialize(SpriteCommon* spriteCommon);
	// 画面を開く（options は1〜4個）
	void Open(const std::vector<Option>& options);
	bool IsOpen() const { return isOpen; }
	// 更新。決定したフレームだけ、選んだ選択肢の番号を返す（それ以外は -1）
	int32_t Update(Input* input, float deltaTime);
	void Draw();

	const Option& GetOption(size_t index) const { return options[index]; }

private:
	static inline const size_t kMaxOptions = static_cast<size_t>(GrowthStat::kCount);
	static inline const size_t kMaxDigits = 5;

	// カード1枚分（背景の絵と「+○○%」の数字）
	struct Card {
		std::unique_ptr<Sprite> sprite;
		std::array<std::unique_ptr<Sprite>, kMaxDigits> digits;
		size_t digitCount = 0;
		float lift = 0.0f; // 選ばれているときに上に浮く量（なめらかに変える）
	};
	std::array<Card, kMaxOptions> cards;
	std::vector<Option> options;

	std::unique_ptr<Sprite> background;
	std::unique_ptr<Sprite> title;
	std::unique_ptr<Sprite> hint;

	bool isOpen = false;
	size_t selected = 0;
	float openTime = 0.0f;     // 開いてからの時間（すぐには決定できないようにする）
	bool wasStickTilted = false;
	// マウスが動いたかを見るための、前のフレームの位置
	Vector2 lastMouse = { 0.0f, 0.0f };
	bool hasLastMouse = false;

	// カードの位置（真ん中）
	Vector2 GetCardCenter(size_t index) const;
	void UpdateCard(size_t index);

	// ---- 見た目の数値（ピクセル）----
	static inline const float kCardWidth = 240.0f;
	static inline const float kCardHeight = 300.0f;
	static inline const float kCardSpacing = 270.0f;
	static inline const float kCardCenterY = 390.0f;
	static inline const float kGlyphWidth = 20.0f;   // 数字の画像（BuildHud と同じもの）
	static inline const float kGlyphHeight = 28.0f;
	static inline const float kDigitScale = 1.6f;
	static inline const float kInputDelay = 0.4f;    // 開いてから決定できるまで（秒）
};
