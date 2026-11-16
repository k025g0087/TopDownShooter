#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "Mathutility.h"
#include "Sprite.h"
#include "RunRecord.h"

class SpriteCommon;

// リザルト画面（帰ったとき・力尽きたとき）
// 到達階層・タイム・倒した敵・集めたコインと、ベスト記録を出す
class ResultScreen
{
public:
	// 今回の結果
	struct Result {
		bool returned = false;  // true：帰還　false：力尽きた
		int32_t floor = 1;      // 到達階層（1から）
		float seconds = 0.0f;   // 遊んだ時間
		int32_t kills = 0;
		int32_t coins = 0;
	};

	// 使う画像（シーンで読み込む）
	static inline const char* kTextureTitleReturn = "resources/result_title_return.png";
	static inline const char* kTextureTitleDead = "resources/result_title_dead.png";
	static inline const char* kTextureLabelFloor = "resources/result_label_floor.png";
	static inline const char* kTextureLabelTime = "resources/result_label_time.png";
	static inline const char* kTextureLabelKills = "resources/result_label_kills.png";
	static inline const char* kTextureLabelCoins = "resources/result_label_coins.png";
	static inline const char* kTextureLabelBest = "resources/result_label_best.png";
	static inline const char* kTextureNewRecord = "resources/result_new.png";
	static inline const char* kTextureHint = "resources/result_hint.png";
	static inline const char* kTextureDigits = "resources/result_digits.png"; // 「0123456789:F」

	void Initialize(SpriteCommon* spriteCommon);
	// 画面を開く（best：ベスト記録。isNewRecord：今回ベストを更新したか）
	void Open(const Result& result, const RunRecord::Entry& best, bool isNewRecord);
	bool IsOpen() const { return isOpen; }
	// 入力を受けてよいか（開いてすぐは受けない）
	bool CanAcceptInput() const { return isOpen && openTime >= kInputDelay; }
	void Update(float deltaTime);
	void Draw();

	// 秒を「mm:ss」にする
	static std::string FormatTime(float seconds);

private:
	// 数字の並び1つ（「12F」「03:25」など）
	struct NumberText {
		std::vector<std::unique_ptr<Sprite>> glyphs;
		std::string text;
	};
	// 1行（左に見出し、右に数字）
	struct Row {
		std::unique_ptr<Sprite> label;
		NumberText value;
	};

	void SetText(NumberText& number, const std::string& text);
	// 数字を並べる（right：右端の位置）
	void LayoutText(NumberText& number, const Vector2& right, float scale, const Vector4& color);

	SpriteCommon* spriteCommon = nullptr;
	std::unique_ptr<Sprite> background;
	std::unique_ptr<Sprite> panel;
	std::unique_ptr<Sprite> title;
	std::unique_ptr<Sprite> newRecord;
	std::unique_ptr<Sprite> hint;
	static inline const size_t kRowCount = 5; // 到達階層・タイム・倒した敵・コイン・ベスト
	std::array<Row, kRowCount> rows;

	bool isOpen = false;
	bool isNewRecord = false;
	bool returned = false;
	float openTime = 0.0f;

	// ---- 見た目の数値（ピクセル）----
	static inline const float kGlyphWidth = 32.0f;
	static inline const float kGlyphHeight = 48.0f;
	static inline const float kPanelWidth = 620.0f;
	static inline const float kRowTop = 260.0f;
	static inline const float kRowSpacing = 66.0f;
	static inline const float kInputDelay = 1.0f; // 開いてから入力を受けるまで（秒）
};
