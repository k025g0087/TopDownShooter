#include "CharacterRig.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include "Object3dCommon.h"

using namespace MathUtility;

namespace {
	const float kPi = std::numbers::pi_v<float>;

	// Y軸回りに回す（エンジンの回転行列と同じ向き。角度 0 で +Z を向いているものが、angle の向きを向く）
	Vector3 RotateY(const Vector3& v, float angle)
	{
		float c = std::cos(angle), s = std::sin(angle);
		return { v.x * c + v.z * s, v.y, -v.x * s + v.z * c };
	}
	// X軸回りに回す（正の角度で、上にあるものが前（+Z）へ倒れる）
	Vector3 RotateX(const Vector3& v, float angle)
	{
		float c = std::cos(angle), s = std::sin(angle);
		return { v.x, v.y * c - v.z * s, v.y * s + v.z * c };
	}
	// 最初が速くて、最後がゆっくり
	float EaseOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
}

void CharacterRig::Initialize(const CharacterParts& sourceParts)
{
	parts = sourceParts;
	const std::string* models[kPartCount] = { &parts.body, &parts.foot, &parts.foot, &parts.rightHand, &parts.leftHand };
	for (size_t i = 0; i < kPartCount; ++i) {
		objects[i] = std::make_unique<Object3d>();
		objects[i]->Initialize(Object3dCommon::GetInstance());
		objects[i]->SetModel(*models[i]);
	}
	// 左足は右足の左右反対の位置
	baseOffsets[kBody] = parts.bodyOffset;
	baseOffsets[kFootRight] = parts.footOffset;
	baseOffsets[kFootLeft] = { -parts.footOffset.x, parts.footOffset.y, parts.footOffset.z };
	baseOffsets[kHandRight] = parts.rightHandOffset;
	baseOffsets[kHandLeft] = parts.leftHandOffset;
}

void CharacterRig::PlayAttack(float duration)
{
	attackDuration = (std::max)(0.05f, duration);
	attackTimer = attackDuration;
}

void CharacterRig::Update(const State& state, float deltaTime)
{
	idleTime += deltaTime;
	attackTimer = (std::max)(0.0f, attackTimer - deltaTime);
	// 歩きの強さを、実際の速さへなめらかに近づける（急に止まっても、足がピタッと止まらないように）
	float targetWalk = (std::min)(1.0f, state.moveRate);
	walkAmount += (targetWalk - walkAmount) * 0.2f;
	walkPhase += deltaTime * kStepSpeed * (std::min)(1.5f, state.moveRate);

	// 何もしていないときの置き方から始めて、動きを順に足していく
	std::array<Pose, kPartCount> poses;
	for (size_t i = 0; i < kPartCount; ++i) {
		poses[i].offset = baseOffsets[i];
	}
	if (state.rolling) {
		ApplyRoll(poses, state.rollProgress);
	} else {
		ApplyWalkAndIdle(poses);
		ApplyAttack(poses);
	}
	ApplyHurt(poses, state.hurt);

	// キャラの向きに回して、世界の位置にする
	for (size_t i = 0; i < kPartCount; ++i) {
		Vector3 world = state.position + RotateY(poses[i].offset, state.facing);
		objects[i]->SetScale(poses[i].scale);
		objects[i]->SetRotate({ poses[i].rotate.x, poses[i].rotate.y + state.facing, poses[i].rotate.z });
		objects[i]->SetTranslate(world);
		objects[i]->Update();
	}
}

void CharacterRig::ApplyWalkAndIdle(std::array<Pose, kPartCount>& poses)
{
	float w = walkAmount;
	float s = std::sin(walkPhase);
	float c = std::cos(walkPhase);

	// 足：前後に出す。前へ動いている間（cos が正）だけ持ち上げる。左足は半周ずらす
	poses[kFootRight].offset.z += s * kStepLength * w;
	poses[kFootRight].offset.y += (std::max)(0.0f, c) * kStepHeight * w;
	poses[kFootLeft].offset.z -= s * kStepLength * w;
	poses[kFootLeft].offset.y += (std::max)(0.0f, -c) * kStepHeight * w;

	// 体：1歩ごとにはずむ（1周で2回）。少し前かがみ
	float bob = std::abs(s) * kBobHeight * w;
	poses[kBody].offset.y += bob;
	poses[kBody].rotate.x += 0.1f * w;

	// 手：足と反対に振る（右足が前のときは右手が後ろ）。体と一緒にはずむ
	poses[kHandRight].offset.z -= s * kArmSwing * w;
	poses[kHandLeft].offset.z += s * kArmSwing * w;
	poses[kHandRight].offset.y += bob;
	poses[kHandLeft].offset.y += bob;

	// 待機：止まっているほど強く、息をするように体がふくらむ。手も少し上下する
	float idle = 1.0f - w;
	float breath = std::sin(idleTime * kBreathSpeed);
	poses[kBody].scale.y += breath * kBreathScale * idle;
	poses[kBody].offset.y += breath * 0.01f * idle;
	poses[kHandRight].offset.y += breath * 0.015f * idle;
	poses[kHandLeft].offset.y += std::sin(idleTime * kBreathSpeed + 0.6f) * 0.015f * idle;
}

void CharacterRig::ApplyAttack(std::array<Pose, kPartCount>& poses)
{
	if (attackTimer <= 0.0f) {
		return;
	}
	float p = 1.0f - attackTimer / attackDuration; // 0→1
	float wave = std::sin(p * kPi);                // 0→1→0
	// 動きの最初（すばやく構える）と最後（ゆっくり戻す）を、なめらかにつなぐ
	float blend = (std::min)({ 1.0f, p / 0.12f, (1.0f - p) / 0.3f });

	switch (parts.attackMotion) {
	case AttackMotion::kSwing: {
		// 剣を横に振る：右手を、右から左へ肩のまわりの弧に沿って動かす。剣は前へ倒して横向きにする
		float angle = 1.6f + (-1.4f - 1.6f) * EaseOut(p);   // +が右、-が左
		Vector3 shoulder = { 0.12f, 0.45f, 0.0f };
		Vector3 swingPosition = shoulder + Vector3{ std::sin(angle) * 0.38f, -0.05f, std::cos(angle) * 0.38f };
		Pose& hand = poses[kHandRight];
		hand.offset = Lerp(hand.offset, swingPosition, blend);
		hand.rotate.x += 1.4f * blend;
		hand.rotate.y += angle * blend;
		// 体も少しひねる
		poses[kBody].rotate.y += (0.25f - 0.55f * p) * blend;
		break;
	}
	case AttackMotion::kThrust: {
		// 杖を前へ突き出す：右手を前と上へ出して、杖の先を前へ倒す
		Pose& hand = poses[kHandRight];
		hand.offset.z += 0.32f * wave;
		hand.offset.y += 0.08f * wave;
		hand.offset.x -= 0.08f * wave;
		hand.rotate.x += 1.0f * wave;
		poses[kBody].rotate.x += 0.08f * wave;
		break;
	}
	case AttackMotion::kDraw: {
		// 弓を引く：弓を持った手を前の真ん中へ出して、もう片方の手で弦を引いてから放す
		// （弓を右手・左手のどちらに持っていても同じ動きになるように、左右を side で反対にする）
		float side = parts.weaponInLeftHand ? 1.0f : -1.0f; // 弦を引く手の側（右が +）
		Pose& bow = poses[parts.weaponInLeftHand ? kHandLeft : kHandRight];
		Pose& hand = poses[parts.weaponInLeftHand ? kHandRight : kHandLeft];
		bow.offset = Lerp(bow.offset, Vector3{ -side * 0.05f, 0.45f, 0.32f }, blend);
		float pull = (p < 0.6f) ? p / 0.6f : 1.0f - (p - 0.6f) / 0.4f; // 引いて（0→1）、放す（1→0）
		hand.offset = Lerp(hand.offset, Vector3{ 0.0f, 0.45f, 0.3f - 0.32f * pull }, blend);
		poses[kBody].rotate.y += side * 0.2f * blend;
		break;
	}
	}
}

void CharacterRig::ApplyHurt(std::array<Pose, kPartCount>& poses, float hurt)
{
	if (hurt <= 0.0f) {
		return;
	}
	// のけぞる：体を後ろへ倒して、全体を少し後ろへ。手は上がる
	poses[kBody].rotate.x -= 0.5f * hurt;
	for (Pose& pose : poses) {
		pose.offset.z -= 0.1f * hurt;
	}
	poses[kHandRight].offset.y += 0.1f * hurt;
	poses[kHandLeft].offset.y += 0.1f * hurt;
}

void CharacterRig::ApplyRoll(std::array<Pose, kPartCount>& poses, float progress)
{
	// 前へ1回転：回る中心（おなかのあたり）のまわりに、全部の部品をまとめて回す
	// 回っている間は体を丸める（部品を中心へ寄せる）。真ん中で少し跳ねる
	float angle = 2.0f * kPi * EaseOut(progress);
	float curl = 1.0f - 0.35f * std::sin(progress * kPi);
	float lift = 0.2f * std::sin(progress * kPi);
	Vector3 pivot = { 0.0f, kRollPivotHeight, 0.0f };
	for (Pose& pose : poses) {
		Vector3 relative = (pose.offset - pivot) * curl;
		pose.offset = pivot + RotateX(relative, angle) + Vector3{ 0.0f, lift, 0.0f };
		pose.rotate.x += angle;
	}
}

void CharacterRig::SetCamera(Camera* camera)
{
	for (std::unique_ptr<Object3d>& object : objects) {
		object->SetCamera(camera);
	}
}

void CharacterRig::Draw()
{
	for (std::unique_ptr<Object3d>& object : objects) {
		object->Draw();
	}
}