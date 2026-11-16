#pragma once
#include <array>
#include <functional>
#include <memory>
#include <string>
#include "Mathutility.h"
#include "Object3d.h"
#include "CharacterData.h"

class Camera;

// シークレット：伝説の武器（ガチャでとても低い確率で、虹色のカプセルから出る。1回の挑戦で1つだけ）
// キャラごとに効果が違う。強いけれど、ゲームが簡単になりすぎないように「新しい動きが増える」形にしている
//   ナイト「聖剣」  ：斬撃が敵の弾をかき消す（これは StageScene で弾どうしを見る）
//   ウィッチ「星の杖」：爆発した場所に、少しの間炎が残る（自分には当たらない）
//   アーチャー「天弓」：矢が消えた場所に、小さな矢の雨が降る
// このクラスは、炎と矢の雨の管理と、見た目（キラキラ）を受け持つ
class LegendaryWeapon
{
public:
	// 使う画像・モデル（シーンで読み込む）
	static const char* GetBigIconTexture(CharacterType character);
	static const char* GetIconTexture(CharacterType character);
	static const char* GetGetTexture(CharacterType character);
	static const char* GetDescriptionTexture(CharacterType character);
	static inline const char* kTextureSecret = "resources/gacha_secret.png";
	static inline const char* kModelFireZone = "test_fire_zone.obj";
	// パーティクルのグループの名前（シーンで作る）
	static inline const char* kParticleSparkle = "legend_sparkle";
	static inline const char* kParticleFire = "legend_fire";

	// 色相（0〜1。1周で元に戻る）から、あざやかな色を作る（虹色に変わる演出で使う）
	static Vector4 HueToColor(float hue);

	// 当たり判定をシーンにまかせるための関数（中心・半径・ダメージ）
	using AreaDamageFunction = std::function<void(const Vector3& center, float radius, float damage)>;
	// その場所が壁かを聞く関数（矢の雨が壁の上に降らないように）
	using SolidCheckFunction = std::function<bool(const Vector3& position)>;

	// arrowModel：矢の雨に使うモデル（アーチャーの矢）
	void Initialize(CharacterType character, const std::string& arrowModel);
	// 新しい挑戦：持っていない状態に戻す
	void Reset();
	// フロアが変わった：残っている炎と矢の雨を消す
	void ClearEffects();

	void Obtain() { owned = true; }
	bool IsOwned() const { return owned; }
	CharacterType GetCharacter() const { return character; }

	// ウィッチ：炎を残す（damagePerTick：0.3秒ごとのダメージ）
	void AddFireZone(const Vector3& center, float radius, float damagePerTick);
	// アーチャー：矢の雨を降らせる（damage：矢1本のダメージ）
	void AddArrowRain(const Vector3& center, float damage);

	// 更新（playerPosition：キラキラを出す場所）
	void Update(float deltaTime, const Vector3& playerPosition, const AreaDamageFunction& damageArea, const SolidCheckFunction& isSolid);
	void SetCamera(Camera* camera);
	void Draw();

private:
	// 残っている炎1つ
	struct FireZone {
		std::unique_ptr<Object3d> object;
		Vector3 center = { 0.0f, 0.0f, 0.0f };
		float radius = 0.0f;
		float damage = 0.0f;
		float time = 0.0f;      // 出てからの時間（0なら使っていない）
		float tickTimer = 0.0f; // 次にダメージを与えるまで
		float emitTimer = 0.0f; // 次に炎のパーティクルを出すまで
		bool active = false;
	};
	// 降ってくる矢1本
	struct RainArrow {
		std::unique_ptr<Object3d> object;
		Vector3 target = { 0.0f, 0.0f, 0.0f };
		float damage = 0.0f;
		float delay = 0.0f;  // 降り始めるまで
		float fall = 0.0f;   // 降り始めてからの時間
		bool active = false;
	};

	CharacterType character = CharacterType::kKnight;
	bool owned = false;
	float time = 0.0f;
	float sparkleTimer = 0.0f;
	static inline const size_t kMaxFireZones = 6;
	static inline const size_t kMaxRainArrows = 30;
	std::array<FireZone, kMaxFireZones> fireZones;
	std::array<RainArrow, kMaxRainArrows> rainArrows;

	// ---- 調整用の数値 ----
	static inline const float kFireDuration = 1.5f;   // 炎が残る時間
	static inline const float kFireTick = 0.3f;       // 炎がダメージを与える間隔
	static inline const int32_t kRainCount = 5;       // 矢の雨の本数
	static inline const float kRainSpread = 1.6f;     // 矢の雨が降る範囲（半径）
	static inline const float kRainHitRadius = 0.9f;  // 矢1本が当たる範囲
	static inline const float kRainFallTime = 0.28f;  // 矢が空から落ちてくるまで
	static inline const float kRainHeight = 7.0f;     // 矢が降り始める高さ
};
