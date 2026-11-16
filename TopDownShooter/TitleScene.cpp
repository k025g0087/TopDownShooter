#include "TitleScene.h"
#include <cassert>
#include <string>
#include "WinApp.h"
#include "DirectXCommon.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "BlendMode.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "SceneManager.h"
#include "GameSettings.h"

namespace {
	// タイトル画像
	const std::string kTexturePathTitle = "resources/title.png";
}

void TitleScene::Initialize()
{
	// エンジン層の汎用システムは、シングルトンから取得する
	spriteCommon = SpriteCommon::GetInstance();
	dxCommon = spriteCommon->GetDxCommon();

	// テクスチャ読み込み
	TextureManager* textureManager = TextureManager::GetInstance();
	textureManager->LoadTexture(kTexturePathTitle);
	textureManager->LoadTexture(UiKit::kTextureFont);
	textureManager->LoadTexture(UiKit::kTextureWhite);
	// 転送コマンドを実行して完了を待ち、中間リソースを解放する
	dxCommon->ExecuteCommandList();
	textureManager->ReleaseIntermediateResources();

	// タイトルのスプライトを画面の中央に置く（ゲームプレイシーンとは違う画像を使う）
	titleSprite = std::make_unique<Sprite>();
	titleSprite->Initialize(spriteCommon, kTexturePathTitle);
	titleSprite->SetAnchorPoint({ 0.5f, 0.5f });
	titleSprite->SetPosition({ float(WinApp::kClientWidth) / 2.0f, float(WinApp::kClientHeight) / 2.0f });
	// 文字や四角を出す道具（設定ボタン・設定画面）
	ui.Initialize(spriteCommon);

	// 平行光源（スプライトはライティングしないが、ルートシグネチャに必要なので作っておく）
	directionalLightResource = dxCommon->CreateBufferResource(sizeof(Object3d::DirectionalLight));
	HRESULT hr = directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	assert(SUCCEEDED(hr));
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;
}

void TitleScene::Finalize()
{
	// スプライト解放
	titleSprite.reset();

	// 平行光源リソース解放
	directionalLightResource.Reset();
	directionalLightData = nullptr;
}

void TitleScene::Update()
{
	Input* input = Input::GetInstance();
	ui.BeginFrame();
	// 設定画面が開いている間は、そちらだけを動かす
	if (settingsMenu.IsOpen()) {
		settingsMenu.Update(input, &ui);
		titleSprite->Update();
		return;
	}
	// 設定ボタン（右下）：クリックか Tab で開く
	{
		const float x = 1090.0f, y = 650.0f, w = 170.0f, h = 46.0f;
		bool hover = UiKit::Inside(input->GetMousePosition(), x, y, w, h);
		ui.Rect(x, y, w, h, hover ? UiColor::kFocus : UiColor::kPanel);
		ui.Frame(x, y, w, h, 2.0f, UiColor::kBorder);
		ui.Text(JP("設定  (Tab)"), x + w / 2.0f, y + 9.0f, 24.0f, UiColor::kText, UiKit::Align::kCenter);
		if (input->TriggerKey(DIK_TAB) || (hover && input->TriggerMouse(Input::MouseButton::kLeft))) {
			settingsMenu.Open();
			titleSprite->Update();
			return;
		}
	}
	// スペースキー・パッドのAで、キャラ選択へ
	if (input->TriggerKey(DIK_SPACE) || input->TriggerPad(XINPUT_GAMEPAD_A)) {
		// シーン切り替え（次のシーン名を渡すだけ）
		SceneManager::GetInstance()->ChangeScene("SELECT");
	}

#ifdef USE_IMGUI
	ImGui::Begin("Title Scene");
	ImGui::Text("This is TitleScene");
	ImGui::Text("SPACE / Pad A: CharacterSelectScene");
	ImGui::End();
#endif

	// スプライトの更新
	titleSprite->Update();
}

void TitleScene::Draw()
{
	// スプライトの描画準備
	spriteCommon->CommonDrawSetting(kBlendModeNormal);
	// スプライトのルートシグネチャで使う平行光源をセットする
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
	// スプライトの描画
	titleSprite->Draw();
	// 設定ボタン・設定画面
	ui.Draw();
}
