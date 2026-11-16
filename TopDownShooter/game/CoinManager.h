#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include "Mathutility.h"
#include "Object3d.h"

class Camera;
class DungeonMap;

// お金（コイン）。敵を倒すと飛び出して、近づくと吸い寄せられて拾える
class CoinManager
{
public:
	static inline const char* kModelCoin = "test_coin.obj";
	static inline const size_t kMaxCoins = 160;

	void Initialize();
	// 全部消す（フロアが変わったとき）
	void Clear();
	// position から count 枚、まわりに飛び散らせる
	void Spawn(const Vector3& position, int32_t count);
	// 更新して、このフレームで拾った枚数を返す
	int32_t Update(const Vector3& playerPosition, const DungeonMap& map, float deltaTime);
	void Draw();
	void SetCamera(Camera* camera);

private:
	struct Coin {
		bool active = false;
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 velocity = { 0.0f, 0.0f, 0.0f };
		float age = 0.0f;
	};
	std::array<Coin, kMaxCoins> coins;
	std::array<std::unique_ptr<Object3d>, kMaxCoins> objects;
	std::minstd_rand random{ 12345u };

	// ---- 調整用の数値 ----
	static inline const float kMagnetDistance = 3.5f; // これより近いと吸い寄せられる
	static inline const float kMagnetSpeed = 14.0f;   // 吸い寄せられる速さ
	static inline const float kPickupDistance = 0.6f; // これより近いと拾える
	static inline const float kPickupDelay = 0.35f;   // 飛び出してから拾えるようになるまで（秒）
	static inline const float kGravity = 18.0f;
};
