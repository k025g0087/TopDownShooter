#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Mathutility.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "UiFontData.h"

// 画面に文字や四角を出す小さな道具（リリースビルドでも使える。ImGuiは使わない）
// 文字は、あらかじめ作った1枚の文字画像（resources/font_ui.png）から、1文字ずつスプライトで並べて描く
// 使い方：毎フレーム BeginFrame → Text / Rect を呼ぶ（更新のとき）→ Draw（描画のとき）
// 使える文字は UiFontData.h にある文字だけ（足りない文字は tools/gen_ui_font.py で足す）
class UiKit
{
public:
	static inline const char* kTextureFont = "resources/font_ui.png";
	static inline const char* kTextureWhite = "resources/white.png";

	enum class Align { kLeft, kCenter, kRight };

	void Initialize(SpriteCommon* common) { spriteCommon = common; commands.clear(); whiteUsed = 0; fontUsed = 0; }

	// 今フレームの描く内容を空にする
	void BeginFrame() { commands.clear(); whiteUsed = 0; fontUsed = 0; }

	// 四角（x, y は左上）
	void Rect(float x, float y, float w, float h, const Vector4& color)
	{
		Sprite* sprite = Acquire(whitePool, whiteUsed, kTextureWhite);
		sprite->SetAnchorPoint({ 0.0f, 0.0f });
		sprite->SetPosition({ x, y });
		sprite->SetSize({ w, h });
		sprite->SetTextureLeftTop({ 0.0f, 0.0f });
		sprite->SetColor(color);
		sprite->Update();
		commands.push_back(sprite);
	}

	// 四角のわく（太さ thickness）
	void Frame(float x, float y, float w, float h, float thickness, const Vector4& color)
	{
		Rect(x, y, w, thickness, color);
		Rect(x, y + h - thickness, w, thickness, color);
		Rect(x, y, thickness, h, color);
		Rect(x + w - thickness, y, thickness, h, color);
	}

	// 文字（UTF-8）。y は文字の上端。size は文字の高さ（ピクセル）
	void Text(const std::string& text, float x, float y, float size, const Vector4& color, Align align = Align::kLeft)
	{
		float scale = size / static_cast<float>(UiFontData::kGlyphSize);
		float width = TextWidth(text, size);
		if (align == Align::kCenter) {
			x -= width / 2.0f;
		} else if (align == Align::kRight) {
			x -= width;
		}
		size_t i = 0;
		while (i < text.size()) {
			uint32_t code = NextCode(text, i);
			int32_t index = FindGlyph(code);
			if (index < 0) {
				// 画像にない文字は、空白と同じ幅だけ空ける
				x += static_cast<float>(UiFontData::kGlyphSize) * 0.5f * scale;
				continue;
			}
			if (code != ' ') {
				Sprite* sprite = Acquire(fontPool, fontUsed, kTextureFont);
				int32_t col = index % UiFontData::kColumns;
				int32_t row = index / UiFontData::kColumns;
				float cell = static_cast<float>(UiFontData::kCellSize);
				sprite->SetAnchorPoint({ 0.0f, 0.0f });
				sprite->SetTextureLeftTop({ static_cast<float>(col) * cell, static_cast<float>(row) * cell });
				sprite->SetTextureSize({ cell, cell });
				float margin = (cell - static_cast<float>(UiFontData::kGlyphSize)) * 0.5f * scale;
				sprite->SetPosition({ x - margin, y - margin });
				sprite->SetSize({ cell * scale, cell * scale });
				sprite->SetColor(color);
				sprite->Update();
				commands.push_back(sprite);
			}
			x += static_cast<float>(UiFontData::kAdvance[index]) * scale;
		}
	}

	// 文字の横幅
	float TextWidth(const std::string& text, float size) const
	{
		float scale = size / static_cast<float>(UiFontData::kGlyphSize);
		float width = 0.0f;
		size_t i = 0;
		while (i < text.size()) {
			uint32_t code = NextCode(text, i);
			int32_t index = FindGlyph(code);
			width += (index < 0) ? static_cast<float>(UiFontData::kGlyphSize) * 0.5f * scale : static_cast<float>(UiFontData::kAdvance[index]) * scale;
		}
		return width;
	}

	// 描く（更新で積んだ内容を、積んだ順に描く）
	void Draw()
	{
		for (Sprite* sprite : commands) {
			sprite->Draw();
		}
	}

	// 点が四角の中か
	static bool Inside(const Vector2& point, float x, float y, float w, float h)
	{
		return point.x >= x && point.x <= x + w && point.y >= y && point.y <= y + h;
	}

private:
	// UTF-8 の次の1文字を取り出す
	static uint32_t NextCode(const std::string& text, size_t& i)
	{
		unsigned char c = static_cast<unsigned char>(text[i]);
		uint32_t code = c;
		size_t length = 1;
		if (c >= 0xF0) { code = c & 0x07; length = 4; }
		else if (c >= 0xE0) { code = c & 0x0F; length = 3; }
		else if (c >= 0xC0) { code = c & 0x1F; length = 2; }
		for (size_t k = 1; k < length && i + k < text.size(); ++k) {
			code = (code << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
		}
		i += length;
		return code;
	}
	// 文字画像の中の番号（なければ -1）
	static int32_t FindGlyph(uint32_t code)
	{
		const uint32_t* begin = UiFontData::kCodes;
		const uint32_t* end = UiFontData::kCodes + UiFontData::kCount;
		const uint32_t* it = std::lower_bound(begin, end, code);
		return (it != end && *it == code) ? static_cast<int32_t>(it - begin) : -1;
	}
	// 使い回すスプライトを1つ取り出す（足りなければ作る）
	Sprite* Acquire(std::vector<std::unique_ptr<Sprite>>& pool, size_t& used, const char* texture)
	{
		if (used >= pool.size()) {
			std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
			sprite->Initialize(spriteCommon, texture);
			pool.push_back(std::move(sprite));
		}
		return pool[used++].get();
	}

	SpriteCommon* spriteCommon = nullptr;
	std::vector<std::unique_ptr<Sprite>> whitePool, fontPool;
	size_t whiteUsed = 0, fontUsed = 0;
	std::vector<Sprite*> commands; // 描く順
};
