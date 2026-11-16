#pragma once
#include <array>
#include <functional>
#include <memory>
#include "Mathutility.h"
#include "Sprite.h"
#include "BlendMode.h"
#include "ItemData.h"
#include "CharacterData.h"

class SpriteCommon;
class Input;

// ガチャを回したときの、画面全体を使った演出
// 暗くなる → カプセルが落ちてくる → ガタガタ揺れる → パカッと開いて光る → アイテムが出てくる
// 開く前に決定ボタンを押すと、開くところまで飛ばせる
class GachaEffect
{
public:
	// 使う画像（シーンで読み込む）
	static inline const char* kTextureRays = "resources/gacha_rays.png";
	static inline const char* kTextureGlow = "resources/gacha_glow.png";
	static inline const char* kTextureStar = "resources/gacha_star.png";
	static inline const char* kTextureCapsuleTop = "resources/gacha_capsule_top.png";
	static inline const char* kTextureCapsuleBottom = "resources/gacha_capsule_bottom.png";
	static inline const char* kTextureHint = "resources/gacha_hint.png";
	// アイテムごとの大きなアイコンと説明
	static const char* GetBigIconTexture(ItemType type);
	static const char* GetDescriptionTexture(ItemType type);

	void Initialize(SpriteCommon* spriteCommon);
	// 演出を始める（item：出てくるアイテム）
	void Start(ItemType item);
	// シークレット：虹色のカプセルから伝説の武器が出る演出を始める
	void StartLegendary(CharacterType character);
	bool IsPlaying() const { return isPlaying; }
	bool IsLegendary() const { return legendary; }
	ItemType GetItem() const { return item; }
	// 更新。演出が終わった（閉じた）フレームだけ true を返す
	bool Update(Input* input, float deltaTime);
	// 描画（setBlend：スプライトのブレンドモードを切りかえる関数。光るところは加算で描く）
	void Draw(const std::function<void(BlendMode)>& setBlend);

private:
	// 飛び散る星1つ
	struct Star {
		std::unique_ptr<Sprite> sprite;
		Vector2 velocity = { 0.0f, 0.0f };
		float spin = 0.0f;
		float size = 0.0f;
	};

	void UpdateCapsule();
	void UpdateReveal();
	// 星を真ん中から飛び散らせる
	void BurstStars();

	std::unique_ptr<Sprite> background;    // 画面全体を暗くする板
	std::unique_ptr<Sprite> rays;          // 回る光の筋（加算）
	std::unique_ptr<Sprite> glow;          // ぼんやりした光（加算）
	std::unique_ptr<Sprite> capsuleTop;    // カプセルの上半分（白っぽい半透明）
	std::unique_ptr<Sprite> capsuleBottom; // カプセルの下半分（アイテムの色）
	std::unique_ptr<Sprite> icon;          // 出てきたアイテムの大きなアイコン
	std::unique_ptr<Sprite> getText;       // 「○○ を手に入れた！」
	std::unique_ptr<Sprite> description;   // アイテムの説明
	std::unique_ptr<Sprite> hint;          // 「スペース・Ⓐ・クリック：つぎへ」
	std::unique_ptr<Sprite> flash;         // 開いた瞬間の白い光（加算）
	std::unique_ptr<Sprite> secret;        // シークレットのときの「SECRET!!」
	static inline const size_t kStarCount = 14;
	std::array<Star, kStarCount> stars;

	// 共通の始め方（アイコン・文字・説明の画像を決めてから呼ぶ）
	void Begin();

	bool isPlaying = false;
	bool legendary = false; // シークレット（伝説の武器）か
	ItemType item = ItemType::kBounce;
	Vector4 itemColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float time = 0.0f;
	bool opened = false; // カプセルが開いたか（星を1回だけ出すため）

	// ---- 時間（秒）----
	static inline const float kDropStart = 0.15f;  // カプセルが落ち始める
	static inline const float kDropEnd = 0.65f;    // 落ちきる
	static inline const float kOpenTime = 1.75f;   // パカッと開く（それまではガタガタ揺れる）
	static inline const float kCloseDelay = 2.5f;  // これより後なら、決定で閉じられる
	// ---- 位置・大きさ（ピクセル）----
	static inline const float kCenterY = 320.0f;
	static inline const float kCapsuleWidth = 240.0f;
	static inline const float kCapsuleHalfHeight = 120.0f;
	static inline const float kIconSize = 200.0f;
};
