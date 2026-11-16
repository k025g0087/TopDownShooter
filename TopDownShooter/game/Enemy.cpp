#include "Enemy.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>
#include "Object3dCommon.h"

using namespace MathUtility;

namespace {

	const float kPi = std::numbers::pi_v<float>;

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
	const float kChargerWait = 6.0f;         // 順番待ち・すきのときに、プレイヤーのまわりを回る距離
	const float kChargerWindup = 0.6f;       // ため（秒）
	const float kChargerDoubleWindup = 0.5f; // 2連続のとき、最初のため
	const float kChargerChainWindup = 0.3f;  // フェイント・2連続の、2回目のため
	const float kChargerDashSpeed = 15.0f;   // 突進の速さ
	const float kChargerDashTime = 0.45f;    // 突進の長さ（秒）
	const float kChargerRecover = 0.8f;      // 突進の後のすき（秒）
	const float kChargerFeintTime = 0.45f;   // フェイントで止まる（さがる）時間

	// 射撃
	const float kShooterNear = 5.5f;         // これより近いと離れる
	const float kShooterFar = 10.5f;         // これより遠いと近づく
	const float kShooterSpreadWindup = 0.45f;
	const float kShooterBurstWindup = 0.38f;
	const float kShooterSnipeWindup = 1.0f;
	const float kSnipeLockTime = 0.35f;      // 狙撃：撃つ少し前に狙いを固定する（よける時間）
	const float kSnipeSpeed = 15.0f;
	const float kSnipeLength = 16.0f;        // 予告の線の長さ
	const float kBurstGap = 0.13f;           // 連射の間隔
	const float kShooterIntervalMin = 2.2f;
	const float kShooterIntervalMax = 3.4f;

	// 群れ
	const float kSwarmRing = 3.8f;           // プレイヤーを囲む輪の半径
	const float kSwarmLungeRange = 7.5f;
	const float kSwarmWindup = 0.28f;
	const float kSwarmLungeSpeed = 8.5f;
	const float kSwarmLungeTime = 0.55f;

	// 重装
	const float kTankRingWindup = 0.7f;
	const float kTankSpiralWindup = 0.55f;
	const float kTankFanWindup = 0.6f;
	const float kTankIntervalMin = 3.5f;
	const float kTankIntervalMax = 4.5f;
	const int32_t kTankShotCount = 10;       // 全方向のときの弾の数
	const int32_t kTankSpiralTicks = 12;     // 渦巻き：撃つ回数
	const float kTankSpiralGap = 0.12f;      // 渦巻き：撃つ間隔

	// 角度の差を -π〜π にする
	float WrapAngle(float angle)
	{
		while (angle > kPi) { angle -= kPi * 2.0f; }
		while (angle < -kPi) { angle += kPi * 2.0f; }
		return angle;
	}
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
	aimLine = std::make_unique<Object3d>();
	aimLine->Initialize(Object3dCommon::GetInstance());
	aimLine->SetModel(kModelAimLine);
	ring = std::make_unique<Object3d>();
	ring->Initialize(Object3dCommon::GetInstance());
	ring->SetModel(kModelRing);

	position = startPosition;
	knockback = { 0.0f, 0.0f, 0.0f };
	hp = data->maxHp;
	isAlive = true;
	hitTimer = 0.0f;
	state = State::kMove;
	attack = Attack::kNone;
	stateTimer = 0.0f;
	thenWindup = false;
	feintDone = false;
	dashesLeft = 0;
	fireLeft = 0;
	fireTimer = 0.0f;
	aimVisible = false;
	ringVisible = false;
	// 性格：敵ごとに、せっかちさと先読みの強さを少しずつ変える（みんな同じ動きにならないように）
	aggression = RandomRange(0.85f, 1.2f);
	leadRate = RandomRange(0.6f, 1.0f);
	// 出てきてすぐは攻撃しない（ばらける）
	attackCooldown = RandomRange(1.0f, 2.5f);
	wobble = RandomRange(0.0f, 10.0f);
	strafeSign = (RandomRange(0.0f, 1.0f) < 0.5f) ? -1.0f : 1.0f;
	strafeTimer = RandomRange(0.5f, 2.0f);
	UpdateModel();
}

float Enemy::RandomRange(float min, float max)
{
	std::uniform_real_distribution<float> distribution(min, max);
	return distribution(random);
}

void Enemy::SetCamera(Camera* camera)
{
	object->SetCamera(camera);
	aimLine->SetCamera(camera);
	ring->SetCamera(camera);
}

Vector3 Enemy::ChaseDirection(const EnemyPerception& p, const Vector3& direction) const
{
	// 見えているなら、まっすぐ。見えていないなら、壁をよける道順（なければ、まっすぐ）
	if (p.hasLineOfSight || Length(p.flowDirection) < 0.01f) {
		return direction;
	}
	return p.flowDirection;
}

Vector3 Enemy::AimDirection(const EnemyPerception& p, float speed) const
{
	Vector3 toTarget = { p.targetPosition.x - position.x, 0.0f, p.targetPosition.z - position.z };
	float distance = Length(toTarget);
	if (distance < 0.001f) {
		return { 0.0f, 0.0f, 1.0f };
	}
	// 弾が飛んでいる間にプレイヤーが動く分を足した場所へ向ける（長く飛ぶほど、先を狙う）
	float flightTime = (std::min)(distance / (std::max)(speed, 0.1f), 1.0f);
	Vector3 predicted = {
		p.targetPosition.x + p.targetVelocity.x * flightTime * leadRate,
		0.0f,
		p.targetPosition.z + p.targetVelocity.z * flightTime * leadRate };
	Vector3 aim = { predicted.x - position.x, 0.0f, predicted.z - position.z };
	return (Length(aim) > 0.001f) ? Normalize(aim) : toTarget / distance;
}

Vector3 Enemy::OrbitMove(const Vector3& direction, float distance, float desired) const
{
	// 遠ければ近づき、近ければ離れ、ちょうどよければ横に回る
	Vector3 radial = { 0.0f, 0.0f, 0.0f };
	if (distance < desired - 0.8f) {
		radial = -direction;
	} else if (distance > desired + 0.8f) {
		radial = direction;
	}
	Vector3 tangent = Vector3{ -direction.z, 0.0f, direction.x } * strafeSign;
	return Normalize(radial + tangent * 0.8f);
}

void Enemy::UpdateStrafe(float deltaTime)
{
	strafeTimer -= deltaTime;
	if (strafeTimer <= 0.0f) {
		strafeTimer = RandomRange(1.2f, 2.8f);
		strafeSign = (RandomRange(0.0f, 1.0f) < 0.5f) ? -1.0f : 1.0f;
	}
}

void Enemy::StartWindup(Attack next, float time)
{
	state = State::kWindup;
	attack = next;
	// 毎回ぴったり同じ長さだと読まれやすいので、ほんの少しだけばらす
	windupTotal = time * RandomRange(0.94f, 1.06f);
	stateTimer = windupTotal;
}

void Enemy::StartRecover(float time, float newCooldownMin, float newCooldownMax)
{
	state = State::kRecover;
	stateTimer = time;
	cooldownMin = newCooldownMin;
	cooldownMax = newCooldownMax;
}

Enemy::ShotRequest Enemy::Update(const EnemyPerception& p, float deltaTime)
{
	ShotRequest shot;
	if (!isAlive) {
		return shot;
	}

	// 予告の表示は、ため中の敵が毎フレーム出し直す
	aimVisible = false;
	ringVisible = false;
	wobble += deltaTime;

	Vector3 toTarget = { p.targetPosition.x - position.x, 0.0f, p.targetPosition.z - position.z };
	float distance = Length(toTarget);
	Vector3 direction = (distance > 0.001f) ? toTarget / distance : Vector3{ 0.0f, 0.0f, 1.0f };

	if (state == State::kRecover) {
		// 攻撃のあとのすき（立ち止まる）。フェイントのときは、後ろにさがる
		stateTimer -= deltaTime;
		if (type != EnemyType::kCharger) {
			facingAngle = std::atan2(direction.x, direction.z);
		}
		if (attack == Attack::kFeint && thenWindup) {
			position -= dashDirection * (2.5f * deltaTime);
		}
		if (stateTimer <= 0.0f) {
			if (thenWindup) {
				// フェイント・2連続：もう一度ためて、もう一度突っ込む
				thenWindup = false;
				StartWindup(attack, kChargerChainWindup);
			} else {
				state = State::kMove;
				attack = Attack::kNone;
				feintDone = false;
				attackCooldown = RandomRange(cooldownMin, cooldownMax) / aggression;
			}
		}
	} else {
		switch (type) {
		case EnemyType::kCharger:
			UpdateCharger(p, direction, distance, deltaTime);
			break;
		case EnemyType::kShooter:
			UpdateShooter(p, direction, distance, deltaTime, shot);
			break;
		case EnemyType::kSwarm:
			UpdateSwarm(p, direction, distance, deltaTime);
			break;
		case EnemyType::kTank:
		default:
			UpdateTank(p, direction, distance, deltaTime, shot);
			break;
		}
	}

	// のけぞり（だんだん弱まる）
	position += knockback * deltaTime;
	knockback = knockback * 0.85f;

	hitTimer -= deltaTime;
	UpdateModel();
	UpdateTelegraph();
	return shot;
}

// ---------------------------------------------------------------- 突進
void Enemy::UpdateCharger(const EnemyPerception& p, const Vector3& direction, float distance, float deltaTime)
{
	stateTimer -= deltaTime;
	switch (state) {
	case State::kMove: {
		UpdateStrafe(deltaTime);
		attackCooldown -= deltaTime;
		// 味方が撃った直後なら、少し早めに飛び込む（連携）
		bool ready = attackCooldown <= 0.0f || (p.comboReady && attackCooldown < 1.2f);
		facingAngle = std::atan2(direction.x, direction.z);

		Vector3 move = { 0.0f, 0.0f, 0.0f };
		if (!p.hasLineOfSight) {
			// 見えないときは、壁をよけて、見えるところまで回り込む
			move = ChaseDirection(p, direction);
		} else if (ready && p.canAttack) {
			// 飛び込める距離まで近づく
			if (distance > kChargerRange * 0.8f) {
				move = direction;
			}
		} else {
			// 次の準備中・順番待ち：近づきすぎず、プレイヤーのまわりを回る
			move = OrbitMove(direction, distance, kChargerWait);
		}
		position += move * (data->moveSpeed * deltaTime);

		if (ready && p.canAttack && p.hasLineOfSight && distance < kChargerRange) {
			// 突進の種類を選ぶ（近すぎるときはフェイントにしない）
			float dashWeight = 55.0f;
			float feintWeight = (distance > 4.0f) ? 25.0f : 0.0f;
			float doubleWeight = 20.0f;
			float roll = RandomRange(0.0f, dashWeight + feintWeight + doubleWeight);
			feintDone = false;
			dashesLeft = 0;
			if (roll < dashWeight) {
				StartWindup(Attack::kDash, kChargerWindup);
			} else if (roll < dashWeight + feintWeight) {
				StartWindup(Attack::kFeint, kChargerWindup);
			} else {
				dashesLeft = 1;
				StartWindup(Attack::kDouble, kChargerDoubleWindup);
			}
			dashDirection = AimDirection(p, kChargerDashSpeed);
		}
		break;
	}
	case State::kWindup:
		// ため中はプレイヤーを狙い続け、終わった瞬間の向きで突っ込む（動いていれば少し先を狙う）
		dashDirection = AimDirection(p, kChargerDashSpeed);
		facingAngle = std::atan2(dashDirection.x, dashDirection.z);
		// 突っ込む先に、赤い線を出す（フェイントのときも同じ。だまされる）
		aimVisible = true;
		aimLength = kChargerDashSpeed * kChargerDashTime;
		aimWidth = data->radius * 2.0f;
		if (stateTimer <= 0.0f) {
			if (attack == Attack::kFeint && !feintDone) {
				// フェイント：突っ込まずに、さがって、もう一度ためる
				feintDone = true;
				thenWindup = true;
				StartRecover(kChargerFeintTime, 0.9f, 1.8f);
			} else {
				state = State::kDash;
				stateTimer = kChargerDashTime;
			}
		}
		break;
	case State::kDash:
		facingAngle = std::atan2(dashDirection.x, dashDirection.z);
		position += dashDirection * (kChargerDashSpeed * deltaTime);
		if (stateTimer <= 0.0f) {
			if (attack == Attack::kDouble && dashesLeft > 0) {
				// 2連続：すぐにもう一度
				--dashesLeft;
				thenWindup = true;
				StartRecover(0.22f, 0.9f, 1.8f);
			} else {
				StartRecover(kChargerRecover, 0.9f, 1.8f);
			}
		}
		break;
	default:
		state = State::kMove;
		break;
	}
}

// ---------------------------------------------------------------- 射撃
void Enemy::UpdateShooter(const EnemyPerception& p, const Vector3& direction, float distance, float deltaTime, ShotRequest& shot)
{
	// 弾の出る場所（体の少し前）
	auto muzzle = [this](const Vector3& dir) {
		return Vector3{ position.x + dir.x * data->radius, 0.6f, position.z + dir.z * data->radius };
	};

	switch (state) {
	case State::kWindup: {
		stateTimer -= deltaTime;
		if (attack == Attack::kSnipe) {
			// 狙撃：最初は狙い続けて、撃つ少し前に狙いを固定する（そのあいだに、よけられる）
			if (stateTimer > kSnipeLockTime) {
				dashDirection = AimDirection(p, kSnipeSpeed);
			}
			facingAngle = std::atan2(dashDirection.x, dashDirection.z);
			aimVisible = true;
			aimLength = kSnipeLength;
			// 固定されたら、線が太く（濃く）なる
			aimWidth = (stateTimer > kSnipeLockTime) ? 0.12f : 0.3f;
		} else {
			facingAngle = std::atan2(direction.x, direction.z);
		}
		if (stateTimer > 0.0f) {
			break;
		}
		switch (attack) {
		case Attack::kBurst:
			// 連射：1発ずつ、そのときのプレイヤーを狙って3発
			state = State::kFire;
			fireLeft = 3;
			fireTimer = 0.0f;
			break;
		case Attack::kSnipe:
			shot.shoot = true;
			shot.direction = dashDirection;
			shot.position = muzzle(dashDirection);
			shot.count = 1;
			shot.spread = 0.0f;
			shot.speed = kSnipeSpeed;
			shot.lifeTime = 2.0f;
			StartRecover(0.6f, 3.0f, 4.2f);
			break;
		case Attack::kSpread:
		default: {
			Vector3 aim = AimDirection(p, 7.0f);
			shot.shoot = true;
			shot.direction = aim;
			shot.position = muzzle(aim);
			shot.count = 3;
			shot.spread = 0.3f;
			shot.speed = 7.0f;
			StartRecover(0.2f, kShooterIntervalMin, kShooterIntervalMax);
			break;
		}
		}
		break;
	}
	case State::kFire: {
		// 連射：少し間をあけて1発ずつ
		facingAngle = std::atan2(direction.x, direction.z);
		fireTimer -= deltaTime;
		if (fireTimer <= 0.0f) {
			Vector3 aim = AimDirection(p, 8.5f);
			shot.shoot = true;
			shot.direction = aim;
			shot.position = muzzle(aim);
			shot.count = 1;
			shot.spread = 0.0f;
			shot.speed = 8.5f;
			--fireLeft;
			fireTimer = kBurstGap;
			if (fireLeft <= 0) {
				StartRecover(0.3f, kShooterIntervalMin, kShooterIntervalMax);
			}
		}
		break;
	}
	case State::kMove:
	default: {
		facingAngle = std::atan2(direction.x, direction.z);
		Vector3 move = { 0.0f, 0.0f, 0.0f };
		if (!p.hasLineOfSight) {
			// 壁にさえぎられているときは、見えるところまで回り込む（少し急ぐ）
			move = ChaseDirection(p, direction) * 1.15f;
		} else if (distance < kShooterNear) {
			move = -direction;
		} else if (distance > kShooterFar) {
			move = direction;
		} else {
			// ちょうどいい距離：割り当てられた角度（みんなでプレイヤーを挟むための場所）へ、ぐるっと回る
			float current = std::atan2(position.x - p.targetPosition.x, position.z - p.targetPosition.z);
			float diff = WrapAngle(p.ringAngle - current);
			Vector3 tangent = { std::cos(current), 0.0f, -std::sin(current) };
			if (std::fabs(diff) > 0.12f) {
				move = tangent * ((diff > 0.0f) ? 0.75f : -0.75f);
			} else {
				// 着いたら、ゆらゆら動いて的をしぼらせない
				move = tangent * (std::sin(wobble * 2.0f) * 0.3f);
			}
		}
		position += move * (data->moveSpeed * deltaTime);

		attackCooldown -= deltaTime;
		// 見えていて、許可があるときだけ攻撃する（壁越しには撃たない）
		if (attackCooldown <= 0.0f && p.canAttack && p.hasLineOfSight && distance < 14.0f) {
			float spreadWeight = (distance < 8.0f) ? 50.0f : 25.0f;
			float burstWeight = 30.0f;
			float snipeWeight = (distance > 7.5f) ? 40.0f : 0.0f;
			float roll = RandomRange(0.0f, spreadWeight + burstWeight + snipeWeight);
			if (roll < spreadWeight) {
				StartWindup(Attack::kSpread, kShooterSpreadWindup);
			} else if (roll < spreadWeight + burstWeight) {
				StartWindup(Attack::kBurst, kShooterBurstWindup);
			} else {
				StartWindup(Attack::kSnipe, kShooterSnipeWindup);
				dashDirection = AimDirection(p, kSnipeSpeed);
			}
		}
		break;
	}
	}
}

// ---------------------------------------------------------------- 群れ
void Enemy::UpdateSwarm(const EnemyPerception& p, const Vector3& direction, float distance, float deltaTime)
{
	stateTimer -= deltaTime;
	switch (state) {
	case State::kMove: {
		attackCooldown -= deltaTime;
		facingAngle = std::atan2(direction.x, direction.z);
		// 許可がもらえたら、飛びかかる（みんな同時ではなく、順番に）
		if (attackCooldown <= 0.0f && p.canAttack && p.hasLineOfSight && distance < kSwarmLungeRange) {
			StartWindup(Attack::kLunge, kSwarmWindup);
			dashDirection = direction;
			break;
		}
		Vector3 move = { 0.0f, 0.0f, 0.0f };
		if (!p.hasLineOfSight || distance > 9.0f) {
			move = ChaseDirection(p, direction);
		} else {
			// プレイヤーのまわりの、割り当てられた場所（輪の上）へ。ふらふらしながら囲む
			Vector3 ringPoint = {
				p.targetPosition.x + std::sin(p.ringAngle) * kSwarmRing,
				0.0f,
				p.targetPosition.z + std::cos(p.ringAngle) * kSwarmRing };
			Vector3 toRing = { ringPoint.x - position.x, 0.0f, ringPoint.z - position.z };
			float ringDistance = Length(toRing);
			Vector3 side = { -direction.z, 0.0f, direction.x };
			if (ringDistance > 0.5f) {
				move = toRing / ringDistance + side * (std::sin(wobble * 5.0f) * 0.3f);
			} else {
				move = side * (std::sin(wobble * 3.0f) * 0.4f);
			}
		}
		if (Length(move) > 0.001f) {
			position += Normalize(move) * (data->moveSpeed * deltaTime);
		}
		break;
	}
	case State::kWindup:
		// ちょっとしゃがんで、狙いを定める
		dashDirection = AimDirection(p, kSwarmLungeSpeed);
		facingAngle = std::atan2(dashDirection.x, dashDirection.z);
		if (stateTimer <= 0.0f) {
			state = State::kDash;
			stateTimer = kSwarmLungeTime;
		}
		break;
	case State::kDash:
		position += dashDirection * (kSwarmLungeSpeed * deltaTime);
		if (stateTimer <= 0.0f) {
			StartRecover(0.5f, 1.3f, 2.6f);
		}
		break;
	default:
		state = State::kMove;
		break;
	}
}

// ---------------------------------------------------------------- 重装
void Enemy::UpdateTank(const EnemyPerception& p, const Vector3& direction, float distance, float deltaTime, ShotRequest& shot)
{
	switch (state) {
	case State::kWindup: {
		stateTimer -= deltaTime;
		facingAngle = std::atan2(direction.x, direction.z);
		// ため中は足元に輪が広がる（攻撃の合図）
		ringVisible = true;
		if (stateTimer > 0.0f) {
			break;
		}
		switch (attack) {
		case Attack::kSpiral:
			state = State::kFire;
			fireLeft = kTankSpiralTicks;
			fireTimer = 0.0f;
			spiralAngle = RandomRange(0.0f, kPi * 2.0f);
			break;
		case Attack::kFan: {
			Vector3 aim = AimDirection(p, 6.5f);
			shot.shoot = true;
			shot.direction = aim;
			shot.position = { position.x, 0.6f, position.z };
			shot.count = 5;
			shot.spread = 0.3f;
			shot.speed = 6.5f;
			StartRecover(0.6f, kTankIntervalMin, kTankIntervalMax);
			break;
		}
		case Attack::kRing:
		default: {
			// 周り全部にばらまく（毎回すき間の位置が変わるように、向きをずらす）
			float offset = RandomRange(0.0f, kPi * 2.0f);
			shot.shoot = true;
			shot.direction = { std::sin(offset), 0.0f, std::cos(offset) };
			shot.position = { position.x, 0.6f, position.z };
			shot.count = kTankShotCount;
			shot.spread = kPi * 2.0f / static_cast<float>(kTankShotCount);
			shot.speed = 5.5f;
			StartRecover(0.6f, kTankIntervalMin, kTankIntervalMax);
			break;
		}
		}
		break;
	}
	case State::kFire: {
		// 渦巻き：向きを少しずつ回しながら、3方向に撃ち続ける
		facingAngle = spiralAngle;
		fireTimer -= deltaTime;
		if (fireTimer <= 0.0f) {
			shot.shoot = true;
			shot.direction = { std::sin(spiralAngle), 0.0f, std::cos(spiralAngle) };
			shot.position = { position.x, 0.6f, position.z };
			shot.count = 3;
			shot.spread = kPi * 2.0f / 3.0f;
			shot.speed = 5.0f;
			spiralAngle += 0.45f;
			fireTimer = kTankSpiralGap;
			--fireLeft;
			if (fireLeft <= 0) {
				StartRecover(0.7f, kTankIntervalMin, kTankIntervalMax);
			}
		}
		break;
	}
	case State::kMove:
	default: {
		facingAngle = std::atan2(direction.x, direction.z);
		// ゆっくり近づく（近づきすぎず、ほどよい距離でのしのし構える）
		Vector3 move = { 0.0f, 0.0f, 0.0f };
		if (!p.hasLineOfSight || distance > 7.5f) {
			move = ChaseDirection(p, direction);
		} else if (distance < 3.5f) {
			move = -direction * 0.6f;
		}
		position += move * (data->moveSpeed * deltaTime);

		attackCooldown -= deltaTime;
		if (attackCooldown <= 0.0f && p.canAttack && distance < 16.0f) {
			float ringWeight = 45.0f;
			float spiralWeight = 30.0f;
			float fanWeight = p.hasLineOfSight ? 25.0f : 0.0f; // 扇形は、見えているときだけ
			float roll = RandomRange(0.0f, ringWeight + spiralWeight + fanWeight);
			if (roll < ringWeight) {
				StartWindup(Attack::kRing, kTankRingWindup);
			} else if (roll < ringWeight + spiralWeight) {
				StartWindup(Attack::kSpiral, kTankSpiralWindup);
			} else {
				StartWindup(Attack::kFan, kTankFanWindup);
			}
		}
		break;
	}
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
		float t = 1.0f - (std::max)(stateTimer, 0.0f) / (std::max)(windupTotal, 0.01f);
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
		rotate.x = (type == EnemyType::kSwarm) ? 0.3f : 0.35f;
		scale.z *= 1.2f;
	} else if (state == State::kFire && type == EnemyType::kTank) {
		// 渦巻き：ぶるぶる震えながら撃つ
		translate.x += std::sin(wobble * 60.0f) * 0.04f;
		scale.y *= 1.0f + std::sin(wobble * 30.0f) * 0.04f;
	} else if (state == State::kRecover && attack == Attack::kFeint && thenWindup) {
		// フェイント：のけぞってさがる
		rotate.x = -0.25f;
	} else if (type == EnemyType::kSwarm) {
		// 群れ：ぴょこぴょこ跳ねる
		translate.y += std::fabs(std::sin(wobble * 10.0f)) * 0.15f;
	}

	object->SetScale(scale);
	object->SetRotate(rotate);
	object->SetTranslate(translate);
	object->Update();
}

void Enemy::UpdateTelegraph()
{
	if (aimVisible) {
		aimLine->SetScale({ aimWidth, 1.0f, aimLength });
		aimLine->SetRotate({ 0.0f, std::atan2(dashDirection.x, dashDirection.z), 0.0f });
		aimLine->SetTranslate({ position.x, 0.0f, position.z });
		aimLine->Update();
	}
	if (ringVisible) {
		// ため中に、足元の輪が0.5倍から2.2倍まで広がる
		float t = 1.0f - (std::max)(stateTimer, 0.0f) / (std::max)(windupTotal, 0.01f);
		ringScale = 0.5f + 1.7f * t;
		ring->SetScale({ ringScale, 1.0f, ringScale });
		ring->SetRotate({ 0.0f, wobble * 2.0f, 0.0f });
		ring->SetTranslate({ position.x, 0.0f, position.z });
		ring->Update();
	}
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
	if (state == State::kDash) {
		// 突進・飛びかかり中に壁にぶつかったら止まって、すきができる（続きの突進もなくなる）
		thenWindup = false;
		dashesLeft = 0;
		if (type == EnemyType::kSwarm) {
			StartRecover(0.5f, 1.3f, 2.6f);
		} else {
			StartRecover(kChargerRecover, 0.9f, 1.8f);
		}
	} else if (state == State::kMove) {
		// 歩いていて壁に当たったら、回る向きを変える（壁に張りついたままにならないように）
		strafeSign = -strafeSign;
	}
}

void Enemy::SetPosition(const Vector3& newPosition)
{
	position = newPosition;
	UpdateModel();
}

void Enemy::Draw()
{
	if (!isAlive) {
		return;
	}
	// 予告は、体より先に描く（床に出るので、体の下になる）
	if (aimVisible) {
		aimLine->Draw();
	}
	if (ringVisible) {
		ring->Draw();
	}
	object->Draw();
}
