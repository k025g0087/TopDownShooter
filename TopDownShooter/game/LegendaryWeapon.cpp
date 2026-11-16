#include "LegendaryWeapon.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include "Object3dCommon.h"
#include "ParticleManager.h"

namespace {
	const char* kBigIcons[] = { "resources/legend_big_knight.png", "resources/legend_big_witch.png", "resources/legend_big_archer.png" };
	const char* kIcons[] = { "resources/legend_icon_knight.png", "resources/legend_icon_witch.png", "resources/legend_icon_archer.png" };
	const char* kGetTexts[] = { "resources/legend_get_knight.png", "resources/legend_get_witch.png", "resources/legend_get_archer.png" };
	const char* kDescriptions[] = { "resources/legend_desc_knight.png", "resources/legend_desc_witch.png", "resources/legend_desc_archer.png" };
	static_assert(sizeof(kBigIcons) / sizeof(kBigIcons[0]) == static_cast<size_t>(CharacterType::kCount), "伝説の武器の数がキャラの数と合っていない");
}

const char* LegendaryWeapon::GetBigIconTexture(CharacterType c) { return kBigIcons[static_cast<size_t>(c)]; }
const char* LegendaryWeapon::GetIconTexture(CharacterType c) { return kIcons[static_cast<size_t>(c)]; }
const char* LegendaryWeapon::GetGetTexture(CharacterType c) { return kGetTexts[static_cast<size_t>(c)]; }
const char* LegendaryWeapon::GetDescriptionTexture(CharacterType c) { return kDescriptions[static_cast<size_t>(c)]; }

Vector4 LegendaryWeapon::HueToColor(float hue)
{
	// 色相を6つの区間に分けて、赤→黄→緑→水色→青→紫→赤と回す（少し白を混ぜてやわらかく）
	float h = (hue - std::floor(hue)) * 6.0f;
	float x = 1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f);
	Vector3 rgb = { 0.0f, 0.0f, 0.0f };
	switch (static_cast<int32_t>(h)) {
	case 0: rgb = { 1.0f, x, 0.0f }; break;
	case 1: rgb = { x, 1.0f, 0.0f }; break;
	case 2: rgb = { 0.0f, 1.0f, x }; break;
	case 3: rgb = { 0.0f, x, 1.0f }; break;
	case 4: rgb = { x, 0.0f, 1.0f }; break;
	default: rgb = { 1.0f, 0.0f, x }; break;
	}
	const float white = 0.25f;
	return { rgb.x + (1.0f - rgb.x) * white, rgb.y + (1.0f - rgb.y) * white, rgb.z + (1.0f - rgb.z) * white, 1.0f };
}

void LegendaryWeapon::Initialize(CharacterType newCharacter, const std::string& arrowModel)
{
	character = newCharacter;
	for (FireZone& zone : fireZones) {
		zone.object = std::make_unique<Object3d>();
		zone.object->Initialize(Object3dCommon::GetInstance());
		zone.object->SetModel(kModelFireZone);
	}
	for (RainArrow& arrow : rainArrows) {
		arrow.object = std::make_unique<Object3d>();
		arrow.object->Initialize(Object3dCommon::GetInstance());
		arrow.object->SetModel(arrowModel);
	}
	// パーティクルのグループ（初めてのときだけ作る）
	ParticleManager* particleManager = ParticleManager::GetInstance();
	for (const char* name : { kParticleSparkle, kParticleFire }) {
		if (!particleManager->ContainsParticleGroup(name)) {
			particleManager->CreateParticleGroup(name, "resources/circle.png");
		}
	}
	Reset();
}

void LegendaryWeapon::Reset()
{
	owned = false;
	ClearEffects();
}

void LegendaryWeapon::ClearEffects()
{
	for (FireZone& zone : fireZones) {
		zone.active = false;
	}
	for (RainArrow& arrow : rainArrows) {
		arrow.active = false;
	}
}

void LegendaryWeapon::AddFireZone(const Vector3& center, float radius, float damagePerTick)
{
	// 空いているところ、なければ一番古い炎を使う（同時に残る炎の数に上限をつけて、強くなりすぎないように）
	FireZone* target = &fireZones[0];
	for (FireZone& zone : fireZones) {
		if (!zone.active) {
			target = &zone;
			break;
		}
		if (zone.time > target->time) {
			target = &zone;
		}
	}
	target->active = true;
	target->center = { center.x, 0.0f, center.z };
	target->radius = radius;
	target->damage = damagePerTick;
	target->time = 0.0f;
	target->tickTimer = kFireTick;
	target->emitTimer = 0.0f;
}

void LegendaryWeapon::AddArrowRain(const Vector3& center, float damage)
{
	// 円の上に均等に、少しずつ時間をずらして降らせる
	int32_t placed = 0;
	for (RainArrow& arrow : rainArrows) {
		if (arrow.active) {
			continue;
		}
		float angle = std::numbers::pi_v<float> * 2.0f * static_cast<float>(placed) / static_cast<float>(kRainCount) + 0.6f;
		float distance = kRainSpread * (0.35f + 0.65f * static_cast<float>((placed * 3) % kRainCount) / static_cast<float>(kRainCount - 1));
		arrow.active = true;
		arrow.target = { center.x + std::cos(angle) * distance, 0.0f, center.z + std::sin(angle) * distance };
		arrow.damage = damage;
		arrow.delay = 0.12f + 0.06f * static_cast<float>(placed);
		arrow.fall = 0.0f;
		if (++placed >= kRainCount) {
			break;
		}
	}
}

void LegendaryWeapon::Update(float deltaTime, const Vector3& playerPosition, const AreaDamageFunction& damageArea, const SolidCheckFunction& isSolid)
{
	time += deltaTime;
	ParticleManager* particleManager = ParticleManager::GetInstance();

	// 持っている間：体のまわりに虹色のキラキラ
	if (owned) {
		sparkleTimer -= deltaTime;
		if (sparkleTimer <= 0.0f) {
			sparkleTimer = 0.1f;
			ParticleEffectSettings sparkle;
			sparkle.count = 2;
			sparkle.shape = ParticleEffectSettings::Shape::Sphere;
			sparkle.spawnRadius = 0.6f;
			sparkle.lifeTimeMin = 0.5f;
			sparkle.lifeTimeMax = 0.9f;
			sparkle.speedMin = 0.2f;
			sparkle.speedMax = 0.6f;
			sparkle.startColor = HueToColor(time * 0.5f);
			sparkle.endColor = { sparkle.startColor.x, sparkle.startColor.y, sparkle.startColor.z, 0.0f };
			sparkle.startScale = 0.22f;
			sparkle.endScale = 0.0f;
			sparkle.gravity = { 0.0f, 0.8f, 0.0f };
			sparkle.blendMode = kBlendModeAdd;
			particleManager->Emit(kParticleSparkle, playerPosition + Vector3{ 0.0f, 0.8f, 0.0f }, sparkle);
		}
	}

	// 炎：ふくらんで出てきて、決まった間隔でダメージ。最後は縮んで消える
	for (FireZone& zone : fireZones) {
		if (!zone.active) {
			continue;
		}
		zone.time += deltaTime;
		if (zone.time >= kFireDuration) {
			zone.active = false;
			continue;
		}
		zone.tickTimer -= deltaTime;
		if (zone.tickTimer <= 0.0f) {
			zone.tickTimer += kFireTick;
			damageArea(zone.center, zone.radius, zone.damage);
		}
		zone.emitTimer -= deltaTime;
		if (zone.emitTimer <= 0.0f) {
			zone.emitTimer = 0.07f;
			ParticleEffectSettings fire;
			fire.count = 2;
			fire.shape = ParticleEffectSettings::Shape::Cone;
			fire.spawnRadius = zone.radius * 0.7f;
			fire.coneAngle = 0.3f;
			fire.lifeTimeMin = 0.4f;
			fire.lifeTimeMax = 0.7f;
			fire.speedMin = 1.0f;
			fire.speedMax = 2.0f;
			fire.startColor = { 1.0f, 0.75f, 0.25f, 1.0f };
			fire.endColor = { 1.0f, 0.1f, 0.0f, 0.0f };
			fire.startScale = 0.35f;
			fire.endScale = 0.05f;
			fire.gravity = { 0.0f, 1.0f, 0.0f };
			fire.blendMode = kBlendModeAdd;
			particleManager->Emit(kParticleFire, zone.center + Vector3{ 0.0f, 0.1f, 0.0f }, fire);
		}
		float grow = (std::min)(1.0f, zone.time / 0.15f);
		float shrink = (std::min)(1.0f, (kFireDuration - zone.time) / 0.3f);
		float size = zone.radius * grow * shrink * (0.95f + 0.05f * std::sin(time * 20.0f));
		zone.object->SetScale({ size, 1.0f, size });
		zone.object->SetRotate({ 0.0f, time * 1.5f, 0.0f });
		zone.object->SetTranslate(zone.center);
		zone.object->Update();
	}

	// 矢の雨：空からまっすぐ落ちてきて、地面に着いたらその場所にダメージ
	for (RainArrow& arrow : rainArrows) {
		if (!arrow.active) {
			continue;
		}
		if (arrow.delay > 0.0f) {
			arrow.delay -= deltaTime;
			// 待っている間は描かない（降り始めたフレームは、この下で位置を計算してから描く）
			if (arrow.delay > 0.0f) {
				continue;
			}
		}
		arrow.fall += deltaTime;
		float t = (std::min)(1.0f, arrow.fall / kRainFallTime);
		if (t >= 1.0f) {
			arrow.active = false;
			if (!isSolid(arrow.target)) {
				damageArea(arrow.target, kRainHitRadius, arrow.damage);
				particleManager->Emit("spark", arrow.target + Vector3{ 0.0f, 0.2f, 0.0f }, 3u);
			}
			continue;
		}
		// 矢のモデルは +Z を向いているので、X軸で90度回して下を向ける
		arrow.object->SetScale({ 0.8f, 0.8f, 0.8f });
		arrow.object->SetRotate({ std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f });
		arrow.object->SetTranslate(arrow.target + Vector3{ 0.0f, kRainHeight * (1.0f - t * t) + 0.3f, 0.0f });
		arrow.object->Update();
	}
}

void LegendaryWeapon::SetCamera(Camera* camera)
{
	for (FireZone& zone : fireZones) {
		zone.object->SetCamera(camera);
	}
	for (RainArrow& arrow : rainArrows) {
		arrow.object->SetCamera(camera);
	}
}

void LegendaryWeapon::Draw()
{
	for (FireZone& zone : fireZones) {
		if (zone.active) {
			zone.object->Draw();
		}
	}
	for (RainArrow& arrow : rainArrows) {
		if (arrow.active && arrow.delay <= 0.0f) {
			arrow.object->Draw();
		}
	}
}
