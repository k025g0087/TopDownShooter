#include "SceneFactory.h"
#include "TitleScene.h"
#include "StageScene.h"
#include "CharacterSelectScene.h"

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName)
{
	// 次のシーンを生成
	std::unique_ptr<BaseScene> newScene = nullptr;

	if (sceneName == "TITLE") {
		newScene = std::make_unique<TitleScene>();
	} else if (sceneName == "SELECT") {
		newScene = std::make_unique<CharacterSelectScene>();
	} else if (sceneName == "STAGE") {
		newScene = std::make_unique<StageScene>();
	}

	// 一致しなければnullptrを返す（unique_ptrなので、所有権ごと呼び出した側に渡る）
	return newScene;
}