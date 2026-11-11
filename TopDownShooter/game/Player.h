#pragma once
#include <memory>
#include "Mathutility.h"
#include "Object3d.h"
#include "PlayerInput.h"
#include "CharacterData.h"

class Camera;

// プレイヤー（ガンジョン風：キビキビ歩く・狙う向きを向く・ロール回避で弾をすり抜ける）
class Player
{
public:
	// 撃った弾の情報（StageScene が弾を出すのに使う）
	struct ShotRequest {
		bool shoot = false;
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 direction = { 0.0f, 0.0f, 1.0f };
	};

	// character：選んだキャラ（見た目と初期ステータスが決まる）
	void Initialize(const Vector3& position, const CharacterData& character);
	// 更新（deltaTime：1フレームの秒数）。撃つフレームなら ShotRequest.shoot が true になる
	ShotRequest Update(const PlayerInput& input, float deltaTime);
	void Draw();

	// 位置を直す（壁に押し戻されたとき）
	void SetPosition(const Vector3& newPosition);

	void SetCamera(Camera* camera) { object->SetCamera(camera); }

	const Vector3& GetPosition() const { return position; }
	// 当たり判定の半径
	float GetRadius() const { return kRadius; }
	// ロール中の無敵か
	bool IsInvincible() const { return isRolling && rollTimer < kRollInvincibleTime; }
	// ダメージを受けられるか（ロールの無敵中・ダメージ直後の無敵中・倒れた後は受けない）
	bool CanTakeDamage() const { return isAlive && !IsInvincible() && damageInvincibleTimer <= 0.0f; }
	// ダメージを受ける（倒れたら true）
	bool TakeDamage(int32_t amount);
	bool IsAlive() const { return isAlive; }
	int32_t GetHp() const { return hp; }
	int32_t GetMaxHp() const { return stats.maxHp; }
	bool IsRolling() const { return isRolling; }
	PlayerStats& GetStats() { return stats; }

	// 弾が飛ぶ高さ（狙う点もこの高さで取る）
	static inline const float kShotHeight = 0.6f;

private:
	void UpdateMove(const PlayerInput& input, float deltaTime);
	void UpdateRoll(float deltaTime);
	void UpdateModel();

	std::unique_ptr<Object3d> object;
	PlayerStats stats;

	Vector3 position = { 0.0f, 0.0f, 0.0f };
	Vector3 velocity = { 0.0f, 0.0f, 0.0f };
	// 向いている角度（Y軸回り。0で +Z を向く）
	float facingAngle = 0.0f;
	// 撃てるようになるまでの時間
	float fireCooldown = 0.0f;

	// HP（1でハート半分）
	int32_t hp = 0;
	bool isAlive = true;
	// ダメージを受けた後の無敵の残り時間（この間は点滅する）
	float damageInvincibleTimer = 0.0f;

	// ロール回避
	bool isRolling = false;
	float rollTimer = 0.0f;
	Vector3 rollDirection = { 0.0f, 0.0f, 1.0f };

	// ---- 調整用の数値 ----
	static inline const float kRadius = 0.4f;
	static inline const float kAcceleration = 0.45f;     // 速さが目標に近づく割合（1で即座。ガンジョン風にほぼ慣性なし）
	static inline const float kRollTime = 0.45f;         // ロールの長さ（秒）
	static inline const float kRollInvincibleTime = 0.32f; // ロールの最初の、無敵の時間（秒）。残りは無防備
	static inline const float kRollSpeed = 13.0f;        // ロール中の速さ（最初の速さ。だんだん落ちる）
	static inline const float kMuzzleDistance = 0.6f;    // 体の中心から弾が出る位置までの距離
	static inline const float kDamageInvincibleTime = 1.0f; // ダメージを受けた後の無敵の時間（秒）
};