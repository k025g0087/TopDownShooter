#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include "BaseScene.h"
#include "Sprite.h"
#include "Object3d.h"

// 前方宣言（ヘッダではポインタしか使わないのでインクルードしない）
class DirectXCommon;
class SpriteCommon;

// タイトルシーン
class TitleScene : public BaseScene
{
public: // メンバ関数
	// 初期化
	void Initialize() override;

	// 終了
	void Finalize() override;

	// 毎フレーム更新
	void Update() override;

	// 描画
	void Draw() override;

private: // メンバ変数
	// エンジン層の汎用システム（シングルトンから取得する。deleteしない）
	DirectXCommon* dxCommon = nullptr;
	SpriteCommon* spriteCommon = nullptr;

	// タイトルのスプライト
	std::unique_ptr<Sprite> titleSprite;

	// 平行光源（スプライトのルートシグネチャで使うので、使わなくてもセットする必要がある）
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
	Object3d::DirectionalLight* directionalLightData = nullptr;
};