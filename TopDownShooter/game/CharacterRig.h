#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include "Mathutility.h"
#include "Object3d.h"
#include "CharacterData.h"

class Camera;

// キャラの体を部品（体・右足・左足・右手・左手）に分けて動かす
// ・歩き：足を交互に出して、体を上下にはずませる。手は足と反対に振る
// ・待機：止まっているときは、ゆっくり息をするように体がふくらむ
// ・攻撃：キャラごとの動き（剣を振る／杖を突き出す／弓を引く）
// ・ロール：体を丸めて前に1回転する
// ・被ダメ：体がのけぞる
class CharacterRig
{
public:
	// 毎フレーム、プレイヤーから受け取る情報
	struct State {
		Vector3 position = { 0.0f, 0.0f, 0.0f }; // 足元の位置
		float facing = 0.0f;       // 向いている角度（Y軸回り。0で +Z）
		float moveRate = 0.0f;     // 歩く速さの割合（0：止まっている、1：ふつうに歩いている）
		bool rolling = false;      // ロール中か
		float rollProgress = 0.0f; // ロールの進み具合（0→1）
		float hurt = 0.0f;         // のけぞりの強さ（ダメージを受けた瞬間が1で、だんだん0へ）
	};

	void Initialize(const CharacterParts& parts);
	// 攻撃の動きを最初から始める（duration：動きの長さ（秒））
	void PlayAttack(float duration);
	void Update(const State& state, float deltaTime);
	void SetCamera(Camera* camera);
	void Draw();

private:
	enum Part { kBody, kFootRight, kFootLeft, kHandRight, kHandLeft, kPartCount };

	// 1つの部品の、体の中での置き方（キャラの向きを 0 としたときの値）
	struct Pose {
		Vector3 offset = { 0.0f, 0.0f, 0.0f };
		Vector3 rotate = { 0.0f, 0.0f, 0.0f };
		Vector3 scale = { 1.0f, 1.0f, 1.0f };
	};

	void ApplyWalkAndIdle(std::array<Pose, kPartCount>& poses);
	void ApplyAttack(std::array<Pose, kPartCount>& poses);
	void ApplyHurt(std::array<Pose, kPartCount>& poses, float hurt);
	void ApplyRoll(std::array<Pose, kPartCount>& poses, float progress);

	CharacterParts parts;
	std::array<std::unique_ptr<Object3d>, kPartCount> objects;
	std::array<Vector3, kPartCount> baseOffsets;

	float walkPhase = 0.0f;  // 歩きの周期（足を出すタイミング）
	float walkAmount = 0.0f; // 歩きの動きの強さ（止まる・歩き出すときに、なめらかに変える）
	float idleTime = 0.0f;
	float attackTimer = 0.0f;
	float attackDuration = 0.2f;

	// ---- 調整用の数値 ----
	static inline const float kStepSpeed = 14.0f;    // 足を動かす速さ（ラジアン／秒）
	static inline const float kStepLength = 0.12f;   // 足を前後に出す幅
	static inline const float kStepHeight = 0.07f;   // 足を上げる高さ
	static inline const float kBobHeight = 0.04f;    // 歩くときに体がはずむ高さ
	static inline const float kArmSwing = 0.07f;     // 歩くときに手を振る幅
	static inline const float kBreathSpeed = 2.5f;   // 待機の息の速さ
	static inline const float kBreathScale = 0.025f; // 待機の息で体がふくらむ割合
	static inline const float kRollPivotHeight = 0.45f; // ロールで回る中心の高さ
};
