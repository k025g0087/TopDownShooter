#pragma once
#include <cstdint>
#include <vector>
#include "Mathutility.h"

class Camera;

// ボスの種類（フロアごとの候補リストに入れて、ランダムに選ぶ）
enum class BossType {
	kDragon, // ドラゴン（炎のブレス・しっぽのなぎ払い・炎の雨）
	kCount,
};

// ボスの共通の形（ボスを増やすときは、これを継承したクラスを作る）
class BossBase
{
public:
	// ボスが撃つ弾
	struct Shot {
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 direction = { 0.0f, 0.0f, 1.0f };
		float speed = 7.0f;
		float lifeTime = 3.0f;
		float damage = 1.0f;
		int32_t style = 0; // 0：ふつうの弾　1：炎の弾
	};
	// 1フレームで起きたこと（StageScene が弾を出したり、画面を揺らしたりする）
	struct Events {
		std::vector<Shot> shots;
		std::vector<Vector3> explosions; // 爆発した場所（火花と画面の揺れ）
		bool roar = false;                // 吠えた（大きく揺らす）
	};

	virtual ~BossBase() = default;

	// roomCenter / roomHalfSize：ボス部屋の中心と、動ける範囲の半分（ワールド座標）
	virtual void Initialize(const Vector3& position, const Vector3& roomCenter, const Vector2& roomHalfSize, uint32_t seed) = 0;
	// target：プレイヤーの位置
	virtual void Update(const Vector3& target, float deltaTime, Events& events) = 0;
	virtual void Draw() = 0;
	virtual void SetCamera(Camera* camera) = 0;

	// 弾が当たった（HPが0になったら true）
	virtual bool Hit(float damage) = 0;
	// 位置を直す（壁に押し戻されたとき）
	virtual void SetPosition(const Vector3& position) = 0;

	virtual const Vector3& GetPosition() const = 0;
	virtual float GetRadius() const = 0;
	// HPの割合（0〜1。HPバーに使う）
	virtual float GetHpRate() const = 0;
	// HPが残っているか
	virtual bool IsAlive() const = 0;
	// 倒れる演出が終わったか（終わったら部屋クリア）
	virtual bool IsDeathFinished() const = 0;
	// 今、弾が当たるか（登場中・飛んでいる間などは当たらない）
	virtual bool IsHittable() const = 0;
	// 今、体に触れるとダメージを受けるか
	virtual bool IsTouchDangerous() const = 0;
	// 体のまわりの攻撃の範囲（しっぽのなぎ払いなど。0なら無し）
	virtual float GetAreaAttackRadius() const { return 0.0f; }
	// 壁の当たり判定をするか（飛んでいる間はしない）
	virtual bool UsesWallCollision() const { return true; }
	// HPバーに出す名前の画像
	virtual const char* GetNameTexture() const = 0;
};
