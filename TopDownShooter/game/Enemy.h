#pragma once
#include <cstdint>
#include <memory>
#include <random>
#include "Mathutility.h"
#include "Object3d.h"

class Camera;

// 敵の種類（倒した種類によって、プレイヤーの伸びるステータスが変わる）
enum class EnemyType {
	kCharger, // 突進：ためてから一直線に突っ込んでくる　→ 移動速度が伸びる
	kShooter, // 射撃：距離を取って3方向に撃つ　　　　　→ 射程が伸びる
	kSwarm,   // 群れ：小さくて弱いが、数が多くて速い　　→ 連射が伸びる
	kTank,    // 重装：硬くて遅い、周りに弾をばらまく　　→ 威力が伸びる
	kCount,
};

// 敵（種類ごとに動きが違う。数値は Enemy.cpp の表にまとめてある）
class Enemy
{
public:
	// 撃つ弾の情報（StageScene が弾を出すのに使う）
	struct ShotRequest {
		bool shoot = false;
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 direction = { 0.0f, 0.0f, 1.0f }; // 真ん中の弾の向き
		int32_t count = 1;       // 弾の数
		float spread = 0.0f;     // 弾と弾の間の角度（ラジアン）
		float speed = 7.0f;
		float lifeTime = 3.0f;
		float damage = 1.0f;     // 1でハート半分
	};

	// 種類ごとの数値
	struct Data {
		const char* name = "";       // ImGui 用の名前
		const char* modelName = "";
		float maxHp = 5.0f;
		float radius = 0.5f;
		float moveSpeed = 1.6f;
		float knockbackRate = 1.0f;  // のけぞりやすさ（重い敵は小さく）
	};
	static const Data& GetData(EnemyType type);

	// seed：撃つタイミングなどをばらけさせるための乱数の種
	void Initialize(EnemyType type, const Vector3& position, uint32_t seed);
	// 更新（target：プレイヤーの位置）。撃つフレームなら ShotRequest.shoot が true になる
	ShotRequest Update(const Vector3& target, float deltaTime);
	void Draw();

	// 弾が当たった（倒れたら true）
	bool Hit(float damage, const Vector3& bulletVelocity);
	// 壁にぶつかった（突進が止まる）
	void OnHitWall();

	void SetCamera(Camera* camera) { object->SetCamera(camera); }
	// 位置を直す（壁や他の敵に押し戻されたとき）
	void SetPosition(const Vector3& newPosition);
	bool IsAlive() const { return isAlive; }
	EnemyType GetType() const { return type; }
	const Vector3& GetPosition() const { return position; }
	float GetRadius() const { return data->radius; }
	// 突進中か（突進中に体当たりされると痛い、などに使える）
	bool IsDashing() const { return state == State::kDash; }

private:
	// 動きの段階
	enum class State {
		kMove,    // ふつうに動く
		kWindup,  // 攻撃の前のため（止まって膨らむ＝合図）
		kDash,    // 突進中（突進する敵だけ）
		kRecover, // 攻撃の後のすき
	};

	void UpdateCharger(const Vector3& toTarget, float distance, float deltaTime, ShotRequest& shot);
	void UpdateShooter(const Vector3& toTarget, float distance, float deltaTime, ShotRequest& shot);
	void UpdateSwarm(const Vector3& toTarget, float distance, float deltaTime);
	void UpdateTank(const Vector3& toTarget, float distance, float deltaTime, ShotRequest& shot);
	void UpdateModel();
	float RandomRange(float min, float max);

	EnemyType type = EnemyType::kShooter;
	const Data* data = nullptr;
	std::unique_ptr<Object3d> object;
	std::minstd_rand random;

	Vector3 position = { 0.0f, 0.0f, 0.0f };
	Vector3 knockback = { 0.0f, 0.0f, 0.0f };
	float hp = 1.0f;
	bool isAlive = true;
	float hitTimer = 0.0f;     // 当たったときにぷるっと膨らむ演出の残り時間
	float facingAngle = 0.0f;

	State state = State::kMove;
	float stateTimer = 0.0f;   // 今の段階の残り時間
	float attackCooldown = 0.0f; // 次の攻撃までの時間
	Vector3 dashDirection = { 0.0f, 0.0f, 1.0f };
	float wobble = 0.0f;       // 群れの敵がふらふら動くための時間
	float strafeSign = 1.0f;   // 射撃の敵が横に回り込む向き

	static inline const float kHitTime = 0.12f;
};
