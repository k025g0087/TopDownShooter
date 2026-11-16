#pragma once
#include <cstdint>
#include <memory>
#include <random>
#include "Mathutility.h"
#include "Object3d.h"

class Camera;

// 敵の種類（倒した種類によって、プレイヤーの伸びるステータスが変わる）
enum class EnemyType {
	kCharger, // 突進：ためてから突っ込む（フェイント・2連続もある）　→ 移動速度が伸びる
	kShooter, // 射撃：距離を取って、3方向／連射／狙撃から選んで撃つ　→ 射程が伸びる
	kSwarm,   // 群れ：小さくて弱い。プレイヤーを囲んで、順番に飛びかかる　→ 連射が伸びる
	kTank,    // 重装：硬くて遅い。全方向／渦巻き／扇形から選んで撃つ　→ 威力が伸びる
	kCount,
};

// 敵が「いま見えている世界」（StageScene が毎フレーム作って渡す）
// 敵は壁やマップを直接は知らない。見えているか・どっちへ歩けばよいかなどを、ここから受け取る
struct EnemyPerception
{
	Vector3 targetPosition = { 0.0f, 0.0f, 0.0f }; // プレイヤーの位置
	Vector3 targetVelocity = { 0.0f, 0.0f, 0.0f }; // プレイヤーの動く速さ（先読みして撃つのに使う）
	bool hasLineOfSight = true;                    // 壁にさえぎられずに、プレイヤーが見えているか
	Vector3 flowDirection = { 0.0f, 0.0f, 0.0f };  // 壁をよけて近づくときの、次に歩く向き（長さ0なら使えない）
	bool canAttack = true;                         // 攻撃してよいか（同時に攻撃する敵の数を制限するための許可）
	bool comboReady = false;                       // 味方の射撃のすぐあとか（突進が、それにあわせて飛び込む）
	float ringAngle = 0.0f;                        // プレイヤーのまわりで、この敵が陣取る角度（ラジアン。みんなで囲む・挟むために、シーンが割り当てる）
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

	// 攻撃の予告に使うモデル（シーンで読み込む）
	static inline const char* kModelAimLine = "test_aim_line.obj";
	static inline const char* kModelRing = "test_warning.obj";

	// seed：撃つタイミングなどをばらけさせるための乱数の種
	void Initialize(EnemyType type, const Vector3& position, uint32_t seed);
	// 更新。撃つフレームなら ShotRequest.shoot が true になる
	ShotRequest Update(const EnemyPerception& perception, float deltaTime);
	void Draw();

	// 弾が当たった（倒れたら true）
	bool Hit(float damage, const Vector3& bulletVelocity);
	// 壁にぶつかった（突進が止まる・歩いていたら向きを変える）
	void OnHitWall();

	void SetCamera(Camera* camera);
	// 位置を直す（壁や他の敵に押し戻されたとき）
	void SetPosition(const Vector3& newPosition);
	bool IsAlive() const { return isAlive; }
	EnemyType GetType() const { return type; }
	const Vector3& GetPosition() const { return position; }
	float GetRadius() const { return data->radius; }
	// 突進（飛びかかり）中か
	bool IsDashing() const { return state == State::kDash; }
	// 攻撃中か（ため・突進・連射の最中）。同時に攻撃する数の制限に使う
	bool IsAttacking() const { return state == State::kWindup || state == State::kDash || state == State::kFire; }

private:
	// 動きの段階
	enum class State {
		kMove,    // ふつうに動く（近づく・距離をとる・囲む）
		kWindup,  // 攻撃の前のため（止まって膨らむ＝合図）
		kDash,    // 突進・飛びかかり中
		kFire,    // 連射・渦巻きなど、撃ち続けている最中
		kRecover, // 攻撃の後のすき
	};
	// 今やろうとしている攻撃の種類
	enum class Attack {
		kNone,
		kSpread, // 射撃：3方向
		kBurst,  // 射撃：連射（3発）
		kSnipe,  // 射撃：狙撃（長いため＋細い線の予告＋速い1発）
		kDash,   // 突進：ふつう
		kFeint,  // 突進：フェイント（一度止まってから突っ込む）
		kDouble, // 突進：2連続
		kRing,   // 重装：全方向
		kSpiral, // 重装：渦巻き
		kFan,    // 重装：扇形（プレイヤーの方へ5発）
		kLunge,  // 群れ：飛びかかり
	};

	void UpdateCharger(const EnemyPerception& p, const Vector3& direction, float distance, float deltaTime);
	void UpdateShooter(const EnemyPerception& p, const Vector3& direction, float distance, float deltaTime, ShotRequest& shot);
	void UpdateSwarm(const EnemyPerception& p, const Vector3& direction, float distance, float deltaTime);
	void UpdateTank(const EnemyPerception& p, const Vector3& direction, float distance, float deltaTime, ShotRequest& shot);
	void UpdateModel();
	void UpdateTelegraph();
	float RandomRange(float min, float max);

	// 近づく向き：見えているなら、まっすぐ。見えていないなら、壁をよける道順
	Vector3 ChaseDirection(const EnemyPerception& p, const Vector3& direction) const;
	// 先読みした狙いの向き（弾の速さ speed で飛ばしたとき、動いているプレイヤーに当たる向き）
	Vector3 AimDirection(const EnemyPerception& p, float speed) const;
	// プレイヤーのまわりを回る動き（desired：回る距離）。攻撃の順番待ちのときなど
	Vector3 OrbitMove(const Vector3& direction, float distance, float desired) const;
	// 次に回る向きを、ときどき入れかえる
	void UpdateStrafe(float deltaTime);
	// ため（攻撃の準備）に入る
	void StartWindup(Attack next, float time);
	// 攻撃のあとのすきに入る（終わったら次の攻撃までの待ちに入る）
	void StartRecover(float time, float cooldownMin, float cooldownMax);

	EnemyType type = EnemyType::kShooter;
	const Data* data = nullptr;
	std::unique_ptr<Object3d> object;
	std::unique_ptr<Object3d> aimLine; // 狙撃・突進の予告の線
	std::unique_ptr<Object3d> ring;    // 重装のため中の輪
	std::minstd_rand random;

	Vector3 position = { 0.0f, 0.0f, 0.0f };
	Vector3 knockback = { 0.0f, 0.0f, 0.0f };
	float hp = 1.0f;
	bool isAlive = true;
	float hitTimer = 0.0f;     // 当たったときにぷるっと膨らむ演出の残り時間
	float facingAngle = 0.0f;

	State state = State::kMove;
	Attack attack = Attack::kNone;
	float stateTimer = 0.0f;   // 今の段階の残り時間
	float windupTotal = 0.5f;  // 今のための長さ（膨らみ方の計算に使う）
	float attackCooldown = 0.0f; // 次の攻撃までの時間
	float cooldownMin = 1.0f, cooldownMax = 2.0f; // 攻撃のあと、次までに待つ時間（recoverのあとに使う）
	bool thenWindup = false;   // すきのあとに、もう一度ために入る（フェイント・2連続）
	bool feintDone = false;    // フェイントのやり直しを済ませたか
	int32_t dashesLeft = 0;    // あと何回、突進するか（2連続）
	int32_t fireLeft = 0;      // あと何発、撃つか（連射）
	float fireTimer = 0.0f;    // 次の1発までの時間（連射・渦巻き）
	float spiralAngle = 0.0f;  // 渦巻きの今の向き
	Vector3 dashDirection = { 0.0f, 0.0f, 1.0f }; // 突進・狙撃の向き（ため中に決まる）
	float wobble = 0.0f;       // ふらふら動くための時間
	float strafeSign = 1.0f;   // 横に回り込む向き
	float strafeTimer = 0.0f;  // 次に向きを変えるまでの時間

	// 性格（敵ごとに少しずつ違う。1より大きいと、せっかちで狙いも正確）
	float aggression = 1.0f;
	float leadRate = 0.8f;     // 先読みの強さ（1で完全な先読み）

	// 予告の見た目
	bool aimVisible = false;
	float aimLength = 0.0f;
	float aimWidth = 0.2f;
	bool ringVisible = false;
	float ringScale = 1.0f;

	static inline const float kHitTime = 0.12f;
};
