#include "Game.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "ImGuiManager.h"
#include "SceneManager.h"
#include "SceneFactory.h"

void Game::Initialize()
{
	// 基盤システムの初期化
	Framework::Initialize();

	// シーンファクトリーを生成し、マネージャにセット
	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
	// シーンマネージャに最初のシーンをセット（初期化はシーンマネージャ内で呼ばれる）
	SceneManager::GetInstance()->ChangeScene("TITLE");
}

void Game::Finalize()
{
	// 基盤システムの終了（シーンマネージャの解放もFrameworkで行う）
	Framework::Finalize();
}

void Game::Update()
{
	// 基盤システムの更新（メッセージ処理、入力、シーンマネージャの更新）
	Framework::Update();
}

void Game::Draw()
{
	// 描画前処理
	dxCommon->PreDraw();
	// SRVのデスクリプタヒープをセット（1フレームに1度だけ）
	srvManager->PreDraw();

	// シーンマネージャの描画
	sceneManager_->Draw();

	// ImGuiの描画（開発用UIなので最前面に出るよう、全ての描画の最後に呼ぶ）
	imGuiManager->Draw();

	// 描画後処理
	dxCommon->PostDraw();
}