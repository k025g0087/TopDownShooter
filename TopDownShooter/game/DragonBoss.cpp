#include "DragonBoss.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include "Object3dCommon.h"

using namespace MathUtility;

namespace {
	const float kPi = std::numbers::pi_v<float>;

	// 段階ごとの長さ（秒）
	const float kIntroTime = 1.8f;
	const float kBreathWindupTime = 0.7f;
	const float kBreathTime = 1.6f;
	const float kSweepWindupTime = 0.6f;
	const float kSweepTime = 0.5f;
	const float kTakeoffTime = 0.6f;
	const float kFlyingTime = 3.0f;
	const float kLandingTime = 0.5f;
	const float kPhaseChangeTime = 1.5f;
	const float kDyingTime = 2.5f;

	// 角度の差を -π〜π にする
	float WrapAngle(float angle)
	{
		while (angle > kPi) { angle -= kPi * 2.0f; }
		while (angle < -kPi) { angle += kPi * 2.0f; }
		return angle;
	}
}

void DragonBoss::Initialize(const Vector3& startPosition, const Vector3& center, const Vector2& halfSize, uint32_t seed)
{
	random.seed(seed);
	position = startPosition;
	roomCenter = center;
	roomHalfSize = halfSize;
	facingAngle = kPi; // 手前（プレイヤーが入ってくる側）を向いて登場
	height = 0.0f;
	hp = kMaxHp;
	isPhase2 = false;
	time = 0.0f;

	body = std::make_unique<Object3d>();
	body->Initialize(Object3dCommon::GetInstance());
	body->SetModel(kModelBody);
	shadow = std::make_unique<Object3d>();
	shadow->Initialize(Object3dCommon::GetInstance());
	shadow->SetModel(kModelShadow);
	for (Warning& warning : warnings) {
		warning.active = false;
		warning.object = std::make_unique<Object3d>();
		warning.object->Initialize(Object3dCommon::GetInstance());
		warning.object->SetModel(kModelWarning);
	}

	ChangeState(State::kIntro, kIntroTime);
	UpdateModel();
}

float DragonBoss::RandomRange(float min, float max)
{
	std::uniform_real_distribution<float> distribution(min, max);
	return distribution(random);
}

void DragonBoss::ChangeState(State next, float duration)
{
	state = next;
	stateTimer = duration;
	stateDuration = (duration > 0.0f) ? duration : 1.0f;
	fireTimer = 0.0f;
}

void DragonBoss::Update(const Vector3& target, float deltaTime, Events& events)
{
	time += deltaTime;
	hitTimer -= deltaTime;
	// 第2形態は、段階の時間の進みも少し速い
	float stateDelta = (state == State::kIntro || state == State::kDying || state == State::kPhaseChange) ? deltaTime : deltaTime * PhaseRate();
	stateTimer -= stateDelta;

	Vector3 toTarget = { target.x - position.x, 0.0f, target.z - position.z };
	float distance = Length(toTarget);
	Vector3 forward = { std::sin(facingAngle), 0.0f, std::cos(facingAngle) };

	switch (state) {
	case State::kIntro:
		// 地面から出てきて、最後に吠える
		if (stateTimer <= 0.0f) {
			events.roar = true;
			ChangeState(State::kMove, 1.0f);
		}
		break;

	case State::kMove:
		TurnToward(target, 2.0f * deltaTime * PhaseRate());
		if (distance > 3.0f) {
			position += forward * (kMoveSpeed * PhaseRate() * deltaTime);
		}
		if (stateTimer <= 0.0f) {
			ChooseNextAttack(distance);
		}
		break;

	case State::kBreathWindup:
		TurnToward(target, 3.0f * deltaTime);
		if (stateTimer <= 0.0f) {
			ChangeState(State::kBreath, kBreathTime);
		}
		break;

	case State::kBreath: {
		// 口から炎を扇形にまき散らす。向きはゆっくりプレイヤーを追う
		TurnToward(target, (isPhase2 ? 1.4f : 0.8f) * deltaTime);
		fireTimer -= deltaTime;
		float interval = isPhase2 ? 0.035f : 0.055f;
		while (fireTimer <= 0.0f) {
			fireTimer += interval;
			float spread = isPhase2 ? 0.45f : 0.35f;
			float angle = facingAngle + RandomRange(-spread, spread);
			Shot shot;
			shot.position = { position.x + forward.x * 2.0f, 1.0f, position.z + forward.z * 2.0f };
			shot.direction = { std::sin(angle), 0.0f, std::cos(angle) };
			shot.speed = RandomRange(8.5f, 11.0f);
			shot.lifeTime = 1.3f;
			shot.style = 1;
			events.shots.push_back(shot);
		}
		if (stateTimer <= 0.0f) {
			ChangeState(State::kMove, RandomRange(1.0f, 1.6f));
		}
		break;
	}

	case State::kSweepWindup:
		if (stateTimer <= 0.0f) {
			ChangeState(State::kSweep, kSweepTime);
		}
		break;

	case State::kSweep:
		// その場で1回転（回転は UpdateModel で見せる）。第2形態は最後に弾の輪
		if (stateTimer <= 0.0f) {
			if (isPhase2) {
				AddRing(events, position, 16, 6.0f, RandomRange(0.0f, kPi), 0);
			}
			ChangeState(State::kMove, RandomRange(0.8f, 1.3f));
		}
		break;

	case State::kTakeoff:
		if (stateTimer <= 0.0f) {
			ChangeState(State::kFlying, kFlyingTime);
		}
		break;

	case State::kFlying: {
		// 空からプレイヤーの上へ動きつつ、炎の雨の予告を落とす
		Vector3 move = toTarget;
		float speed = 6.0f;
		if (Length(move) > speed * deltaTime) {
			move = Normalize(move) * (speed * deltaTime);
		}
		position += move;
		fireTimer -= deltaTime;
		if (fireTimer <= 0.0f) {
			fireTimer = isPhase2 ? 0.22f : 0.35f;
			// 1つ目はプレイヤーの真下、あとは周りにばらける
			Vector3 point = target;
			if (stateTimer < kFlyingTime - 0.1f) {
				point += Vector3{ RandomRange(-3.5f, 3.5f), 0.0f, RandomRange(-3.5f, 3.5f) };
			}
			SpawnWarning(point);
		}
		if (stateTimer <= 0.0f) {
			ChangeState(State::kLanding, kLandingTime);
		}
		break;
	}

	case State::kLanding:
		if (stateTimer <= 0.0f) {
			// 着地の衝撃波（第2形態は2重）
			events.explosions.push_back(position);
			AddRing(events, position, 20, 6.0f, 0.0f, 0);
			if (isPhase2) {
				AddRing(events, position, 20, 4.0f, kPi / 20.0f, 0);
			}
			ChangeState(State::kMove, RandomRange(1.0f, 1.5f));
		}
		break;

	case State::kPhaseChange:
		if (stateTimer <= 0.0f) {
			events.roar = true;
			AddRing(events, position, 24, 5.0f, 0.0f, 1);
			ChangeState(State::kMove, 0.8f);
		}
		break;

	case State::kDying:
		// あちこちで爆発しながら倒れる
		fireTimer -= deltaTime;
		if (fireTimer <= 0.0f) {
			fireTimer = 0.18f;
			events.explosions.push_back(position + Vector3{ RandomRange(-1.5f, 1.5f), RandomRange(0.5f, 2.0f), RandomRange(-1.5f, 1.5f) });
		}
		if (stateTimer <= 0.0f) {
			events.roar = true;
			ChangeState(State::kDead, 0.0f);
		}
		break;

	case State::kDead:
	default:
		break;
	}

	// 飛んでいる高さ（飛び上がる・空中・着地）
	if (state == State::kTakeoff) {
		height = kFlyHeight * (1.0f - stateTimer / stateDuration);
	} else if (state == State::kFlying) {
		height = kFlyHeight;
	} else if (state == State::kLanding) {
		height = kFlyHeight * (std::max)(stateTimer / stateDuration, 0.0f);
	} else {
		height = 0.0f;
	}

	UpdateWarnings(deltaTime, events);
	ClampToRoom();
	UpdateModel();
}

void DragonBoss::ChooseNextAttack(float distance)
{
	// 近ければなぎ払いが多め。同じ攻撃は続けにくくする
	float sweep = (distance < 5.5f) ? 60.0f : 5.0f;
	float breath = 45.0f;
	float rain = 30.0f;
	if (lastAttack == State::kSweepWindup) { sweep *= 0.3f; }
	if (lastAttack == State::kBreathWindup) { breath *= 0.4f; }
	if (lastAttack == State::kTakeoff) { rain *= 0.2f; }
	float total = sweep + breath + rain;
	float roll = RandomRange(0.0f, total);
	if (roll < sweep) {
		lastAttack = State::kSweepWindup;
		ChangeState(State::kSweepWindup, kSweepWindupTime);
	} else if (roll < sweep + breath) {
		lastAttack = State::kBreathWindup;
		ChangeState(State::kBreathWindup, kBreathWindupTime);
	} else {
		lastAttack = State::kTakeoff;
		ChangeState(State::kTakeoff, kTakeoffTime);
	}
}

void DragonBoss::AddRing(Events& events, const Vector3& center, int32_t count, float speed, float offset, int32_t style)
{
	for (int32_t i = 0; i < count; ++i) {
		float angle = offset + kPi * 2.0f * static_cast<float>(i) / static_cast<float>(count);
		Shot shot;
		shot.position = { center.x, 0.6f, center.z };
		shot.direction = { std::sin(angle), 0.0f, std::cos(angle) };
		shot.speed = speed;
		shot.lifeTime = 4.0f;
		shot.style = style;
		events.shots.push_back(shot);
	}
}

void DragonBoss::SpawnWarning(const Vector3& target)
{
	for (Warning& warning : warnings) {
		if (warning.active) {
			continue;
		}
		warning.active = true;
		warning.position = { target.x, 0.03f, target.z };
		warning.timer = kWarningTime;
		return;
	}
}

void DragonBoss::UpdateWarnings(float deltaTime, Events& events)
{
	for (Warning& warning : warnings) {
		if (!warning.active) {
			continue;
		}
		warning.timer -= deltaTime;
		if (warning.timer <= 0.0f) {
			// 爆発：その場所から弾が8方向に飛ぶ
			warning.active = false;
			events.explosions.push_back(warning.position);
			AddRing(events, warning.position, 8, 5.0f, RandomRange(0.0f, kPi), 1);
			continue;
		}
		// 予告の輪：だんだん大きくなって、爆発の直前に点滅する
		float t = 1.0f - warning.timer / kWarningTime;
		float scale = 0.4f + 0.6f * t;
		if (t > 0.75f && static_cast<int32_t>(time * 20.0f) % 2 == 0) {
			scale *= 1.1f;
		}
		warning.object->SetScale({ scale, 1.0f, scale });
		warning.object->SetRotate({ 0.0f, time * 2.0f, 0.0f });
		warning.object->SetTranslate(warning.position);
		warning.object->Update();
	}
}

void DragonBoss::ClampToRoom()
{
	float limitX = (std::max)(roomHalfSize.x - kRadius, 0.0f);
	float limitZ = (std::max)(roomHalfSize.y - kRadius, 0.0f);
	position.x = std::clamp(position.x, roomCenter.x - limitX, roomCenter.x + limitX);
	position.z = std::clamp(position.z, roomCenter.z - limitZ, roomCenter.z + limitZ);
}

void DragonBoss::TurnToward(const Vector3& target, float maxTurn)
{
	float targetAngle = std::atan2(target.x - position.x, target.z - position.z);
	float diff = WrapAngle(targetAngle - facingAngle);
	facingAngle += std::clamp(diff, -maxTurn, maxTurn);
}

void DragonBoss::UpdateModel()
{
	float t = 1.0f - (std::max)(stateTimer, 0.0f) / stateDuration; // 今の段階の進み（0→1）
	Vector3 scale = { 1.0f, 1.0f, 1.0f };
	Vector3 rotate = { 0.0f, facingAngle, 0.0f };
	Vector3 offset = { 0.0f, height, 0.0f };

	switch (state) {
	case State::kIntro:
		// 地面からせり上がる
		offset.y = -3.0f * (1.0f - (std::min)(t * 1.5f, 1.0f));
		if (t > 0.7f) {
			float roar = (t - 0.7f) / 0.3f;
			scale = { 1.0f + 0.1f * roar, 1.0f + 0.1f * roar, 1.0f + 0.1f * roar };
		}
		break;
	case State::kMove:
		// のしのし歩く
		offset.y += std::fabs(std::sin(time * 4.0f * PhaseRate())) * 0.12f;
		break;
	case State::kBreathWindup:
		rotate.x = -0.35f * t; // 首を引く
		break;
	case State::kBreath:
		rotate.x = 0.1f;
		offset += Vector3{ std::sin(time * 50.0f) * 0.04f, 0.0f, 0.0f };
		break;
	case State::kSweepWindup:
		scale = { 1.0f + 0.1f * t, 1.0f - 0.15f * t, 1.0f + 0.1f * t }; // しゃがむ
		rotate.y -= 0.4f * t;
		break;
	case State::kSweep:
		rotate.y += kPi * 2.0f * t - 0.4f * (1.0f - t); // 1回転
		break;
	case State::kPhaseChange:
		// 吠えて震える
		scale = { 1.1f, 1.1f, 1.1f };
		offset += Vector3{ std::sin(time * 60.0f) * 0.08f, 0.0f, std::cos(time * 53.0f) * 0.08f };
		rotate.x = -0.3f;
		break;
	case State::kDying:
		offset += Vector3{ std::sin(time * 70.0f) * 0.1f, -0.6f * t, 0.0f };
		rotate.z = 0.5f * t;
		break;
	default:
		break;
	}

	// 当たったときの膨らみ
	if (hitTimer > 0.0f) {
		float pop = 1.0f + 0.06f * (hitTimer / 0.1f);
		scale = scale * pop;
	}

	body->SetScale(scale);
	body->SetRotate(rotate);
	body->SetTranslate(position + offset);
	body->Update();

	// 影：飛ぶと小さくなる
	float shadowScale = 1.0f - 0.5f * (height / kFlyHeight);
	shadow->SetScale({ shadowScale, 1.0f, shadowScale });
	shadow->SetTranslate({ position.x, 0.02f, position.z });
	shadow->Update();
}

void DragonBoss::Draw()
{
	if (state == State::kDead) {
		return;
	}
	if (height > 0.1f) {
		shadow->Draw();
	}
	body->Draw();
	for (Warning& warning : warnings) {
		if (warning.active) {
			warning.object->Draw();
		}
	}
}

void DragonBoss::SetCamera(Camera* camera)
{
	body->SetCamera(camera);
	shadow->SetCamera(camera);
	for (Warning& warning : warnings) {
		warning.object->SetCamera(camera);
	}
}

bool DragonBoss::Hit(float damage)
{
	if (!IsHittable()) {
		return false;
	}
	hp -= damage;
	hitTimer = 0.1f;
	// HPが半分を切ったら第2形態へ
	if (!isPhase2 && hp <= kMaxHp * 0.5f && hp > 0.0f) {
		isPhase2 = true;
		for (Warning& warning : warnings) {
			warning.active = false;
		}
		ChangeState(State::kPhaseChange, kPhaseChangeTime);
	}
	if (hp <= 0.0f) {
		hp = 0.0f;
		for (Warning& warning : warnings) {
			warning.active = false;
		}
		ChangeState(State::kDying, kDyingTime);
		return true;
	}
	return false;
}

bool DragonBoss::IsHittable() const
{
	// 登場中・変化中・空にいる間・倒れた後は当たらない
	if (state == State::kIntro || state == State::kPhaseChange || state == State::kDying || state == State::kDead) {
		return false;
	}
	return height < 1.5f;
}

bool DragonBoss::IsTouchDangerous() const
{
	if (state == State::kIntro || state == State::kDying || state == State::kDead) {
		return false;
	}
	return height < 1.0f;
}

float DragonBoss::GetAreaAttackRadius() const
{
	return (state == State::kSweep) ? kSweepRadius : 0.0f;
}
