#include "Enemy.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>
#include "Object3dCommon.h"

using namespace MathUtility;

namespace {

	// ---- 種類ごとの数値（ここをいじると敵の強さが変わる）----
	const std::array<Enemy::Data, static_cast<size_t>(EnemyType::kCount)> kEnemyData = { {
		//  名前        モデル                 HP     半径   速さ   のけぞり
		{ "Charger", "test_charger.obj",  6.0f, 0.5f,  2.2f, 0.8f },
		{ "Shooter", "test_shooter.obj",  5.0f, 0.5f,  1.8f, 1.0f },
		{ "Swarm",   "test_swarm.obj",    2.0f, 0.32f, 3.4f, 1.3f },
		{ "Tank",    "test_tank.obj",    16.0f, 0.85f, 0.9f, 0.25f },
	} };

	// 突進
	const float kChargerRange = 9.0f;        // この距離より近いと突進の準備に入る
	const float kChargerWindup = 0.6f;       // ため（秒）
	const float kChargerDashSpeed = 15.0f;   // 突進の速さ
	const float kChargerDashTime = 0.45f;    // 突進の長さ（秒）
	const float kChargerRecover = 0.8f;      // 突進の後のすき（秒）

	// 射撃
	const float kShooterNear = 6.0f;         // これより近いと離れる
	const float kShooterFar = 10.0f;         // これより遠いと近づく
	const float kShooterWindup = 0.45f;
	const float kShooterIntervalMin = 2.2f;
	const float kShooterIntervalMax = 3.4f;

	// 重装
	const float kTankWindup = 0.7f;
	const float kTankIntervalMin = 3.5f;
	const float kTankIntervalMax = 4.5f;
	const int32_t kTankShotCount = 10;       // 周りにばらまく弾の数
}

const Enemy::Data& Enemy::GetData(EnemyType type)
{
	size_t index = static_cast<size_t>(type);
	assert(index < kEnemyData.size());
	return kEnemyData[index];
}

void Enemy::Initialize(EnemyType enemyType, const Vector3& startPosition, uint32_t seed)
{
	type = enemyType;
	data = &GetData(type);
	random.seed(seed);

	object = std::make_unique<Object3d>();
	object->Initialize(Object3dCommon::GetInstance());
	object->SetModel(data->modelName);

	position = startPosition;
	knockback = { 0.0f, 0.0f, 0.0f };
	hp = data->maxHp;
	isAlive = true;
	hitTimer = 0.0f;
	state = State::kMove;
	stateTimer = 0.0f;
	// 出てきてすぐは攻撃しない（ばらける）
	attackCooldown = RandomRange(1.0f, 2.5f);
	wobble = RandomRange(0.0f, 10.0f);
	strafeSign = (RandomRange(0.0f, 1.0f) < 0.5f) ? -1.0f : 1.0f;
	UpdateModel();
}

float Enemy::RandomRange(float min, float max)
{
	std::uniform_real_distribution<float> distribution(min, max);
	return distribution(random);
}

Enemy::ShotRequest Enemy::Update(const Vector3& target, float deltaTime)
{
	ShotRequest shot;
	if (!isAlive) {
		return shot;
	}

	Vector3 toTarget = { target.x - position.x, 0.0f, target.z - position.z };
	float distance = Length(toTarget);

	switch (type) {
	case EnemyType::kCharger:
		UpdateCharger(toTarget, distance, deltaTime, shot);
		break;
	case EnemyType::kShooter:
		UpdateShooter(toTarget, distance, deltaTime, shot);
		break;
	case EnemyType::kSwarm:
		UpdateSwarm(toTarget, distance, deltaTime);
		break;
	case EnemyType::kTank:
	default:
		UpdateTank(toTarget, distance, deltaTime, shot);
		break;
	}

	// のけぞり（だんだん弱まる）
	position += knockback * deltaTime;
	knockback = knockback * 0.85f;

	hitTimer -= deltaTime;
	UpdateModel();
	return shot;
}

void Enemy::UpdateCharger(const Vector3& toTarget, float distance, float deltaTime, ShotRequest& /*shot*/)
{
	Vector3 direction = (distance > 0.001f) ? toTarget / distance : Vector3{ 0.0f, 0.0f, 1.0f };
	stateTimer -= deltaTime;
	switch (state) {
	case State::kMove:
		// 近づいて、届く距離になったら突進の準備
		position += direction * data->moveSpeed * deltaTime;
		facingAngle = std::atan2(direction.x, direction.z);
		attackCooldown -= deltaTime;
		if (attackCooldown <= 0.0f && distance < kChargerRange) {
			state = State::kWindup;
			stateTimer = kChargerWindup;
		}
		break;
	case State::kWindup:
		// ため中はプレイヤーを狙い続け、終わった瞬間の向きで突っ込む
		facingAngle = std::atan2(direction.x, direction.z);
		dashDirection = direction;
		if (stateTimer <= 0.0f) {
			state = State::kDash;
			stateTimer = kChargerDashTime;
		}
		break;
	case State::kDash:
		position += dashDirection * kChargerDashSpeed * deltaTime;
		if (stateTimer <= 0.0f) {
			state = State::kRecover;
			stateTimer = kChargerRecover;
		}
		break;
	case State::kRecover:
	default:
		if (stateTimer <= 0.0f) {
			state = State::kMove;
			attackCooldown = RandomRange(0.6f, 1.4f);
		}
		break;
	}
}

void Enemy::UpdateShooter(const Vector3& toTarget, float distance, float deltaTime, ShotRequest& shot)
{
	Vector3 direction = (distance > 0.001f) ? toTarget / distance : Vector3{ 0.0f, 0.0f, 1.0f };
	facingAngle = std::atan2(direction.x, direction.z);
	stateTimer -= deltaTime;
	if (state == State::kWindup) {
		if (stateTimer <= 0.0f) {
			shot.shoot = true;
			shot.direction = direction;
			shot.position = { position.x + direction.x * data->radius, 0.6f, position.z + direction.z * data->radius };
			shot.count = 3;
			shot.spread = 0.3f;
			shot.speed = 7.0f;
			state = State::kMove;
			attackCooldown = RandomRange(kShooterIntervalMin, kShooterIntervalMax);
		}
		return;
	}

	// ちょうどいい距離を保つ（近いと離れる・遠いと近づく・その間は横に回り込む）
	Vector3 move = { 0.0f, 0.0f, 0.0f };
	if (distance < kShooterNear) {
		move = -direction;
	} else if (distance > kShooterFar) {
		move = direction;
	} else {
		move = Vector3{ -direction.z, 0.0f, direction.x } * (strafeSign * 0.6f);
	}
	position += move * data->moveSpeed * deltaTime;

	attackCooldown -= deltaTime;
	if (attackCooldown <= 0.0f && distance < 15.0f) {
		state = State::kWindup;
		stateTimer = kShooterWindup;
		// 次は反対側に回り込む
		strafeSign = -strafeSign;
	}
}

void Enemy::UpdateSwarm(const Vector3& toTarget, float distance, float deltaTime)
{
	// ふらふらしながら一直線に寄ってくる
	wobble += deltaTime;
	Vector3 direction = (distance > 0.001f) ? toTarget / distance : Vector3{ 0.0f, 0.0f, 1.0f };
	Vector3 side = { -direction.z, 0.0f, direction.x };
	Vector3 move = direction + side * (std::sin(wobble * 5.0f) * 0.5f);
	position += Normalize(move) * data->moveSpeed * deltaTime;
	facingAngle = std::atan2(direction.x, direction.z);
}

void Enemy::UpdateTank(const Vector3& toTarget, float distance, float deltaTime, ShotRequest& shot)
{
	Vector3 direction = (distance > 0.001f) ? toTarget / distance : Vector3{ 0.0f, 0.0f, 1.0f };
	stateTimer -= deltaTime;
	if (state == State::kWindup) {
		if (stateTimer <= 0.0f) {
			// 周り全部にばらまく（少しずつ回して、毎回すき間の位置が変わるように）
			shot.shoot = true;
			float offset = RandomRange(0.0f, std::numbers::pi_v<float> * 2.0f);
			shot.direction = { std::sin(offset), 0.0f, std::cos(offset) };
			shot.position = { position.x, 0.6f, position.z };
			shot.count = kTankShotCount;
			shot.spread = std::numbers::pi_v<float> * 2.0f / static_cast<float>(kTankShotCount);
			shot.speed = 5.5f;
			state = State::kMove;
			attackCooldown = RandomRange(kTankIntervalMin, kTankIntervalMax);
		}
		return;
	}
	// ゆっくり近づく
	if (distance > 1.5f) {
		position += direction * data->moveSpeed * deltaTime;
	}
	facingAngle = std::atan2(direction.x, direction.z);
	attackCooldown -= deltaTime;
	if (attackCooldown <= 0.0f && distance < 16.0f) {
		state = State::kWindup;
		stateTimer = kTankWindup;
	}
}

void Enemy::UpdateModel()
{
	// 当たったときの膨らみ
	float pop = (hitTimer > 0.0f) ? 1.0f + 0.25f * (hitTimer / kHitTime) : 1.0f;
	Vector3 scale = { pop, pop, pop };
	Vector3 rotate = { 0.0f, facingAngle, 0.0f };
	Vector3 translate = position;

	if (state == State::kWindup) {
		// ため：横に膨らんで縦につぶれる（＝攻撃の合図）。突進する敵は後ろにのけぞる
		float windupTime = (type == EnemyType::kCharger) ? kChargerWindup : (type == EnemyType::kTank) ? kTankWindup : kShooterWindup;
		float t = 1.0f - (std::max)(stateTimer, 0.0f) / windupTime;
		scale.x *= 1.0f + 0.3f * t;
		scale.z *= 1.0f + 0.3f * t;
		scale.y *= 1.0f - 0.2f * t;
		if (type == EnemyType::kCharger) {
			rotate.x = -0.4f * t;
			// 小刻みに震える
			translate.x += std::sin(stateTimer * 80.0f) * 0.05f;
		}
	} else if (state == State::kDash) {
		// 突進：前に倒れ込む
		rotate.x = 0.35f;
		scale.z *= 1.2f;
	} else if (type == EnemyType::kSwarm) {
		// 群れ：ぴょこぴょこ跳ねる
		translate.y += std::fabs(std::sin(wobble * 10.0f)) * 0.15f;
	}

	object->SetScale(scale);
	object->SetRotate(rotate);
	object->SetTranslate(translate);
	object->Update();
}

bool Enemy::Hit(float damage, const Vector3& bulletVelocity)
{
	if (!isAlive) {
		return false;
	}
	hp -= damage;
	hitTimer = kHitTime;
	// 弾が飛んできた向きに少し押される（重い敵はあまり動かない）
	knockback = Normalize(Vector3{ bulletVelocity.x, 0.0f, bulletVelocity.z }) * (6.0f * data->knockbackRate);
	if (hp <= 0.0f) {
		isAlive = false;
		return true;
	}
	return false;
}

void Enemy::OnHitWall()
{
	// 突進中に壁にぶつかったら止まって、すきができる
	if (state == State::kDash) {
		state = State::kRecover;
		stateTimer = kChargerRecover;
	}
}

void Enemy::SetPosition(const Vector3& newPosition)
{
	position = newPosition;
	UpdateModel();
}

void Enemy::Draw()
{
	if (isAlive) {
		object->Draw();
	}
}
