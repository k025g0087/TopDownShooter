#include "D3DResourceLeakChecker.h"
#include <memory>
#include "Game.h"

// メインエントリー
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	// ゲームより先に宣言する（ゲームを全て解放した後で、解放し忘れがないかチェックするため）
	D3DResourceLeakChecker leakCheck;

	// ポリモーフィズムを使うため、基底クラス（Framework）のポインタで派生クラス（Game）を扱う
	std::unique_ptr<Framework> game = std::make_unique<Game>();

	// 実行（初期化、ゲームループ、後始末）
	game->Run();

	// gameはunique_ptrなので、関数を抜けるときに自動で解放される（leakCheckより先に解放される）

	return 0;
}