#include "BulletManager.h"
#include <cmath>
#include "Object3dCommon.h"

using namespace MathUtility;

void BulletManager::Initialize(const std::string& defaultModelName)
{
	objects.clear();
	for (size_t i = 0; i < kMaxBullets; ++i) {
		std::unique_ptr<Object3d> object = std::make_unique<Object3d>();
		object->Initialize(Object3dCommon::GetInstance());
		object->SetModel(defaultModelName);
		objects.push_back(std::move(object));
		bullets[i] = Bullet{};
	}
}

void BulletManager::Fire(const Vector3& position, const Vector3& direction, float speed, float lifeTime, float damage, const ShotStyle& style, bool fromPlayer)
{
	FireAndGet(position, direction, speed, lifeTime, damage, style, fromPlayer);
}

BulletManager::Bullet* BulletManager::FireAndGet(const Vector3& position, const Vector3& direction, float speed, float lifeTime, float damage, const ShotStyle& style, bool fromPlayer)
{
	for (size_t i = 0; i < kMaxBullets; ++i) {
		Bullet& bullet = bullets[i];
		if (bullet.active) {
			continue;
		}
		bullet = Bullet{};
		bullet.active = true;
		bullet.position = position;
		bullet.velocity = Normalize(direction) * speed;
		bullet.lifeTime = lifeTime;
		bullet.damage = damage;
		bullet.radius = style.radius;
		bullet.scale = style.scale;
		bullet.pierceLeft = style.pierce;
		bullet.explosionRadius = style.explosionRadius;
		bullet.explosionDamageRate = style.explosionDamageRate;
		bullet.spin = style.spin;
		bullet.growth = style.growth;
		bullet.fromPlayer = fromPlayer;
		// 見た目を攻撃の種類に合わせる
		objects[i]->SetModel(style.modelName);
		return &bullet;
	}
	return nullptr;
}

void BulletManager::Update(float deltaTime, float limit)
{
	for (size_t i = 0; i < kMaxBullets; ++i) {
		Bullet& bullet = bullets[i];
		if (!bullet.active) {
			continue;
		}
		bullet.position += bullet.velocity * deltaTime;
		bullet.lifeTime -= deltaTime;
		bullet.age += deltaTime;
		if (bullet.lifeTime <= 0.0f || std::fabs(bullet.position.x) > limit || std::fabs(bullet.position.z) > limit) {
			bullet.active = false;
			continue;
		}
		// 見た目：くるくる回すか、進む向きを向く
		float angle = std::atan2(bullet.velocity.x, bullet.velocity.z);
		Vector3 rotate = bullet.spin ? Vector3{ 0.0f, bullet.age * 12.0f, 0.0f } : Vector3{ 0.0f, angle, 0.0f };
		float grow = 1.0f + bullet.growth * bullet.age;
		Object3d* object = objects[i].get();
		object->SetScale(bullet.scale * grow);
		object->SetRotate(rotate);
		object->SetTranslate(bullet.position);
		object->Update();
	}
}

BulletManager::Bullet* BulletManager::Fire(const Vector3& position, const Vector3& direction, const PlayerStats& stats, const ShotStyle& style, const ShotModifiers& modifiers)
{
	// 大きさを変えた攻撃の性質で出してから、アイテムの性能を付ける
	ShotStyle scaled = style;
	scaled.radius *= modifiers.sizeRate;
	scaled.scale = style.scale * modifiers.sizeRate;
	Bullet* bullet = FireAndGet(position, direction, stats.bulletSpeed, stats.bulletLifeTime, stats.bulletDamage, scaled, true);
	if (bullet == nullptr) {
		return nullptr;
	}
	bullet->bounceLeft = modifiers.bounceCount;
	bullet->homingTurnSpeed = modifiers.homingTurnSpeed;
	bullet->splitCount = modifiers.splitCount;
	return bullet;
}

void BulletManager::FireSplit(const Bullet& parent, const Vector3& direction, const std::string& modelName)
{
	// 小さく・弱く・短く。モデルは元の弾と同じものを使う
	for (size_t i = 0; i < kMaxBullets; ++i) {
		if (bullets[i].active) {
			continue;
		}
		Bullet& bullet = bullets[i];
		bullet = parent;
		bullet.active = true;
		bullet.velocity = Normalize(direction) * (Length(parent.velocity) * 0.8f);
		bullet.lifeTime = 0.45f;
		bullet.age = 0.0f;
		bullet.damage = parent.damage * 0.4f;
		bullet.radius = parent.radius * 0.6f;
		bullet.scale = parent.scale * 0.6f;
		bullet.growth = 0.0f;
		bullet.explosionRadius = parent.explosionRadius * 0.5f;
		bullet.pierceLeft = 0;
		bullet.splitCount = 0; // 分裂した弾は、もう分裂しない
		bullet.rainOnEnd = false; // 矢の雨も、元の弾だけ
		// hitMask は受け継ぐ（分かれた直後に、同じ敵にもう一度当たらないように）
		objects[i]->SetModel(modelName);
		return;
	}
}

void BulletManager::ClearEnemyBullets()
{
	for (Bullet& bullet : bullets) {
		if (!bullet.fromPlayer) {
			bullet.active = false;
		}
	}
}

void BulletManager::Draw()
{
	for (size_t i = 0; i < kMaxBullets; ++i) {
		if (bullets[i].active) {
			objects[i]->Draw();
		}
	}
}

void BulletManager::SetCamera(Camera* camera)
{
	for (std::unique_ptr<Object3d>& object : objects) {
		object->SetCamera(camera);
	}
}