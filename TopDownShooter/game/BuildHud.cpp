#include "BuildHud.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "SpriteCommon.h"

namespace {
	const char* kIconTextures[] = {
		"resources/hud_icon_move.png",
		"resources/hud_icon_range.png",
		"resources/hud_icon_firerate.png",
		"resources/hud_icon_damage.png",
	};
	const char* kPopupTextures[] = {
		"resources/hud_up_move.png",
		"resources/hud_up_range.png",
		"resources/hud_up_firerate.png",
		"resources/hud_up_damage.png",
	};
}

const char* BuildHud::GetIconTexture(GrowthStat stat)
{
	return kIconTextures[static_cast<size_t>(stat)];
}

const char* BuildHud::GetPopupTexture(GrowthStat stat)
{
	return kPopupTextures[static_cast<size_t>(stat)];
}

void BuildHud::Initialize(SpriteCommon* spriteCommon)
{
	for (size_t i = 0; i < kStatCount; ++i) {
		Row& row = rows[i];
		row.icon = std::make_unique<Sprite>();
		row.icon->Initialize(spriteCommon, GetIconTexture(static_cast<GrowthStat>(i)));
		row.icon->SetAnchorPoint({ 0.5f, 0.5f });
		for (std::unique_ptr<Sprite>& digit : row.digits) {
			digit = std::make_unique<Sprite>();
			digit->Initialize(spriteCommon, kTextureDigits);
			digit->SetAnchorPoint({ 0.0f, 0.5f });
		}
		row.digitCount = 0;
		row.flashTimer = 0.0f;
	}
	for (Popup& popup : popups) {
		popup.sprite = std::make_unique<Sprite>();
		popup.sprite->Initialize(spriteCommon, GetPopupTexture(GrowthStat::kMoveSpeed));
		popup.sprite->SetAnchorPoint({ 0.0f, 0.5f });
		popup.timer = 0.0f;
	}
}

void BuildHud::Update(const BuildSystem& build, float deltaTime)
{
	for (size_t i = 0; i < kStatCount; ++i) {
		Row& row = rows[i];
		GrowthStat stat = static_cast<GrowthStat>(i);
		row.flashTimer = (std::max)(0.0f, row.flashTimer - deltaTime);

		float y = kTop + kRowHeight * (static_cast<float>(i) + 0.5f);
		// まだ伸びていないステータスは暗く表示する（どの敵で何が伸びるか、最初から見えるように）
		bool grown = build.GetPoints(stat) > 0;
		float flash = row.flashTimer / kFlashTime;
		float brightness = grown ? 1.0f : 0.4f;
		Vector4 color = { brightness, brightness, brightness, grown ? 1.0f : 0.7f };
		float iconScale = 1.0f + 0.35f * flash * flash;

		row.icon->SetPosition({ kLeft + kIconSize * 0.5f, y });
		row.icon->SetSize({ kIconSize * iconScale, kIconSize * iconScale });
		row.icon->SetColor(color);
		row.icon->Update();

		// 伸びた直後は数字を黄色く光らせる
		Vector4 textColor = color;
		if (flash > 0.0f) {
			textColor = { 1.0f, 1.0f - 0.2f * flash, 1.0f - 0.8f * flash, 1.0f };
		}
		std::string text = "+" + std::to_string(build.GetBonusPercent(stat)) + "%";
		SetText(row, text, kLeft + kIconSize + 8.0f, y, textColor);
	}

	// 浮かぶ「○○UP!」：上へ動きながら消える
	for (Popup& popup : popups) {
		if (popup.timer <= 0.0f) {
			continue;
		}
		popup.timer -= deltaTime;
		float t = 1.0f - (std::max)(popup.timer, 0.0f) / kPopupTime; // 0→1
		float y = kTop + kRowHeight * (static_cast<float>(popup.row) + 0.5f) - 30.0f * t;
		float alpha = (t < 0.7f) ? 1.0f : 1.0f - (t - 0.7f) / 0.3f;
		// 出た瞬間に少し大きく
		float scale = (t < 0.1f) ? 1.3f - 3.0f * t : 1.0f;
		popup.sprite->SetPosition({ kLeft + 150.0f, y });
		const Vector2& textureSize = popup.sprite->GetTextureSize();
		popup.sprite->SetSize({ textureSize.x * scale, textureSize.y * scale });
		popup.sprite->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
		popup.sprite->Update();
	}
}

void BuildHud::SetText(Row& row, const std::string& text, float x, float y, const Vector4& color)
{
	row.digitCount = (std::min)(text.size(), kMaxDigits);
	float glyphWidth = kGlyphWidth * kDigitScale;
	for (size_t i = 0; i < row.digitCount; ++i) {
		const char* found = std::strchr(kGlyphOrder, text[i]);
		size_t glyph = (found != nullptr) ? static_cast<size_t>(found - kGlyphOrder) : 0;
		Sprite* digit = row.digits[i].get();
		// 数字の画像から、その文字の部分だけを切り出す
		digit->SetTextureLeftTop({ static_cast<float>(glyph) * kGlyphWidth, 0.0f });
		digit->SetTextureSize({ kGlyphWidth, kGlyphHeight });
		digit->SetSize({ glyphWidth, kGlyphHeight * kDigitScale });
		digit->SetPosition({ x + static_cast<float>(i) * glyphWidth, y });
		digit->SetColor(color);
		digit->Update();
	}
}

void BuildHud::Draw()
{
	for (Row& row : rows) {
		row.icon->Draw();
		for (size_t i = 0; i < row.digitCount; ++i) {
			row.digits[i]->Draw();
		}
	}
	for (Popup& popup : popups) {
		if (popup.timer > 0.0f) {
			popup.sprite->Draw();
		}
	}
}

void BuildHud::OnGrowth(GrowthStat stat)
{
	size_t index = static_cast<size_t>(stat);
	rows[index].flashTimer = kFlashTime;

	// 同じ行の「UP!」がもう出ていたら、それを出し直す（重ならないように）
	Popup* target = nullptr;
	for (Popup& popup : popups) {
		if (popup.timer > 0.0f && popup.row == index) {
			target = &popup;
			break;
		}
	}
	// なければ空いているもの、全部使っていたら一番古いもの
	if (target == nullptr) {
		for (Popup& popup : popups) {
			if (popup.timer <= 0.0f) {
				target = &popup;
				break;
			}
		}
	}
	if (target == nullptr) {
		target = &popups[0];
		for (Popup& popup : popups) {
			if (popup.timer < target->timer) {
				target = &popup;
			}
		}
	}
	target->row = index;
	target->timer = kPopupTime;
	target->sprite->ChangeTexture(GetPopupTexture(stat));
}