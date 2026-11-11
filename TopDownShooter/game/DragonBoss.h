#pragma once
#include <array>
#include <memory>
#include <random>
#include "BossBase.h"
#include "Object3d.h"

// ドラゴン（神殿の主）
// 動く → 攻撃（炎のブレス／しっぽのなぎ払い／飛び上がって炎の雨）をくり返す
// HPが半分を切ると第2形態（速く・激しく）
class DragonBoss : public BossBase
{
public:
	// 使うモデル（シーンで読み込む）
	static inline const char* kModelBody = "test_dragon.obj";
	static inline const char* kModelShadow = "test_shadow.obj";
	static inline const char* kModelWarning = "test_warning.obj";
	static inline const char* kTextureName = "resources/boss_name_dragon.png";

	void Initialize(const Vector3& position, const Vector3& roomCenter, const Vector2& roomHalfSize, uint32_t seed) override;
	void Update(const Vector3& target, float deltaTime, Events& events) override;
	void Draw() override;
	void SetCamera(Camera* camera) override;

	bool Hit(float damage) override;
	void SetPosition(const Vector3& newPosition) override { position = newPosition; }

	const Vector3& GetPosition() const override { return position; }
	float GetRadius() const override { return kRadius; }
	float GetHpRate() const override { return hp / kMaxHp; }
	bool IsAlive() const override { return hp > 0.0f; }
	bool IsDeathFinished() const override { return state == State::kDead; }
	bool IsHittable() const override;
	bool IsTouchDangerous() const override;
	float GetAreaAttackRadius() const override;
	bool UsesWallCollision() const override { return height < 0.5f; }
	const char* GetNameTexture() const override { return kTextureName; }

private:
	enum class State {
		kIntro,        // 登場（地面から出てきて吠える）
		kMove,         // プレイヤーへ歩く
		kBreathWindup, // ブレスの前のため（首を引く）
		kBreath,       // 炎のブレス
		kSweepWindup,  // なぎ払いの前のため（しゃがむ）
		kSweep,        // しっぽのなぎ払い（その場で1回転）
		kTakeoff,      // 飛び上がる
		kFlying,       // 空から炎の雨を降らせる
		kLanding,      // 着地（衝撃波）
		kPhaseChange,  // 第2形態への変化（吠える）
		kDying,        // 倒れる演出
		kDead,         // 倒れた
	};

	// 炎の雨の予告（床の赤い輪。時間が来ると爆発する）
	struct Warning {
		bool active = false;
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		float timer = 0.0f;
		std::unique_ptr<Object3d> object;
	};

	void ChangeState(State next, float duration);
	void ChooseNextAttack(float distance);
	void AddRing(Events& events, const Vector3& center, int32_t count, float speed, float offset, int32_t style);
	void SpawnWarning(const Vector3& target);
	void UpdateWarnings(float deltaTime, Events& events);
	void ClampToRoom();
	void TurnToward(const Vector3& target, float maxTurn);
	void UpdateModel();
	float RandomRange(float min, float max);
	// 第2形態なら速くする倍率
	float PhaseRate() const { return isPhase2 ? 1.35f : 1.0f; }

	std::unique_ptr<Object3d> body;
	std::unique_ptr<Object3d> shadow;
	std::array<Warning, 16> warnings;
	std::minstd_rand random;

	Vector3 position = { 0.0f, 0.0f, 0.0f };
	Vector3 roomCenter = { 0.0f, 0.0f, 0.0f };
	Vector2 roomHalfSize = { 10.0f, 10.0f };
	float facingAngle = 0.0f;
	float height = 0.0f;       // 地面からの高さ（飛んでいるとき）
	float hp = kMaxHp;
	bool isPhase2 = false;

	State state = State::kIntro;
	float stateTimer = 0.0f;    // 今の段階の残り時間
	float stateDuration = 1.0f; // 今の段階の長さ（動きの割合の計算に使う）
	float fireTimer = 0.0f;     // ブレス・炎の雨の次の発射までの時間
	float hitTimer = 0.0f;      // 当たったときの膨らみ
	float time = 0.0f;
	State lastAttack = State::kMove; // 同じ攻撃が続きすぎないように

	// ---- 調整用の数値 ----
	static inline const float kMaxHp = 450.0f;
	static inline const float kRadius = 1.6f;
	static inline const float kMoveSpeed = 2.2f;
	static inline const float kSweepRadius = 4.2f;   // なぎ払いの届く範囲
	static inline const float kFlyHeight = 6.0f;
	static inline const float kWarningTime = 0.9f;   // 炎の雨の予告から爆発まで（秒）
};
