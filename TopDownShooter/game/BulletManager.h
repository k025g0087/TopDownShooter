#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Mathutility.h"
#include "Object3d.h"
#include "CharacterData.h"

class Camera;

// 弾（斬撃・魔法弾・矢など、飛んでいく攻撃全部）の管理
// 決まった数を最初に作っておいて使い回す。毎回作ったり消したりしないので軽い
class BulletManager
{
public:
	// 同時に出せる弾の数
	static inline const size_t kMaxBullets = 256;

	// 1発の弾
	struct Bullet {
		bool active = false;
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 velocity = { 0.0f, 0.0f, 0.0f };
		float lifeTime = 0.0f;   // 残りの時間（秒）
		float age = 0.0f;        // 出てからの時間（秒）
		float damage = 1.0f;
		float radius = 0.16f;
		Vector3 scale = { 1.0f, 1.0f, 1.0f };
		int32_t pierceLeft = 0;  // あと何体貫通できるか（-1：何体でも）
		float explosionRadius = 0.0f;
		float explosionDamageRate = 0.0f;
		bool spin = false;
		float growth = 0.0f;
		uint64_t hitMask = 0;    // もう当たった敵（敵の番号のビット）。貫通する弾が同じ敵に何度も当たらないように
		bool fromPlayer = true;  // プレイヤーの弾か（敵の弾か）
	};

	void Initialize(const std::string& defaultModelName);
	// 弾を1発出す（空きがなければ何もしない）
	void Fire(const Vector3& position, const Vector3& direction, float speed, float lifeTime, float damage, const ShotStyle& style, bool fromPlayer);
	// プレイヤーの攻撃：ステータスから速さ・射程・威力を取る
	void Fire(const Vector3& position, const Vector3& direction, const PlayerStats& stats, const ShotStyle& style, bool fromPlayer)
	{
		Fire(position, direction, stats.bulletSpeed, stats.bulletLifeTime, stats.bulletDamage, style, fromPlayer);
	}
	// 敵の弾を全部消す（部屋をクリアしたときなど）
	void ClearEnemyBullets();
	// 更新（limit：闘技場の端。外に出たら消す）
	void Update(float deltaTime, float limit);
	void Draw();

	void SetCamera(Camera* camera);
	// 全部の弾（当たり判定で使う。当たった弾は active を false にする）
	std::array<Bullet, kMaxBullets>& GetBullets() { return bullets; }
	// 当たり判定で大きさが変わることがあるので、今の半径（育った分を含む）を返す
	static float GetCurrentRadius(const Bullet& bullet) { return bullet.radius * (1.0f + bullet.growth * bullet.age); }

private:
	std::array<Bullet, kMaxBullets> bullets;
	// 弾ごとの見た目（弾と同じ番号）
	std::vector<std::unique_ptr<Object3d>> objects;
};