#include "InventoryHud.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "WinApp.h"
#include "SpriteCommon.h"

namespace {
	const char* kGlyphOrder = "0123456789+%:x";
}

void InventoryHud::Initialize(SpriteCommon* common)
{
	spriteCommon = common;

	coinIcon = std::make_unique<Sprite>();
	coinIcon->Initialize(spriteCommon, kTextureCoin);
	coinIcon->SetAnchorPoint({ 0.5f, 0.5f });
	coinIcon->SetSize({ 30.0f, 30.0f });
	InitializeLine(coinText);

	getPopup = std::make_unique<Sprite>();
	getPopup->Initialize(spriteCommon, ItemInventory::GetData(ItemType::kBounce).getTexture);
	getPopup->SetAnchorPoint({ 0.5f, 0.5f });
	legendaryIcon = std::make_unique<Sprite>();
	legendaryIcon->Initialize(spriteCommon, "resources/white.png");
	legendaryIcon->SetAnchorPoint({ 0.5f, 0.5f });
	showLegendary = false;

	Reset();
}

void InventoryHud::Reset()
{
	slots.clear();
	getTimer = 0.0f;
	coinBounce = 0.0f;
	showLegendary = false;
}

void InventoryHud::InitializeLine(TextLine& line)
{
	for (std::unique_ptr<Sprite>& glyph : line.glyphs) {
		glyph = std::make_unique<Sprite>();
		glyph->Initialize(spriteCommon, kTextureDigits);
		glyph->SetAnchorPoint({ 0.0f, 0.5f });
	}
	line.count = 0;
}

void InventoryHud::SetText(TextLine& line, const std::string& text, const Vector2& position, float scale, const Vector4& color)
{
	line.count = (std::min)(text.size(), line.glyphs.size());
	float width = kGlyphWidth * scale;
	for (size_t i = 0; i < line.count; ++i) {
		const char* found = std::strchr(kGlyphOrder, text[i]);
		size_t glyph = (found != nullptr) ? static_cast<size_t>(found - kGlyphOrder) : 0;
		Sprite* sprite = line.glyphs[i].get();
		sprite->SetTextureLeftTop({ static_cast<float>(glyph) * kGlyphWidth, 0.0f });
		sprite->SetTextureSize({ kGlyphWidth, kGlyphHeight });
		sprite->SetSize({ width, kGlyphHeight * scale });
		sprite->SetPosition({ position.x + static_cast<float>(i) * width, position.y });
		sprite->SetColor(color);
		sprite->Update();
	}
}

void InventoryHud::DrawLine(TextLine& line)
{
	for (size_t i = 0; i < line.count; ++i) {
		line.glyphs[i]->Draw();
	}
}

void InventoryHud::ShowGetPopup(const char* texture)
{
	getPopup->ChangeTexture(texture);
	getTimer = kGetTime;
}

void InventoryHud::SetLegendaryIcon(const char* texture)
{
	showLegendary = (texture != nullptr);
	if (showLegendary) {
		legendaryIcon->ChangeTexture(texture);
		legendaryTime = 0.0f;
	}
}

void InventoryHud::ShowItemGet(ItemType type, bool showPopup)
{
	// 「○○ を手に入れた！」（ガチャの演出で大きく見せたときは出さない）
	if (showPopup) {
		getPopup->ChangeTexture(ItemInventory::GetData(type).getTexture);
		getTimer = kGetTime;
	}

	// 初めてのアイテムなら、左下の列の最後に足す
	auto found = std::find_if(slots.begin(), slots.end(), [type](const ItemSlot& slot) { return slot.type == type; });
	if (found == slots.end()) {
		ItemSlot slot;
		slot.type = type;
		slot.icon = std::make_unique<Sprite>();
		slot.icon->Initialize(spriteCommon, ItemInventory::GetData(type).iconTexture);
		slot.icon->SetAnchorPoint({ 0.5f, 0.5f });
		InitializeLine(slot.countText);
		slots.push_back(std::move(slot));
		found = slots.end() - 1;
	}
	found->pop = 1.0f;
}

void InventoryHud::Update(int32_t coins, const ItemInventory& inventory, float deltaTime)
{
	// コイン：アイコンと枚数
	coinBounce = (std::max)(0.0f, coinBounce - deltaTime * 6.0f);
	float bounce = 1.0f + 0.25f * coinBounce;
	coinIcon->SetPosition({ 35.0f, kCoinTop });
	coinIcon->SetSize({ 30.0f * bounce, 30.0f * bounce });
	coinIcon->Update();
	SetText(coinText, std::to_string((std::min)(coins, 9999)), { 56.0f, kCoinTop }, 0.85f * bounce, { 1.0f, 0.92f, 0.5f, 1.0f });

	// アイテム：左下に横に並べる。2つ以上なら「x2」
	for (size_t i = 0; i < slots.size(); ++i) {
		ItemSlot& slot = slots[i];
		slot.pop = (std::max)(0.0f, slot.pop - deltaTime * 2.0f);
		float scale = 1.0f + 0.4f * slot.pop * slot.pop;
		Vector2 center = { 20.0f + kItemSize * 0.5f + static_cast<float>(i) * kItemSpacing, kItemBottom };
		slot.icon->SetPosition(center);
		slot.icon->SetSize({ kItemSize * scale, kItemSize * scale });
		slot.icon->Update();
		int32_t count = inventory.GetCount(slot.type);
		if (count >= 2) {
			SetText(slot.countText, "x" + std::to_string(count), { center.x + 6.0f, center.y + 14.0f }, 0.6f, { 1.0f, 1.0f, 1.0f, 1.0f });
		} else {
			slot.countText.count = 0;
		}
	}

	// 伝説の武器：アイテムの列の上に、少し大きく。手に入れたときはぽんと大きくなる
	if (showLegendary) {
		legendaryTime += deltaTime;
		float pop = (std::max)(0.0f, 1.0f - legendaryTime * 2.0f);
		float size = (kItemSize + 8.0f) * (1.0f + 0.5f * pop * pop) * (1.0f + 0.04f * std::sin(legendaryTime * 4.0f));
		legendaryIcon->SetPosition({ 20.0f + (kItemSize + 8.0f) * 0.5f, kItemBottom - kItemSpacing - 6.0f });
		legendaryIcon->SetSize({ size, size });
		legendaryIcon->Update();
	}

	// 「手に入れた！」：ぽんと出て、少し上がりながら消える
	if (getTimer > 0.0f) {
		getTimer -= deltaTime;
		float t = 1.0f - getTimer / kGetTime; // 0→1
		float scale = (t < 0.1f) ? 0.6f + 5.0f * t : (t < 0.15f ? 1.1f - 2.0f * (t - 0.1f) : 1.0f);
		float alpha = (t < 0.8f) ? 1.0f : (1.0f - t) / 0.2f;
		const Vector2& size = getPopup->GetTextureSize();
		getPopup->SetSize({ size.x * scale, size.y * scale });
		getPopup->SetPosition({ static_cast<float>(WinApp::kClientWidth) / 2.0f, 260.0f - 20.0f * t });
		getPopup->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
		getPopup->Update();
	}
}

void InventoryHud::Draw()
{
	coinIcon->Draw();
	DrawLine(coinText);
	for (ItemSlot& slot : slots) {
		slot.icon->Draw();
		DrawLine(slot.countText);
	}
	if (showLegendary) {
		legendaryIcon->Draw();
	}
	if (getTimer > 0.0f) {
		getPopup->Draw();
	}
}