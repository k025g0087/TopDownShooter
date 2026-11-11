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
		return;
	}
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