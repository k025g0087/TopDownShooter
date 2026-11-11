#pragma once
#include <array>
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include "BaseScene.h"
#include "Mathutility.h"
#include "Object3d.h"
#include "Sprite.h"
#include "CharacterData.h"

// 前方宣言（ヘッダではポインタしか使わないのでインクルードしない）
class DirectXCommon;
class SpriteCommon;
class Object3dCommon;
class Input;
class Camera;

// キャラ選択シーン（3人を並べて、左右で選んで決定する）
class CharacterSelectScene : public BaseScene
{
public: // メンバ関数
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private: // メンバ関数
	// 左右の入力（-1：左／+1：右／0：なし）。押した瞬間だけ返す
	int32_t ReadHorizontalTrigger();
	// 選んでいるキャラを変える
	void ChangeSelection(int32_t step);
	// キャラのモデルの動き
	void UpdateCharacters();

private: // メンバ変数
	// エンジン層の汎用システム（シングルトンから取得する。deleteしない）
	DirectXCommon* dxCommon = nullptr;
	SpriteCommon* spriteCommon = nullptr;
	Object3dCommon* object3dCommon = nullptr;
	Input* input = nullptr;
	// このシーンのカメラ（CameraManager が持っている。deleteしない）
	Camera* camera = nullptr;

	static inline const size_t kCharacterCount = static_cast<size_t>(CharacterType::kCount);

	// 3D：床・3人のキャラ・選んでいる印
	std::unique_ptr<Object3d> ground;
	std::array<std::unique_ptr<Object3d>, kCharacterCount> characters;
	std::unique_ptr<Object3d> cursor;
	// キャラごとの大きさ（選ばれると大きくなる。なめらかに変える）
	std::array<float, kCharacterCount> characterScales = {};

	// 2D：見出し・説明カード・操作説明
	std::unique_ptr<Sprite> titleSprite;
	std::unique_ptr<Sprite> cardSprite;
	std::unique_ptr<Sprite> hintSprite;
	// カードが切り替わったときに少し跳ねさせる
	float cardBounce = 0.0f;

	// 平行光源（スプライトのルートシグネチャで使うので、使わなくてもセットする必要がある）
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
	Object3d::DirectionalLight* directionalLightData = nullptr;

	// 選んでいるキャラの番号
	size_t selectedIndex = 0;
	// 経過時間（キャラを揺らすのに使う）
	float time = 0.0f;
	// スティックを倒し続けても1回だけ動くように、前のフレームで倒していたかを覚える
	bool wasStickTilted = false;

	// ---- 調整用の数値 ----
	static inline const float kDeltaTime = 1.0f / 60.0f;
	static inline const float kCharacterSpacing = 3.2f;   // キャラの横の間隔
	static inline const float kSelectedScale = 1.35f;      // 選ばれたキャラの大きさ
	static inline const float kNormalScale = 1.0f;         // 選ばれていないキャラの大きさ
	static inline const float kStickThreshold = 0.5f;      // スティックをこれ以上倒したら1回動く
};
