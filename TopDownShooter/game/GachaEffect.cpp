#include "GachaEffect.h"
#include "GameSettings.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include "WinApp.h"
#include "SpriteCommon.h"
#include "Input.h"
#include "LegendaryWeapon.h"

using namespace MathUtility;

namespace {
	const char* kBigIconTextures[] = {
		"resources/item_big_bounce.png",
		"resources/item_big_split.png",
		"resources/item_big_homing.png",
		"resources/item_big_spread.png",
		"resources/item_big_giant.png",
	};
	const char* kDescriptionTextures[] = {
		"resources/item_desc_bounce.png",
		"resources/item_desc_split.png",
		"resources/item_desc_homing.png",
		"resources/item_desc_spread.png",
		"resources/item_desc_giant.png",
	};
	// アイテムの色（アイコンのふちの色と同じ）。カプセルと光をこの色にする
	const Vector4 kItemColors[] = {
		{ 0.43f, 0.78f, 1.0f, 1.0f },  // 跳ね返り
		{ 1.0f, 0.59f, 0.31f, 1.0f },  // 分裂
		{ 0.78f, 0.47f, 1.0f, 1.0f },  // 追尾
		{ 0.47f, 0.9f, 0.51f, 1.0f },  // 拡散
		{ 1.0f, 0.84f, 0.31f, 1.0f },  // 巨大化
	};
	static_assert(sizeof(kItemColors) / sizeof(kItemColors[0]) == static_cast<size_t>(ItemType::kCount), "アイテムの色の数が合っていない");

	// 0→1 を、行きすぎてから戻る動きにする（ポンと出てくる）
	float EaseOutBack(float t)
	{
		const float c1 = 1.70158f;
		const float c3 = c1 + 1.0f;
		return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
	}
	// 0→1 を、床で何回か跳ねる動きにする（カプセルが落ちてくる）
	float EaseOutBounce(float t)
	{
		const float n1 = 7.5625f;
		const float d1 = 2.75f;
		if (t < 1.0f / d1) {
			return n1 * t * t;
		} else if (t < 2.0f / d1) {
			t -= 1.5f / d1;
			return n1 * t * t + 0.75f;
		} else if (t < 2.5f / d1) {
			t -= 2.25f / d1;
			return n1 * t * t + 0.9375f;
		}
		t -= 2.625f / d1;
		return n1 * t * t + 0.984375f;
	}
	float Clamp01(float t) { return std::clamp(t, 0.0f, 1.0f); }
}

const char* GachaEffect::GetBigIconTexture(ItemType type)
{
	return kBigIconTextures[static_cast<size_t>(type)];
}

const char* GachaEffect::GetDescriptionTexture(ItemType type)
{
	return kDescriptionTextures[static_cast<size_t>(type)];
}

void GachaEffect::Initialize(SpriteCommon* spriteCommon)
{
	const Vector2 screenSize = { static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) };
	auto make = [spriteCommon](std::unique_ptr<Sprite>& sprite, const char* texture, const Vector2& anchor) {
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon, texture);
		sprite->SetAnchorPoint(anchor);
	};
	make(background, "resources/white.png", { 0.0f, 0.0f });
	background->SetSize(screenSize);
	make(flash, "resources/white.png", { 0.0f, 0.0f });
	flash->SetSize(screenSize);
	make(rays, kTextureRays, { 0.5f, 0.5f });
	make(glow, kTextureGlow, { 0.5f, 0.5f });
	// カプセルの上半分は下の辺、下半分は上の辺を中心にする（合わせ目で開くように）
	make(capsuleTop, kTextureCapsuleTop, { 0.5f, 1.0f });
	make(capsuleBottom, kTextureCapsuleBottom, { 0.5f, 0.0f });
	make(icon, GetBigIconTexture(ItemType::kBounce), { 0.5f, 0.5f });
	make(getText, "resources/white.png", { 0.5f, 0.5f });
	make(description, GetDescriptionTexture(ItemType::kBounce), { 0.5f, 0.5f });
	make(hint, kTextureHint, { 0.5f, 1.0f });
	hint->SetPosition({ screenSize.x / 2.0f, screenSize.y - 30.0f });
	for (Star& star : stars) {
		make(star.sprite, kTextureStar, { 0.5f, 0.5f });
	}
	make(secret, LegendaryWeapon::kTextureSecret, { 0.5f, 0.5f });
	isPlaying = false;
}

void GachaEffect::Start(ItemType newItem)
{
	item = newItem;
	legendary = false;
	itemColor = kItemColors[static_cast<size_t>(item)];
	icon->ChangeTexture(GetBigIconTexture(item));
	getText->ChangeTexture(ItemInventory::GetData(item).getTexture);
	description->ChangeTexture(GetDescriptionTexture(item));
	Begin();
}

void GachaEffect::StartLegendary(CharacterType character)
{
	legendary = true;
	itemColor = LegendaryWeapon::HueToColor(0.0f); // 色は毎フレーム虹色に変える
	icon->ChangeTexture(LegendaryWeapon::GetBigIconTexture(character));
	getText->ChangeTexture(LegendaryWeapon::GetGetTexture(character));
	description->ChangeTexture(LegendaryWeapon::GetDescriptionTexture(character));
	Begin();
}

void GachaEffect::Begin()
{
	isPlaying = true;
	opened = false;
	time = 0.0f;
	getText->SetSize(getText->GetTextureSize());
	description->SetSize(description->GetTextureSize());
	secret->SetSize(secret->GetTextureSize());
	for (Star& star : stars) {
		star.size = 0.0f;
	}
}

bool GachaEffect::Update(Input* input, float deltaTime)
{
	if (!isPlaying) {
		return false;
	}
	time += deltaTime;
	// シークレット：カプセルも光も、虹色にくるくる変わる
	if (legendary) {
		itemColor = LegendaryWeapon::HueToColor(time * 0.7f);
	}

	bool decide = input->TriggerKey(DIK_SPACE) || input->TriggerPad(XINPUT_GAMEPAD_A) ||
		GameSettings::GetInstance()->Trigger(input, GameAction::kInteract) || input->TriggerPad(XINPUT_GAMEPAD_X) || input->TriggerMouse(Input::MouseButton::kLeft);
	// 開く前：決定で、開くところまで飛ばす（回したボタンをそのまま押してしまわないように、少し待ってから）
	if (decide && time > 0.3f && time < kOpenTime) {
		time = kOpenTime;
		decide = false;
	}
	// 開いた後：決定で閉じる
	if (decide && time >= kCloseDelay) {
		isPlaying = false;
		return true;
	}

	// 画面を暗くする
	float fade = Clamp01(time / 0.25f);
	background->SetColor({ 0.0f, 0.0f, 0.04f, 0.78f * fade });
	background->Update();

	UpdateCapsule();
	UpdateReveal();
	return false;
}

void GachaEffect::UpdateCapsule()
{
	const float centerX = static_cast<float>(WinApp::kClientWidth) / 2.0f;
	Vector2 center = { centerX, kCenterY };
	float rotation = 0.0f;
	float glowPower = 0.0f;

	if (time < kOpenTime) {
		// 落ちてくる（上から床で跳ねるように）
		float drop = EaseOutBounce(Clamp01((time - kDropStart) / (kDropEnd - kDropStart)));
		center.y = -150.0f + (kCenterY + 150.0f) * drop;
		// ガタガタ揺れる（開く時間が近いほど激しく）
		if (time > kDropEnd) {
			float t = Clamp01((time - kDropEnd) / (kOpenTime - kDropEnd));
			float amount = 0.04f + 0.22f * t * t;
			float speed = 18.0f + 30.0f * t;
			rotation = std::sin(time * speed) * amount;
			center.x += std::sin(time * speed * 1.3f) * 10.0f * t;
			glowPower = 0.3f + 0.7f * t;
		}
		capsuleTop->SetPosition(center);
		capsuleTop->SetRotation(rotation);
		capsuleTop->SetSize({ kCapsuleWidth, kCapsuleHalfHeight });
		capsuleTop->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		capsuleBottom->SetPosition(center);
		capsuleBottom->SetRotation(rotation);
		capsuleBottom->SetSize({ kCapsuleWidth, kCapsuleHalfHeight });
		capsuleBottom->SetColor({ itemColor.x, itemColor.y, itemColor.z, 1.0f });
	} else {
		// 開いた：上は左上へ、下は右下へ、回りながら飛んで消える
		float t = Clamp01((time - kOpenTime) / 0.5f);
		float fly = t * t * 0.4f + t * 0.6f;
		capsuleTop->SetPosition({ center.x - 220.0f * fly, center.y - 260.0f * fly });
		capsuleTop->SetRotation(-1.2f * fly);
		capsuleTop->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f - t });
		capsuleBottom->SetPosition({ center.x + 220.0f * fly, center.y + 200.0f * fly });
		capsuleBottom->SetRotation(1.0f * fly);
		capsuleBottom->SetColor({ itemColor.x, itemColor.y, itemColor.z, 1.0f - t });
		glowPower = 1.0f;
	}
	capsuleTop->Update();
	capsuleBottom->Update();

	// 後ろの光：揺れている間はだんだん強く、開いたら大きく
	float pulse = 0.85f + 0.15f * std::sin(time * 12.0f);
	float glowSize = 320.0f + (time >= kOpenTime ? 260.0f : 0.0f);
	glow->SetPosition({ centerX, kCenterY });
	glow->SetSize({ glowSize * pulse, glowSize * pulse });
	glow->SetColor({ itemColor.x, itemColor.y, itemColor.z, glowPower * 0.9f });
	glow->Update();
}

void GachaEffect::UpdateReveal()
{
	const float centerX = static_cast<float>(WinApp::kClientWidth) / 2.0f;
	float sinceOpen = time - kOpenTime;

	// 開いた瞬間：星を飛び散らせる
	if (sinceOpen >= 0.0f && !opened) {
		opened = true;
		BurstStars();
	}

	// 白い光：開いた瞬間に一番明るく、すぐ消える
	float flashAlpha = (sinceOpen >= 0.0f) ? (std::max)(0.0f, 1.0f - sinceOpen / 0.35f) : 0.0f;
	flash->SetColor({ 1.0f, 1.0f, 1.0f, flashAlpha * 0.9f });
	flash->Update();

	// 回る光の筋
	float raysIn = Clamp01(sinceOpen / 0.3f);
	float raysSize = 760.0f * (0.6f + 0.4f * raysIn);
	rays->SetPosition({ centerX, kCenterY });
	rays->SetSize({ raysSize, raysSize });
	rays->SetRotation(time * 0.6f);
	rays->SetColor({ itemColor.x, itemColor.y, itemColor.z, raysIn * 0.85f });
	rays->Update();

	// アイコン：ポンと出てきて、ふわふわ浮く
	float iconIn = Clamp01(sinceOpen / 0.35f);
	float scale = (sinceOpen >= 0.0f) ? EaseOutBack(iconIn) : 0.0f;
	float floatY = std::sin(time * 2.5f) * 6.0f;
	icon->SetPosition({ centerX, kCenterY + floatY });
	icon->SetSize({ kIconSize * scale, kIconSize * scale });
	icon->SetColor({ 1.0f, 1.0f, 1.0f, (sinceOpen >= 0.0f) ? 1.0f : 0.0f });
	icon->Update();

	// 文字：少し遅れて下から出てくる
	float textIn = Clamp01((sinceOpen - 0.35f) / 0.25f);
	getText->SetPosition({ centerX, kCenterY + 170.0f + (1.0f - textIn) * 20.0f });
	getText->SetColor({ 1.0f, 1.0f, 1.0f, textIn });
	getText->Update();
	float descIn = Clamp01((sinceOpen - 0.55f) / 0.25f);
	description->SetPosition({ centerX, kCenterY + 215.0f + (1.0f - descIn) * 20.0f });
	description->SetColor({ 1.0f, 1.0f, 1.0f, descIn });
	description->Update();
	// 「SECRET!!」：アイコンの上で、大きいところから縮んで出てきて、ゆらゆら揺れる
	if (legendary) {
		float secretIn = Clamp01((sinceOpen - 0.2f) / 0.25f);
		float secretScale = 1.0f + (1.0f - secretIn) * 1.5f;
		Vector2 secretSize = secret->GetTextureSize();
		secret->SetSize({ secretSize.x * secretScale, secretSize.y * secretScale });
		secret->SetPosition({ centerX, kCenterY - 150.0f });
		secret->SetRotation(std::sin(time * 3.0f) * 0.06f);
		secret->SetColor({ 1.0f, 1.0f, 1.0f, secretIn });
		secret->Update();
	}
	float hintIn = Clamp01((time - kCloseDelay) / 0.3f);
	hint->SetColor({ 1.0f, 1.0f, 1.0f, hintIn });
	hint->Update();

	// 星：外へ飛びながら、ゆっくりになって小さくなる
	const float deltaTime = 1.0f / 60.0f;
	for (Star& star : stars) {
		if (star.size <= 0.0f) {
			continue;
		}
		Vector2 position = star.sprite->GetPosition();
		position.x += star.velocity.x * deltaTime;
		position.y += star.velocity.y * deltaTime;
		star.velocity.x *= 0.95f;
		star.velocity.y *= 0.95f;
		star.size = (std::max)(0.0f, star.size - 30.0f * deltaTime);
		star.sprite->SetPosition(position);
		star.sprite->SetRotation(star.sprite->GetRotation() + star.spin * deltaTime);
		star.sprite->SetSize({ star.size, star.size });
		// シークレットのときは、星も1つずつ違う虹色にする
		Vector4 starColor = legendary ? LegendaryWeapon::HueToColor(time + static_cast<float>(&star - stars.data()) * 0.07f) : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
		star.sprite->SetColor(starColor);
		star.sprite->Update();
	}
}

void GachaEffect::BurstStars()
{
	const float centerX = static_cast<float>(WinApp::kClientWidth) / 2.0f;
	for (size_t i = 0; i < stars.size(); ++i) {
		Star& star = stars[i];
		// 均等な向きに、少しずつ速さと大きさを変えて飛ばす
		float angle = std::numbers::pi_v<float> * 2.0f * static_cast<float>(i) / static_cast<float>(stars.size()) + 0.2f * static_cast<float>(i % 3);
		float speed = 520.0f + 160.0f * static_cast<float>(i % 4);
		star.velocity = { std::cos(angle) * speed, std::sin(angle) * speed };
		star.spin = (i % 2 == 0) ? 4.0f : -4.0f;
		star.size = 34.0f + 10.0f * static_cast<float>(i % 3);
		star.sprite->SetPosition({ centerX, kCenterY });
		star.sprite->SetRotation(angle);
	}
}

void GachaEffect::Draw(const std::function<void(BlendMode)>& setBlend)
{
	if (!isPlaying) {
		return;
	}
	setBlend(kBlendModeNormal);
	background->Draw();
	// 光るもの（加算）
	setBlend(kBlendModeAdd);
	glow->Draw();
	if (time >= kOpenTime) {
		rays->Draw();
	}
	// カプセルとアイコン（ふつう）
	setBlend(kBlendModeNormal);
	if (time < kOpenTime + 0.5f) {
		capsuleBottom->Draw();
		capsuleTop->Draw();
	}
	if (time >= kOpenTime) {
		icon->Draw();
		if (legendary) {
			secret->Draw();
		}
		getText->Draw();
		description->Draw();
		hint->Draw();
	}
	// 星と白い光（加算）
	setBlend(kBlendModeAdd);
	for (Star& star : stars) {
		if (star.size > 0.0f) {
			star.sprite->Draw();
		}
	}
	if (time >= kOpenTime) {
		flash->Draw();
	}
	// 元に戻す
	setBlend(kBlendModeNormal);
}
