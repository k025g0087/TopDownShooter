#include "Player.h"
#include <cmath>
#include <numbers>
#include "Object3dCommon.h"

using namespace MathUtility;

void Player::Initialize(const Vector3& startPosition, const CharacterData& character)
{
	object = std::make_unique<Object3d>();
	object->Initialize(Object3dCommon::GetInstance());
	object->SetModel(character.modelName);
	stats = character.stats;
	hp = stats.maxHp;
	isAlive = true;
	damageInvincibleTimer = 0.0f;
	position = startPosition;
	UpdateModel();
}

Player::ShotRequest Player::Update(const PlayerInput& input, float deltaTime)
{
	ShotRequest shot;
	damageInvincibleTimer -= deltaTime;
	// 倒れた後は何もしない
	if (!isAlive) {
		return shot;
	}

	// ロールの開始（ロール中でなければ。入力がなければ向いている方へ転がる）
	if (input.roll && !isRolling) {
		isRolling = true;
		rollTimer = 0.0f;
		if (Length(input.move) > 0.1f) {
			Vector2 move = Normalize(input.move);
			rollDirection = { move.x, 0.0f, move.y };
		} else {
			rollDirection = { std::sin(facingAngle), 0.0f, std::cos(facingAngle) };
		}
	}

	if (isRolling) {
		UpdateRoll(deltaTime);
	} else {
		UpdateMove(input, deltaTime);
	}

	// 向き：ロール中は転がる方向、それ以外は狙う方向
	if (isRolling) {
		facingAngle = std::atan2(rollDirection.x, rollDirection.z);
	} else if (Length(input.aim) > 0.0f) {
		facingAngle = std::atan2(input.aim.x, input.aim.y);
	}

	// 撃つ（ロール中は撃てない）
	fireCooldown -= deltaTime;
	if (input.fire && !isRolling && fireCooldown <= 0.0f && Length(input.aim) > 0.0f) {
		fireCooldown = stats.fireInterval;
		Vector3 direction = { input.aim.x, 0.0f, input.aim.y };
		shot.shoot = true;
		shot.direction = direction;
		shot.position = { position.x + direction.x * kMuzzleDistance, kShotHeight, position.z + direction.z * kMuzzleDistance };
	}

	UpdateModel();
	return shot;
}

void Player::UpdateMove(const PlayerInput& input, float deltaTime)
{
	// 目標の速さへすばやく近づける（止まる・曲がるがすぐ効く）
	Vector3 target = { input.move.x * stats.moveSpeed, 0.0f, input.move.y * stats.moveSpeed };
	velocity = Lerp(velocity, target, kAcceleration);
	position += velocity * deltaTime;
}

void Player::UpdateRoll(float deltaTime)
{
	rollTimer += deltaTime;
	float t = rollTimer / kRollTime;
	// 最初が一番速く、だんだん遅くなる
	float speed = kRollSpeed * (1.0f - 0.6f * t);
	velocity = rollDirection * speed;
	position += velocity * deltaTime;
	if (rollTimer >= kRollTime) {
		isRolling = false;
		// ロールの勢いを少しだけ残す
		velocity = rollDirection * stats.moveSpeed * 0.5f;
	}
}

void Player::SetPosition(const Vector3& newPosition)
{
	position = newPosition;
	UpdateModel();
}

void Player::UpdateModel()
{
	Vector3 scale = { 1.0f, 1.0f, 1.0f };
	Vector3 rotate = { 0.0f, facingAngle, 0.0f };
	Vector3 translate = position;
	if (isRolling) {
		// ロール中：前へかがんで、つぶれて、小さく跳ねる（転がっているように見せる仮の動き）
		float t = rollTimer / kRollTime;
		float wave = std::sin(t * std::numbers::pi_v<float>);
		rotate.x = 0.9f * wave;
		scale = { 1.0f + 0.15f * wave, 1.0f - 0.35f * wave, 1.0f + 0.15f * wave };
		translate.y += 0.25f * wave;
	}
	object->SetScale(scale);
	object->SetRotate(rotate);
	object->SetTranslate(translate);
	object->Update();
}

bool Player::TakeDamage(int32_t amount)
{
	if (!CanTakeDamage()) {
		return false;
	}
	hp -= amount;
	damageInvincibleTimer = kDamageInvincibleTime;
	if (hp <= 0) {
		hp = 0;
		isAlive = false;
		return true;
	}
	return false;
}

void Player::Draw()
{
	if (!isAlive) {
		return;
	}
	// ダメージ直後の無敵中は点滅させる（描くフレームと描かないフレームを交互に）
	if (damageInvincibleTimer > 0.0f && static_cast<int32_t>(damageInvincibleTimer * 15.0f) % 2 == 0) {
		return;
	}
	object->Draw();
}