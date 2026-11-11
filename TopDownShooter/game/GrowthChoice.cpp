#include "GrowthChoice.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "WinApp.h"
#include "SpriteCommon.h"
#include "Input.h"
#include "BuildHud.h"

using namespace MathUtility;

namespace {
	const char* kCardTextures[] = {
		"resources/choice_card_move.png",
		"resources/choice_card_range.png",
		"resources/choice_card_firerate.png",
		"resources/choice_card_damage.png",
	};
	const char* kGlyphOrder = "0123456789+%";
	// 数字の色（カードの色に合わせる）
	const Vector4 kDigitColors[] = {
		{ 0.55f, 0.85f, 1.0f, 1.0f },
		{ 0.6f, 1.0f, 0.65f, 1.0f },
		{ 1.0f, 0.88f, 0.4f, 1.0f },
		{ 1.0f, 0.55f, 0.55f, 1.0f },
	};
}

const char* GrowthChoice::GetCardTexture(GrowthStat stat)
{
	return kCardTextures[static_cast<size_t>(stat)];
}

void GrowthChoice::Initialize(SpriteCommon* spriteCommon)
{
	const Vector2 screenSize = { static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) };

	background = std::make_unique<Sprite>();
	background->Initialize(spriteCommon, "resources/white.png");
	background->SetSize(screenSize);
	background->SetColor({ 0.0f, 0.0f, 0.05f, 0.6f });
	background->Update();

	title = std::make_unique<Sprite>();
	title->Initialize(spriteCommon, kTextureTitle);
	title->SetAnchorPoint({ 0.5f, 0.0f });
	title->SetPosition({ screenSize.x / 2.0f, 70.0f });
	title->Update();

	hint = std::make_unique<Sprite>();
	hint->Initialize(spriteCommon, kTextureHint);
	hint->SetAnchorPoint({ 0.5f, 1.0f });
	hint->SetPosition({ screenSize.x / 2.0f, screenSize.y - 30.0f });
	hint->Update();

	for (Card& card : cards) {
		card.sprite = std::make_unique<Sprite>();
		card.sprite->Initialize(spriteCommon, GetCardTexture(GrowthStat::kMoveSpeed));
		card.sprite->SetAnchorPoint({ 0.5f, 0.5f });
		for (std::unique_ptr<Sprite>& digit : card.digits) {
			digit = std::make_unique<Sprite>();
			digit->Initialize(spriteCommon, BuildHud::kTextureDigits);
			digit->SetAnchorPoint({ 0.0f, 0.5f });
		}
	}
	isOpen = false;
}

void GrowthChoice::Open(const std::vector<Option>& newOptions)
{
	options = newOptions;
	if (options.size() > kMaxOptions) {
		options.resize(kMaxOptions);
	}
	isOpen = !options.empty();
	selected = 0;
	openTime = 0.0f;
	wasStickTilted = true; // 開いた瞬間にスティックが倒れていても動かないように
	hasLastMouse = false;
	for (size_t i = 0; i < options.size(); ++i) {
		cards[i].sprite->ChangeTexture(GetCardTexture(options[i].stat));
		cards[i].lift = 0.0f;
	}
}

Vector2 GrowthChoice::GetCardCenter(size_t index) const
{
	float count = static_cast<float>(options.size());
	float x = static_cast<float>(WinApp::kClientWidth) / 2.0f + (static_cast<float>(index) - (count - 1.0f) / 2.0f) * kCardSpacing;
	return { x, kCardCenterY };
}

int32_t GrowthChoice::Update(Input* input, float deltaTime)
{
	if (!isOpen) {
		return -1;
	}
	openTime += deltaTime;
	int32_t count = static_cast<int32_t>(options.size());

	// 左右で選ぶ（キーボード・十字キー・スティック）
	int32_t step = 0;
	if (input->TriggerKey(DIK_A) || input->TriggerKey(DIK_LEFT) || input->TriggerPad(XINPUT_GAMEPAD_DPAD_LEFT)) {
		step = -1;
	}
	if (input->TriggerKey(DIK_D) || input->TriggerKey(DIK_RIGHT) || input->TriggerPad(XINPUT_GAMEPAD_DPAD_RIGHT)) {
		step = 1;
	}
	float stickX = input->GetLeftStick().x;
	bool tilted = std::fabs(stickX) >= 0.5f;
	if (tilted && !wasStickTilted) {
		step = (stickX < 0.0f) ? -1 : 1;
	}
	wasStickTilted = tilted;
	if (step != 0) {
		selected = static_cast<size_t>((static_cast<int32_t>(selected) + step + count) % count);
	}

	// マウス：カードの上に乗せると選ぶ
	const Vector2& mouse = input->GetMousePosition();
	int32_t hovered = -1;
	for (size_t i = 0; i < options.size(); ++i) {
		Vector2 center = GetCardCenter(i);
		if (std::fabs(mouse.x - center.x) < kCardWidth / 2.0f && std::fabs(mouse.y - center.y) < kCardHeight / 2.0f) {
			hovered = static_cast<int32_t>(i);
		}
	}
	// マウスが動いたときだけ、乗っているカードを選ぶ（キーで選んだのを勝手に戻さないように）
	if (!hasLastMouse) {
		lastMouse = mouse;
		hasLastMouse = true;
	}
	bool mouseMoved = std::fabs(mouse.x - lastMouse.x) > 1.0f || std::fabs(mouse.y - lastMouse.y) > 1.0f;
	lastMouse = mouse;
	if (hovered >= 0 && mouseMoved) {
		selected = static_cast<size_t>(hovered);
	}

	for (size_t i = 0; i < options.size(); ++i) {
		UpdateCard(i);
	}

	// 決定（開いてすぐは受けない。撃っていたボタンをそのまま押してしまわないように）
	if (openTime >= kInputDelay) {
		bool decide = input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) || input->TriggerPad(XINPUT_GAMEPAD_A);
		if (input->TriggerMouse(Input::MouseButton::kLeft) && hovered >= 0) {
			selected = static_cast<size_t>(hovered);
			decide = true;
		}
		if (decide) {
			isOpen = false;
			return static_cast<int32_t>(selected);
		}
	}
	return -1;
}

void GrowthChoice::UpdateCard(size_t index)
{
	Card& card = cards[index];
	const Option& option = options[index];
	bool isSelected = (index == selected);

	// 選ばれているカードは浮いて大きく、他は暗く
	card.lift = Lerp(card.lift, isSelected ? 1.0f : 0.0f, 0.25f);
	float scale = 1.0f + 0.08f * card.lift;
	Vector2 center = GetCardCenter(index);
	center.y -= 18.0f * card.lift;
	// 開いたときに下から出てくる
	float appear = std::clamp(openTime / 0.25f - static_cast<float>(index) * 0.15f, 0.0f, 1.0f);
	center.y += (1.0f - appear) * 60.0f;
	float brightness = 0.55f + 0.45f * card.lift;

	card.sprite->SetPosition(center);
	card.sprite->SetSize({ kCardWidth * scale, kCardHeight * scale });
	card.sprite->SetColor({ brightness, brightness, brightness, appear });
	card.sprite->Update();

	// 「+○○%」をカードの下のほうに並べる
	std::string text = "+" + std::to_string(option.percent) + "%";
	card.digitCount = (std::min)(text.size(), kMaxDigits);
	float glyphWidth = kGlyphWidth * kDigitScale * scale;
	float startX = center.x - glyphWidth * static_cast<float>(card.digitCount) / 2.0f;
	float y = center.y + 78.0f * scale;
	Vector4 color = kDigitColors[static_cast<size_t>(option.stat)];
	color = { color.x * brightness, color.y * brightness, color.z * brightness, appear };
	for (size_t i = 0; i < card.digitCount; ++i) {
		const char* found = std::strchr(kGlyphOrder, text[i]);
		size_t glyph = (found != nullptr) ? static_cast<size_t>(found - kGlyphOrder) : 0;
		Sprite* digit = card.digits[i].get();
		digit->SetTextureLeftTop({ static_cast<float>(glyph) * kGlyphWidth, 0.0f });
		digit->SetTextureSize({ kGlyphWidth, kGlyphHeight });
		digit->SetSize({ glyphWidth, kGlyphHeight * kDigitScale * scale });
		digit->SetPosition({ startX + static_cast<float>(i) * glyphWidth, y });
		digit->SetColor(color);
		digit->Update();
	}
}

void GrowthChoice::Draw()
{
	if (!isOpen) {
		return;
	}
	background->Draw();
	title->Draw();
	hint->Draw();
	for (size_t i = 0; i < options.size(); ++i) {
		cards[i].sprite->Draw();
		for (size_t d = 0; d < cards[i].digitCount; ++d) {
			cards[i].digits[d]->Draw();
		}
	}
}
