#pragma once
#include "Framework.h"

// ゲーム（このゲーム固有の部分。汎用部分はFrameworkが受け持つ）
class Game : public Framework
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

protected: // メンバ関数
	// タイトルバーに表示する文字列
	const wchar_t* GetWindowTitle() const override { return L"TopDownShooter"; }
	// 起動時はフルスクリーン（ウィンドウで起動したいときは false にする）
	bool StartInFullscreen() const override { return true; }
};