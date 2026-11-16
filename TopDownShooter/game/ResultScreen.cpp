#include "ResultScreen.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "WinApp.h"
#include "SpriteCommon.h"

namespace {
	const char* kGlyphOrder = "0123456789:F";
}

std::string ResultScreen::FormatTime(float seconds)
{
	int32_t total = static_cast<int32_t>(seconds);
	int32_t minutes = (std::min)(total / 60, 99);
	char buffer[8] = {};
	snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, total % 60);
	return buffer;
}

void ResultScreen::Initialize(SpriteCommon* common)
{
	spriteCommon = common;
	const Vector2 screenSize = { static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) };

	background = std::make_unique<Sprite>();
	background->Initialize(spriteCommon, "resources/white.png");
	background->SetSize(screenSize);

	panel = std::make_unique<Sprite>();
	panel->Initialize(spriteCommon, "resources/white.png");
	panel->SetAnchorPoint({ 0.5f, 0.0f });

	title = std::make_unique<Sprite>();
	title->Initialize(spriteCommon, kTextureTitleReturn);
	title->SetAnchorPoint({ 0.5f, 0.5f });

	newRecord = std::make_unique<Sprite>();
	newRecord->Initialize(spriteCommon, kTextureNewRecord);
	newRecord->SetAnchorPoint({ 0.5f, 0.5f });

	hint = std::make_unique<Sprite>();
	hint->Initialize(spriteCommon, kTextureHint);
	hint->SetAnchorPoint({ 0.5f, 1.0f });
	hint->SetPosition({ screenSize.x / 2.0f, screenSize.y - 30.0f });

	const char* labels[kRowCount] = { kTextureLabelFloor, kTextureLabelTime, kTextureLabelKills, kTextureLabelCoins, kTextureLabelBest };
	for (size_t i = 0; i < kRowCount; ++i) {
		rows[i].label = std::make_unique<Sprite>();
		rows[i].label->Initialize(spriteCommon, labels[i]);
		rows[i].label->SetAnchorPoint({ 0.0f, 0.5f });
	}
	isOpen = false;
}

void ResultScreen::SetText(NumberText& number, const std::string& text)
{
	number.text = text;
	// 足りない分だけ文字の板を作る
	while (number.glyphs.size() < text.size()) {
		std::unique_ptr<Sprite> glyph = std::make_unique<Sprite>();
		glyph->Initialize(spriteCommon, kTextureDigits);
		glyph->SetAnchorPoint({ 0.0f, 0.5f });
		number.glyphs.push_back(std::move(glyph));
	}
}

void ResultScreen::Open(const Result& result, const RunRecord::Entry& best, bool newRecordFlag)
{
	isOpen = true;
	openTime = 0.0f;
	isNewRecord = newRecordFlag;
	returned = result.returned;

	// 帰還は金色、力尽きたは赤
	title->ChangeTexture(returned ? kTextureTitleReturn : kTextureTitleDead);
	title->SetSize(title->GetTextureSize());

	SetText(rows[0].value, std::to_string(result.floor) + "F");
	SetText(rows[1].value, FormatTime(result.seconds));
	SetText(rows[2].value, std::to_string(result.kills));
	SetText(rows[3].value, std::to_string(result.coins));
	// ベスト：「12F 03:25」（空白のところは何も描かない）
	SetText(rows[4].value, best.valid ? std::to_string(best.floor) + "F " + FormatTime(best.seconds) : "");
}

void ResultScreen::LayoutText(NumberText& number, const Vector2& right, float scale, const Vector4& color)
{
	float width = kGlyphWidth * scale;
	float startX = right.x - width * static_cast<float>(number.text.size());
	for (size_t i = 0; i < number.text.size(); ++i) {
		const char* found = std::strchr(kGlyphOrder, number.text[i]);
		Sprite* glyph = number.glyphs[i].get();
		// 並びにない文字（空白）は透明にする
		float alpha = (found != nullptr) ? color.w : 0.0f;
		size_t index = (found != nullptr) ? static_cast<size_t>(found - kGlyphOrder) : 0;
		glyph->SetTextureLeftTop({ static_cast<float>(index) * kGlyphWidth, 0.0f });
		glyph->SetTextureSize({ kGlyphWidth, kGlyphHeight });
		glyph->SetSize({ width, kGlyphHeight * scale });
		glyph->SetPosition({ startX + static_cast<float>(i) * width, right.y });
		glyph->SetColor({ color.x, color.y, color.z, alpha });
		glyph->Update();
	}
}

void ResultScreen::Update(float deltaTime)
{
	if (!isOpen) {
		return;
	}
	openTime += deltaTime;
	const float centerX = static_cast<float>(WinApp::kClientWidth) / 2.0f;

	// 背景がだんだん暗くなる
	float fade = (std::min)(openTime / 0.5f, 1.0f);
	background->SetColor({ 0.0f, 0.0f, 0.03f, 0.75f * fade });
	background->Update();

	// 見出し：少し大きい所から縮んで出てくる
	float titleT = std::clamp((openTime - 0.1f) / 0.35f, 0.0f, 1.0f);
	float titleScale = 1.0f + (1.0f - titleT) * 0.6f;
	Vector2 titleSize = title->GetTextureSize();
	title->SetSize({ titleSize.x * titleScale, titleSize.y * titleScale });
	title->SetPosition({ centerX, 150.0f });
	title->SetColor({ 1.0f, 1.0f, 1.0f, titleT });
	title->Update();

	// 板（行の後ろ）
	panel->SetPosition({ centerX, kRowTop - 40.0f });
	panel->SetSize({ kPanelWidth, kRowSpacing * static_cast<float>(kRowCount) + 20.0f });
	panel->SetColor({ 0.08f, 0.08f, 0.14f, 0.85f * fade });
	panel->Update();

	// 行：上から順に、右からすべりこんでくる
	const float left = centerX - kPanelWidth / 2.0f + 40.0f;
	const float right = centerX + kPanelWidth / 2.0f - 40.0f;
	for (size_t i = 0; i < kRowCount; ++i) {
		float t = std::clamp((openTime - 0.35f - static_cast<float>(i) * 0.12f) / 0.25f, 0.0f, 1.0f);
		float slide = (1.0f - t) * (1.0f - t) * 80.0f;
		float y = kRowTop + static_cast<float>(i) * kRowSpacing;
		// ベストの行は少し離して、色を変える
		bool isBest = (i == kRowCount - 1);
		if (isBest) {
			y += 10.0f;
		}
		Vector4 labelColor = isBest ? Vector4{ 1.0f, 0.85f, 0.45f, t } : Vector4{ 1.0f, 1.0f, 1.0f, t };
		rows[i].label->SetSize(rows[i].label->GetTextureSize());
		rows[i].label->SetPosition({ left + slide, y });
		rows[i].label->SetColor(labelColor);
		rows[i].label->Update();
		// 到達階層とタイムは大きく
		float scale = (i <= 1) ? 1.0f : 0.8f;
		Vector4 color = (i <= 1) ? Vector4{ 1.0f, 0.95f, 0.7f, t } : Vector4{ 0.9f, 0.9f, 0.95f, t };
		if (isBest) {
			scale = 0.75f;
			color = { 1.0f, 0.85f, 0.45f, t };
		}
		LayoutText(rows[i].value, { right + slide, y }, scale, color);
	}

	// NEW RECORD!：行が出そろってから、ぴょこぴょこ跳ねる
	if (isNewRecord) {
		float t = std::clamp((openTime - 1.1f) / 0.2f, 0.0f, 1.0f);
		float bounce = std::fabs(std::sin(openTime * 4.0f)) * 6.0f;
		Vector2 size = newRecord->GetTextureSize();
		newRecord->SetSize(size);
		newRecord->SetPosition({ centerX + kPanelWidth / 2.0f - size.x / 2.0f + 10.0f, kRowTop - 52.0f - bounce });
		newRecord->SetColor({ 1.0f, 1.0f, 1.0f, t });
		newRecord->Update();
	}

	// 操作説明：入力を受けられるようになったら出す
	float hintAlpha = std::clamp((openTime - kInputDelay) / 0.3f, 0.0f, 1.0f);
	hint->SetColor({ 1.0f, 1.0f, 1.0f, hintAlpha });
	hint->Update();
}

void ResultScreen::Draw()
{
	if (!isOpen) {
		return;
	}
	background->Draw();
	title->Draw();
	panel->Draw();
	for (Row& row : rows) {
		row.label->Draw();
		for (size_t i = 0; i < row.value.text.size(); ++i) {
			row.value.glyphs[i]->Draw();
		}
	}
	if (isNewRecord) {
		newRecord->Draw();
	}
	hint->Draw();
}
