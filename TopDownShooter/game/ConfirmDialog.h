#pragma once
#include <array>
#include <memory>
#include "Mathutility.h"
#include "Sprite.h"

class SpriteCommon;
class Input;

// 「はい／いいえ」の確認画面（出口の小部屋の階段・転送陣に触れたときに出す）
// 左右で選んで、スペース・Ⓐ・クリックで決定。Esc・Ⓑ は「いいえ」
class ConfirmDialog
{
public:
	// 何を聞くか（文の画像が変わる）
	enum class Kind {
		kNextFloor, // 次の層へ進みますか？
		kReturn,    // この層で帰還しますか？
	};
	// 答え
	enum class Answer {
		kNone, // まだ答えていない
		kYes,
		kNo,
	};

	// 使う画像（シーンで読み込む）
	static inline const char* kTexturePanel = "resources/confirm_panel.png";
	static inline const char* kTextureMessageNext = "resources/confirm_msg_next.png";
	static inline const char* kTextureMessageReturn = "resources/confirm_msg_return.png";
	static inline const char* kTextureYes = "resources/confirm_yes.png";
	static inline const char* kTextureNo = "resources/confirm_no.png";
	static inline const char* kTextureHint = "resources/confirm_hint.png";

	void Initialize(SpriteCommon* spriteCommon);
	// 開く（defaultYes：最初に「はい」を選んでおくか）
	void Open(Kind kind, bool defaultYes);
	bool IsOpen() const { return isOpen; }
	Kind GetKind() const { return kind; }
	// 更新。答えたフレームだけ kYes / kNo を返す
	Answer Update(Input* input, float deltaTime);
	void Draw();

private:
	// ボタンの真ん中（0：はい、1：いいえ）
	Vector2 GetButtonCenter(size_t index) const;

	std::unique_ptr<Sprite> background;
	std::unique_ptr<Sprite> panel;
	std::unique_ptr<Sprite> message;
	std::array<std::unique_ptr<Sprite>, 2> buttons; // はい・いいえ
	std::array<float, 2> buttonLift = {};          // 選ばれているときの大きさ（なめらかに変える）
	std::unique_ptr<Sprite> hint;

	Kind kind = Kind::kNextFloor;
	bool isOpen = false;
	size_t selected = 0; // 0：はい、1：いいえ
	float openTime = 0.0f;
	bool wasStickTilted = false;
	Vector2 lastMouse = { 0.0f, 0.0f };
	bool hasLastMouse = false;

	// ---- 見た目の数値（ピクセル）----
	static inline const float kPanelWidth = 640.0f;
	static inline const float kPanelHeight = 300.0f;
	static inline const float kButtonWidth = 200.0f;
	static inline const float kButtonHeight = 70.0f;
	static inline const float kButtonSpacing = 260.0f; // ボタンの真ん中どうしの間
	static inline const float kInputDelay = 0.35f;     // 開いてから決定できるまで（秒）
};
