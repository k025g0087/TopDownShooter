#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "Mathutility.h"
#include "Sprite.h"
#include "ItemData.h"

class SpriteCommon;

// 持ち物の表示
//   コインの枚数（左、ビルド表示の下）
//   アイテム（左下に手に入れた順に並べる。重ねたら「x2」）
//   ガチャで出たアイテムの「○○を手に入れた！」（画面の真ん中）
class InventoryHud
{
public:
	static inline const char* kTextureCoin = "resources/hud_coin.png";
	static inline const char* kTextureDigits = "resources/hud_digits.png"; // 「0123456789+%:x」

	void Initialize(SpriteCommon* spriteCommon);
	void Reset();
	void Update(int32_t coins, const ItemInventory& inventory, float deltaTime);
	void Draw();

	// アイテムを手に入れた（真ん中に大きく出して、左下に並べる）
	void ShowItemGet(ItemType type, bool showPopup = true);
	// 「○○ を手に入れた！」だけを出す（左下の列には足さない。バリアなど）
	void ShowGetPopup(const char* texture);
	// 伝説の武器のアイコンを、アイテムの列の上に出す（nullptr で消す）
	void SetLegendaryIcon(const char* texture);
	// コインを拾った（数字を少し跳ねさせる）
	void OnCoinCollected() { coinBounce = 1.0f; }

private:
	// 数字（と記号）を並べるための1行
	struct TextLine {
		std::array<std::unique_ptr<Sprite>, 5> glyphs;
		size_t count = 0;
	};
	void InitializeLine(TextLine& line);
	void SetText(TextLine& line, const std::string& text, const Vector2& position, float scale, const Vector4& color);
	void DrawLine(TextLine& line);

	SpriteCommon* spriteCommon = nullptr;

	// コイン
	std::unique_ptr<Sprite> coinIcon;
	TextLine coinText;
	float coinBounce = 0.0f;

	// アイテム（手に入れた順）
	struct ItemSlot {
		ItemType type = ItemType::kBounce;
		std::unique_ptr<Sprite> icon;
		TextLine countText;
		float pop = 0.0f; // 手に入れた直後に大きくする
	};
	std::vector<ItemSlot> slots;

	// 「○○を手に入れた！」
	std::unique_ptr<Sprite> getPopup;
	float getTimer = 0.0f;

	// ---- 見た目の数値（ピクセル）----
	static inline const float kCoinTop = 232.0f;
	// 伝説の武器のアイコン（虹色に光らせる）
	std::unique_ptr<Sprite> legendaryIcon;
	bool showLegendary = false;
	float legendaryTime = 0.0f;
	static inline const float kItemSize = 44.0f;
	static inline const float kItemSpacing = 56.0f;
	static inline const float kItemBottom = 698.0f;   // アイコンの真ん中の高さ
	static inline const float kGlyphWidth = 20.0f;
	static inline const float kGlyphHeight = 28.0f;
	static inline const float kGetTime = 2.0f;        // 「手に入れた！」を出す時間（秒）
};