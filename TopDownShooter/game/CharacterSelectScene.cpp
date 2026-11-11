#include "CharacterSelectScene.h"
#include <cassert>
#include <cmath>
#include <numbers>
#include <string>
#include "WinApp.h"
#include "DirectXCommon.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "Model.h"
#include "Camera.h"
#include "CameraManager.h"
#include "Input.h"
#include "SceneManager.h"
#include "BlendMode.h"
#include "ImGuiManager.h"

using namespace MathUtility;

namespace {
	const std::string kCameraName = "select";
	const std::string kModelGround = "test_ground.obj";
	const std::string kModelCursor = "test_bullet.obj";
	const std::string kTextureTitle = "resources/select_title.png";
	const std::string kTextureHint = "resources/select_hint.png";

	// 選ばれていないキャラの色（暗くする）と、選ばれたキャラの色
	const Vector4 kDimColor = { 0.45f, 0.45f, 0.5f, 1.0f };
	const Vector4 kBrightColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	// カメラ：キャラの少し前・上から見る
	const Vector3 kCameraTarget = { 0.0f, 0.4f, 0.0f };
	const float kCameraDistance = 12.0f;
	const float kCameraPitch = 0.18f;
}

void CharacterSelectScene::Initialize()
{
	// エンジン層の汎用システムは、シングルトンから取得する
	spriteCommon = SpriteCommon::GetInstance();
	dxCommon = spriteCommon->GetDxCommon();
	object3dCommon = Object3dCommon::GetInstance();
	input = Input::GetInstance();

	// カメラ（2回目に入ったときは、前に作ったものを使う）
	CameraManager* cameraManager = CameraManager::GetInstance();
	camera = cameraManager->FindCamera(kCameraName);
	if (camera == nullptr) {
		camera = cameraManager->CreateCamera(kCameraName);
	}
	cameraManager->SetActiveCamera(kCameraName);
	Vector3 forward = { 0.0f, -std::sin(kCameraPitch), std::cos(kCameraPitch) };
	camera->SetRotate({ kCameraPitch, 0.0f, 0.0f });
	camera->SetTranslate(kCameraTarget - forward * kCameraDistance);
	camera->Update();

	// モデルの読み込み（2回目は自動で飛ばされる）
	ModelManager* modelManager = ModelManager::GetInstance();
	modelManager->LoadModel(kModelGround);
	modelManager->FindModel(kModelGround)->SetEnableLighting(0);
	modelManager->LoadModel(kModelCursor);
	modelManager->FindModel(kModelCursor)->SetEnableLighting(0);
	for (size_t i = 0; i < kCharacterCount; ++i) {
		const CharacterData& data = GetCharacterData(static_cast<CharacterType>(i));
		modelManager->LoadModel(data.modelName);
		modelManager->FindModel(data.modelName)->SetEnableLighting(2);
	}

	// テクスチャの読み込み（見出し・操作説明・3人分のカード）
	TextureManager* textureManager = TextureManager::GetInstance();
	textureManager->LoadTexture(kTextureTitle);
	textureManager->LoadTexture(kTextureHint);
	for (size_t i = 0; i < kCharacterCount; ++i) {
		textureManager->LoadTexture(GetCharacterData(static_cast<CharacterType>(i)).cardTexture);
	}
	// テクスチャの転送を実行して、中間リソースを解放する（読み込みの後でまとめて1回）
	dxCommon->ExecuteCommandList();
	textureManager->ReleaseIntermediateResources();

	// 床
	ground = std::make_unique<Object3d>();
	ground->Initialize(object3dCommon);
	ground->SetModel(kModelGround);
	ground->SetCamera(camera);
	ground->Update();

	// 3人のキャラを横に並べる
	selectedIndex = static_cast<size_t>(CharacterSelection::GetSelected());
	for (size_t i = 0; i < kCharacterCount; ++i) {
		characters[i] = std::make_unique<Object3d>();
		characters[i]->Initialize(object3dCommon);
		characters[i]->SetModel(GetCharacterData(static_cast<CharacterType>(i)).modelName);
		characters[i]->SetCamera(camera);
		characterScales[i] = (i == selectedIndex) ? kSelectedScale : kNormalScale;
	}

	// 選んでいる印（キャラの頭の上でくるくる回る）
	cursor = std::make_unique<Object3d>();
	cursor->Initialize(object3dCommon);
	cursor->SetModel(kModelCursor);
	cursor->SetCamera(camera);

	// スプライト
	const float centerX = float(WinApp::kClientWidth) / 2.0f;
	titleSprite = std::make_unique<Sprite>();
	titleSprite->Initialize(spriteCommon, kTextureTitle);
	titleSprite->SetAnchorPoint({ 0.5f, 0.0f });
	titleSprite->SetPosition({ centerX, 12.0f });

	cardSprite = std::make_unique<Sprite>();
	cardSprite->Initialize(spriteCommon, GetCharacterData(static_cast<CharacterType>(selectedIndex)).cardTexture);
	cardSprite->SetAnchorPoint({ 0.5f, 0.5f });

	hintSprite = std::make_unique<Sprite>();
	hintSprite->Initialize(spriteCommon, kTextureHint);
	hintSprite->SetAnchorPoint({ 0.5f, 1.0f });
	hintSprite->SetPosition({ centerX, float(WinApp::kClientHeight) - 8.0f });

	// 平行光源（スプライトはライティングしないが、ルートシグネチャに必要なので作っておく）
	directionalLightResource = dxCommon->CreateBufferResource(sizeof(Object3d::DirectionalLight));
	HRESULT hr = directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	assert(SUCCEEDED(hr));
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;

	time = 0.0f;
	cardBounce = 0.0f;
	wasStickTilted = false;
	UpdateCharacters();
}

void CharacterSelectScene::Finalize()
{
	// モデルの色は同じモデルを使う全部に効くので、ステージで暗いままにならないよう元に戻す
	ModelManager* modelManager = ModelManager::GetInstance();
	for (size_t i = 0; i < kCharacterCount; ++i) {
		Model* model = modelManager->FindModel(GetCharacterData(static_cast<CharacterType>(i)).modelName);
		if (model) {
			model->SetColor(kBrightColor);
		}
	}

	for (std::unique_ptr<Object3d>& character : characters) {
		character.reset();
	}
	cursor.reset();
	ground.reset();
	titleSprite.reset();
	cardSprite.reset();
	hintSprite.reset();
	directionalLightResource.Reset();
	directionalLightData = nullptr;
	camera = nullptr;
}

void CharacterSelectScene::Update()
{
	time += kDeltaTime;

	// 左右で選ぶ
	int32_t step = ReadHorizontalTrigger();
	if (step != 0) {
		ChangeSelection(step);
	}

	// 決定：選んだキャラを覚えて、ステージへ
	if (input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) || input->TriggerPad(XINPUT_GAMEPAD_A)) {
		CharacterSelection::SetSelected(static_cast<CharacterType>(selectedIndex));
		SceneManager::GetInstance()->ChangeScene("STAGE");
		return;
	}
	// 戻る：タイトルへ
	if (input->TriggerKey(DIK_ESCAPE) || input->TriggerPad(XINPUT_GAMEPAD_B)) {
		SceneManager::GetInstance()->ChangeScene("TITLE");
		return;
	}

	UpdateCharacters();

	// カード：切り替えた直後だけ少し大きくして、すぐ戻す
	cardBounce = (cardBounce > 0.0f) ? cardBounce - kDeltaTime * 5.0f : 0.0f;
	float bounce = 1.0f + 0.06f * cardBounce * cardBounce;
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(cardSprite->GetTextureFilePath());
	cardSprite->SetSize({ float(metadata.width) * bounce, float(metadata.height) * bounce });
	cardSprite->SetPosition({ float(WinApp::kClientWidth) / 2.0f, 540.0f });

	titleSprite->Update();
	cardSprite->Update();
	hintSprite->Update();

#ifdef USE_IMGUI
	ImGui::Begin("Character Select");
	ImGui::Text("Selected: %s", GetCharacterData(static_cast<CharacterType>(selectedIndex)).name);
	ImGui::Text("A/D, Left/Right, Stick: select  Enter/Space/Pad A: decide  Esc/Pad B: title");
	ImGui::End();
#endif
}

int32_t CharacterSelectScene::ReadHorizontalTrigger()
{
	int32_t step = 0;
	if (input->TriggerKey(DIK_A) || input->TriggerKey(DIK_LEFT) || input->TriggerPad(XINPUT_GAMEPAD_DPAD_LEFT)) {
		step = -1;
	}
	if (input->TriggerKey(DIK_D) || input->TriggerKey(DIK_RIGHT) || input->TriggerPad(XINPUT_GAMEPAD_DPAD_RIGHT)) {
		step = 1;
	}
	// スティック：倒した瞬間だけ1回動く（戻すまで次は動かない）
	float stickX = input->GetLeftStick().x;
	bool tilted = std::fabs(stickX) >= kStickThreshold;
	if (tilted && !wasStickTilted) {
		step = (stickX < 0.0f) ? -1 : 1;
	}
	wasStickTilted = tilted;
	return step;
}

void CharacterSelectScene::ChangeSelection(int32_t step)
{
	// 端まで行ったら反対側へ回る
	int32_t count = static_cast<int32_t>(kCharacterCount);
	int32_t next = (static_cast<int32_t>(selectedIndex) + step + count) % count;
	selectedIndex = static_cast<size_t>(next);
	cardSprite->ChangeTexture(GetCharacterData(static_cast<CharacterType>(selectedIndex)).cardTexture);
	cardBounce = 1.0f;
}

void CharacterSelectScene::UpdateCharacters()
{
	ModelManager* modelManager = ModelManager::GetInstance();
	for (size_t i = 0; i < kCharacterCount; ++i) {
		bool selected = (i == selectedIndex);
		float x = (static_cast<float>(i) - static_cast<float>(kCharacterCount - 1) / 2.0f) * kCharacterSpacing;

		// 大きさをなめらかに変える
		float targetScale = selected ? kSelectedScale : kNormalScale;
		characterScales[i] = Lerp(characterScales[i], targetScale, 0.2f);

		// 選ばれたキャラは跳ねて左右に揺れる。他はこっちを向いて止まる
		float bob = selected ? std::fabs(std::sin(time * 4.0f)) * 0.25f : 0.0f;
		float sway = selected ? std::sin(time * 2.0f) * 0.5f : 0.0f;
		Object3d* object = characters[i].get();
		object->SetScale({ characterScales[i], characterScales[i], characterScales[i] });
		// モデルは +Z が前なので、π回してカメラ（-Z側）の方を向ける
		object->SetRotate({ 0.0f, std::numbers::pi_v<float> + sway, 0.0f });
		object->SetTranslate({ x, bob, 0.0f });
		object->Update();

		// 色：選ばれていないキャラを暗くする（3人ともモデルが別なので、モデルの色で変えられる）
		Model* model = modelManager->FindModel(GetCharacterData(static_cast<CharacterType>(i)).modelName);
		if (model) {
			model->SetColor(selected ? kBrightColor : kDimColor);
		}
	}

	// 印：選ばれたキャラの頭の上
	float cursorX = (static_cast<float>(selectedIndex) - static_cast<float>(kCharacterCount - 1) / 2.0f) * kCharacterSpacing;
	cursor->SetScale({ 1.8f, 1.8f, 1.8f });
	cursor->SetRotate({ 0.0f, time * 3.0f, 0.0f });
	cursor->SetTranslate({ cursorX, 2.95f + std::sin(time * 3.0f) * 0.12f, 0.0f });
	cursor->Update();
}

void CharacterSelectScene::Draw()
{
	// 3D
	object3dCommon->CommonDrawSetting();
	object3dCommon->SetPipelineState(kBlendModeNormal, false);
	ground->Draw();
	for (std::unique_ptr<Object3d>& character : characters) {
		character->Draw();
	}
	cursor->Draw();

	// 2D（3Dの上に重ねる）
	spriteCommon->CommonDrawSetting(kBlendModeNormal);
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
	titleSprite->Draw();
	cardSprite->Draw();
	hintSprite->Draw();
}
