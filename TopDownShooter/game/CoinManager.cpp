#include "CoinManager.h"
#include <cmath>
#include <numbers>
#include "Object3dCommon.h"
#include "DungeonMap.h"

using namespace MathUtility;

void CoinManager::Initialize()
{
	for (size_t i = 0; i < kMaxCoins; ++i) {
		objects[i] = std::make_unique<Object3d>();
		objects[i]->Initialize(Object3dCommon::GetInstance());
		objects[i]->SetModel(kModelCoin);
	}
	Clear();
}

void CoinManager::Clear()
{
	for (Coin& coin : coins) {
		coin.active = false;
	}
}

void CoinManager::Spawn(const Vector3& position, int32_t count)
{
	std::uniform_real_distribution<float> angleDistribution(0.0f, std::numbers::pi_v<float> * 2.0f);
	std::uniform_real_distribution<float> speedDistribution(1.5f, 4.0f);
	for (int32_t n = 0; n < count; ++n) {
		for (Coin& coin : coins) {
			if (coin.active) {
				continue;
			}
			// 上に跳ねながら、まわりに散らばる
			float angle = angleDistribution(random);
			float speed = speedDistribution(random);
			coin.active = true;
			coin.position = { position.x, 0.5f, position.z };
			coin.velocity = { std::sin(angle) * speed, 6.0f, std::cos(angle) * speed };
			coin.age = 0.0f;
			break;
		}
	}
}

int32_t CoinManager::Update(const Vector3& playerPosition, const DungeonMap& map, float deltaTime)
{
	int32_t collected = 0;
	for (size_t i = 0; i < kMaxCoins; ++i) {
		Coin& coin = coins[i];
		if (!coin.active) {
			continue;
		}
		coin.age += deltaTime;

		Vector3 toPlayer = { playerPosition.x - coin.position.x, 0.0f, playerPosition.z - coin.position.z };
		float distance = Length(toPlayer);
		if (coin.age > kPickupDelay && distance < kMagnetDistance) {
			// 吸い寄せ：プレイヤーへまっすぐ飛ぶ
			Vector3 direction = (distance > 0.001f) ? toPlayer / distance : Vector3{ 0.0f, 0.0f, 0.0f };
			coin.position += direction * (kMagnetSpeed * deltaTime);
			coin.position.y += (0.5f - coin.position.y) * 0.3f;
			if (distance < kPickupDistance) {
				coin.active = false;
				collected++;
				continue;
			}
		} else {
			// 飛び散る：重力で落ちて、床で止まる。壁の中には入らない
			Vector3 next = coin.position + coin.velocity * deltaTime;
			if (map.IsSolidAt({ next.x, 0.0f, coin.position.z })) {
				coin.velocity.x = -coin.velocity.x * 0.5f;
				next.x = coin.position.x;
			}
			if (map.IsSolidAt({ coin.position.x, 0.0f, next.z })) {
				coin.velocity.z = -coin.velocity.z * 0.5f;
				next.z = coin.position.z;
			}
			coin.position = next;
			coin.velocity.y -= kGravity * deltaTime;
			if (coin.position.y < 0.3f) {
				coin.position.y = 0.3f;
				coin.velocity = { coin.velocity.x * 0.5f, std::fabs(coin.velocity.y) * 0.3f, coin.velocity.z * 0.5f };
				if (std::fabs(coin.velocity.y) < 0.8f) {
					coin.velocity = { 0.0f, 0.0f, 0.0f };
				}
			}
		}

		// 見た目：くるくる回る
		objects[i]->SetRotate({ 0.0f, coin.age * 5.0f, 0.0f });
		objects[i]->SetTranslate(coin.position);
		objects[i]->Update();
	}
	return collected;
}

void CoinManager::Draw()
{
	for (size_t i = 0; i < kMaxCoins; ++i) {
		if (coins[i].active) {
			objects[i]->Draw();
		}
	}
}

void CoinManager::SetCamera(Camera* camera)
{
	for (std::unique_ptr<Object3d>& object : objects) {
		object->SetCamera(camera);
	}
}
