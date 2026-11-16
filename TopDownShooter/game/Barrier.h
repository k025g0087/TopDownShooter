#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include "Mathutility.h"
#include "Object3d.h"

class Camera;

// 宝箱で手に入るバリア
// ・1枚につき、ダメージを1回なくす（そのとき場にある敵の弾も全部消す。消すのはシーンの方）
// ・重ねられる。枚数の分だけ、プレイヤーのまわりの五角形の球が重なって見える
class Barrier
{
public:
	void Initialize();
	// 枚数を 0 にする（シーンの最初）
	void Reset();
	// 1枚足す
	void Add();
	// 1枚使う。持っていなければ false（＝バリアなし。ふつうにダメージを受ける）
	bool Consume();

	int32_t GetCount() const { return count; }

	// center：プレイヤーの位置
	void Update(const Vector3& center, float deltaTime);
	void SetCamera(Camera* camera);
	// 半透明で描くので、不透明なものを全部描いた後で呼ぶ
	void Draw();

	static inline const char* kModel = "test_barrier.obj";
	// 割れる演出用（形は同じ。色を別に変えたいので、別のモデルとして読む）
	static inline const char* kModelBreak = "test_barrier_break.obj";
	static inline const int32_t kMaxLayers = 5; // 見た目に出す層の数の上限（枚数はいくらでも持てる）

private:
	// 1つの層（五角形の球）
	struct Layer {
		std::unique_ptr<Object3d> object;
		float appear = 0.0f; // 0→1 で出てくる（手に入れたときにふくらむ）
		bool ready = false;  // Update で位置・大きさを計算したか（まだなら描かない）
	};
	std::array<Layer, kMaxLayers> layers;
	// 割れたときの破片代わりの球（ふくらみながら消える）
	std::unique_ptr<Object3d> breakObject;
	float breakTimer = 0.0f;
	bool breakReady = false; // 割れる演出の位置・大きさを計算したか（まだなら描かない）

	int32_t count = 0;
	float time = 0.0f;
	Vector3 position = { 0.0f, 0.0f, 0.0f };

	static inline const float kBaseRadius = 1.0f;   // 一番内側の層の大きさ（モデルは半径1）
	static inline const float kLayerStep = 0.22f;   // 層ごとに大きくする量
	static inline const float kHeight = 0.7f;       // 球の中心の高さ
	static inline const float kBreakTime = 0.35f;   // 割れる演出の長さ（秒）
};