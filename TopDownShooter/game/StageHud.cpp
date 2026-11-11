#include "StageHud.h"
#include <algorithm>
#include <cmath>
#include "WinApp.h"
#include "SpriteCommon.h"
#include <cstdio>
#include <cstring>

void StageHud::Initialize(SpriteCommon* common)
{
	spriteCommon = common;
	hearts.clear();
	const Vector2 screenSize = { static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) };

	damageFlash = std::make_unique<Sprite>();
	damageFlash->Initialize(spriteCommon, kTextureWhite);
	damageFlash->SetSize(screenSize);
	damageFlash->SetColor({ 1.0f, 0.1f, 0.1f, 0.0f });

	gameOverBack = std::make_unique<Sprite>();
	gameOverBack->Initialize(spriteCommon, kTextureWhite);
	gameOverBack->SetSize(screenSize);
	gameOverBack->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });

	gameOverText = std::make_unique<Sprite>();
	gameOverText->Initialize(spriteCommon, kTextureGameOver);
	gameOverText->SetAnchorPoint({ 0.5f, 0.5f });
	gameOverText->SetPosition({ screenSize.x / 2.0f, screenSize.y / 2.0f });

	gameClearText = std::make_unique<Sprite>();
	gameClearText->Initialize(spriteCommon, kTextureGameClear);
	gameClearText->SetAnchorPoint({ 0.5f, 0.5f });
	gameClearText->SetPosition({ screenSize.x / 2.0f, screenSize.y / 2.0f - 40.0f });
	for (std::unique_ptr<Sprite>& digit : timeDigits) {
		digit = std::make_unique<Sprite>();
		digit->Initialize(spriteCommon, kTextureDigits);
		digit->SetAnchorPoint({ 0.0f, 0.5f });
	}

	// ボスのHPバー（画面の上の真ん中）
	float barLeft = (screenSize.x - kBossBarWidth) / 2.0f;
	bossBarBack = std::make_unique<Sprite>();
	bossBarBack->Initialize(spriteCommon, kTextureWhite);
	bossBarBack->SetPosition({ barLeft - 3.0f, kBossBarTop - 3.0f });
	bossBarBack->SetSize({ kBossBarWidth + 6.0f, kBossBarHeight + 6.0f });
	bossBarBack->SetColor({ 0.05f, 0.03f, 0.05f, 0.85f });
	bossBarDelay = std::make_unique<Sprite>();
	bossBarDelay->Initialize(spriteCommon, kTextureWhite);
	bossBarDelay->SetPosition({ barLeft, kBossBarTop });
	bossBarDelay->SetColor({ 1.0f, 0.9f, 0.6f, 1.0f });
	bossBarFill = std::make_unique<Sprite>();
	bossBarFill->Initialize(spriteCommon, kTextureWhite);
	bossBarFill->SetPosition({ barLeft, kBossBarTop });
	bossBarFill->SetColor({ 0.9f, 0.2f, 0.25f, 1.0f });
	bossName = std::make_unique<Sprite>();
	bossName->Initialize(spriteCommon, kTextureWhite);
	bossName->SetAnchorPoint({ 0.5f, 1.0f });
	bossName->SetPosition({ screenSize.x / 2.0f, kBossBarTop - 4.0f });

	Reset();
}

void StageHud::Reset()
{
	flashTimer = 0.0f;
	shakeTimer = 0.0f;
	gameOver = false;
	gameOverTime = 0.0f;
	gameClear = false;
	showBoss = false;
	bossHpRate = 1.0f;
	bossDelayRate = 1.0f;
}

void StageHud::SetBoss(const char* nameTexture)
{
	showBoss = (nameTexture != nullptr);
	if (showBoss) {
		bossName->ChangeTexture(nameTexture);
		// ChangeTexture は表示の大きさを変えないので、画像の大きさに合わせる
		bossName->SetSize(bossName->GetTextureSize());
		bossHpRate = 1.0f;
		bossDelayRate = 1.0f;
	}
}

void StageHud::ShowClear(float seconds)
{
	gameClear = true;
	gameOverTime = 0.0f;
	// かかった時間を「mm:ss」にする
	int32_t total = static_cast<int32_t>(seconds);
	int32_t minutes = (std::min)(total / 60, 99);
	int32_t secs = total % 60;
	char buffer[8] = {};
	snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, secs);
	clearTimeText = buffer;
}

void StageHud::Update(int32_t hp, int32_t maxHp, float deltaTime)
{
	time += deltaTime;
	flashTimer = (std::max)(0.0f, flashTimer - deltaTime);
	shakeTimer = (std::max)(0.0f, shakeTimer - deltaTime);

	// ハートの数（最大HPの半分。切り上げ）が変わったら作り直す
	size_t heartCount = static_cast<size_t>((maxHp + 1) / 2);
	while (hearts.size() < heartCount) {
		std::unique_ptr<Sprite> heart = std::make_unique<Sprite>();
		heart->Initialize(spriteCommon, kTextureHeartFull);
		heart->SetAnchorPoint({ 0.5f, 0.5f });
		hearts.push_back(std::move(heart));
	}
	hearts.resize(heartCount);

	// ハートの絵：HP 2つで満タン、1つで半分、0で空
	float shake = (shakeTimer > 0.0f) ? 4.0f * (shakeTimer / kShakeTime) : 0.0f;
	for (size_t i = 0; i < hearts.size(); ++i) {
		int32_t remaining = hp - static_cast<int32_t>(i) * 2;
		const char* texture = kTextureHeartEmpty;
		if (remaining >= 2) {
			texture = kTextureHeartFull;
		} else if (remaining == 1) {
			texture = kTextureHeartHalf;
		}
		if (hearts[i]->GetTextureFilePath() != texture) {
			hearts[i]->ChangeTexture(texture);
		}
		// HPが少ないとき（ハート1つ以下）は、最後のハートがドクドク脈打つ
		float scale = 1.0f;
		if (hp <= 2 && hp > 0 && i == 0) {
			scale = 1.0f + 0.12f * std::fabs(std::sin(time * 5.0f));
		}
		float offsetX = std::sin(time * 60.0f + static_cast<float>(i)) * shake;
		hearts[i]->SetSize({ 40.0f * scale, 36.0f * scale });
		hearts[i]->SetPosition({ kHeartLeft + 20.0f + static_cast<float>(i) * kHeartSpacing + offsetX, kHeartTop + 18.0f });
		hearts[i]->Update();
	}

	// 赤い点滅：だんだん消える
	damageFlash->SetColor({ 1.0f, 0.1f, 0.1f, 0.35f * (flashTimer / kFlashTime) });
	damageFlash->Update();

	// ボスのHPバー：減った分（明るい色）は少し遅れて縮む
	if (showBoss) {
		bossDelayRate = (bossDelayRate > bossHpRate) ? (std::max)(bossHpRate, bossDelayRate - deltaTime * 0.35f) : bossHpRate;
		bossBarFill->SetSize({ kBossBarWidth * bossHpRate, kBossBarHeight });
		bossBarDelay->SetSize({ kBossBarWidth * bossDelayRate, kBossBarHeight });
		bossBarBack->Update();
		bossBarDelay->Update();
		bossBarFill->Update();
		bossName->Update();
	}

	// クリア：暗くして、文字と時間を出す
	if (gameClear) {
		gameOverTime += deltaTime;
		float darkness = (std::min)(gameOverTime / 1.0f, 1.0f) * 0.55f;
		gameOverBack->SetColor({ 0.0f, 0.0f, 0.0f, darkness });
		float textAlpha = std::clamp((gameOverTime - 0.4f) / 0.5f, 0.0f, 1.0f);
		gameClearText->SetColor({ 1.0f, 1.0f, 1.0f, textAlpha });
		gameClearText->Update();
		// 時間の数字（文字の下に大きく）
		const char* glyphs = "0123456789+%:";
		const float glyphWidth = 20.0f, glyphHeight = 28.0f, scale = 2.0f;
		float startX = static_cast<float>(WinApp::kClientWidth) / 2.0f - glyphWidth * scale * static_cast<float>(clearTimeText.size()) / 2.0f;
		for (size_t i = 0; i < timeDigits.size() && i < clearTimeText.size(); ++i) {
			const char* found = std::strchr(glyphs, clearTimeText[i]);
			size_t glyph = (found != nullptr) ? static_cast<size_t>(found - glyphs) : 0;
			timeDigits[i]->SetTextureLeftTop({ static_cast<float>(glyph) * glyphWidth, 0.0f });
			timeDigits[i]->SetTextureSize({ glyphWidth, glyphHeight });
			timeDigits[i]->SetSize({ glyphWidth * scale, glyphHeight * scale });
			timeDigits[i]->SetPosition({ startX + static_cast<float>(i) * glyphWidth * scale, static_cast<float>(WinApp::kClientHeight) / 2.0f + 30.0f });
			timeDigits[i]->SetColor({ 1.0f, 0.95f, 0.7f, textAlpha });
			timeDigits[i]->Update();
		}
	}

	// ゲームオーバー：暗くして、文字を少し遅れて出す
	if (gameOver) {
		gameOverTime += deltaTime;
		float darkness = (std::min)(gameOverTime / 1.0f, 1.0f) * 0.6f;
		gameOverBack->SetColor({ 0.0f, 0.0f, 0.0f, darkness });
		float textAlpha = std::clamp((gameOverTime - 0.6f) / 0.5f, 0.0f, 1.0f);
		gameOverText->SetColor({ 1.0f, 1.0f, 1.0f, textAlpha });
	}
	gameOverBack->Update();
	gameOverText->Update();
}

void StageHud::Draw()
{
	for (std::unique_ptr<Sprite>& heart : hearts) {
		heart->Draw();
	}
	if (flashTimer > 0.0f) {
		damageFlash->Draw();
	}
	if (showBoss) {
		bossBarBack->Draw();
		bossBarDelay->Draw();
		bossBarFill->Draw();
		bossName->Draw();
	}
	if (gameOver) {
		gameOverBack->Draw();
		gameOverText->Draw();
	}
	if (gameClear) {
		gameOverBack->Draw();
		gameClearText->Draw();
		for (size_t i = 0; i < timeDigits.size() && i < clearTimeText.size(); ++i) {
			timeDigits[i]->Draw();
		}
	}
}

void StageHud::OnDamaged()
{
	flashTimer = kFlashTime;
	shakeTimer = kShakeTime;
}

void StageHud::ShowGameOver()
{
	gameOver = true;
	gameOverTime = 0.0f;
}